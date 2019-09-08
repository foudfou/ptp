/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
#include <stddef.h>

bool get_home_dir(char out[]);
bool resolve_path(const char path[], char out[]);
bool file_read(char buf[], size_t *buf_len, const char path[]);
bool file_write(const char path[], char buf[], size_t buf_len);

#endif /* FILE_H */
