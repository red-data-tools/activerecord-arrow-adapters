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

class Mysql2ResultTest < Test::Unit::TestCase
  def setup
    @client = Mysql2::Client.new(
      host: "localhost",
      username: "root",
      database: "test"
    )

    @query = <<~SQL
      SELECT
        tiny_int_test
        , small_int_test
        , medium_int_test
        , int_test
        , big_int_test
        , double_test
        , decimal_test
        , varchar_test
        , binary_test
        , tiny_text_test
        , text_test
        , medium_text_test
        , long_text_test
      FROM mysql2_test LIMIT 30000
    SQL
    @result = @client.query(@query)
  end

  sub_test_case("#to_arrow") do
    test("schema") do
      table = @result.to_arrow
      assert_kind_of(Arrow::Table,
                     table)
      assert_equal(30_000,
                   table.n_rows)
      assert_equal(13,
                   table.n_columns)
      assert_equal(Arrow::Schema.new(
                     tiny_int_test: :int8,
                     small_int_test: :int16,
                     medium_int_test: :int32,
                     int_test: :int32,
                     big_int_test: :int64,
                     double_test: :double,
                     decimal_test: {
                                     type: :decimal128,
                                     precision: 10,  # TODO
                                     scale: 3
                                   },
                     varchar_test: :string,
                     binary_test: :binary,
                     tiny_text_test: :binary, # TODO
                     text_test: :binary, # TODO
                     medium_text_test: :binary, # TODO
                     long_text_test: :binary # TODO
                   ),
                   table.schema)
    end

    test("content") do
      query = @query.sub(/LIMIT \d+/, "LIMIT 10")

      table = @client.query(query).to_arrow

      expected_records = @client.query(query, as: :array).to_a
      expected_records.each do |record|
        # TODO: red-arrow has bug of BigDecimal treatment
        a, b = record[6].to_s('F').split('.', 2)
        b = "#{b}000"[0, 3]
        record[6] = [a, b].join('.')
      end
      record_batch = Arrow::RecordBatch.new(table.schema, expected_records)
      expected_table = Arrow::Table.new(record_batch.schema, [record_batch])
      assert_equal(expected_table, table)
    end

    test("raw_records") do
      query = @query.sub(/LIMIT \d+/, "LIMIT 10")

      table = @client.query(query).to_arrow
      records = table.raw_records

      expected_records = @client.query(query, as: :array).to_a
      # assert_equal(expected_records, records)

      expected_records.each_with_index do |record, i|
        assert_equal([i, record], [i, records[i]])
      end
    end
  end
end
