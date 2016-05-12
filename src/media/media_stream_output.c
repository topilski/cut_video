// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#include "media_stream_output.h"

#include "macros.h"
#include "time_utils.h"
#include "ffmpeg_utils.h"

#define PCM_SAMPLES_COUNT 1024

#define SAVE_LOCAL_TIME 0
#define SAVE_FRAME_ID 2
#define SAVE_FRAME_POLICY SAVE_FRAME_ID

media_stream_t* alloc_video_stream(const char * path_to_save, media_stream_params_t* params,
                                   int need_encode) {
  if (!path_to_save || !params) {
    return NULL;
  }

  media_stream_t* stream = (media_stream_t*)malloc(sizeof(media_stream_t));
  if (!stream) {
    return NULL;
  }

  stream->ostream = NULL;
  stream->audio_pcm_id = 0;
  stream->video_frame_id = 0;
  stream->video_frame_sps_pps_id = 0;
  stream->cur_ts_video_local_msec = 0;
  stream->ts_fpackv_in_stream_msec = 0;
  stream->ts_fpacka_in_stream_msec = 0;
  stream->sample_id = 0;
#if DUMP_MEDIA
  char media_dump_path[PATH_MAX] = {0};
  sprintf(media_dump_path, "%s.data", path_to_save);
  stream->media_dump = fopen(media_dump_path, "wb");
#endif
  int res;
  AVFormatContext* formatContext = NULL;

  if (need_encode){
    stream->ostream = alloc_output_stream(NULL, path_to_save, NULL);
    if (stream->ostream) {
      debug_msg("Created output media file path: %s!\n", path_to_save);
      formatContext = stream->ostream->oformat_context;
      res = add_video_stream(stream->ostream, params->codec_id,
                                               params->width_video, params->height_video,
                                               params->bit_stream, params->video_fps);
      if(res == ERROR_RESULT_VALUE){
          debug_error("WARNING output video stream failed to add!");
          free(stream);
          return NULL;
      }
    } else {
      debug_error("WARNING initiator output video stream with path %s not opened!", path_to_save);
      free(stream);
      return NULL;
    }

    res = open_video_stream(stream->ostream, NULL);
    if (res == ERROR_RESULT_VALUE){
        debug_error("WARNING output video stream open failed!");
        free(stream);
        return NULL;
    }
  } else {
    stream->ostream = alloc_output_stream_without_codec(path_to_save);
    if (stream->ostream) {
      debug_msg("Created output media file path: %s!\n", path_to_save);
      formatContext = stream->ostream->oformat_context;
      res = add_video_stream_without_codec(stream->ostream, params->codec_id,
                                               params->width_video, params->height_video,
                                               params->video_fps);
    } else {
      debug_error("WARNING output video stream with path %s not opened!", path_to_save);
      free(stream);
      return NULL;
    }
  }

  if (res == ERROR_RESULT_VALUE) {
    debug_av_perror("add_video_stream", res);
    free_output_stream(stream->ostream);
    free(stream);
    return NULL;
  }

  res = avformat_write_header(formatContext, NULL);
  if (res < 0) {
    debug_error("avformat_write_header failed: error %d!", res);
  }

  av_dump_format(stream->ostream->oformat_context, 0, path_to_save, 1);

  stream->params = *params;

  return stream;
}

const char * get_media_stream_file_path(media_stream_t* stream) {
  if (!stream || !stream->ostream) {
    return NULL;
  }

  AVFormatContext *formatContext = stream->ostream->oformat_context;
  if (!formatContext) {
    return NULL;
  }

  return formatContext->filename;
}

int media_stream_encode_video_frame(media_stream_t* stream, const AVFrame* encoded_frame, AVPacket* out_packet,
                       int* got_packet) {
  if (!stream || !encoded_frame || !out_packet) {
    return ERROR_RESULT_VALUE;
  }

  return encode_ostream_video_frame(stream->ostream, encoded_frame, out_packet, got_packet);
}

int media_stream_write_video_frame(media_stream_t* stream, const uint8_t* data, size_t size) {
  if (!stream || !data) {
    return ERROR_RESULT_VALUE;
  }

#if DUMP_MEDIA
  if (stream->media_dump) {
    fwrite(data, sizeof(uint8_t), size, stream->media_dump);
  }
#endif

  uint32_t mst = currentms();
  if (stream->ts_fpackv_in_stream_msec == 0) {
    stream->ts_fpackv_in_stream_msec = mst;
  }

  AVPacket pkt;
  uint32_t cur_msl = mst - stream->ts_fpackv_in_stream_msec;
#if SAVE_FRAME_POLICY == SAVE_FRAME_ID
  init_video_packet(stream->ostream, (uint8_t*)data, size, stream->video_frame_id, &pkt);
#elif SAVE_FRAME_POLICY == SAVE_LOCAL_TIME
  init_video_packet_ms(stream->ostream, data, size, cur_msl, &pkt);
#else
#error please specify policy to save
#endif
  stream->cur_ts_video_local_msec = cur_msl;
  stream->video_frame_id++;
  write_video_frame(stream->ostream, &pkt);

  return SUCCESS_RESULT_VALUE;
}

void free_video_stream(media_stream_t * stream) {
  if (!stream) {
    return;
  }

  if (stream->ostream) {
    AVFormatContext *formatContext = stream->ostream->oformat_context;
    int ret = av_write_trailer(formatContext);
    if (ret < 0) {
      debug_av_perror("av_write_trailer", ret);
    }
    close_output_stream(stream->ostream);
    free_output_stream(stream->ostream);
    stream->ostream = NULL;
  }
#if DUMP_MEDIA
  if (stream->media_dump) {
    fclose(stream->media_dump);
    stream->media_dump = NULL;
  }
#endif

  stream->video_frame_id = 0;
  stream->video_frame_sps_pps_id = 0;
  stream->cur_ts_video_local_msec = 0;
  stream->ts_fpackv_in_stream_msec = 0;
  stream->ts_fpacka_in_stream_msec = 0;
  stream->sample_id = 0;
  stream->audio_pcm_id = 0;

  free(stream);
}
