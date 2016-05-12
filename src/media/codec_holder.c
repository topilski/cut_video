// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#include "codec_holder.h"

#include "macros.h"
#include "ffmpeg_utils.h"

#define STREAM_FRAME_RATE2 90000
//AV_PIX_FMT_GRAY8
//AV_PIX_FMT_RGBA
//AV_PIX_FMT_RGB24
//AV_PIX_FMT_YUV420P
#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P /* default pix_fmt */

static void prepare_audio_ctx(AVCodecContext* ctx, AVCodec* codec, int sampleRate,
                       int channels, int audioBitrate) {
  if (!ctx || !codec) {
    return;
  }

  if (codec->type == AVMEDIA_TYPE_AUDIO) {
    ctx->bit_rate = audioBitrate;
    ctx->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    ctx->sample_rate = sampleRate;
    ctx->channels = channels;
    ctx->channel_layout = av_get_default_channel_layout(channels);
    // ctx->time_base = (AVRational) {1, sampleRate};
    ctx->time_base.num  = 1;
    ctx->time_base.den  = sampleRate;
    ctx->codec_type = codec->type;
    ctx->frame_size = sampleRate;
  }
}

static void prepare_video_ctx(AVCodecContext* ctx, AVCodec* codec,
                       int width, int height) {
  if (!ctx || !codec) {
    return;
  }

  if (codec->type == AVMEDIA_TYPE_VIDEO) {
    ctx->codec_id = codec->id;
    ctx->codec_type = AVMEDIA_TYPE_VIDEO;

    ctx->width = width;
    ctx->height = height;
    // ctx->time_base = (AVRational) {1, STREAM_FRAME_RATE};
    ctx->pix_fmt = codec->pix_fmts ? codec->pix_fmts[0] : STREAM_PIX_FMT;

    if (codec->capabilities & CODEC_CAP_TRUNCATED) {
      ctx->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */
    }
  }
}

static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt) {
  const enum AVSampleFormat *p = codec->sample_fmts;

  while (*p != AV_SAMPLE_FMT_NONE) {
    if (*p == sample_fmt) {
      return 1;
    }
    p++;
  }

  return 0;
}

static void prepare_video_decoder(decoder_t* codec_holder, int width, int height) {
  if (!codec_holder) {
    return;
  }

  AVCodecContext* ctx = codec_holder->context;
  AVCodec* codec = codec_holder->codec;

  prepare_video_ctx(ctx, codec, width, height);

  /*
    if (ctx->codec_id == AV_CODEC_ID_H264) {
      ctx->thread_count = 5;
      ctx->thread_type = FF_THREAD_SLICE | FF_THREAD_FRAME;
    }
  */
}

static void prepare_video_encoder(encoder_t* codec_holder, int width, int height, AVRational time_base) {
  if (!codec_holder) {
    return;
  }

  AVCodecContext* ctx = codec_holder->context;
  AVCodec* codec = codec_holder->codec;

  prepare_video_ctx(ctx, codec, width, height);

  ctx->gop_size = 12; /* emit one intra frame every twelve frames at most */
  ctx->time_base = time_base;

  if (ctx->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
    /* just for testing, we also add B frames */
    ctx->max_b_frames = 2;
  }
  if (ctx->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
    /* Needed to avoid using macroblocks in which some coeffs overflow.
     * This does not happen with normal video, it just happens here as
     * the motion of the chroma plane does not match the luma plane. */
     ctx->mb_decision = 2;
  }
}

static void prepare_audio_decoder(decoder_t* codec_holder, int sampleRate,
                           int channels, int audioBitrate) {
  if (!codec_holder) {
    return;
  }

  AVCodecContext* ctx = codec_holder->context;
  AVCodec* codec = codec_holder->codec;

  prepare_audio_ctx(ctx, codec, sampleRate, channels, audioBitrate);
}

static void prepare_audio_encoder(encoder_t* codec_holder, int sampleRate,
                           int channels, int audioBitrate) {
  if (!codec_holder) {
    return;
  }

  AVCodecContext* ctx = codec_holder->context;
  AVCodec* codec = codec_holder->codec;

  prepare_audio_ctx(ctx, codec, sampleRate, channels, audioBitrate);
}

