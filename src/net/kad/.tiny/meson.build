# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

defs_alt = configuration_data()
defs_alt.set('guid_size_in_bytes', 2)
defs_alt.set('k_const', 3)
configure_file(input : '../defs.h.in',
               output : 'kad_defs.h',
               configuration : defs_alt)
