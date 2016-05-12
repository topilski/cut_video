// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#include "log.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

const char * log_text[] = {
  "MSG",
  "WARNING",
  "ERROR",
  "CRITICAL_ERROR"
};

log_level_t g_level = LOG_WARNING;

void dprintva(FILE * file, log_level_t level, const char * format, va_list ap) {
#ifdef WIN32
  #pragma message plz_implement
  return;
#else
  struct tm stm;
  time_t etime;
  char buf[16] = {0};

  time(&etime);
  localtime_r(&etime, &stm);
  strftime(buf, sizeof(buf), "%d-%b@%T", &stm);

  char * ret = NULL;
  const char * level_str = log_text[level];
  int res = asprintf(&ret, "[%s] %s : %s", level_str, buf, format);
  vfprintf(file, ret, ap);
  free(ret);
#endif
}

void set_log_level(log_level_t level) {
  g_level = level;
}

void debug_msg(const char *format, ...) {
  if (g_level <= LOG_MSG) {
    va_list ap;
    va_start(ap, format);
    dprintva(stdout, LOG_MSG, format, ap);
    va_end(ap);
  }
}

void debug_warning(const char *format, ...) {
  if (g_level <= LOG_WARNING) {
    va_list ap;
    va_start(ap, format);
    dprintva(stdout, LOG_WARNING, format, ap);
    va_end(ap);
  }
}

void debug_error(const char *format, ...) {
  if (g_level <= LOG_ERROR) {
    va_list ap;
    va_start(ap, format);
    dprintva(stdout, LOG_ERROR, format, ap);
    va_end(ap);
  }
}

void debug_critical_error(const char *format, ...) {
  if (g_level <= LOG_CRITICAL_ERROR) {
    va_list ap;
    va_start(ap, format);
    dprintva(stdout, LOG_CRITICAL_ERROR, format, ap);
    va_end(ap);
  }
}

void debug_critical_notify(const char *format, ...) {
  if (g_level <= LOG_CRITICAL_ERROR) {
    va_list ap;
    va_start(ap, format);
    dprintva(stdout, LOG_CRITICAL_ERROR, format, ap);
    va_end(ap);
  }
}

void debug_perror(const char* function, int err) {
  char* strer = strerror(err);
  debug_error("Function: %s, %s\n", function, strer);
}
