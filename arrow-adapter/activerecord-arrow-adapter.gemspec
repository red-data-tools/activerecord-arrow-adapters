# -*- ruby -*-
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

require_relative "lib/activerecord-arrow-adapter/version"

Gem::Specification.new do |spec|
  spec.name = "activerecord-arrow-adapter"
  version_components = [
    ActiveRecordArrowAdapter::Version::MAJOR.to_s,
    ActiveRecordArrowAdapter::Version::MINOR.to_s,
    ActiveRecordArrowAdapter::Version::MICRO.to_s,
    ActiveRecordArrowAdapter::Version::TAG,
  ]
  spec.version = version_components.compact.join(".")
  spec.homepage = "https://github.com/red-data-tools/activerecord-arrow-adapters"
  spec.authors = ["Kenta Murata"]
  spec.email = ["mrkn@mrkn.jp"]

  spec.summary = "ActiveRecord connection adapter for Apache Arrow"
  spec.description = "ActiveRecord connection adapter for Apache Arrow"
  spec.license = "Apache-2.0"

  spec.add_runtime_dependency("red-arrow", "> 0.12.0")
  spec.add_runtime_dependency("activerecord", ">= 5.2.2")

  spec.add_development_dependency("bundler")
  spec.add_development_dependency("rake")
  spec.add_development_dependency("pkg-config")
  spec.add_development_dependency("test-unit")
  spec.add_development_dependency("test-unit-rr")
end
