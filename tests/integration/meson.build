# -*- mode: meson -*-
# Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved.

integration_tests = [
  ['single-server default options', 'single-server', [main_exe, '-c', '/tmp/.NONEXISTENT_FOR_DEFAULTS']],
]

foreach t : integration_tests
  tname = t.get(0)
  texe = find_program(t.get(1))
  targs = t.get(2)
  test(tname, texe, args : targs)
endforeach
