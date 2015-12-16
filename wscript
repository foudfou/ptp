#!/usr/bin/env python
# Copyright (c) 2014 Foudil Brétel. All rights reserved.

import waflib
from waflib import Build, ConfigSet, Logs, Options, Scripting, Task

VERSION = '0.0.1'
APPNAME = 'ptp'

top = '.'
out = 'build'


def options(ctx):
    ctx.load('compiler_c')
    ctx.load('waf_unit_test')

    cnf = ctx.get_option_group('Configuration options')
    cnf.add_option('--without-debug', action='store_false', default=True,
                   dest='debug', help='Compile with debug information')

    bld = ctx.get_option_group('Build and installation options')
    bld.add_option('--valgrind', type='string', action='store', default=False,
                   dest='do_valgrind', help='Run valgrind with arguments [string]')
    bld.add_option('--coverage', action='store_true', default=False,
                   dest='do_coverage', help='Run code coverage')

    compil = ctx.get_option_group('Configuration options')
    compil.add_option('--without-debug', action='store_false', default=True,
                   dest='debug', help='Compile with debug information')
    compil.add_option('--without-coverage', action='store_false', default=True,
                   dest='coverage', help='Compile with coverage instrumentation')


def configure(ctx):
    ctx.env.APPNAME = APPNAME
    ctx.recurse('src tests')

    ctx.load('compiler_c')
    ctx.load('waf_unit_test')

    CFLAGS_MIN = ['-Wall', '-std=c11']
    ctx.check_cc(cflags=CFLAGS_MIN)
    ctx.check_cc(function_name='strsignal', header_name="string.h")

    ctx.find_program('valgrind', var='VALGRIND', mandatory=False)
    ctx.find_program('lcov', var='LCOV', mandatory=ctx.options.coverage)
    ctx.find_program('genhtml', var='GENHTML', mandatory=ctx.options.coverage)

    release = 'Release'
    if ctx.options.debug:
        release = 'Debug'
        ctx.env.append_value('CFLAGS', ['-g'])
    ctx.msg("Compilation mode", release)

    ctx.env.append_unique("CFLAGS", CFLAGS_MIN + [
        '-Wfatal-errors', '-pedantic', '-Wextra',
    ])

    data_dir = ctx.path.make_node('data')
    data_dir.mkdir()
    ctx.env.DATA_DIR = data_dir.abspath()
    with_coverage = 'no'
    if ctx.options.coverage:
        with_coverage = 'yes'
        ctx.env.append_value('CFLAGS', ['--coverage'])
        lcov_dir = data_dir.make_node('/cov')
        lcov_dir.mkdir()
        ctx.env.LCOV_DIR = lcov_dir.abspath()

        if ctx.env['COMPILER_CC'] == 'clang':
            ctx.env.append_value('STLIBPATH', ['/usr/lib/clang/3.4.2/lib/linux'])
            ctx.env.append_value('STLIB', ['clang_rt.profile-x86_64'])
        elif ctx.env['COMPILER_CC'] == 'gcc':
            ctx.env.append_value('LIB', ['gcov'])
    ctx.msg("Coverage enabled", with_coverage)

    ctx.define('PACKAGE_NAME', APPNAME)
    ctx.define('PACKAGE_VERSION', VERSION)
    ctx.define('PACKAGE_COPYRIGHT',
               "Copyright (c) 2014 Foudil Brétel. All rights reserved.")
    ctx.write_config_header('config.h')


def build(ctx):
    ctx.stash = {}

    ctx.recurse('src tests')

    if ctx.cmd == "test":
        tsk = runtests(env=ctx.env)
        args = [ctx.stash['test_runner_node']] + ctx.stash['test_nodes']
        tsk.set_inputs(args)
        ctx.add_to_group(tsk)


class runtests(Task.Task):
    def run(self):
        import os
        os.system(" ".join([t.abspath() for t in self.inputs]))
runtests = waflib.Task.always_run(runtests)


class TestContext(Build.BuildContext):
    "run tests (waf_unit_test blocks on io)"
    cmd = 'test'


def tags(ctx):
    cmd = 'find src include tests -type f -name "*.c" -o -name "*.h"' \
          ' | etags -'
    ctx.exec_command(cmd)


def distclean(ctx):
    import os

    CACHE_PATH = out + "/c4che/_cache.py"
    node = ctx.path.find_node(CACHE_PATH)
    if node:
        env = ConfigSet.ConfigSet()
        env.load(node.abspath())
        if ctx.options.coverage:
            _rm_in_dir(env.get_flat('LCOV_DIR'))

    if env['COMPILER_CC'] == 'gcc':
        gch = ctx.path.ant_glob('**/*.gch')
        for f in gch:
            os.remove(f.abspath())

    Scripting.distclean(ctx)


def _rm_in_dir(tgt):
    import glob, os
    for root, dirs, files in os.walk(tgt, topdown=False):
        for f in files:
            if f != ".gitignore":
                os.remove(os.path.join(root, f))
        for d in dirs:
            os.rmdir(os.path.join(root, d))

# TODO: splint, tests
