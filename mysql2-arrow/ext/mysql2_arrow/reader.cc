#include "mysql2_arrow.hpp"

#include <arrow/table_builder.h>
#include <arrow/type_traits.h>
#include <arrow/util/decimal.h>
#include <arrow/util/task-group.h>
#include <arrow/util/thread-pool.h>

#undef _
#include <arrow/vendored/datetime.h>

using arrow::Status;

namespace mysql2_arrow {

  namespace {
    unsigned int usec_char_to_uint(char* msec_char, size_t len) {
      for (size_t i = 0; i < (len - 1); ++i) {
        if (msec_char[i] == '\0') {
          msec_char[i] = '0';
        }
      }
      return (unsigned int)std::strtoul(msec_char, NULL, 10);
    }

    arrow::Status GetDataType(const MYSQL_FIELD& field, bool cast, bool cast_bool,
                              std::shared_ptr<arrow::DataType>* out) {
      const auto field_type = field.type;
      if (!cast) {
        if (field_type == MYSQL_TYPE_NULL) {
          *out = arrow::null();
        } else {
          *out = arrow::utf8();
        }
        return Status::OK();
      }

      const auto field_length = field.length;
      const auto unsigned_p = (0 != (field.flags & UNSIGNED_FLAG));
      switch (field_type) {
      case MYSQL_TYPE_TINY:
        if (cast_bool && field_length == 1) {
          *out = arrow::boolean();
        } else {
          *out = unsigned_p ? arrow::uint8() : arrow::int8();
        }
        return Status::OK();

      case MYSQL_TYPE_SHORT:
      case MYSQL_TYPE_YEAR:
        *out = unsigned_p ? arrow::uint16() : arrow::int16();
        return Status::OK();

      case MYSQL_TYPE_INT24:
      case MYSQL_TYPE_LONG:
        *out = unsigned_p ? arrow::uint32() : arrow::int32();
        return Status::OK();

      case MYSQL_TYPE_LONGLONG:
        *out = unsigned_p ? arrow::uint64() : arrow::int64();
        return Status::OK();

      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
        *out = std::make_shared<arrow::Decimal128Type>(field_length - 2, // sign + dot
                                                       field.decimals);
        return Status::OK();

      case MYSQL_TYPE_FLOAT:
        *out = arrow::float32();
        return Status::OK();

      case MYSQL_TYPE_DOUBLE:
        *out = arrow::float64();
        return Status::OK();

      case MYSQL_TYPE_BIT:
        if (cast_bool && field_length == 1) {
          *out = arrow::boolean();
        } else {
          *out = arrow::binary();
        }
        return Status::OK();

      case MYSQL_TYPE_TIMESTAMP:
        *out = std::make_shared<arrow::TimestampType>(arrow::TimeUnit::MICRO);
        return Status::OK();

      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_NEWDATE:
        // TODO: need to reconsider about data type for DATE field
        *out = arrow::date32();
        return Status::OK();

      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_DATETIME:
        *out = std::make_shared<arrow::TimestampType>(arrow::TimeUnit::MICRO);
        return Status::OK();

      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_VARCHAR:
        // TODO: encoding support
        if ((field.flags & BINARY_FLAG) != 0) {
          *out = arrow::binary();
        } else {
          *out = arrow::utf8();
        }
        return Status::OK();

      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
        *out = arrow::binary();
        return Status::OK();

      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_GEOMETRY:
        // TODO: support them
        return Status::Invalid("Unsupported MySQL data type");

      case MYSQL_TYPE_NULL:
        *out = arrow::null();
        return Status::OK();

      default:
        return Status::Invalid("Invalid MySQL data type: ", field.name, " (", field_type, ")");
      }
    }

    Status MakeArrowSchema(const MYSQL_FIELD* fields, unsigned int n_fields,
                           bool cast, bool cast_bool,
                           std::shared_ptr<arrow::Schema>* out) {
      std::vector<std::shared_ptr<arrow::Field>> arrow_fields;
      arrow_fields.reserve(n_fields);
      for (unsigned int i = 0; i < n_fields; ++i) {
        const bool nullable = !IS_NOT_NULL(fields[i].flags);
        std::shared_ptr<arrow::DataType> data_type;
        RETURN_NOT_OK(GetDataType(fields[i], cast, cast_bool, &data_type));
        arrow_fields.emplace_back(std::make_shared<arrow::Field>(
              fields[i].name, data_type, nullable));
      }
      *out = std::make_shared<arrow::Schema>(std::move(arrow_fields));
      return Status::OK();
    }
  }

  ///////////////////////////////////////////////////////////////////////
  // BaseMysqlResultReader class

