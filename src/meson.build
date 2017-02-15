# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

configure_file(input : 'config.h.in',
               output : 'config.h',
               configuration : conf)

main_sources = [
    'main.c',
    'log.c',
    'options.c',
    'server.c',
    'signals.c',
    'utils/safe.c',
]

main_exe = executable(
    meson.project_name(), main_sources,
    c_args : ['-D_XOPEN_SOURCE=700L', '-D_DEFAULT_SOURCE'],
    link_args : ['-lrt', '-lpthread']
)
