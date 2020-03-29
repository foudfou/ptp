# -*- mode: meson -*-
# Copyright (c) 2016 Foudil Brétel.  All rights reserved.

configure_file(input : 'config.h.in',
               output : 'config.h',
               configuration : conf)

# A dyn lib is a simple solution to let our test executables build against most
# of the code, that is except main.o.
libmain_sources = [
  'events.c',
  'file.c',
  'log.c',
  'net/iobuf.c',
  'net/actions.c',
  'net/msg.c',
  'net/socket.c',
  'options.c',
  'server.c',
  'signals.c',
  'timers.c',
  'utils/safer.c',
  'utils/u64.c',
]

cc = meson.get_compiler('c')
rt_dep = cc.find_library('rt', required : false)
thread_dep = dependency('threads')
lib_deps = [rt_dep, thread_dep]

lib_cargs = ['-D_XOPEN_SOURCE=700L', '-D_DEFAULT_SOURCE']

subdir('net/kad')

libmain_a = static_library(
  proj_name, libmain_sources,
  c_args : lib_cargs,
  include_directories : main_inc,
  dependencies : lib_deps,
  link_with : libkad_a,
)

main_exe = executable(
  proj_name, 'main.c',
  c_args : lib_cargs,
  include_directories : main_inc,
  link_with : libmain_a,
  dependencies : lib_deps,
  install : true
)


# We use different constants for some tests.
libalt_so = shared_library(
  proj_name+'alt', libmain_sources,
  c_args : lib_cargs,
  include_directories : alt_inc,
  dependencies : lib_deps,
  link_with : libkadalt_a,
)