static void prepare_audio_output_stream(output_stream_t* ostream, AVCodec* codec, int sampleRate,
                                 int channels, int audioBitrate) {
  if (!ostream || !codec) {
    return;
  }

  AVCodecContext* ctx = ostream->audio_stream->codec;
  AVRational sr = { 1, sampleRate };
  ostream->audio_stream->time_base = sr;

  prepare_audio_ctx(ctx, codec, sampleRate, channels, audioBitrate);

  /* Some formats want stream headers to be separate. */
  if (ostream->oformat_context->flags & AVFMT_GLOBALHEADER) {
    ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }
}

static void prepare_video_output_stream(output_stream_t* ostream, AVCodec* codec, int width, int height,
                                 int bit_rate, int fps) {
  if (!ostream || !codec) {
    return;
  }

  AVStream* stream = ostream->video_stream;
  AVCodecContext* ctx = stream->codec;

  stream->id = ostream->oformat_context->nb_streams - 1;


  /* timebase: This is the fundamental unit of time (in seconds) in terms
  * of which frame timestamps are represented. For fixed-fps content,
  * timebase should be 1/framerate and timestamp increments should be
  * identical to 1. */
  AVRational tb = { 1, fps };
  AVRational tb2 = { 1, STREAM_FRAME_RATE2 };
  stream->time_base = tb2;

  encoder_t enc;
  enc.codec = codec;
  enc.context = ctx;
  prepare_video_encoder(&enc, width, height, tb);

  /* Some formats want stream headers to be separate. */
  if (ostream->oformat_context->flags & AVFMT_GLOBALHEADER) {
    ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }
}

// =========== end prepare =============== //

static int write_frame(AVFormatContext* fmt_ctx, AVStream* st, AVPacket* pkt) {
  pkt->stream_index = st->index;
  int ret = av_write_frame(fmt_ctx, pkt);
  if (ret < 0) {
    debug_av_perror("av_write_frame", ret);
    return ret;
  }

  return ret;
}

static int init_output_frame(AVFrame **frame, AVCodecContext *output_codec_context, int frame_size) {
  int error;

  /** Create a new frame to store the audio samples. */
  if (!(*frame = av_frame_alloc())) {
    fprintf(stderr, "Could not allocate output frame\n");
    return AVERROR_EXIT;
  }

  /**
   * Set the frame's parameters, especially its size and format.
   * av_frame_get_buffer needs this to allocate memory for the
   * audio samples of the frame.
   * Default channel layouts based on the number of channels
   * are assumed for simplicity.
   */
  (*frame)->nb_samples     = frame_size;
  (*frame)->channel_layout = output_codec_context->channel_layout;
  (*frame)->format         = output_codec_context->sample_fmt;
  (*frame)->sample_rate    = output_codec_context->sample_rate;

  /**
   * Allocate the samples of the created frame. This call will make
   * sure that the audio frame can hold as many samples as specified.
   */
  if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
    debug_error("Could allocate output frame samples (error '%d')\n", error);
    av_frame_free(frame);
    return error;
  }

  return 0;
}

decoder_t* alloc_video_decoder_by_codecid(enum AVCodecID codec_id) {
  decoder_t* codec_holder = (decoder_t*)calloc(1, sizeof(decoder_t));
  if (!codec_holder) {
    debug_perror("calloc", ENOMEM);
    return NULL;
  }

  codec_holder->codec = avcodec_find_decoder(codec_id);
  if (!codec_holder->codec) {
    free(codec_holder);
    return NULL;
  }

  codec_holder->context = avcodec_alloc_context3(codec_holder->codec);
  if (!codec_holder->context) {
    free(codec_holder);
    return NULL;
  }

  int nres = avcodec_open2(codec_holder->context, codec_holder->codec, 0);
  if (nres < 0) {
    debug_av_perror("avcodec_open2", nres);
    avcodec_free_context(&codec_holder->context);
    return NULL;
  }

  return codec_holder;
}

