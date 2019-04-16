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

class ArrowMysql2AdapterTest < Test::Unit::TestCase
  def setup
    ActiveRecord::Base.establish_connection(
      host: 'localhost',
      username: 'root',
      database: 'test',
      adapter: 'arrow_mysql2'
    )
    query_limit = 10
    query_columns = %i[int_test double_test varchar_test text_test]
    @query_statement = <<~SQL
      SELECT
        #{query_columns.join(', ')}
      FROM mysql2_test LIMIT #{query_limit}
    SQL
    @connection = ActiveRecord::Base.connection
  end

  sub_test_case('.select_all') do
    test('with_arrow(true)') do
      @connection.with_arrow(true) do
        assert_kind_of(ActiveRecordArrowAdapter::ArrowResult,
                       @connection.select_all(@query_statement))
      end
    end

    test('with_arrow(false)') do
      @connection.with_arrow(false) do
        assert_kind_of(ActiveRecord::Result,
                       @connection.select_all(@query_statement))
      end
    end
  end
end
