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
require 'active_record/connection_adapters/mysql2_adapter'
require_relative 'arrow_result'

module ActiveRecord
  module ConnectionHandling
    def arrow_mysql2_connection(config)
      config = config.symbolize_keys
      config[:flag] ||= 0

      if config[:flag].kind_of? Array
        config[:flag].push "FOUND_ROWS".freeze
      else
        config[:flag] |= Mysql2::Client::FOUND_ROWS
      end

      client = Mysql2::Client.new(config)
      ActiveRecordArrowAdapter::ArrowMysql2Adapter.new(client, logger, nil, config)
    rescue Mysql2::Error => error
      if error.message.include?("Unknown database")
        raise ActiveRecord::NoDatabaseError
      else
        raise
      end
    end
  end
end

module ActiveRecordArrowAdapter
  class ArrowMysql2Adapter < ActiveRecord::ConnectionAdapters::Mysql2Adapter
    def exec_query(sql, name = "SQL", binds = [], prepare: false)
      return super unless use_arrow?

      if without_prepared_statement?(binds)
        execute_and_free(sql, name) do |result|
          ArrowResult.new(result.to_arrow) if result
        end
      else
        exec_stmt_and_free(sql, name, binds, cache_stmt: prepare) do |_, result|
          ArrowResult.new(result.to_arrow) if result
        end
      end
    end

    def select_all(arel, name = nil, binds = [], preparable: nil, use_arrow: true)
      with_arrow(use_arrow) do
        super(arel, name, binds, preparable: preparable)
      end
    end

    private

    def use_arrow?
      @use_arrow
    end

    def with_arrow(new_value)
      old_value, @use_arrow = @use_arrow, new_value
      yield
    ensure
      @use_arrow = old_value
    end
  end
end
