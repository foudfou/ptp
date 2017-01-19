# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

project('ptp', 'c', version : '0.0.2', license : 'MIT',
        default_options : ['c_std=c11'])

conf = configuration_data()
conf.set_quoted('packagename', meson.project_name())
conf.set_quoted('packagevers', meson.project_version())
conf.set_quoted('packagecopy',
                'Copyright (c) 2017 Foudil Brétel. All rights reserved.')

add_global_arguments('-Wfatal-errors', '-pedantic', '-Wextra', language : 'c')

inc = include_directories(['src'])

subdir('src')
subdir('tests')
