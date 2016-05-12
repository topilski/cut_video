// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define SUCCESS_RESULT_VALUE 1
#define ERROR_RESULT_VALUE -1
#define INVALID_DESCRIPTOR -1
#define INFINITE_TIMEOUT_MSEC -1

#ifdef NDEBUG
#define VERIFY(x) x
#define DCHECK(x)
#define DCHECK_EQ(x, y)
#define DCHECK_NE(x, y)
#define DCHECK_LE(x, y)
#define DCHECK_LT(x, y)
#define DCHECK_GE(x, y)
#define DCHECK_GT(x, y)
#else
#define VERIFY(x) assert(x)
#define DCHECK(x) VERIFY(x)
#define DCHECK_EQ(x, y) assert(x == y)
#define DCHECK_NE(x, y) assert(x != y)
#define DCHECK_LE(x, y) assert(x <= y)
#define DCHECK_LT(x, y) assert(x < y)
#define DCHECK_GE(x, y) assert(x >= y)
#define DCHECK_GT(x, y) assert(x > y)
#endif

void handle_failure();
extern void debug_critical_error(const char *format, ...);

#define CHECK(x) if (!(x)) { \
  debug_critical_error("CHECK! %s:%d\n", __FILE__, __LINE__); handle_failure(); \
  }

#define DNOTREACHED() debug_critical_error("NOTREACHED! %s:%d\n", __FILE__, __LINE__); DCHECK(0)
#define NOTREACHED() DNOTREACHED(); handle_failure()
