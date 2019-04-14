require "pg_arrow/version"

# This is for preventing warnings of "already initialized constant BigDecimal::XXX"
require "bigdecimal"

require "pg"
require "arrow"

begin
  require "pg_arrow.so"
rescue LoadError
  $LOAD_PATH.unshift File.expand_path("../../ext/pg_arrow", __FILE__)
  require "pg_arrow.so"
end
