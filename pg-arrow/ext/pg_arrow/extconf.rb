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

require "extpp"
require "mkmf-gnome2"

unless required_pkg_config_package("arrow",
                                  debian: "libarrow-dev",
                                  redhat: "arrow-devel",
                                  homebrew: "apache-arrow",
                                  msys2: "apache-arrow")
  exit(false)
end

unless required_pkg_config_package("arrow-glib",
                                   debian: "libarrow-glib-dev",
                                   redhat: "arrow-glib-devel",
                                   homebrew: "apache-arrow-glib",
                                   msys2: "apache-arrow")
  exit(false)
end

[
  ["glib2", "ext/glib2"],
].each do |name, source_dir|
  spec = find_gem_spec(name)
  source_dir = File.join(spec.full_gem_path, source_dir)
  build_dir = source_dir
  add_depend_package_path(name, source_dir, build_dir)
end

pg_includedir, pg_libdir = dir_config('pg')
unless pg_includedir && pg_libdir
  pg_config = with_config('pg_config')
  pg_config = find_executable('pg_config') if pg_config.nil? || pg_config == true
  pg_includedir = `#{pg_config} --includedir`.chomp
  pg_libdir = `#{pg_config} --libdir`.chomp
  pg_libs = `#{pg_config} --libs`.chomp
  $INCFLAGS << " " << "-I#{pg_includedir}"
  $LDFLAGS << " " << "-L#{pg_libdir}"
end

unless have_header('libpq-fe.h') &&
       have_header('libpq/libpq-fs.h') &&
       have_header('pg_config_manual.h')
  $stderr.puts "Unable to find header files of PostgreSQL client library"
  abort
end

unless have_library('pq', 'PQconnectdb', ['libpq-fe.h']) ||
       have_library('libpq', 'PQconnectdb', ['libpq-fe.h']) ||
       have_library('ms/libpq', 'PQconnectdb', ['libpq-fe.h'])
  $stderr.puts "Unable to find PostgreSQL client library"
  abort
end

checking_for(checking_message("ruby-pg"), "%s") do
  pg_spec = Gem::Specification.find_by_name("pg")
  $INCFLAGS << " " << "-I#{pg_spec.gem_dir}/ext"
  pg_spec.version
end


create_makefile('pg_arrow')
