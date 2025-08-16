# -*- mode: meson -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

integration_tests = [
  ['send_to_server.py defaults', 'send_to_server.py',
   ['defaults', main_exe, '-c', '/NONEXISTENT_FOR_DEFAULTS']],
  ['send_to_server.py routes state', 'send_to_server.py',
   ['routes_state', main_exe, '-c', join_paths(meson.current_source_dir(), 'data')]],
  ['listen_to_client.py bootstrap', 'listen_to_client.py', [main_exe]],
  ['multi_nodes.py bootstrap', 'multi_nodes.py', [main_exe]],
]

foreach t : integration_tests
  tname = t.get(0)
  texe = find_program(t.get(1))
  targs = t.get(2)
  test('integration/'+tname, texe, args : targs, is_parallel : false)
endforeach
