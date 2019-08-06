/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
bool get_home_dir(char out[]);
bool resolve_path(const char path[], char out[]);


#endif /* FILE_H */
