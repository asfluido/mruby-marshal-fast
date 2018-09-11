MRuby::Gem::Specification::new('mruby-marshal-fast') do |spec|
  spec.license='MIT'
  spec.author='Carlo E. Prelz (Unibe/TPF)'
  spec.summary='Marhshal module for mruby in C, with speed advantages'
  
  ['-g','-O6','-funsigned-char','-fPIC','-ffast-math','-Werror','-Wall','-Wcast-align',
    '-Wno-declaration-after-statement','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-but-set-variable','-Wno-discarded-qualifiers',
    '-Wno-unused-result','-Wno-format-security','-DGLX_USE_TLS'].each do |f|
    spec.cc.flags.push(f)   
  end

  spec.rbfiles=(Dir.glob("#{dir}/mrblib/*.rb")+Dir.glob("#{dir}/mrblib/*/*.rb")).reject do |s|
    /\/attic\//.match(s)
  end.sort
end
