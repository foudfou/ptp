# Copyright (c) 2017 Foudil Brétel.  All rights reserved.
# -*- mode: python -*-

tests = [
    ['list', 'list.c'],
    ['hash', 'hash.c'],
    ['bstree', 'bstree.c'],
    ['rbtree', 'rbtree.c'],
    ['aatree', 'aatree.c']
]

foreach t : tests
  exe = executable(t.get(0), t.get(1), include_directories : inc)
  test(t.get(0), exe)
endforeach
