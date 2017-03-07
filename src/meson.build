# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

proj_name = meson.project_name()

configure_file(input : 'config.h.in',
               output : 'config.h',
               configuration : conf)

# A lib is a simple solution to let our test executables build against most of
# the code, that is except main.o.
lib_sources = [
    'log.c',
    'net/iobuf.c',
    'net/kad/dht.c',
    'net/kad/rpc.c',
    'net/kad/bencode.c',
    'net/msg.c',
    'options.c',
    'server.c',
    'signals.c',
    'utils/safer.c',
    'utils/u64.c',
]

cc = meson.get_compiler('c')
rt_dep = cc.find_library('rt', required : false)
thread_dep = dependency('threads')
lib_deps = [rt_dep, thread_dep]

lib_cargs = ['-D_XOPEN_SOURCE=700L', '-D_DEFAULT_SOURCE']

# A shared lib saves us having to propagate `dependencies : lib_deps` to the
# main exe and the tests.
lib_exe = shared_library(
    proj_name, lib_sources,
    c_args : lib_cargs,
    dependencies : lib_deps,
    install : false
)

main_exe = executable(
    proj_name, 'main.c',
    c_args : lib_cargs,
    link_with : lib_exe,
)
