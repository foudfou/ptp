# -*- mode: python -*-

project('ptp', 'c', version : '0.0.1', license : 'MIT',
        default_options : ['c_std=c11'])

conf = configuration_data()
conf.set_quoted('appname', meson.project_name())
conf.set_quoted('appvers', meson.project_version())
conf.set_quoted('appcopy',
                'Copyright (c) 2014 Foudil Brétel. All rights reserved.')

add_global_arguments('-Wfatal-errors', '-pedantic', '-Wextra', language : 'c')

inc = include_directories(['.', 'src', 'src/utils'])

# subdir('include')
subdir('src')
subdir('tests')
