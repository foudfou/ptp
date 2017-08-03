# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

tests_sources = [
    'utils/aatree.c',
    'utils/bits.c',
    'utils/bstree.c',
    'utils/byte_array.c',
    'utils/hash.c',
    'utils/list.c',
    'utils/lookup.c',
    'utils/rbtree.c',
    'utils/u64.c',
    'kad/bencode.c',
    'kad/dht.c',
    'kad/rpc.c',
]

foreach fname : tests_sources
  test_name = fname.split('.').get(0).underscorify()
  exe = executable(test_name, fname,
                   include_directories : inc,
                   # propagated for inclusion of .c files in tests.
                   c_args : lib_cargs,
                   link_with : lib_exe,
  )
  test(fname, exe)
endforeach

subdir('integration')
