// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#pragma once

#include "macros.h"

#define NAL_TYPE_HEADER_SIZE 3

// Table 7-1 NAL unit type codes
#define NAL_UNIT_TYPE_UNSPECIFIED                    0    // Unspecified
#define NAL_UNIT_TYPE_CODED_SLICE_NON_IDR            1    // Coded slice of a non-IDR picture
#define NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_A   2    // Coded slice data partition A
#define NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_B   3    // Coded slice data partition B
#define NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_C   4    // Coded slice data partition C
#define NAL_UNIT_TYPE_CODED_SLICE_IDR                5    // Coded slice of an IDR picture
#define NAL_UNIT_TYPE_SEI                            6    // Supplemental enhancement information (SEI)
#define NAL_UNIT_TYPE_SPS                            7    // Sequence parameter set
#define NAL_UNIT_TYPE_PPS                            8    // Picture parameter set
#define NAL_UNIT_TYPE_AUD                            9    // Access unit delimiter
#define NAL_UNIT_TYPE_END_OF_SEQUENCE               10    // End of sequence
#define NAL_UNIT_TYPE_END_OF_STREAM                 11    // End of stream
#define NAL_UNIT_TYPE_FILLER                        12    // Filler data
#define NAL_UNIT_TYPE_SPS_EXT                       13    // Sequence parameter set extension
                                             // 14..18    // Reserved
#define NAL_UNIT_TYPE_CODED_SLICE_AUX               19    // Coded slice of an auxiliary coded picture without partitioning
                                             // 20..23    // Reserved
                                             // 24..31    // Unspecified

#define OWN_NAL_UNIT_TYPE 0
#define OWN_FRAME_TYPE 1

#pragma pack(push, 1)

typedef struct len_value_t {
  uint32_t len;
  uint8_t value[16];
} len_value_t;

typedef struct own_nal_unit_t {
  uint8_t frametype;
  uint32_t total_size;
  size_t parametr_count;
  len_value_t *parametrs;
} own_nal_unit_t;

/*
    - (B5) 1 byte, 1, for video data
    - 24 bytes, CMTime {value: int64, timescale: int32, flags: int32, epoch: int64} - presentation timestamp
    - 24 bytes, CMTime - decode timestamp
    - 24 bytes, CMTime - duration (now 1/15 sec)
    - 4 bytes, attachments length
    - <attachments> in JSON format
    - 4 bytes, buffer length
    - <buffer> - video data
*/

typedef struct cmtype_t {
  int64_t value;
  int32_t timescale;
  int32_t flags;
  int64_t epoch;
} cmtype_t;

typedef struct json_attach_t {
  uint32_t len;
  uint8_t* value;
} json_attach_t;

typedef struct frame_data_t {
  uint32_t len;
  uint8_t* data;
} frame_data_t;

typedef struct header_enc_frame_t {
  uint8_t frametype;
  cmtype_t t1;
  cmtype_t decode_ts;
  cmtype_t duration;
  json_attach_t json_attach;
  frame_data_t frame_data;
} header_enc_frame_t;

#pragma pack(pop)

int find_nal_unit(uint8_t* buf, int size, int* nal_start, int* nal_end, uint8_t* nal_type, int skip_header);

own_nal_unit_t * alloc_own_nal_unit_from_string(const uint8_t* data, uint32_t * len);
void free_own_nal_unit(own_nal_unit_t * nal_unit);

uint8_t* create_sps_nal_unit(len_value_t* raw_sps, uint32_t * len);
uint8_t* create_pps_nal_unit(len_value_t* raw_pps, uint32_t * len);

header_enc_frame_t * alloc_header_enc_frame_from_string(const uint8_t *data, uint32_t * olen);
void free_header_enc_frame(header_enc_frame_t * hencfr);

uint8_t* create_non_idr_nal_unit(frame_data_t* raw_slice, uint32_t * len);
int is_key_frame(uint8_t* raw_idr, int32_t len);
uint8_t* create_sps_pps_key_frame(own_nal_unit_t * nal_u, uint8_t* raw_idr, int32_t len,
                                  uint32_t* olen);
