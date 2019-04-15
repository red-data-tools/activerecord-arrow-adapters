require_relative 'prelude'

ActiveRecord::Base.logger = Logger.new(STDOUT)

n = Integer(ENV.fetch('LIMIT', 10000))

Mysql2Test.test_pluck(n, use_arrow: true)
