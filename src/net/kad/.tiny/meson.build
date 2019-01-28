# -*- mode: python -*-
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

defs_alt = configuration_data()
# DON'T CHANGE THESE VALUES! TESTS RELY ON IT.
defs_alt.set('guid_size_in_bytes', 1)
# yes, we'll only be using the first 4 bits
defs_alt.set('guid_size_in_bits', 4)
defs_alt.set('k_const', 6)
configure_file(input : '../defs.h.in',
               output : 'kad_defs.h',
               configuration : defs_alt)
