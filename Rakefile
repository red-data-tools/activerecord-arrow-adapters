desc "Run all tests"
task :test do
  subdirs = Dir.glob("*/*.gemspec").map {|gs| File.dirname(gs) }
  subdirs.each do |dir|
    Dir.chdir(dir) do
      ruby("-S rake test")
    end
  end
end

task default: :test
