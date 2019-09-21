/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils/safer.h"

bool get_home_dir(char out[], const size_t len)
{
    char *home = getenv("HOME");
    if (home) {
        return strcpy_safer(out, home, len);
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

bool resolve_path(const char path[], char out[], const size_t out_len)
{
    if (strncmp(path, "~/", 2) == 0) {
        char home[PATH_MAX] = "\0";
        if (!get_home_dir(home, PATH_MAX))
            return false;
        if ((size_t)snprintf(out, PATH_MAX, "%s%s", home, path+1) > out_len) {
            fprintf(stderr, "Can't snprintf as destination buffer too small\n");
            return false;
        }
    }
    else {
        if ((size_t)snprintf(out, PATH_MAX, "%s", path) > out_len) {
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
    bool ret = true;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("Failed fopen");
        ret = false; goto cleanup;
    }
    if (fseek(fp, 0L, SEEK_END) < 0) {
        perror("Failed fseek");
        ret = false; goto cleanup;
    }
    long pos = ftell(fp);
    if (pos < 0) {
        perror("Failed ftell");
        ret = false; goto cleanup;
    }
    *buf_len = pos;
    if (fseek(fp, 0L, SEEK_SET) < 0) {
        perror("Failed rewind");
        ret = false; goto cleanup;
    }
    clearerr(fp);
    fread(buf, *buf_len, 1, fp);
    if (ferror(fp)) {
        perror("Failed fread");
        ret = false; goto cleanup;
    }

  cleanup:
    if (fclose(fp)) {
        perror("Failed fclose");
        ret = false;
    }
    return ret;
}

bool file_write(const char path[], char buf[], size_t buf_len)
{
    bool ret = true;

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("Failed fopen");
        ret = false; goto cleanup;
    }
    clearerr(fp);
    fwrite((void*)buf, buf_len, 1, fp);
    if (ferror(fp)) {
        perror("Failed fwrite");
        ret = false; goto cleanup;
    }

  cleanup:
    if (fclose(fp)) {
        perror("Failed fclose");
        ret = false;
    }
    return ret;
}
