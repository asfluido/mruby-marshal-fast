# A version of the marshalling code for [http://mruby.org/](Mruby)

Here is my humble contribution to Mruby, marshalling code written in `C`.

The current marshalling code for Mruby is written in `c++`. I am in
the process of creating a small-footprint linux environment. One of
its prerequisites is that the `c++` compiler is not even generated.

The current marshalling code is the only part of Mruby that I use that
is written in `c++`. Furthermore, I had problems with the code in the
path, and deeply regretted not being able to easily see what was going
on, due to the well-known opaqueness of many `c++` colloquialisms.

Having bounced on this problem again yesterday morning, I decided to
see if I could write something. As what is inside here (less than 600
lines of `C` code) has been the result of barely one and a half days'
work, I am quite satisfied. The code matches my needs.

The following are marshalled:

* nil
* true
* false
* integers
* floating-points
* strings
* symbols
* arrays
* hashes
* classes
* modules
* objects:
	* if the object defines functions `marshal_dump` and
      `marshal_load`, marshals whatever object is returned from
      `marshal_dump`, and restores the object passing to
      `marshal_load` that object,
	* otherwise, backs up and restores all instance variables.

In the directory you find a quick-and-dirty exerciser, that can also
be run under Ruby (MRI).

## Usage:

Just add a line like this one:

```ruby
  conf.gem(:github=>'asfluido/mruby-marshal-c')
```

into your `build_config.rb` file.

