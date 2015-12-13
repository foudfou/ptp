#if defined(__STRICT_ANSI__) || defined(PEDANTIC)
# ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 700
# endif
#endif

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils/defs.h"
#include "utils/string_s.h"

#define PIPE_STDOUT 0
#define PIPE_STDERR 1
#define PIPE_READ   0
#define PIPE_WRITE  1

#ifndef PATH_MAX
  #define PATH_MAX    2048
#endif
#define BUF_SIZE      2048

struct test_
{
    char  filename[PATH_MAX];
    int   pout[2];
    int   perr[2];
    pid_t pid;
    int   ret;
    char  out[BUF_SIZE];
    char  err[BUF_SIZE];
};

int read_child_input(const int fd, char *buf) {
    int count = read(fd, buf, BUF_SIZE-1);
    buf[count] = '\0';
    return count;
}

void test_report(const struct test_ test, const int cols) {
    printf("%s ", test.filename);

    const int PAD_MAX = cols / 2;
    int pad_len = PAD_MAX - (int)strlen(test.filename);
    char pad[PAD_MAX];
    snprintf(pad, pad_len, "%s", "................................................................................");

    int rv = 0;
    if (WIFEXITED(test.ret)) {
        rv = WEXITSTATUS(test.ret);
        printf("%s" ANSI_COLOR_GREEN " PASS" ANSI_COLOR_RESET " (exit %d)\n",
               pad, rv);
    }
    else if (WIFSIGNALED(test.ret)) {
        rv = WTERMSIG(test.ret);
        char *sig = strsignal(rv);
        printf("%s" ANSI_COLOR_RED " FAILED" ANSI_COLOR_RESET " (killed %s)\n",
               pad, sig);
        printf("  " ANSI_COLOR_WHITE "stderr:" ANSI_COLOR_RESET " %s\n", test.err);
        printf("  " ANSI_COLOR_WHITE "stdout:" ANSI_COLOR_RESET " %s\n", test.out);
    }
    else {
        printf("%s" ANSI_COLOR_MAGENTA " UNKNOWN" ANSI_COLOR_RESET " (exit %d)\n",
               pad, test.ret);
        printf("  " ANSI_COLOR_WHITE "stderr:" ANSI_COLOR_RESET " %s\n", test.err);
    }
}

int main(int argc, char *argv[])
{
    int tests_count = argc - 1;
    struct test_ tests[tests_count];

    for (int i = 0; i < tests_count; i++) {
        int rv = safe_strcpy(tests[i].filename, argv[i + 1], PATH_MAX);
        if (rv != PTP_SUCC)
            fprintf(stderr, "strcpy failed for %s: %d\n", argv[i + 1], rv);

        if ((pipe(tests[i].pout) == -1) ||
            (pipe(tests[i].perr) == -1)) {
            perror("pipe");
            return PTP_ESYS;
        }

        fflush(stdout); fflush(stderr);
        switch(tests[i].pid = fork()) {
        case -1:
            perror("fork");
            return PTP_ESYS;

        case 0:                 /* Child - writes to pipes */
            close(tests[i].pout[PIPE_READ]);
            close(tests[i].perr[PIPE_READ]);

            if (dup2(tests[i].pout[PIPE_WRITE], STDOUT_FILENO) < 0) {
                perror ("dup2");
                return PTP_ESYS;
            }
            if (dup2(tests[i].perr[PIPE_WRITE], STDERR_FILENO) < 0) {
                perror ("dup2");
                return PTP_ESYS;
            }
            close(tests[i].pout[PIPE_WRITE]);
            close(tests[i].perr[PIPE_WRITE]);

            execl(tests[i].filename, tests[i].filename, (char *)NULL);
            perror("execl");
            return PTP_ESYS;

        default:                /* Parent - reads from pipe */
            close(tests[i].pout[PIPE_WRITE]);
            close(tests[i].perr[PIPE_WRITE]);
        }
    }

    struct winsize win;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);

    pid_t wpid;
    int status = 0;
    while ((wpid = wait(&status)) > 0) {
        int i = 0;
        for (; i < tests_count; i++)
            if (wpid == tests[i].pid) break;
        if (i == tests_count) {
            fprintf(stderr, "Unknown pid %d\n", (int)wpid);
            return PTP_ERUN;
        }

        read_child_input(tests[i].perr[PIPE_READ], tests[i].err);
        read_child_input(tests[i].pout[PIPE_READ], tests[i].out);
        close(tests[i].pout[PIPE_READ]);
        close(tests[i].perr[PIPE_READ]);

        if (WIFSTOPPED(status)) {
            fprintf(stderr, "Child %s (%d) stopped\n",
                    tests[i].filename, (int)wpid);
            // FIXME: handle error -> kill()
        }
        tests[i].ret = status;

        test_report(tests[i], win.ws_col);
    }

    return(0);
}
