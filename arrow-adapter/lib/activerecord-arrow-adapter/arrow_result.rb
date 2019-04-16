# frozen_string_literal: true
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

require 'active_record'
require 'arrow'

module ActiveRecordArrowAdapter
  class ArrowResult < ActiveRecord::Result
    def initialize(record_batch)
      @record_batch = record_batch
      @columns = nil
      @column_types = {}
      @rows = nil
      @hash_rows = nil
    end

    def columns
      @columns ||= generate_columns
    end

    def column_types
      @column_types ||= generate_column_types
    end

    def rows
      @rows ||= generate_rows
    end

    def length
      @record_batch.n_rows
    end

    def each
      if block_given?
        hash_rows.each(&proc)
      else
        hash_rows.to_enum { length }
      end
    end

    def to_hash
      hash_rows
    end

    alias :map! :map
    alias :collect! :map

    def empty?
      @record_batch.empty?
    end

    def to_ary
      hash_rows
    end

    def [](idx)
      hash_rows[idx]
    end

    def first
      return nil if @record_batch.empty?
      Hash[columns.zip(rows.first)] # TODO
    end

    def last
      return nil if @record_batch.empty?
      Hash[columns.zip(rows.last)] # TODO
    end

    def cast_values(type_overrides = {}) # :nodoc:
      types = columns.map { |name| column_type(name, type_overrides) }
      unless cast_needed?(types)
        return rows
      end

      if columns.one?
        # Separated to avoid allocating an array per row
        type = types[0]
        rows.map do |(value)|
          type.deserialize(value)
        end
      else
        rows.map do |values|
          Array.new(values.size) { |i| types[i].deserialize(values[i]) }
        end
      end
    end

    private

      def hash_rows
        @hash_rows ||=
          begin
            columns = self.columns.map { |c| c.dup.freeze }
            rows.map do |row|
              {}.tap do |hash|
                index = 0
                length = columns.length

                while index < length
                  hash[columns[index]] = row[index]
                  index += 1
                end
              end
            end
          end
      end

      def generate_columns
        @record_batch.schema.fields.map do |field|
          field.name
        end
      end

      def generate_rows
        @record_batch.raw_records
      end

      DIRECT_TYPE_MAP = {
        ActiveRecord::Type::BigInteger => Arrow::IntegerDataType,
        ActiveRecord::Type::Binary     => Arrow::BinaryDataType,
        ActiveRecord::Type::Date       => Arrow::Date32DataType,
        ActiveRecord::Type::DateTime   => Arrow::TimestampDataType,
        ActiveRecord::Type::Decimal    => Arrow::Decimal128DataType,
        ActiveRecord::Type::Float      => Arrow::FloatingPointDataType,
        ActiveRecord::Type::Integer    => Arrow::IntegerDataType,
        ActiveRecord::Type::Text       => Arrow::BinaryDataType,
        ActiveRecord::Type::String     => Arrow::StringDataType, # must follows AR::Type::Text
        ActiveRecord::Type::Time       => Arrow::TimestampDataType
      }.freeze

      def cast_needed?(types)
        arrow_fields = @record_batch.schema.fields
        !types.each_with_index.all? { |type, i|
          DIRECT_TYPE_MAP.any? { |ar_type, arrow_type|
            type.is_a?(ar_type) && arrow_fields[i].data_type.is_a?(arrow_type)
          }
        }
      end
  end
end
