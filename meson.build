# -*- mode: python -*-
# Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved.

project('ptp', 'c', version : '0.0.2', license : 'MIT',
        default_options : [
            'c_std=c11',
            'warning_level=3',
            'werror=true'
        ],
        meson_version: '>=0.50')

conf = configuration_data()
conf.set_quoted('packagename', meson.project_name())
conf.set_quoted('packagevers', meson.project_version())
conf.set_quoted('packagecopy',
                'Copyright (c) 2017-2019 Foudil Brétel. All rights reserved.')

compiler_id = meson.get_compiler('c').get_id()
if compiler_id == 'clang'
  # Silence clang wrongly complaining about missing braces around
  # initialization of subobject (`struc aggr some = {0}`)
  # Related: https://bugs.llvm.org/show_bug.cgi?id=21689
  # In gcc: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119
  add_project_arguments('-Wno-missing-braces', language : 'c')
endif

base_inc = include_directories(['src'])

subdir('src')
subdir('tests')
