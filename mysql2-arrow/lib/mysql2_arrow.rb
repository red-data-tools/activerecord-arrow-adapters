require "mysql2_arrow/version"
require "mysql2"
require "arrow"

begin
  require "mysql2_arrow.so"
rescue LoadError
  $LOAD_PATH.unshift File.expand_path("../../ext/mysql2_arrow", __FILE__)
  require "mysql2_arrow.so"
end

Mysql2::Result.include Mysql2Arrow::ResultExtension
