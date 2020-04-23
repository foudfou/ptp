# -*- mode: meson -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

GUID_SIZE_IN_BYTES = 20

defs = configuration_data()
defs.set('guid_size_in_bytes', GUID_SIZE_IN_BYTES)
# Yes, this assumes CHAR_BIT == 8
defs.set('guid_size_in_bits', GUID_SIZE_IN_BYTES * 8)
defs.set('k_const', 8)
defs.set('alpha_const', 3)
configure_file(input : '../defs.h.in',
               output : 'kad_defs.h',
               configuration : defs)
