# -*- mode: meson -*-
# Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved.

cdata = configuration_data()
cdata.set('PROJ_NAME', proj_name)
cdata.set('PROJ_NAME_UPPER', proj_name.to_upper())
cdata.set('DATADIR', get_option('prefix') / get_option('datadir') / proj_name)
man = configure_file(input : 'ptp.1.in',
                     output : 'ptp.1',
                     configuration : cdata)

install_man(man)
