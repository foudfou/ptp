# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

libkad_sources = [
    'dht.c',
    'rpc.c',
    'bencode.c',
]

subdir('_full')
main_inc = [base_inc, include_directories('_full')]

libkad_a = static_library(
    proj_name+'kad', libkad_sources,
    c_args : lib_cargs,
    dependencies : lib_deps,
    include_directories : main_inc,
)


subdir('_tiny')
alt_inc = [base_inc, include_directories('_tiny')]

libkadalt_a = static_library(
    proj_name+'kadalt', libkad_sources,
    c_args : lib_cargs,
    dependencies : lib_deps,
    include_directories : alt_inc,
)
