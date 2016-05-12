// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#pragma once

#include <time.h>
#include <stdint.h>

char* convert_ms_2string(uint64_t mssec);  // deleting the storage when it is no longer needed
uint64_t currentms(void);
uint64_t systemms(void);
uint64_t currentns(void);
uint64_t convert_timespec_2ms(struct timespec tp);
char * format_timestamp(uint64_t timestamp);  // deleting the storage when it is no longer needed
