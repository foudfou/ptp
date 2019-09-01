# -*- mode: python -*-
# Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved.

libkad_sources = [
    'dht.c',
    'rpc.c',
    'bencode/parser.c',
    'bencode/serde.c',
    'bencode/rpc_msg.c',
    'bencode/dht.c',
]

subdir('.full')
main_inc = [base_inc, include_directories('.full')]

libkad_a = static_library(
    proj_name+'kad', libkad_sources,
    c_args : lib_cargs,
    dependencies : lib_deps,
    include_directories : main_inc,
)


subdir('.tiny')
alt_inc = [base_inc, include_directories('.tiny')]

libkadalt_a = static_library(
    proj_name+'kadalt', libkad_sources,
    c_args : lib_cargs,
    dependencies : lib_deps,
    include_directories : alt_inc,
)
