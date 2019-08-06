/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils/safer.h"

bool get_home_dir(char out[])
{
    char *home = getenv("HOME");
    if (home) {
        return strcpy_safer(out, home, PATH_MAX);
    }

    char *login = getlogin();
    if (!login) {
        errno = 0;
        perror("Failed getlogin: %s");
        return false;
    }

    struct passwd *pw_result = getpwnam(login);
    if (!pw_result) {
        errno = 0;
        perror("Failed getpwnam: %s");
        return false;
    }

    return strcpy_safer(out, pw_result->pw_dir, PATH_MAX);
}

bool resolve_path(const char path[], char out[])
{
    if (strncmp(path, "~/", 2) == 0) {
        char home[PATH_MAX] = "\0";
        if (!get_home_dir(home))
            return false;
        if (snprintf(out, PATH_MAX, "%s%s", home, path+1) > PATH_MAX) {
            fprintf(stderr, "Can't snprintf as destination buffer too small\n");
            return false;
        }
    }
    else {
        if (snprintf(out, PATH_MAX, "%s", path) > PATH_MAX) {
            fprintf(stderr, "Can't snprintf as destination buffer too small\n");
            return false;
        }
    }
    return true;
}
