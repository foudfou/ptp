# -*- mode: meson -*-
# Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved.

cover_bin = find_program('coverage')

cover = run_target('cover', command : cover_bin)

# Use '--xml' to find error id to ignore.
cppcheck = run_target('cppcheck', command : [
  'cppcheck', '--enable=all', '--std=c11', '--inconclusive',
  # '--project='+join_paths(meson.build_root(), 'compile_commands.json'),
  '--error-exitcode=1', '--inline-suppr',
  '--suppress=assertWithSideEffect',
  '--suppress=redundantAssignment:'+join_paths(meson.source_root(), 'tests/kad/dht_alt.c'),
  '--suppress=nullPointerRedundantCheck:'+join_paths(meson.source_root(), 'tests/utils/rbtree.c'),
  '-I'+join_paths(meson.source_root(), 'src'),
  '-I'+join_paths(meson.source_root(), 'tests'),
  '-I'+join_paths(meson.build_root(), 'src'),
  '-I'+join_paths(meson.build_root(), 'src/net/kad/.full'),
  '-I'+join_paths(meson.build_root(), 'src/net/kad/.tiny'),
  join_paths(meson.source_root(), 'src'),
  join_paths(meson.source_root(), 'tests'),
  '--xml',
  # '--check-config',
])
