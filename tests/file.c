/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"

int main ()
{
    char home[PATH_MAX] = "\0";
    assert(get_home_dir(home, PATH_MAX));
    assert(strlen(home) > 0);

    int res = unsetenv("HOME");
    assert(res == 0);
    assert(get_home_dir(home, PATH_MAX));
    assert(strlen(home) > 0);

    char path[] = "~/.hello";
    char path_full[PATH_MAX] = "\0";
    assert(resolve_path(path, path_full, PATH_MAX));
    assert(strlen(path) < strlen(path_full));

    assert(resolve_path("/tmp", path_full, PATH_MAX));
    assert(strcmp("/tmp", path_full) == 0);

    return 0;
}
