# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

defs = configuration_data()
defs.set('guid_size_in_bytes', 20)
defs.set('k_const', 8)
configure_file(input : '../defs.h.in',
               output : 'kad_defs.h',
               configuration : defs)
