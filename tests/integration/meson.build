# -*- mode: meson -*-
# Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved.

integration_tests = [
  ['single-server defaults', 'single-server',
   ['defaults', main_exe, '-c', '/NONEXISTENT_FOR_DEFAULTS']],
  ['single-server dht state', 'single-server',
   ['dht_state', main_exe, '-c', join_paths(meson.current_source_dir(), '../kad/data')]],
]

foreach t : integration_tests
  tname = t.get(0)
  texe = find_program(t.get(1))
  targs = t.get(2)
  test(tname, texe, args : targs, is_parallel : false)
endforeach
