#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>
#include "config.h"
#include "log.h"

#define LOG_QUEUE_NAME     "/" PACKAGE_NAME "_log"
#define MSG_STOP           "/exit"
#define LOG_MSG_PREFIX_LEN 56

static struct log_ctx_t {
    int       fmask;
    FILE*     flog;
    pthread_t th_cons;
    mqd_t     mqw;
    mqd_t     mqr;
} log_ctx = {
    .fmask   = 0,
    .flog    = NULL,
    .th_cons = 0,
    .mqw     = -1,
    .mqr     = -1,
};

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
        strftime(tstr, LOG_MSG_LEN, LOG_TIME_FORMAT, &lt);
    }
}

int log_stream_setlogmask(int mask)
{
  int oldmask = log_ctx.fmask;
  if(mask == 0)
    return oldmask; /* POSIX definition for 0 mask */
  log_ctx.fmask = mask;
  return oldmask;
}

void log_stream_msg(int prio, const char *fmt, ...)
{
  if (!(LOG_MASK(prio) & log_ctx.fmask))
    return;

  char time[LOG_MSG_PREFIX_LEN] = {0};
  log_time(time);

  char buf[LOG_MSG_LEN+1] = {0};
  va_list arglist;
  va_start(arglist, fmt);
  int written = snprintf(buf, LOG_MSG_LEN, "%s [%s] ", time,
                         log_level_prefix(prio));
  written += vsnprintf(buf + written, LOG_MSG_LEN - written, fmt, arglist);
  buf[written]    = '\n';
  buf[LOG_MSG_LEN] = '\0';
  va_end(arglist);

  if (mq_send(log_ctx.mqw, buf, LOG_MSG_LEN, 0) < 0)
      perror("mq_send log_mq");
}

void log_perror(const char *fmt, const int errnum)
{
    char errtxt[LOG_ERR_LEN];
    strerror_r(errnum, errtxt, LOG_ERR_LEN);
    log_msg(LOG_ERR, fmt, errtxt);
}

void *log_queue_consumer(void *data)
{
    (void)data;

    char buf[LOG_MSG_LEN + 1];
    int must_stop = 0;

    do {
        ssize_t bytes_read = mq_receive(log_ctx.mqr, buf, LOG_MSG_LEN, NULL);
        if (bytes_read < 0) {
            perror("mq_receive");
        }

        buf[bytes_read] = '\0';
        if (!strncmp(buf, MSG_STOP, strlen(MSG_STOP)))
            must_stop = 1;
        else
            fputs(buf, log_ctx.flog);
    }
    while (!must_stop);

    // TODO: void pthread_cleanup_push(void (*routine)(void *), void *arg);
    if (log_ctx.mqr != (mqd_t)-1 && mq_close(log_ctx.mqr) == (mqd_t)-1)
        perror("mq_close RO");
    if (mq_unlink(LOG_QUEUE_NAME) == (mqd_t)-1)
        perror("mq_unlink");

    pthread_exit(NULL);
}

bool log_queue_shutdown()
{
    bool ret = true;

    char buf[LOG_MSG_LEN + 1];
    memset(buf, 0, LOG_MSG_LEN + 1);
    strcpy(buf, MSG_STOP);
    if (mq_send(log_ctx.mqw, buf, LOG_MSG_LEN, 0) < 0)
        perror("mq_send STOP");

    if (log_ctx.mqw != (mqd_t)-1 && mq_close(log_ctx.mqw) == (mqd_t)-1)
        ret = false;

    int rv = pthread_join(log_ctx.th_cons, NULL); // FIXME: void **retval

    return ret;
}

bool log_queue_init(void)
{
    // FIXME:
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = LOG_MSG_LEN;
    attr.mq_curmsgs = 0;

    int attempts = 0;
    while (log_ctx.mqw == (mqd_t)-1 && attempts++ < 3) {
        log_ctx.mqw = mq_open(LOG_QUEUE_NAME, O_CREAT | O_EXCL | O_WRONLY,
                         0644, &attr);
        if (log_ctx.mqw == (mqd_t)-1) {
            if (errno == EEXIST) {
                fprintf(stderr, "Removing stale message queue.\n");
                mq_unlink(LOG_QUEUE_NAME);
                continue;
            }
            else {
                perror("mq_open WO");
                return false;
            }
        }
    }

    log_ctx.mqr = mq_open(LOG_QUEUE_NAME, O_RDONLY, 0644, &attr);
    if (log_ctx.mqr == (mqd_t)-1) {
        perror("mq_open RO");
        return false;
    }

    return true;
}

bool log_init(int logtype, int logmask)
{
    log_msg = &log_stream_msg;
    log_setmask = &log_stream_setlogmask;

    switch (logtype) {
    case LOG_TYPE_SYSLOG:
        log_msg = &syslog;
        log_setmask = &setlogmask;
        openlog(PACKAGE_NAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        break;

    case LOG_TYPE_STDOUT:
        log_ctx.flog = stdout;
        break;

    case LOG_TYPE_STDERR:
        log_ctx.flog = stderr;
        break;

    default:
        fprintf(stderr, "Unsupported log type %d.\n", logtype);
        return false;
    }

    log_setmask(logmask);

    if (!log_queue_init()) {
        fprintf(stderr, "Failed to init message queue.\n");
        return false;
    }

    // FIXME: catch sigterm to cleanup
    if (pthread_create(&log_ctx.th_cons, NULL, log_queue_consumer, NULL) == -1) {
        perror("pthread_create");
        return false;
    }

    return true;
}

bool log_shutdown(int logtype)
{
    log_debug("Stopping logging.");

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

    log_queue_shutdown();

    return true;
}
