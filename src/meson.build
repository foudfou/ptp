# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

configure_file(input : 'config.h.in',
               output : 'config.h',
               configuration : conf)

main_sources = [
    'main.c',
    'utils/safe.c',
    'options.c',
    'log.c',
    'server.c',
]

mainexe = executable('server', main_sources,
                     c_args : ['-D_POSIX_C_SOURCE=200809L', '-D_DEFAULT_SOURCE'])
