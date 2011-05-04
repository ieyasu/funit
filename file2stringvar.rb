#!/usr/bin/env ruby
# file2stringvar.rb - turn a file into a C string variable
# Usage: file2stringvar.rb varname <data_file >src.c

puts "const char #{ARGV[0]}[] = \\"

while (line = STDIN.gets)
  puts "  #{line.inspect} \\"
end
puts ";"
