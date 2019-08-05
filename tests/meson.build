# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

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
    'kad/bencode.c',
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
                   link_with : libmain_a,
  )
  test(fname, exe)
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
