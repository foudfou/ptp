# -*- mode: meson -*-
# Copyright (c) 2016 Foudil Brétel.  All rights reserved.

project('ptp', 'c', version : '0.0.2', license : 'MIT',
        default_options : [
          'c_std=c23',
          'warning_level=3',
          'werror=true'
        ],
        meson_version: '>=0.56')

proj_name = meson.project_name()

conf = configuration_data()
conf.set_quoted('packagename', meson.project_name())
conf.set_quoted('packagevers', meson.project_version())
conf.set_quoted('packagecopy',
                'Copyright (c) 2014 Foudil Brétel. All rights reserved.')
conf.set_quoted('datadir', get_option('prefix') / get_option('datadir') / proj_name)

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
subdir('tools')
subdir('doc')
subdir('data')

sh = find_program('sh')
run_command(sh, '-c', 'git config core.hooksPath tools/git-hooks', check: true)
