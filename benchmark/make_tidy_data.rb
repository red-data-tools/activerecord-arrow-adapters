require 'arrow'
require 'pathname'

# The following schema occurs SEGV when adding the value in the kind column
# during building a record batch
# schema = Arrow::Schema.new(
#   total_records: :int32,
#   batch_size: :int32,
#   kind: {
#           type: :dictionary,
#           index_data_type: :int8,
#           dictionary: Arrow::StringArray.new(%w[time memory]),
#           ordered: true
#         },
#   method: {
#             type: :dictionary,
#             index_data_type: :int8,
#             dictionary: Arrow::StringArray.new(%w[original by_arrow]),
#             ordered: true
#           },
#   value: :double,
#   timestamp: {
#                type: :timestamp,
#                unit: :milli
#              }
# )

schema = Arrow::Schema.new(
  total_records: :int32,
  batch_size: :int32,
  kind: :string,
  method: :string,
  value: :double,
  timestamp: {
               type: :timestamp,
               unit: :milli
             }
)

records = []

ARGV.each do |filename|
  Pathname(filename).open('r') do |input|
    while line = input.gets
      line.strip!
      case line
      when /\ALIMIT=(\d+) BATCH_SIZE=(\d+) TIMESTAMP=(\d+)\z/
        limit, batch_size = $1.to_i, $2.to_i
        timestamp = Time.strptime($3, '%Y%m%d%H%M%S')
      when /Execution time \(s\):\z/
        kind = 'time'
      when /\AMax resident set size \(bytes\):\z/
        kind = 'memory'
      when /\A(by_arrow|original)\s+(\d+(?:\.\d+)?)(M)?\z/
        method = $1
        value = $2.to_f
        if kind == :memory
          case $3
          when 'M'
            value *= 1_000_000
          end
        end
        record = [
          limit,
          batch_size,
          kind,
          method,
          value,
          timestamp
        ]
        records << record
        p record
        Arrow::RecordBatch.new(schema, records)
      end
    end
  end
end

pp Arrow::RecordBatch.new(schema, records)
