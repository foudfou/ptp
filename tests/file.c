/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"

int main ()
{
    char home[PATH_MAX] = "\0";
    assert(get_home_dir(home));
    assert(strlen(home) > 0);

    int res = unsetenv("HOME");
    assert(res == 0);
    assert(get_home_dir(home));
    assert(strlen(home) > 0);

    char path[] = "~/.hello";
    char path_full[PATH_MAX] = "\0";
    assert(resolve_path(path, path_full));
    assert(strlen(path) < strlen(path_full));

    assert(resolve_path("/tmp", path_full));
    assert(strcmp("/tmp", path_full) == 0);

    return 0;
}
