# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

tests_lib = [
    ['bits', 'bits.c'],
    ['list', 'list.c'],
    ['hash', 'hash.c'],
    ['bstree', 'bstree.c'],
    ['rbtree', 'rbtree.c'],
    ['aatree', 'aatree.c'],
    ['misc', 'misc.c'],
    ['u64', 'u64.c'],
    ['kad_bencode', 'kad_bencode.c'],
    ['kad_dht', 'kad_dht.c'],
]

foreach t : tests_lib
  exe = executable(t.get(0), t.get(1),
                   include_directories : inc,
                   # propagated for inclusion of .c files in tests.
                   c_args : lib_cargs,
                   link_with : lib_exe,
  )
  test(t.get(0), exe)
endforeach
