require_relative 'prelude'
require 'stackprof'
require 'tempfile'

module Mysql2
  class Result
    alias to_arrow_native to_arrow
    def to_arrow(*args, **kwargs)
      s = Time.now
      to_arrow_native(*args, **kwargs)
    ensure
      t = Time.now - s
      puts "to_arrow: #{t} [sec]"
    end
  end
end

module Arrow
  class Table
    alias raw_records_native raw_records
    def raw_records
      s = Time.now
      raw_records_native
    ensure
      t = Time.now - s
      puts "raw_records: #{t} [sec]"
    end
  end
end

ActiveRecord::Base.logger = Logger.new(STDOUT)

def help
  puts "#{$0} [-p PROF_MODE] [-l LIMIT] [-f] (arrow|original)"
end

def check_prof_mode(mode)
  case mode
  when 'wall', 'object'
    mode.to_sym
  else
    $stderr.puts "Invalid prof_mode: #{mode}"
    help
    abort
  end
end

def check_limit(limit)
  n = Integer(limit) rescue nil
  return n if n

  $srtderr.puts "Invalid limit: #{limit}"
  help
  abort
end

def check_arrow(kind)
  case kind
  when 'arrow', 'original'
    kind == 'arrow'
  else
    $stderr.puts "Invalid kind: #{kind}"
    help
    abort
  end
end

def save_graphviz(report, limit, arrow_p)
  mode = report.data[:mode]
  temp = `mktemp /tmp/framegraph-#{mode}-limit#{limit}-#{arrow_p ? "arrow" : "original"}-XXXXXXXX`.strip
  data_path = "#{temp}.dot"
  system "mv #{temp} #{data_path}"

  puts data_path
  File.open(data_path, "w") do |f|
    report.print_graphviz({}, f)
  end
end

def open_flamegraph(report, limit, arrow_p)
  mode = report.data[:mode]
  temp = `mktemp /tmp/stackflame-#{mode}-limit#{limit}-#{arrow_p ? "arrow" : "original"}-XXXXXXXX`.strip
  data_path = "#{temp}.js"
  system "mv #{temp} #{data_path}"

  puts data_path
  File.open(data_path, "w") do |f|
    report.print_flamegraph(f)
  end

  viewer_path = File.join(`bundle show stackprof`.strip, 'lib/stackprof/flamegraph/viewer.html')
  url = "file://#{viewer_path}\?data=#{data_path}"
  puts %Q[open "#{url}"]
end

while arg = ARGV.shift
  case arg
  when '-m'
    prof_mode = check_prof_mode(ARGV.shift)
  when '-l'
    limit = check_limit(ARGV.shift)
  when '-f'
    flamegraph = true
  when '-g'
    graphviz = true
  else
    arrow_p = check_arrow(arg)
  end
end

StackProf.start(mode: prof_mode, interval: 1, raw: flamegraph)
Mysql2Test.test_pluck(limit, use_arrow: arrow_p)
StackProf.stop

report = StackProf::Report.new(StackProf.results)
report.print_text(false)

save_graphviz(report, limit, arrow_p) if graphviz

open_flamegraph(report, limit, arrow_p) if flamegraph
