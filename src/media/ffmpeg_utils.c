// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#include "ffmpeg_utils.h"

#include <stdio.h>

#include <libavutil/error.h>

#include "log.h"

void debug_av_perror(const char* function, int err) {
  char err_buff[AV_ERROR_MAX_STRING_SIZE] = {0};
  char * err_str = av_make_error_string(err_buff, AV_ERROR_MAX_STRING_SIZE, err);
  fprintf(stderr, "Function: %s, %s\n", function, err_str);
  fflush(stderr);
}
