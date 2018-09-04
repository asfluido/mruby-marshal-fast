# Quick-and-dirty exerciser of ruby's marshaller
# I can run it from both Ruby and Mruby

z=rand(150)+10

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
  def marshal_dump
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
end

module BLOTTO
end

a={}
z.times do |v|
  a[v]=Z2::new(['z'*(1+rand(10)),rand(),rand(10000),syms[rand(syms.length)],nil],Z,BLOTTO)
end

a1=Marshal::dump(a)
a2=Marshal::restore(a1)

a.keys.each do |k|
  STDERR.puts("k: #{k} from: #{a[k].to_s} to: #{a2[k].to_s}. Are #{a[k]===a2[k] ? 'EQUAL' : 'different'}")
end
