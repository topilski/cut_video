// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#pragma once

#define CALC_FFMPEG_VERSION(a,b,c) ( a<<16 | b<<8 | c )

void debug_av_perror(const char* function, int err);