decoder_t* alloc_audio_decoder_by_codecid(enum AVCodecID codec_id) {
  decoder_t* codec_holder = (decoder_t*)malloc(sizeof(decoder_t));
  if (!codec_holder) {
    return NULL;
  }

  codec_holder->codec = avcodec_find_decoder(codec_id);
  if (!codec_holder->codec) {
    free(codec_holder);
    return NULL;
  }

  codec_holder->context = avcodec_alloc_context3(codec_holder->codec);
  if (!codec_holder->context) {
    free(codec_holder);
    return NULL;
  }

  int nres = avcodec_open2(codec_holder->context, codec_holder->codec, 0);
  if (nres < 0) {
    debug_av_perror("avcodec_open2", nres);
    avcodec_free_context(&codec_holder->context);
    return NULL;
  }

  return codec_holder;
}

decoder_t* alloc_video_decoder(enum AVCodecID codec_id, int width, int height, int bit_rate) {
  decoder_t* codec_holder = (decoder_t*)calloc(1, sizeof(decoder_t));
  if (!codec_holder) {
    debug_perror("calloc", ENOMEM);
    return NULL;
  }

  codec_holder->codec = avcodec_find_decoder(codec_id);
  if (!codec_holder->codec) {
    free(codec_holder);
    return NULL;
  }

  codec_holder->context = avcodec_alloc_context3(codec_holder->codec);
  if (!codec_holder->context) {
    free(codec_holder);
    return NULL;
  }

  prepare_video_decoder(codec_holder, width, height);

  int nres = avcodec_open2(codec_holder->context, codec_holder->codec, 0);
  if (nres < 0) {
    debug_av_perror("avcodec_open2", nres);
    avcodec_free_context(&codec_holder->context);
    return NULL;
  }

  return codec_holder;
}

decoder_t* alloc_audio_decoder(enum AVCodecID codec_id, int sample_rate,
                               int channels, int audio_bitrate) {
  decoder_t* codec_holder = (decoder_t*)malloc(sizeof(decoder_t));
  if (!codec_holder) {
    return NULL;
  }

  codec_holder->codec = avcodec_find_decoder(codec_id);
  if (!codec_holder->codec) {
    free(codec_holder);
    return NULL;
  }

  codec_holder->context = avcodec_alloc_context3(codec_holder->codec);
  if (!codec_holder->context) {
    free(codec_holder);
    return NULL;
  }

  prepare_audio_decoder(codec_holder, sample_rate, channels, audio_bitrate);

  int nres = avcodec_open2(codec_holder->context, codec_holder->codec, 0);
  if (nres < 0) {
    debug_av_perror("avcodec_open2", nres);
    avcodec_free_context(&codec_holder->context);
    return NULL;
  }

  return codec_holder;
}

decoder_t* alloc_decoder_by_ctx(const AVCodecContext *ctx) {
  if (!ctx) {
    debug_perror("alloc_decoder_by_ctx", EINVAL);
    return NULL;
  }

  decoder_t* codec_holder = (decoder_t*)malloc(sizeof(decoder_t));
  if (!codec_holder) {
    return NULL;
  }

  codec_holder->codec = avcodec_find_decoder(ctx->codec_id);
  if (!codec_holder->codec) {
    free(codec_holder);
    return NULL;
  }

  codec_holder->context = avcodec_alloc_context3(NULL);
  if (!codec_holder->context) {
    free(codec_holder);
    return NULL;
  }

  int nres = avcodec_copy_context(codec_holder->context, ctx);
  if (nres < 0) {
    debug_av_perror("avcodec_copy_context", nres);
    avcodec_free_context(&codec_holder->context);
    return NULL;
  }

  nres = avcodec_open2(codec_holder->context, codec_holder->codec, 0);
  if (nres < 0) {
    debug_av_perror("avcodec_open2", nres);
    avcodec_free_context(&codec_holder->context);
    return NULL;
  }

  return codec_holder;
}

