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

class ArrowResultTest < Test::Unit::TestCase
  def setup
    @mysql2_client = Mysql2::Client.new(
      host: 'localhost',
      username: 'root',
      database: 'test'
    )
    @mysql2_client.query_options[:as] = :array

    @query_limit = 10
    @query_columns = %i[int_test double_test varchar_test text_test]
  end

  def query_statement
    <<~SQL
      SELECT
        #{@query_columns.join(', ')}
      FROM mysql2_test LIMIT #{@query_limit}
    SQL
  end

  def result_record_batch
    @record_batch ||= @mysql2_client.query(query_statement).to_arrow
  end

  def result
    @result ||= ActiveRecordArrowAdapter::ArrowResult.new(result_record_batch)
  end

  def ar_result
    r = @mysql2_client.query(query_statement)
    @ar_result ||= ActiveRecord::Result.new(r.fields, r.to_a)
  end

  test('#columns') do
    assert_equal(ar_result.columns,
                 result.columns)
  end

  test('#columns_types') do
    assert_equal(ar_result.column_types,
                 result.column_types)
  end

  test('#length') do
    assert_equal(ar_result.length,
                 result.length)
  end

  test('#rows') do
    rows = result.rows
    assert_same(rows,
                result.rows)
    assert_kind_of(Array,
                   rows)
    assert_equal(ar_result.rows,
                 rows)
  end

  test('#cast_values') do
    values = result.cast_values
    assert_kind_of(Array,
                   values)
    assert_equal(ar_result.cast_values,
                 values)
  end
end
