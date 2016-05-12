// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

typedef struct decoder_t {
  AVCodec* codec;
  AVCodecContext* context;
} decoder_t;

decoder_t* alloc_video_decoder_by_codecid(enum AVCodecID codec_id);  // avcodec_find_decoder
decoder_t* alloc_audio_decoder_by_codecid(enum AVCodecID codec_id);  // avcodec_find_decoder

decoder_t* alloc_video_decoder(enum AVCodecID codec_id, int width,
                               int height, int bit_rate);  // avcodec_find_decoder
decoder_t* alloc_audio_decoder(enum AVCodecID codec_id, int sample_rate,
                               int channels, int audio_bitrate);  // avcodec_find_decoder

decoder_t* alloc_decoder_by_ctx(const AVCodecContext *ctx);  // avcodec_find_decoder
void free_decoder(decoder_t *holder);

int decoder_decode_video(decoder_t *holder, AVFrame *picture, const AVPacket *avpkt);
int decoder_decode_audio(decoder_t *holder, AVFrame *frame, const AVPacket *avpkt);

typedef struct encoder_t {
  AVCodec* codec;
  AVCodecContext* context;
} encoder_t;

encoder_t* alloc_video_encoder_by_codecid(enum AVCodecID codec_id, int width, int height,
                                          int fps, AVDictionary * opt);
encoder_t* alloc_audio_encoder_by_codecid(enum AVCodecID codec_id, int sample_rate,
                                          int channels, int audio_bitrate,
                                          AVDictionary *opt);  // avcodec_find_encoder
void free_encoder(encoder_t *holder);

int encoder_encode_audio(encoder_t *holder, AVPacket* pkt, const AVFrame *frame, int *got_packet);


typedef struct output_stream_t {
  AVFormatContext* oformat_context;
  AVStream* audio_stream;
  AVStream* video_stream;

  AVFrame* auduo_frame_buffer;
} output_stream_t;

AVOutputFormat* find_avoutformat_by_codecids(enum AVCodecID vcodec_id, enum AVCodecID acodec_id);

output_stream_t* alloc_output_stream(AVOutputFormat *oformat,
                                     const char *file_path, const char *format_name);
output_stream_t* alloc_output_stream_without_codec(const char *file_path);
void free_output_stream(output_stream_t *ostream);

int add_audio_stream(output_stream_t* ostream, enum AVCodecID codec_id, int sample_rate,
                     int channels, int audio_bitrate);
int open_audio_stream(output_stream_t* ostream, AVDictionary *opt);

int add_video_stream(output_stream_t *ostream, enum AVCodecID codec_id, int width, int height,
                     int bit_rate, int fps);
int add_video_stream_without_codec(output_stream_t *ostream, enum AVCodecID codec_id,
                                   int width, int height,
                                   int fps);  // open not needed, encode impossible
int open_video_stream(output_stream_t* ostream, AVDictionary *opt_arg);

int encode_audio_frame(AVCodecContext* ctx, const AVFrame* frame,
                       AVPacket* pktout, int* got_packet);
int encode_ostream_audio_frame(output_stream_t* ostream, AVFrame* frame,
                               AVPacket* pktout, int* got_packet);
int encode_ostream_audio_buffer(output_stream_t* ostream, const uint8_t *buf, int buf_size,
                                AVPacket* pktout, int *got_packet);

int encode_video_frame(AVCodecContext* ctx, const AVFrame* frame,
                       AVPacket* pktout, int* got_packet);
int encode_ostream_video_frame(output_stream_t* ostream, const AVFrame* frame,
                               AVPacket* pktout, int* got_packet);

void update_packet_pts(AVRational ctime_base, AVRational stime_base,
                       int64_t frame_id, AVPacket *pkt);
void update_video_packet_pts(output_stream_t* ostream, int64_t frame_id, AVPacket *pkt);
void update_video_packet_pts_ms(output_stream_t* ostream, int64_t msec_ts, AVPacket *pkt);

void update_audio_packet_pts(output_stream_t* ostream, int sample_id, AVPacket *pkt);
void update_audio_packet_pts_ms(output_stream_t* ostream, int64_t msec_ts, AVPacket *pkt);

void init_video_packet_ms(output_stream_t* ostream, uint8_t *data, int size,
                          int64_t msec_ts, AVPacket *pkt);
void init_video_packet(output_stream_t* ostream, uint8_t* data, int size,
                       int64_t frame_id, AVPacket *pkt);

void init_video_packet_ms(output_stream_t* ostream, uint8_t *data, int size,
                          int64_t msec_ts, AVPacket *pkt);
void init_audio_packet(output_stream_t* ostream, uint8_t *data, int size,
                       int sample_id, AVPacket *pkt);

int write_audio_frame(output_stream_t* ostream, AVPacket *pkt);
int write_video_frame(output_stream_t *ost, AVPacket *pkt);

void close_output_stream(output_stream_t* ostream);
