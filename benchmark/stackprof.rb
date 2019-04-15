require_relative 'prelude'
require 'stackprof'

ActiveRecord::Base.logger = Logger.new(STDOUT)

n = Integer(ENV.fetch('LIMIT', 10000))

# Warming up
Mysql2Test.test_pluck(n, use_arrow: true)
Mysql2Test.test_pluck(n, use_arrow: false)

StackProf.run(mode: :wall, out: 'stackprof-arrow-wall.dump') do
  Mysql2Test.test_pluck(n, use_arrow: true)
end

StackProf.run(mode: :wall, out: 'stackprof-original-wall.dump') do
  Mysql2Test.test_pluck(n, use_arrow: false)
end

StackProf.run(mode: :cpu, out: 'stackprof-arrow-cpu.dump') do
  Mysql2Test.test_pluck(n, use_arrow: true)
end

StackProf.run(mode: :cpu, out: 'stackprof-original-cpu.dump') do
  Mysql2Test.test_pluck(n, use_arrow: false)
end

StackProf.run(mode: :object, out: 'stackprof-arrow-obj.dump') do
  Mysql2Test.test_pluck(n, use_arrow: true)
end

StackProf.run(mode: :object, out: 'stackprof-original-obj.dump') do
  Mysql2Test.test_pluck(n, use_arrow: false)
end
