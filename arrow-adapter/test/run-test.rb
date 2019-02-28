#!/usr/bin/env ruby
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

$VERBOSE = true

require "pathname"

base_dir = Pathname.new(__dir__).parent.expand_path
mysql2_arrow_base_dir = base_dir.parent + "mysql2-arrow"

lib_dir = base_dir + "lib"
test_dir = base_dir + "test"

mysql2_arrow_lib_dir = mysql2_arrow_base_dir + "lib"
mysql2_arrow_ext_dir = mysql2_arrow_base_dir + "ext/mysql2_arrow"

$LOAD_PATH.unshift(mysql2_arrow_ext_dir.to_s)
$LOAD_PATH.unshift(mysql2_arrow_lib_dir.to_s)
$LOAD_PATH.unshift(lib_dir.to_s)

require_relative 'test_helper'

ENV["TEST_UNIT_MAX_DIFF_TARGET_STRING_SIZE"] ||= "10000"

exit(Test::Unit::AutoRunner.run(true, test_dir.to_s))
