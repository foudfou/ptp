# -*- mode: meson -*-
# Copyright (c) 2016 Foudil Brétel.  All rights reserved.

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
  'kad/bencode/parser.c',
  'kad/bencode/routes.c',
  'kad/bencode/rpc_msg.c',
  'kad/req_lru.c',
  'kad/routes.c',
  'kad/rpc.c',
  'timers_once.c',
  'timers_periodic.c',
  'utils/aatree.c',
  'utils/bitfield.c',
  'utils/bits.c',
  'utils/bstree.c',
  'utils/byte_array.c',
  'utils/hash.c',
  'utils/heap.c',
  'utils/list.c',
  'utils/lookup.c',
  'utils/queue.c',
  'utils/rbtree.c',
  'utils/u64.c',
]

foreach fname : tests_sources
  test_name = fname.split('.').get(0).underscorify()
  exe = executable(test_name, fname,
                   include_directories : main_inc,
                   # propagated for inclusion of .c files in tests.
                   c_args : lib_cargs,
                   dependencies : lib_deps,
                   link_with : [libtest_so],
                  )
  test('unit/'+fname, exe, args : meson.project_source_root())
endforeach

tests_alt_sources = [
  'kad/routes_alt.c',
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
  test('unit/'+fname, exe)
endforeach

subdir('integration')
