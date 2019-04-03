#include <iostream>

#include "mysql2_arrow.hpp"

// TODO: support encoding
// #include <mysql2/mysql_enc_to_ruby.h>

#include <ruby/thread.h>

#include <rbgobject.h>

#include <arrow-glib/error.hpp>
#include <arrow-glib/table.hpp>

namespace rb {
  using RawThreadCallback = void* (*)(void*);
  void* thread_call_without_gvl(RawThreadCallback callback, void* callback_data,
                                rb_unblock_function_t* ubf, void* ubf_data) {
    return rb_thread_call_without_gvl(callback, callback_data, ubf, ubf_data);
  }

  template <class NoArgumentCallback>
  void* thread_call_without_gvl(NoArgumentCallback callback,
                                rb_unblock_function_t* ubf, void* ubf_data) {
    struct Data {
      explicit Data(NoArgumentCallback callback)
        : callback_(callback) {}
      NoArgumentCallback callback_;
    } data(callback);
    void* callback_data = &data;
    return thread_call_without_gvl([](void* callback_data) -> void* {
                                     auto data = reinterpret_cast<Data*>(callback_data);
                                     return data->callback_();
                                   }, callback_data, ubf, ubf_data);
  }
}

namespace mysql2_arrow {
  using arrow::Status;

  namespace {
    void check_status(const Status& status, const char* context) {
      GError* error = nullptr;
      if (!garrow_error_check(&error, status, context)) {
        RG_RAISE_ERROR(error);
      }
    }

    struct Timezone {
      enum type {
        unknown,
        local,
        utc
      };
    };

    rb_encoding *binaryEncoding;

    ID intern_utc, intern_local, intern_merge;
    VALUE sym_symbolize_keys, sym_as, sym_array, sym_cast_booleans,
          sym_cache_rows, sym_cast, sym_database_timezone, sym_application_timezone, sym_local,
          sym_utc, sym_parallel, sym_batch_size;

    VALUE mysql2_result_to_arrow(int argc, VALUE* argv, VALUE self) {
      GET_RESULT(self);

      // TODO: support prepared statement
      if (wrapper->stmt_wrapper) {
        rb_raise(rb_eNotImpError, "Prepared statement is not supported");
      }

      if (wrapper->stmt_wrapper && wrapper->stmt_wrapper->closed) {
        rb_raise(eMysql2Error, "Statement handle already closed");
      }

      // The default query options
      VALUE defaults = rb_iv_get(self, "@query_options");
      Check_Type(defaults, T_HASH);

      // Getting query options from the method arguments
      VALUE opts;
      if (rb_scan_args(argc, argv, "01", &opts) == 1) {
        opts = rb_funcall(defaults, intern_merge, 1, opts);
      } else {
        opts = defaults;
      }

      if (RTEST(rb_hash_aref(opts, sym_cache_rows))) {
        rb_warn(":cache_rows is ignored in to_arrow method");
      }

      if (wrapper->stmt_wrapper && !wrapper->is_streaming) {
        rb_warn("Rows are not cached in to_arrow method even for prepared statements (if not streaming)");
      }

      const bool castBool = RTEST(rb_hash_aref(opts, sym_cast_booleans));
      const bool cast     = RTEST(rb_hash_aref(opts, sym_cast));
      const bool parallel = RTEST(rb_hash_lookup2(opts, sym_parallel, Qtrue));
      const long batch_size = NUM2LONG(rb_hash_lookup2(opts, sym_batch_size, LONG2NUM(1000)));

      if (wrapper->stmt_wrapper && !cast) {
        rb_warn(":cast is forced for prepared statements");
      }

      Timezone::type dbTimezone;
      {
        VALUE dbTz = rb_hash_aref(opts, sym_database_timezone);
        if (dbTz == sym_local) {
          dbTimezone = Timezone::local;
        } else if (dbTz == sym_utc) {
          dbTimezone = Timezone::utc;
        } else {
          if (!NIL_P(dbTz)) {
            rb_warn(":database_timezone option must be :utc or :local - defaulting to :local");
          }
          dbTimezone = Timezone::local;
        }
      }

      Timezone::type appTimezone;
      {
        VALUE appTz = rb_hash_aref(opts, sym_application_timezone);
        if (appTz == sym_local) {
          appTimezone = Timezone::local;
        } else if (appTz == sym_utc) {
          appTimezone = Timezone::utc;
        } else {
          appTimezone = Timezone::unknown;
        }
      }

      try {
        std::shared_ptr<MysqlResultReader> reader;
        std::shared_ptr<arrow::Table> table;

        arrow::MemoryPool *pool = arrow::default_memory_pool();

        return rb::protect([&]{
          if (wrapper->stmt_wrapper) {
            check_status(MysqlResultReader::Make(pool, wrapper->stmt_wrapper->stmt,
                                                 wrapper->result, cast, castBool,
                                                 parallel, batch_size, &reader),
                         "[mysql2_arrow][MysqlResultReader::Make][streaming]");
          } else {
            check_status(MysqlResultReader::Make(pool, wrapper->result, cast, castBool,
                                                 parallel, batch_size, &reader),
                         "[mysql2_arrow][MysqlResultReader::Make][stored]");
          }

          (void)rb::thread_call_without_gvl([&]() -> void* {
            check_status(reader->Read(&table), "[mysql2_arrow][read_result]");
            return nullptr;
          }, RUBY_UBF_IO, nullptr);

          auto gobj_table = garrow_table_new_raw(&table);
          return GOBJ2RVAL_UNREF(gobj_table);
        });
      } catch (rb::State& state) {
        state.jump();
      }
    }
  }

  void init_mysql2_result_extension() {
    VALUE mResultExtension;

    mResultExtension = rb_define_module_under(mMysql2Arrow, "ResultExtension");

    rb_define_method(mResultExtension, "to_arrow",
                     reinterpret_cast<rb::RawMethod>(mysql2_result_to_arrow), -1);

    intern_utc          = rb_intern("utc");
    intern_local        = rb_intern("local");
    intern_merge        = rb_intern("merge");
    // intern_localtime    = rb_intern("localtime");
    // intern_local_offset = rb_intern("local_offset");
    // intern_civil        = rb_intern("civil");
    // intern_new_offset   = rb_intern("new_offset");
    // intern_BigDecimal   = rb_intern("BigDecimal");

    sym_symbolize_keys  = ID2SYM(rb_intern("symbolize_keys"));
    sym_as              = ID2SYM(rb_intern("as"));
    sym_array           = ID2SYM(rb_intern("array"));
    sym_local           = ID2SYM(rb_intern("local"));
    sym_utc             = ID2SYM(rb_intern("utc"));
    sym_cast_booleans   = ID2SYM(rb_intern("cast_booleans"));
    sym_database_timezone     = ID2SYM(rb_intern("database_timezone"));
    sym_application_timezone  = ID2SYM(rb_intern("application_timezone"));
    sym_cache_rows     = ID2SYM(rb_intern("cache_rows"));
    sym_cast           = ID2SYM(rb_intern("cast"));
    // sym_stream         = ID2SYM(rb_intern("stream"));
    // sym_name           = ID2SYM(rb_intern("name"));
    sym_parallel       = ID2SYM(rb_intern("parallel"));
    sym_batch_size     = ID2SYM(rb_intern("batch_size"));

    binaryEncoding = rb_enc_find("binary");
  }
}
