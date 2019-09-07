# -*- mode: meson -*-
# Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved.

libtest_sources = [
  'kad/test_util.c',
]

libtest_so = shared_library(
  proj_name+'test', libtest_sources,
  c_args : lib_cargs,
  include_directories : main_inc,
  dependencies : lib_deps,
  link_with : libmain_a,
)

tests_sources = [
  'file.c',
  'utils/aatree.c',
  'utils/bitfield.c',
  'utils/queue.c',
  'utils/bits.c',
  'utils/bstree.c',
  'utils/byte_array.c',
  'utils/hash.c',
  'utils/list.c',
  'utils/lookup.c',
  'utils/rbtree.c',
  'utils/u64.c',
  'kad/bencode/parser.c',
  'kad/bencode/rpc_msg.c',
  'kad/bencode/dht.c',
  'kad/dht.c',
  'kad/rpc.c',
]

foreach fname : tests_sources
  test_name = fname.split('.').get(0).underscorify()
  exe = executable(test_name, fname,
                   include_directories : main_inc,
                   # propagated for inclusion of .c files in tests.
                   c_args : lib_cargs,
                   dependencies : lib_deps,
                   link_with : [libmain_a, libtest_so],
                  )
  test(fname, exe, args : meson.source_root())
endforeach

tests_alt_sources = [
  'kad/dht_alt.c',
]

foreach fname : tests_alt_sources
  test_name = fname.split('.').get(0).underscorify()
  exe = executable(test_name, fname,
                   include_directories : alt_inc,
                   # propagated for inclusion of .c files in tests.
                   c_args : lib_cargs,
                   dependencies : lib_deps,
                   link_with : libalt_so,
                  )
  test(fname, exe)
endforeach

subdir('integration')
