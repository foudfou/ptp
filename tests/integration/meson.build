# -*- mode: meson -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

integration_tests = [
  ['send-to-server defaults', 'send-to-server',
   ['defaults', main_exe, '-c', '/NONEXISTENT_FOR_DEFAULTS']],
  ['send-to-server dht state', 'send-to-server',
   ['dht_state', main_exe, '-c', join_paths(meson.current_source_dir(), '../kad/data')]],
  ['listen-to-client bootstrap', 'listen-to-client', [main_exe]],
  ['multi-nodes bootstrap', 'multi-nodes', [main_exe]],
]

foreach t : integration_tests
  tname = t.get(0)
  texe = find_program(t.get(1))
  targs = t.get(2)
  test(tname, texe, args : targs, is_parallel : false)
endforeach
