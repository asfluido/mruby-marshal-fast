# Quick-and-dirty exerciser of ruby's marshaller
# I can run it from both Ruby and Mruby

z=1000

syms=[:a,:b,:cc]

class Z
  def initialize(a,b,c)
    @a,@b,@c=a,b,c
  end
end

class Z2
  attr_reader(:a,:b,:c)
  
  def initialize(a,b,c)
    @a,@b,@c=a,b,c
  end
  def marshal_dump(arg=nil)
    [@a,@b,@c]
  end

  def ===(other)
    raise "A is different (#{[@a,other.a]})" unless(@a==other.a)
    raise "B is different (#{[@b,other.b]})" unless(@b==other.b)
    raise "C is different (#{[@c,other.c]})" unless(@c==other.c)
    true
  end

  def to_s
    "a: #{@a} b: #{@b} c: #{@c}"
  end

  def marshal_load(a)
    @a,@b,@c=a
  end
  def self::marshal_load(a)
    new(*a)
  end
end

module BLOTTO
end

a={}
z.times do |v|
  a[v]=Z2::new(['z'*(1+rand(10)),rand(),rand(10000),syms[rand(syms.length)],nil],Z,BLOTTO)
end

start=Time::now
100.times do
  a1=Marshal::dump(a)
  a2=Marshal::restore(a1)
end
stop=Time::now

puts("Each dump_restore cycle lasted #{(stop-start)/100.0}s")

  
#a.keys.each do |k|
#  STDERR.puts("k: #{k} from: #{a[k].to_s} to: #{a2[k].to_s}. Are #{a[k]===a2[k] ? 'EQUAL' : 'different'}")
#end
