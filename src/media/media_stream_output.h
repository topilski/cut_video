// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "codec_holder.h"

#define DUMP_MEDIA 1

typedef struct media_stream_params_t {
  uint32_t height_video;
  uint32_t width_video;
  uint32_t video_fps;
  uint32_t bit_stream;
  enum AVCodecID codec_id;
} media_stream_params_t;

typedef struct media_stream_t {
  struct output_stream_t* ostream;

  uint64_t audio_pcm_id;
  uint64_t video_frame_id;
  uint64_t video_frame_sps_pps_id;
  uint32_t ts_fpackv_in_stream_msec;
  uint32_t ts_fpacka_in_stream_msec;

  uint32_t cur_ts_video_local_msec;
  uint64_t sample_id;

#if DUMP_MEDIA
  FILE * media_dump;
#endif

  media_stream_params_t params;
} media_stream_t;

media_stream_t* alloc_video_stream(const char * path_to_save, media_stream_params_t* params,
                                   int need_encode);
const char* get_media_stream_file_path(media_stream_t* stream);
int media_stream_encode_video_frame(media_stream_t* stream, const AVFrame* encoded_frame, AVPacket* out_packet,
                       int* got_packet);
int media_stream_write_video_frame(media_stream_t* stream, const uint8_t* data, size_t size);
void free_video_stream(media_stream_t * stream);