  template <class ConcreteReader>
  class BaseMysqlResultReader : public MysqlResultReader {
  public:
    BaseMysqlResultReader(arrow::MemoryPool* pool, MYSQL_RES* result,
                          bool cast, bool cast_bool)
        : pool_(pool),
          result_(result),
          cast_(cast),
          cast_bool_(cast_bool),
          n_fields_(mysql_num_fields(result)),
          fields_(mysql_fetch_fields(result)) {}

    Status Read(std::shared_ptr<arrow::Table>* out) {
      RETURN_NOT_OK(MakeArrowSchema(fields_, n_fields_, cast_, cast_bool_, &schema_));

      auto concrete_this = static_cast<ConcreteReader*>(this);

      RETURN_NOT_OK(concrete_this->InitReader());

      while (!concrete_this->IsFinished()) {
        RETURN_NOT_OK(concrete_this->FetchNextBatch());
      }

      RETURN_NOT_OK(concrete_this->Finish(out));

      return Status::OK();
    }

  protected:
    arrow::MemoryPool* pool_;
    MYSQL_RES* const result_;
    bool const cast_;
    bool const cast_bool_;

    const unsigned int n_fields_;
    const MYSQL_FIELD* const fields_;

    std::shared_ptr<arrow::Schema> schema_;

  private:
  };

  ///////////////////////////////////////////////////////////////////////
  // ThreadedMysqlStoredResultReader class

