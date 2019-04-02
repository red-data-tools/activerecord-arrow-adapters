require "bundler/setup"
require "activerecord-arrow-adapter"
require "mysql2_arrow"

$VERBOSE = nil

ActiveRecord::Base.establish_connection(
  host: 'localhost',
  username: 'root',
  database: 'test',
  adapter: 'arrow_mysql2'
)

class Mysql2Test < ActiveRecord::Base
  self.table_name = 'mysql2_test'

  def self.test_pluck(n=10_000, use_arrow:)
    res = connection.with_arrow(use_arrow) do
      puts " (n=#{n}) "
      limit(n).pluck(
        :int_test, :double_test, :varchar_test, :text_test,
      )
    end
    res.length == n
  end
end
