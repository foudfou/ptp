# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

integration_tests = [
    ['single-server', [main_exe]]
]

foreach t : integration_tests
  tname = t.get(0)
  texe = find_program(tname)
  targs = t.get(1)
  test(tname, texe, args : targs)
endforeach
