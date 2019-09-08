/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <errno.h>
#include <limits.h>
#include <pwd.h>
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
        perror("Failed getlogin");
        return false;
    }

    struct passwd *pw_result = getpwnam(login);
    if (!pw_result) {
        errno = 0;
        perror("Failed getpwnam");
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

/**
 * Reads binary content of @path into @buf. Aka slurp.
 *
 * CAUTION: reads up to @buf_len.
 */
// TODO look into using iobuf
bool file_read(char buf[], size_t *buf_len, const char path[])
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("Failed fopen");
        return false;
    }
    if (fseek(fp, 0L, SEEK_END) < 0) {
        perror("Failed fseek");
        return false;
    }
    long pos = ftell(fp);
    if (pos < 0) {
        perror("Failed ftell");
        return false;
    }
    *buf_len = pos;
    if (fseek(fp, 0L, SEEK_SET) < 0) {
        perror("Failed rewind");
        return false;
    }
    clearerr(fp);
    fread(buf, *buf_len, 1, fp);
    if (ferror(fp)) {
        perror("Failed fread");
        return false;
    }
    if (fclose(fp)) {
        perror("Failed fclose");
        return false;
    }
    return true;
}

bool file_write(const char path[], char buf[], size_t buf_len)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("Failed fopen");
        return false;
    }
    clearerr(fp);
    fwrite((void*)buf, buf_len, 1, fp);
    if (ferror(fp)) {
        perror("Failed fwrite");
        return false;
    }
    if (fclose(fp)) {
        perror("Failed fclose");
        return false;
    }
    return true;
}
