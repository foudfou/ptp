# -*- mode: meson -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

libkad_sources = [
  'bencode/globals.c',
  'bencode/parser.c',
  'bencode/routes.c',
  'bencode/rpc_msg.c',
  'bencode/serde.c',
  'lookup.c',
  'routes.c',
  'rpc.c',
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
