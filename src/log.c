/**
 * logging thread. messages are received via a msgqueue
 *
 * Inspired by http://kev009.com/wp/2010/12/no-nonsense-logging-in-c-and-cpp/
 * and  Knot-DNS.
 */

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include "config.h"
#include "log.h"

#define LOG_BUFLEN 512

static int _streammask;
static FILE* _stream;

int log_stream_setlogmask(int mask)
{
  int oldmask = _streammask;
  if(mask == 0)
    return oldmask; /* POSIX definition for 0 mask */
  _streammask = mask;
  return oldmask;
}

static const char *log_level_prefix(int level)
{
    switch (level) {
    case LOG_DEBUG:   return "debug";
    case LOG_INFO:    return "info";
    case LOG_NOTICE:  return "notice";
    case LOG_WARNING: return "warning";
    case LOG_ERR:     return "error";
    case LOG_CRIT:    return "critical";
    default:
        return NULL;
    };
}

static void log_time(char tstr[])
{
    struct tm lt;
    time_t epoch;
    time(&epoch);
    if (localtime_r(&epoch, &lt) != NULL) {
        strftime(tstr, LOG_BUFLEN, LOG_TIME_FORMAT, &lt);
    }
}

void log_stream(int prio, const char *fmt, ...)
{
  if (!(LOG_MASK(prio) & _streammask))
    return;

  char time[LOG_BUFLEN] = {0};
  log_time(time);

  va_list arglist;
  va_start(arglist, fmt);
  fprintf(_stream, "%s [%s] ", time, log_level_prefix(prio));
  vfprintf(_stream, fmt, arglist);
  fprintf(_stream, "\n");
  va_end(arglist);
}

bool log_setup(int logtype, int logmask)
{
    log_msg = &log_stream;
    log_setmask = &log_stream_setlogmask;

    switch (logtype) {
    case LOG_TYPE_SYSLOG:
        log_msg = &syslog;
        log_setmask = &setlogmask;
        openlog(PACKAGE_NAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        break;

    case LOG_TYPE_STDOUT:
        _stream = stdout;
        break;

    case LOG_TYPE_STDERR:
        _stream = stderr;
        break;

    default:
        // FIXME:
        return false;
    }

    log_setmask(logmask);
    return true;
}

bool log_shutdown(int logtype)
{
    switch (logtype) {
    case LOG_TYPE_SYSLOG: {
        closelog();
        break;
    }
    case LOG_TYPE_STDOUT: {
        break;
    }
    default:
        return false;
    }

    return true;
}
