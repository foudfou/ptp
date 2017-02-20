/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef LOG_H
#define LOG_H
/**
 * Logging component.
 *
 * Logging can be set up to use syslog(3) or some stream (stdout, stderr or a
 * file). In the later case, messages sink to a dedicated thread via a
 * msgqueue.
 *
 * Inspired by http://kev009.com/wp/2010/12/no-nonsense-logging-in-c-and-cpp/
 * and Knot-DNS.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>

#define LOG_TIME_FORMAT "%Y-%m-%dT%H:%M:%S"

#define LOG_MSG_LEN 512
#define LOG_ERR_LEN 256

#define log_fatal(...)   log_msg(LOG_CRIT,    __VA_ARGS__)
#define log_error(...)   log_msg(LOG_ERR,     __VA_ARGS__)
#define log_warning(...) log_msg(LOG_WARNING, __VA_ARGS__)
#define log_notice(...)  log_msg(LOG_NOTICE,  __VA_ARGS__)
#define log_info(...)    log_msg(LOG_INFO,    __VA_ARGS__)
#define log_debug(...)   log_msg(LOG_DEBUG,   __VA_ARGS__)

typedef enum {
    LOG_TYPE_SYSLOG = 0, /*!< Logging to syslog(3) facility. */
    LOG_TYPE_STDOUT = 1, /*!< Print log messages to the stdout. */
    LOG_TYPE_STDERR = 2, /*!< Print log messages to the stderr. */
    LOG_TYPE_FILE   = 3  /*!< Generic logging to (unbuffered) file on the disk. */
} logtype_t;

struct lookup_table {
    int id;
    const char *name;
};

typedef struct lookup_table lookup_table_t;

static const lookup_table_t log_severities[] = {
    { LOG_UPTO(LOG_CRIT),    "critical" },
    { LOG_UPTO(LOG_ERR),     "error" },
    { LOG_UPTO(LOG_WARNING), "warning" },
    { LOG_UPTO(LOG_NOTICE),  "notice" },
    { LOG_UPTO(LOG_INFO),    "info" },
    { LOG_UPTO(LOG_DEBUG),   "debug" },
    { 0, NULL }
};

void (*log_msg)(int, const char *, ...);
int (*log_setmask)(int);

int log_stream_setlogmask(int mask);
void log_stream_msg(int prio, const char *fmt, ...);

/**
 * Log with error text corresponding to @errnum.
 *
 * Provide a *single* `%s` placeholder for the error text in @fmt.
 */
void log_perror(const char *fmt, const int errnum);
void log_debug_hex(const char buf[], const size_t len);

bool log_init(int type, int logmask);
bool log_shutdown(int logtype);

#endif /* LOG_H */