void free_decoder(decoder_t* holder) {
  if (!holder) {
    debug_perror("free_decoder", EINVAL);
    return;
  }

  if (holder->context) {
    avcodec_close(holder->context);
    avcodec_free_context(&holder->context);
    holder->context = NULL;
  }
  free(holder);
}

int decoder_decode_video(decoder_t* holder, AVFrame *picture, const AVPacket *avpkt) {
  if (!holder || !avpkt || !holder->context) {
    debug_perror("decoder_decode_video", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  if (holder->context->codec_type != AVMEDIA_TYPE_VIDEO) {
    debug_perror("decoder_decode_audio invalid codec type", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  int got_picture = 0;
  int len = avcodec_decode_video2(holder->context, picture, &got_picture, avpkt);
  if (len < 0) {
    debug_av_perror("avcodec_decode_video2", len);
    return len;
  }

  if (got_picture) {
    return len;
  } else {
    return ERROR_RESULT_VALUE;
  }
}

int decoder_decode_audio(decoder_t* holder, AVFrame *frame, const AVPacket *avpkt) {
  if (!holder || !avpkt || !holder->context) {
    debug_perror("decoder_decode_audio", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  if (holder->context->codec_type != AVMEDIA_TYPE_AUDIO) {
    debug_perror("decoder_decode_audio invalid codec type", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  int got_sample = 0;
  int len = avcodec_decode_audio4(holder->context, frame, &got_sample, avpkt);
  if (len < 0) {
    debug_av_perror("avcodec_decode_audio4", len);
    return len;
  }

  if (got_sample) {
    return len;
  } else {
    return ERROR_RESULT_VALUE;
  }
}

encoder_t* alloc_video_encoder_by_codecid(enum AVCodecID codec_id, int width, int height,
                                          int fps, AVDictionary * opt) {
  encoder_t* codec_holder = (encoder_t*)malloc(sizeof(encoder_t));
  if (!codec_holder) {
    return NULL;
  }

  codec_holder->codec = avcodec_find_encoder(codec_id);
  if (!codec_holder->codec) {
    free(codec_holder);
    return NULL;
  }

  codec_holder->context = avcodec_alloc_context3(codec_holder->codec);
  if (!codec_holder->context) {
    free(codec_holder);
    return NULL;
  }

  AVRational tb = { 1, fps };
  prepare_video_encoder(codec_holder, width, height, tb);

  int nres = avcodec_open2(codec_holder->context, codec_holder->codec, &opt);
  if (nres < 0) {
    debug_av_perror("avcodec_open2", nres);
    avcodec_free_context(&codec_holder->context);
    free(codec_holder);
    return NULL;
  }

  return codec_holder;
}

encoder_t* alloc_audio_encoder_by_codecid(enum AVCodecID codec_id, int sample_rate, int channels,
                                          int audio_bitrate, AVDictionary * opt) {
  encoder_t* codec_holder = (encoder_t*)malloc(sizeof(encoder_t));
  if (!codec_holder) {
    return NULL;
  }

  codec_holder->codec = avcodec_find_encoder(codec_id);
  if (!codec_holder->codec) {
    free(codec_holder);
    return NULL;
  }

  codec_holder->context = avcodec_alloc_context3(codec_holder->codec);
  if (!codec_holder->context) {
    free(codec_holder);
    return NULL;
  }

  prepare_audio_encoder(codec_holder, sample_rate, channels, audio_bitrate);

  int nres = avcodec_open2(codec_holder->context, codec_holder->codec, &opt);
  if (nres < 0) {
    debug_av_perror("avcodec_open2", nres);
    avcodec_free_context(&codec_holder->context);
    free(codec_holder);
    return NULL;
  }

  return codec_holder;
}

void free_encoder(encoder_t *holder) {
  if (!holder) {
    debug_perror("free_coder", EINVAL);
    return;
  }

  if (holder->context) {
    avcodec_close(holder->context);
    avcodec_free_context(&holder->context);
    holder->context = NULL;
  }
  free(holder);
}

int encoder_encode_audio(encoder_t *holder, AVPacket* pkt, const AVFrame *frame, int* got_packet) {
  if (!holder || !frame || !holder->context) {
    debug_perror("codec_holder_encode_audio", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  return encode_audio_frame(holder->context, frame, pkt, got_packet);
}

AVOutputFormat* find_avoutformat_by_codecids(enum AVCodecID vcodec_id, enum AVCodecID acodec_id) {
  if (vcodec_id == AV_CODEC_ID_NONE && acodec_id == AV_CODEC_ID_NONE) {
    return NULL;
  }

  AVOutputFormat * oformat = av_oformat_next(NULL);
  while (oformat != NULL) {
    if (((vcodec_id != AV_CODEC_ID_NONE && oformat->video_codec == vcodec_id) ||
         (vcodec_id == AV_CODEC_ID_NONE))
      && ((acodec_id != AV_CODEC_ID_NONE && oformat->audio_codec == acodec_id) ||
          (acodec_id == AV_CODEC_ID_NONE)) ) {
        return oformat;
    }
    oformat = av_oformat_next(oformat);
  }

  return NULL;
}

output_stream_t* alloc_output_stream(AVOutputFormat* oformat, const char* file_path,
                                     const char* format_name) {
  if (!file_path && !oformat && !format_name) {
    debug_perror("alloc_output_stream", EINVAL);
    return NULL;
  }

  output_stream_t* ostream = (output_stream_t*)calloc(1, sizeof(output_stream_t));
  if (!ostream) {
    debug_perror("calloc", ENOMEM);
    return NULL;
  }

  int nres = avformat_alloc_output_context2(&ostream->oformat_context, oformat,
                                            format_name, file_path);
  if (!ostream->oformat_context) {
    debug_av_perror("avformat_alloc_output_context2", nres);
    free(ostream);
    return NULL;
  }

  AVFormatContext* oformat_context = ostream->oformat_context;

  AVOutputFormat *fmt = oformat_context->oformat;
  /* open the output file, if needed */
  if (!(fmt->flags & AVFMT_NOFILE)) {
    nres = avio_open(&oformat_context->pb, file_path, AVIO_FLAG_WRITE);
    if (nres < 0) {
      debug_av_perror("avio_open", nres);
      free(ostream);
      return NULL;
    }
  }

  return ostream;
}

output_stream_t* alloc_output_stream_without_codec(const char *file_path) {
  if (!file_path) {
    debug_perror("alloc_output_stream_without_codec", EINVAL);
    return NULL;
  }

  AVOutputFormat* fmt = av_guess_format(NULL, file_path, NULL);
  if (!fmt) {
    debug_error("Could not create outputformat with path: %s\n", file_path);
    return NULL;
  }

  output_stream_t* ostream = (output_stream_t*)calloc(1, sizeof(output_stream_t));
  if (!ostream) {
    debug_perror("calloc", ENOMEM);
    return NULL;
  }

  int nres = 0;

  /*AVFormatContext *formatContext = avformat_alloc_context();
  ostream->oformat_context = formatContext;
  formatContext->oformat = fmt;
  strcpy(formatContext->filename, file_path);*/

  nres = avformat_alloc_output_context2(&ostream->oformat_context, fmt, NULL, NULL);
  if (!ostream->oformat_context) {
    debug_av_perror("avformat_alloc_output_context2", nres);
    free(ostream);
    return NULL;
  }

  AVFormatContext *formatContext = ostream->oformat_context;
  strcpy(formatContext->filename, file_path);

  /* open the output file, if needed */
  if (!(fmt->flags & AVFMT_NOFILE)) {
    nres = avio_open(&ostream->oformat_context->pb, file_path, AVIO_FLAG_WRITE);
    if (nres < 0) {
      debug_av_perror("avio_open", nres);
      free(ostream);
      return NULL;
    }
  }

  return ostream;
}

void free_output_stream(output_stream_t *ostream) {
  if (!ostream) {
    debug_perror("free_coder", EINVAL);
    return;
  }

  if (ostream->auduo_frame_buffer) {
    av_frame_free(&ostream->auduo_frame_buffer);
  }

  AVFormatContext* oformat_context = ostream->oformat_context;

  if (oformat_context) {
    AVOutputFormat *fmt = oformat_context->oformat;
    if (!(fmt->flags & AVFMT_NOFILE)) {
      /* Close the output file. */
      avio_closep(&oformat_context->pb);
    }

    avformat_free_context(ostream->oformat_context);
    ostream->oformat_context = NULL;
  }

  free(ostream);
}

int add_audio_stream(output_stream_t* ostream, enum AVCodecID codec_id, int sample_rate,
                     int channels, int audio_bitrate) {
  if (!ostream) {
    debug_perror("add_audio_stream", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  AVFormatContext* oformat_context = ostream->oformat_context;

  AVCodec* audio_codec = avcodec_find_encoder(codec_id);
  if (!audio_codec) {
    return ERROR_RESULT_VALUE;
  }

  ostream->audio_stream = avformat_new_stream(oformat_context, audio_codec);
  if (!ostream->audio_stream) {
    debug_error("Could not allocate stream\n");
    return ERROR_RESULT_VALUE;
  }

  prepare_audio_output_stream(ostream, audio_codec, sample_rate, channels, audio_bitrate);

  return SUCCESS_RESULT_VALUE;
}

int open_audio_stream(output_stream_t* ostream, AVDictionary* opt) {
  if (!ostream) {
    debug_perror("open_audio_stream", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  AVCodecContext* cc = ostream->audio_stream->codec;

  int ret = avcodec_open2(cc, cc->codec, &opt);
  if (ret < 0) {
    debug_av_perror("avcodec_open2", ret);
    return ERROR_RESULT_VALUE;
  }

  ret = init_output_frame(&ostream->auduo_frame_buffer, cc, cc->frame_size);
  if (ret < 0) {
    debug_av_perror("init_output_frame", ret);
    return ERROR_RESULT_VALUE;
  }

  return SUCCESS_RESULT_VALUE;
}

int add_video_stream(output_stream_t *ostream, enum AVCodecID codec_id, int width, int height,
                     int bit_rate, int fps) {
  if (!ostream) {
    debug_perror("add_audio_stream", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  AVFormatContext* oformat_context = ostream->oformat_context;


  AVCodec* video_codec = avcodec_find_encoder(codec_id);
  if (!video_codec) {
    debug_error("Could not allocate video encoder codec_id: %d\n", codec_id);
    return ERROR_RESULT_VALUE;
  }

  ostream->video_stream = avformat_new_stream(oformat_context, video_codec);
  if (!ostream->video_stream) {
    debug_error("Could not allocate stream\n");
    return ERROR_RESULT_VALUE;
  }

  prepare_video_output_stream(ostream, video_codec, width, height, bit_rate, fps);

  return SUCCESS_RESULT_VALUE;
}

int add_video_stream_without_codec(output_stream_t *ostream, enum AVCodecID codec_id, int width,
                                   int height, int fps) {
  if (!ostream) {
    debug_perror("add_audio_stream", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  AVFormatContext* oformat_context = ostream->oformat_context;

  ostream->video_stream = avformat_new_stream(oformat_context, NULL);
  if (!ostream->video_stream) {
    debug_error("Could not allocate stream\n");
    return ERROR_RESULT_VALUE;
  }

  AVCodecContext * videoContext = ostream->video_stream->codec;
  // avcodec_get_context_defaults3(videoContext, video_codec);

  videoContext->codec_type = AVMEDIA_TYPE_VIDEO;
  videoContext->codec_id = codec_id;
  videoContext->width = width;
  videoContext->height = height;
  videoContext->pix_fmt = STREAM_PIX_FMT;
  videoContext->gop_size = fps;
  videoContext->time_base = (AVRational) {1, fps};
  ostream->video_stream->time_base = (AVRational) {1, STREAM_FRAME_RATE2};

  if (oformat_context->oformat->flags & AVFMT_GLOBALHEADER) {
    videoContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  return SUCCESS_RESULT_VALUE;
}

int open_video_stream(output_stream_t* ostream, AVDictionary *opt_arg) {
  if (!ostream) {
    debug_perror("open_video_stream", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  AVCodecContext *cc = ostream->video_stream->codec;
  /* open the codec */
  int ret = avcodec_open2(cc, cc->codec, &opt_arg);
  if (ret < 0) {
    debug_av_perror("avcodec_open2", ret);
    return ERROR_RESULT_VALUE;
  }

  return SUCCESS_RESULT_VALUE;
}

int encode_audio_frame(AVCodecContext* ctx, const AVFrame* frame,
                       AVPacket* pktout, int* got_packet) {
  *got_packet = -1;
  if (!ctx || !frame) {
    debug_perror("encode_audio_frame", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  if (ctx->codec_type != AVMEDIA_TYPE_AUDIO) {
    debug_perror("encode_audio_frame invalid codec type", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  int len = avcodec_encode_audio2(ctx, pktout, frame, got_packet);
  if (len < 0) {
    debug_av_perror("avcodec_encode_audio2", len);
    return len;
  }

  return len;
}

int encode_ostream_audio_frame(output_stream_t* ostream, AVFrame* frame,
                               AVPacket* pktout, int* got_packet) {
  if (!ostream || !frame) {
    debug_perror("encode_ostream_audio_frame", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  AVCodecContext* cc = ostream->audio_stream->codec;
  return encode_audio_frame(cc, frame, pktout, got_packet);
}

int encode_ostream_audio_buffer(output_stream_t* ostream, const uint8_t *buf, int buf_size,
                                AVPacket* pktout, int* got_packet) {
  if (!ostream || !buf || buf_size <= 0) {
    debug_perror("encode_ostream_audio_frame", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  AVCodecContext* cc = ostream->audio_stream->codec;

  int ret = avcodec_fill_audio_frame(ostream->auduo_frame_buffer, cc->channels, cc->sample_fmt,
                                     buf, buf_size, 0);
  if (ret < 0) {
    debug_av_perror("avcodec_fill_audio_frame", ret);
    return ERROR_RESULT_VALUE;
  }

  return encode_audio_frame(cc, ostream->auduo_frame_buffer, pktout, got_packet);
}

void update_packet_pts(AVRational ctime_base, AVRational stime_base, int64_t frame_id,
                       AVPacket *pkt) {
  if (!pkt) {
    debug_perror("update_packet_pts", EINVAL);
    return;
  }

  pkt->pts = av_rescale_q(frame_id, ctime_base, stime_base);
  pkt->dts = av_rescale_q(frame_id, ctime_base, stime_base);
}

void update_video_packet_pts(output_stream_t* ostream, int64_t frame_id, AVPacket *pkt) {
  if (!ostream || !pkt) {
    debug_perror("update_video_packet_pts", EINVAL);
    return;
  }

  if (ostream->video_stream && ostream->video_stream->codec) {
    update_packet_pts(ostream->video_stream->codec->time_base,
                      ostream->video_stream->time_base, frame_id, pkt);
  }
}

void update_video_packet_pts_ms(output_stream_t* ostream, int64_t msec_ts, AVPacket *pkt) {
  if (!ostream || !pkt) {
    debug_perror("update_video_packet_pts", EINVAL);
    return;
  }

  pkt->pts = msec_ts * STREAM_FRAME_RATE2 / 1000;
  pkt->dts = msec_ts * STREAM_FRAME_RATE2 / 1000;
}

void update_audio_packet_pts(output_stream_t* ostream, int sample_id, AVPacket *pkt) {
  if (!ostream || !pkt) {
    debug_perror("update_audio_packet_pts", EINVAL);
    return;
  }

  pkt->pts = AV_NOPTS_VALUE;
  pkt->dts = AV_NOPTS_VALUE;

  /* DCHECK(ostream->audio_stream);
  if (ostream->audio_stream && ostream->audio_stream->codec) {
    update_packet_pts(ostream->audio_stream->codec->time_base,
                      ostream->audio_stream->time_base, sample_id, pkt);
  } */
}

void update_audio_packet_pts_ms(output_stream_t* ostream, int64_t msec_ts, AVPacket *pkt) {
  if (!ostream || !pkt) {
    debug_perror("update_video_packet_pts", EINVAL);
    return;
  }

  pkt->pts = msec_ts * STREAM_FRAME_RATE2 / 1000;
  pkt->dts = msec_ts * STREAM_FRAME_RATE2 / 1000;
}

void init_video_packet_ms(output_stream_t* ostream, uint8_t *data, int size, int64_t msec_ts,
                          AVPacket *pkt) {
  if (!ostream || !pkt) {
    debug_perror("init_video_packet", EINVAL);
    return;
  }

  av_init_packet(pkt);
  pkt->data = data;
  pkt->size = size;
  update_video_packet_pts_ms(ostream, msec_ts, pkt);
}

void init_video_packet(output_stream_t* ostream, uint8_t* data, int size, int64_t frame_id,
                       AVPacket *pkt) {
  if (!ostream || !pkt) {
    debug_perror("init_video_packet", EINVAL);
    return;
  }

  av_init_packet(pkt);
  pkt->data = data;
  pkt->size = size;
  update_video_packet_pts(ostream, frame_id, pkt);
}

void init_audio_packet_ms(output_stream_t* ostream, uint8_t *data, int size, int64_t msec_ts,
                          AVPacket *pkt) {
  if (!ostream || !pkt) {
      debug_perror("init_video_packet", EINVAL);
      return;
  }

  av_init_packet(pkt);
  pkt->data = data;
  pkt->size = size;
  update_audio_packet_pts(ostream, msec_ts, pkt);
}

void init_audio_packet(output_stream_t* ostream, uint8_t *data, int size, int sample_id,
                       AVPacket *pkt) {
  if (!ostream || !pkt) {
    debug_perror("init_audio_packet", EINVAL);
    return;
  }

  av_init_packet(pkt);
  pkt->data = data;
  pkt->size = size;
  update_audio_packet_pts(ostream, sample_id, pkt);
}

int write_audio_frame(output_stream_t* ostream, AVPacket *pkt) {
  if (!ostream) {
    debug_perror("write_audio_frame", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  return write_frame(ostream->oformat_context, ostream->audio_stream, pkt);
}

int encode_video_frame(AVCodecContext* ctx, const AVFrame* frame,
                       AVPacket* pktout, int* got_packet) {
  *got_packet = -1;
  if (!ctx || !frame || !pktout) {
    debug_perror("encode_audio_frame", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  if (ctx->codec_type != AVMEDIA_TYPE_VIDEO) {
    debug_perror("encode_video_frame invalid codec type", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  int len = avcodec_encode_video2(ctx, pktout, frame, got_packet);
  if (len < 0) {
    debug_av_perror("avcodec_encode_video2", len);
    return ERROR_RESULT_VALUE;
  }

  return SUCCESS_RESULT_VALUE;
}

int encode_ostream_video_frame(output_stream_t* ostream, const AVFrame* frame,
                               AVPacket* pktout, int* got_packet) {
  if (!ostream || !frame || !pktout) {
    debug_perror("encode_ostream_audio_frame", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  AVCodecContext* cc = ostream->video_stream->codec;
  return encode_video_frame(cc, frame, pktout, got_packet);
}

int write_video_frame(output_stream_t *ostream, AVPacket *pkt) {
  if (!ostream) {
    debug_perror("write_audio_frame", EINVAL);
    return ERROR_RESULT_VALUE;
  }

  return write_frame(ostream->oformat_context, ostream->video_stream, pkt);
}

void close_output_stream(output_stream_t* ostream) {
  if (!ostream) {
    debug_perror("close_output_stream", EINVAL);
    return;
  }

  if (ostream->video_stream) {
    avcodec_close(ostream->video_stream->codec);
  }

  if (ostream->audio_stream) {
    avcodec_close(ostream->audio_stream->codec);
  }
}
