// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#include "time_utils.h"

#include <sys/time.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#ifndef CLOCK_MONOTONIC_RAW
  #define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif
#define gmtime_r(tp, tm)  ((gmtime_s((tm), (tp)) == 0) ? (tp) : NULL)
#endif

#ifdef __APPLE__
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
#define CLOCK_MONOTONIC_RAW 0

int clock_gettime(int clk_id, struct timespec* t) {
  struct timeval now;
  int rv = gettimeofday(&now, NULL);
  if (rv) return rv;
  t->tv_sec  = now.tv_sec;
  t->tv_nsec = now.tv_usec * 1000;
  return 0;
}  // namespace
#endif

#define MS_MAX_SIZE 32

char* convert_ms_2string(uint64_t mssec) {
  int msec = mssec % 1000;
  int seconds = (mssec / 1000) % 60;
  int minutes = (mssec / 1000) / 60;
  int hours = (mssec / 1000) / 3600;

  char * fmt = (char*)calloc(MS_MAX_SIZE, sizeof(char));
  if (!fmt) {
      return NULL;
  }

  snprintf(fmt, MS_MAX_SIZE, "%02d:%02d:%02d.%03d", hours, minutes, seconds, msec);

  return fmt;
}

uint64_t currentms(void) {
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
  return convert_timespec_2ms(tp);
}

uint64_t systemms(void) {
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return convert_timespec_2ms(tp);
}

uint64_t currentns(void) {
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
  return ((uint64_t)tp.tv_sec) * 1000000000ULL + tp.tv_nsec;
}

uint64_t convert_timespec_2ms(struct timespec tp) {
  return ((uint64_t)tp.tv_sec) * 1000ULL + tp.tv_nsec / 1000000ULL;
}

char * format_timestamp(uint64_t timestamp) {
  struct timeval  tv;
  struct tm       tm;

  tv.tv_sec = timestamp / 1000ULL;
  tv.tv_usec = timestamp % 1000ULL;
  time_t* ptime = (time_t*)&tv.tv_sec;
  if (gmtime_r(ptime, &tm) != NULL) {
      char fmt[64], buf[64];
      strftime(fmt, sizeof fmt, "%Y-%m-%dT%H:%M:%S.%%03uZ", &tm);
      snprintf(buf, sizeof buf, fmt, tv.tv_usec);
      return strdup(buf);
  }

  return NULL;
}
