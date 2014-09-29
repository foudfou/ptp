#!/usr/bin/env python
# Copyright (c) 2014 Foudil Brétel. All rights reserved.

import waflib
from waflib import Build, ConfigSet, Logs, Options, Scripting

VERSION = '0.0.1'
APPNAME = 'ptp'

top = '.'
out = 'build'


def options(ctx):
    ctx.load('compiler_c')
    cnf = ctx.get_option_group('Configuration options')
    cnf.add_option('--without-debug', action='store_false', default=True,
                   dest='debug', help='Compile with debug information')


def configure(ctx):
    ctx.env.APPNAME = APPNAME
    ctx.recurse('src tests')

    ctx.load('compiler_c')
    ctx.check_cc(cflags=['-Wall', '-std=c99'])

    release = 'Release'
    if ctx.options.debug:
        release = 'Debug'
        ctx.env.append_value('CFLAGS', ['-g'])
    ctx.msg("Compilation mode", release)

    ctx.env.append_unique("CFLAGS", [
        '-Wall', '-pedantic', '-Wextra', '-std=c99', '-Wfatal-errors',
    ])

    ctx.define('PACKAGE_NAME', APPNAME)
    ctx.define('PACKAGE_VERSION', VERSION)
    ctx.define('PACKAGE_COPYRIGHT',
               "Copyright (c) 2014 Foudil Brétel. All rights reserved.")


def build(ctx):
    ctx.recurse('src tests')


def tags(ctx):
    cmd = 'find src include test -type f -name "*.c" -o -name "*.h"' \
          ' | etags -'
    ctx.exec_command(cmd)


# TODO: splint, tests
