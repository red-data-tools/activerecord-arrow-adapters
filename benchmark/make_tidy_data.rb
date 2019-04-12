require 'pathname'
require 'time'

header = %w[total_records batch_size kind method mt value timestamp]
records = []

if ARGV[0] == '-o'
  ARGV.shift
  output = ARGV.shift
else
  output = 'result.csv'
end

ARGV.each do |filename|
  Pathname(filename).open('r') do |input|
    while line = input.gets
      line.strip!
      case line
      when /\ALIMIT=(\d+) BATCH_SIZE=(\d+) TIMESTAMP=(\d+)( MT)?\z/
        limit, batch_size = $1.to_i, $2.to_i
        timestamp = Time.strptime($3, '%Y%m%d%H%M%S')
        multi_thread = !$4.nil?
      when /Execution time \(s\):\z/
        kind = 'time'
      when /\AMax resident set size \(bytes\):\z/
        kind = 'memory'
      when /\A(by_arrow|original)\s+(\d+(?:\.\d+)?)(M|G)?\z/
        method = $1
        value = $2.to_f
        if kind == 'memory'
          case $3
          when 'M'
            value *= 1_000_000
          when 'G'
            value *= 1_000_000_000
          end
        end
        record = [
          limit,
          batch_size,
          kind,
          method,
          multi_thread,
          value,
          timestamp.strftime('%Y%m%d%H%M%S'),
        ]
        records << record
      end
    end
  end
end

require 'csv'
CSV.open(output, 'w') do |csv|
  csv << header
  records.each do |record|
    csv << record
  end
end
