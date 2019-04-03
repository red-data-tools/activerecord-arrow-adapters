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

    @result = @client.query(<<~SQL)
      SELECT
        tiny_int_test
        , small_int_test
        , medium_int_test
        , int_test
        , big_int_test
        , float_test
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
  end

  test("#to_arrow") do
    table = @result.to_arrow
    assert_kind_of(Arrow::Table,
                   table)
    assert_equal(30_000,
                 table.n_rows)
    assert_equal(14,
                 table.n_columns)
  end

  test("#raw_records") do
    query = <<~SQL
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
      FROM mysql2_test LIMIT 10
    SQL

    assert_equal(@client.query(query, as: :array).to_a,
                 @client.query(query).to_arrow.raw_records)
  end
end