  class ThreadedMysqlStoredResultReader
      : public BaseMysqlResultReader<ThreadedMysqlStoredResultReader> {
  private:
    using BaseClass = BaseMysqlResultReader<ThreadedMysqlStoredResultReader>;

    long detect_batch_size(arrow::internal::ThreadPool* thread_pool,
                           unsigned long n_rows, long batch_size) {
      if (batch_size > 0) {
        return batch_size;
      }

      const auto thread_pool_capacity = thread_pool->GetCapacity();
      return (n_rows + thread_pool_capacity - 1) / thread_pool_capacity;
    }

  public:
    ThreadedMysqlStoredResultReader(arrow::MemoryPool* pool, MYSQL_RES* result,
                                    arrow::internal::ThreadPool* thread_pool,
                                    bool cast, bool cast_bool, long batch_size)
        : BaseClass(pool, result, cast, cast_bool),
          thread_pool_(thread_pool),
          batch_size_(detect_batch_size(thread_pool, mysql_num_rows(result), batch_size)),
          finished_(false) {}

    Status InitReader() {
      task_group_ = arrow::internal::TaskGroup::MakeThreaded(thread_pool_);
      return Status::OK();
    }

    bool IsFinished() const { return finished_; }

    Status ProcessBatchField(arrow::NullBuilder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>&,
                             const std::vector<std::vector<unsigned long>>&) {
      return builder->AppendNulls(batch_size_);
    }

    Status ProcessBatchField(arrow::BooleanBuilder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        RETURN_NOT_OK(builder->Append(*row_batch[j][i] == 1));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::Int8Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        auto val = static_cast<int8_t>(std::strtol(row_batch[j][i], nullptr, 10));
        RETURN_NOT_OK(builder->Append(val));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::UInt8Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        auto val = static_cast<uint8_t>(std::strtoul(row_batch[j][i], nullptr, 10));
        RETURN_NOT_OK(builder->Append(val));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::Int16Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        auto val = static_cast<int16_t>(std::strtol(row_batch[j][i], nullptr, 10));
        RETURN_NOT_OK(builder->Append(val));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::UInt16Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        auto val = static_cast<uint16_t>(std::strtoul(row_batch[j][i], nullptr, 10));
        RETURN_NOT_OK(builder->Append(val));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::Int32Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        auto val = static_cast<int32_t>(std::strtol(row_batch[j][i], nullptr, 10));
        RETURN_NOT_OK(builder->Append(val));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::UInt32Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        auto val = static_cast<uint32_t>(std::strtoul(row_batch[j][i], nullptr, 10));
        RETURN_NOT_OK(builder->Append(val));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::Int64Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        auto val = static_cast<int64_t>(std::strtoll(row_batch[j][i], nullptr, 10));
        RETURN_NOT_OK(builder->Append(val));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::UInt64Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        auto val = static_cast<uint64_t>(std::strtoull(row_batch[j][i], nullptr, 10));
        RETURN_NOT_OK(builder->Append(val));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::Decimal128Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>& field_lengths) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        arrow::Decimal128 value;
        auto src = arrow::util::string_view(row_batch[j][i], field_lengths[j][i]);
        RETURN_NOT_OK(arrow::Decimal128::FromString(src, &value));
        RETURN_NOT_OK(builder->Append(value));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::FloatBuilder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        RETURN_NOT_OK(builder->Append(std::strtof(row_batch[j][i], nullptr)));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::DoubleBuilder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        RETURN_NOT_OK(builder->Append(std::strtod(row_batch[j][i], nullptr)));
      }
      return Status::OK();
    }

    // TODO: support timezone
    Status ProcessBatchField(arrow::TimestampBuilder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      unsigned int year, month, day, hour, min, sec;
      char usec_char[7] = {'0', '0', '0', '0', '0', '0', '\0'};
      int tokens;

      switch (fields_[i].type) {
      case MYSQL_TYPE_TIME:
        for (size_t j = 0; j < n; ++j) {
          hour = min = sec = 0;
          tokens = std::sscanf(row_batch[j][i], "%2u:%2u:%2u.%6s", &hour, &min, &sec, usec_char);
          if (tokens < 3) {
            RETURN_NOT_OK(builder->AppendNull());
            continue;
          }

          arrow::util::date::year_month_day ymd(
              arrow::util::date::year(2000),
              arrow::util::date::month(1),
              arrow::util::date::day(1));
          auto seconds = std::chrono::duration<int64_t>(3600U * hour + 60U * min + sec);
          auto tp = arrow::util::date::sys_days(ymd) + seconds;
          auto duration = tp.time_since_epoch();
          auto value = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
          auto usec = usec_char_to_uint(usec_char, sizeof(usec_char));
          RETURN_NOT_OK(builder->Append(value + usec));
        }
        return Status::OK();

      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_DATETIME:
        for (size_t j = 0; j < n; ++j) {
          year = month = day = hour = min = sec = 0;
          tokens = std::sscanf(row_batch[j][i], "%4u-%2u-%2u %2u:%2u:%2u.%6s",
                               &year, &month, &day, &hour, &min, &sec, usec_char);
          if (tokens < 6 // usec might be empty
              || year + month + day == 0) {
            RETURN_NOT_OK(builder->AppendNull());
            continue;
          } else if (month < 1 || day < 1) {
            return Status::Invalid("Invalid date in field '", fields_[i].name,
                                   "': ", row_batch[j][i]);
          }

          arrow::util::date::year_month_day ymd{
              arrow::util::date::year(year),
              arrow::util::date::month(month),
              arrow::util::date::day(day)};
          auto seconds = std::chrono::duration<int64_t>(3600U * hour + 60U * min + sec);
          auto tp = arrow::util::date::sys_days(ymd) + seconds;
          auto duration = tp.time_since_epoch();
          auto value = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
          auto usec = usec_char_to_uint(usec_char, sizeof(usec_char));
          RETURN_NOT_OK(builder->Append(value + usec));
        }
        return Status::OK();

      default:
        return Status::Invalid("Invalid MySQL field type for TimestampBuilder");
      }
    }

    Status ProcessBatchField(arrow::Date32Builder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>&) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        unsigned int year = 0, month = 0, day = 0;
        int tokens = std::sscanf(row_batch[j][i], "%4u-%2u-%2u", &year, &month, &day);
        if (tokens < 3 || year+month+day == 0) {
          RETURN_NOT_OK(builder->AppendNull());
          continue;
        } else if (month < 1 || day < 1) {
          return Status::Invalid("Invalid date in field '", fields_[i].name,
                                 "': ", row_batch[j][i]);
        }

        arrow::util::date::year_month_day ymd{
            arrow::util::date::year(year),
            arrow::util::date::month(month),
            arrow::util::date::day(day)};
        auto tp = arrow::util::date::sys_days(ymd);
        auto duration = tp.time_since_epoch();
        auto days = static_cast<int32_t>(duration.count());
        RETURN_NOT_OK(builder->Append(days));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::BinaryBuilder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>& field_lengths) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        RETURN_NOT_OK(builder->Append(row_batch[j][i], field_lengths[j][i]));
      }
      return Status::OK();
    }

    Status ProcessBatchField(arrow::StringBuilder* builder, const unsigned int i,
                             const std::vector<MYSQL_ROW>& row_batch,
                             const std::vector<std::vector<unsigned long>>& field_lengths) {
      const auto n = row_batch.size();
      for (size_t j = 0; j < n; ++j) {
        // TODO: encoding
        RETURN_NOT_OK(builder->Append(row_batch[j][i], field_lengths[j][i]));
      }
      return Status::OK();
    }

    struct Batch {
      std::vector<MYSQL_ROW> rows;
      std::vector<std::vector<unsigned long>> field_lengths;

      explicit Batch(size_t batch_size) {
        rows.reserve(batch_size);
        field_lengths.reserve(batch_size);
      }
    };

    Status ProcessBatch(std::shared_ptr<arrow::RecordBatchBuilder> builder,
                        const std::shared_ptr<Batch>& batch) {
      task_group_->Append([=]() -> Status {
        for (unsigned int i = 0; i < n_fields_; ++i) {
          auto field = schema_->field(i);
          switch (field->type()->id()) {
          case arrow::Type::NA:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::NullBuilder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::BOOL:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::BooleanBuilder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::INT8:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::Int8Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::UINT8:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::UInt8Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::INT16:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::Int16Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::UINT16:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::UInt16Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::INT32:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::Int32Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::UINT32:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::UInt32Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::INT64:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::Int64Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::UINT64:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::UInt64Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::FLOAT:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::FloatBuilder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::DOUBLE:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::DoubleBuilder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::BINARY:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::BinaryBuilder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::STRING:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::StringBuilder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::DATE32:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::Date32Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::TIMESTAMP:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::TimestampBuilder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          case arrow::Type::DECIMAL:
            RETURN_NOT_OK(ProcessBatchField(builder->GetFieldAs<arrow::Decimal128Builder>(i),
                                            i, batch->rows, batch->field_lengths));
            continue;

          default:
            return Status::Invalid("Unexpected arrow data type: ", field->type()->name());
          }
        }
        return Status::OK();
      });
      return Status::OK();
    }

    Status FetchNextBatch() {
      auto batch = std::make_shared<Batch>(batch_size_);
      for (long i = 0; i < batch_size_; ++i) {
        // Here it isn't necessary to copy items in a MYSQL_ROW (const char**)
        // returned from mysql_fetch_row in the stored result mode.
        MYSQL_ROW row = mysql_fetch_row(result_);
        if (!row) {
          finished_ = true;
          break;
        }

        batch->rows.emplace_back(row);

        unsigned long* lengths = mysql_fetch_lengths(result_);
        batch->field_lengths.emplace_back(lengths, lengths + n_fields_);
      }

      if (batch->rows.size() == 0) {
        return Status::OK();
      }

      {
        std::unique_ptr<arrow::RecordBatchBuilder> builder;
        RETURN_NOT_OK(arrow::RecordBatchBuilder::Make(schema_, pool_, &builder));
        record_batch_builders_.emplace_back(std::move(builder));
      }

      std::shared_ptr<arrow::RecordBatchBuilder> builder = record_batch_builders_[batch_index_];

      task_group_->Append([=]() mutable -> Status {
        RETURN_NOT_OK(ProcessBatch(builder, batch));
        return Status::OK();
      });

      ++batch_index_;

      return Status::OK();
    }

    Status Finish(std::shared_ptr<arrow::Table>* out) {
      // Finish all pending parallel tasks
      RETURN_NOT_OK(task_group_->Finish());

      return MakeTable(out);
    }

    Status MakeTable(std::shared_ptr<arrow::Table>* out) {
      std::vector<std::shared_ptr<arrow::RecordBatch>> record_batches;
      for (auto builder : record_batch_builders_) {
        std::shared_ptr<arrow::RecordBatch> batch;
        RETURN_NOT_OK(builder->Flush(&batch));
        record_batches.emplace_back(batch);
      }
      return arrow::Table::FromRecordBatches(record_batches, out);
    }

  private:
    arrow::internal::ThreadPool* thread_pool_;
    const long batch_size_;
    bool finished_;
    std::shared_ptr<arrow::internal::TaskGroup> task_group_;
    std::vector<std::shared_ptr<arrow::RecordBatchBuilder>> record_batch_builders_;
    int64_t batch_index_ = 0;
  };

  ///////////////////////////////////////////////////////////////////////
  // MysqlResultReader factory functions

  Status MysqlResultReader::Make(arrow::MemoryPool* pool, MYSQL_STMT* stmt,
                                 MYSQL_RES* result, bool cast, bool cast_bool,
                                 bool parallel, long batch_size,
                                 std::shared_ptr<MysqlResultReader>* out) {
    // TODO: support prepared statement
    return Status::NotImplemented("[streaming]");
  }

  Status MysqlResultReader::Make(arrow::MemoryPool* pool, MYSQL_RES* result,
                                 bool cast, bool cast_bool,
                                 bool parallel, long batch_size,
                                 std::shared_ptr<MysqlResultReader>* out) {
    std::shared_ptr<MysqlResultReader> reader;

    if (parallel) {
      reader = std::make_shared<ThreadedMysqlStoredResultReader>(
          pool, result, arrow::internal::GetCpuThreadPool(), cast, cast_bool, batch_size);
      *out = reader;
      return Status::OK();
    } else {
      return Status::NotImplemented("[stored][serial]");
    }
  }
}
