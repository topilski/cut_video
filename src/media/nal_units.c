// Copyright (c) 2016 Alexandr Topilski. All rights reserved.

#include "media/nal_units.h"

#include <errno.h>
#include <memory.h>
#include <stdlib.h>

#include "media/ffmpeg_utils.h"

#define MEMCPY_VAR(type, hx, var, buff) \
  memcpy(&hx->var, buff + offsetof(type, var), sizeof(hx->var))

// https://devtalk.nvidia.com/default/topic/718718/-howto-h-264-mp4-container/
// http://aviadr1.blogspot.com/2010/05/h264-extradata-partially-explained-for.html

const uint8_t sps_header[] = { 0x00, 0x00, 0x01 };
const uint8_t pps_header[] = { 0x00, 0x00, 0x01 };
const uint8_t idr_header[] = { 0x00, 0x00, 0x01 };
const uint8_t slice_header[] = { 0x00, 0x00, 0x01 };

int find_nal_unit(uint8_t* buf, int size, int* nal_start, int* nal_end, uint8_t* nal_type, int skip_header) {
  // find start
  *nal_start = 0;
  *nal_end = 0;

  int i = 0;
  if (!skip_header) {
      while (   //( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) &&
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0 || buf[i+3] != 0x01)
        ) {
        i++; // skip leading zero
        if (i+4 >= size) { return 0; }  // did not find nal start
      }

      if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) {  // ( next_bits( 24 ) != 0x000001 )
        i++;
      }

      if (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) {
        /* error, should never happen */ return 0;
      }
      i+= 3;
  }
  *nal_type = buf[i] & 0x1F;
  *nal_start = i;

  while (   //( next_bits( 24 ) != 0x000000 && next_bits( 24 ) != 0x000001 )
    (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0) &&
    (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01)
    ) {
    i++;
    // FIXME the next line fails when reading a nal that ends exactly at the end of the data
    if (i+3 >= size) { *nal_end = size; return -1; }  // did not find nal end, stream ended first
  }

  *nal_end = i;
  return (*nal_end - *nal_start);
}

own_nal_unit_t *alloc_own_nal_unit_from_string(const uint8_t* data, uint32_t* len) {
  *len = 0;
  if (!data) {
    debug_perror("alloc_header_from_string", EINVAL);
    return NULL;
  }

  own_nal_unit_t* h = (own_nal_unit_t*)(calloc(1, sizeof(own_nal_unit_t)));
  if (!h) {
    debug_perror("calloc", ENOMEM);
    return NULL;
  }

  MEMCPY_VAR(own_nal_unit_t, h, frametype, data);
  DCHECK(h->frametype == OWN_NAL_UNIT_TYPE);
  MEMCPY_VAR(own_nal_unit_t, h, total_size, data);
  MEMCPY_VAR(own_nal_unit_t, h, parametr_count, data);
  uint32_t off = 1 + sizeof(uint32_t) * 2;  // fix my hardcode
  if (h->parametr_count > 0) {
    h->parametrs = (len_value_t *)(calloc(h->parametr_count, sizeof(len_value_t)));
    if (!h->parametrs) {
      debug_perror("calloc", ENOMEM);
      free(h);
      return NULL;
    }

    int i;
    for (i = 0; i < h->parametr_count; ++i) {
      struct len_value_t* cur = &h->parametrs[i];
      memcpy(&cur->len, data + off, sizeof(cur->len));
      off += sizeof(cur->len);
      memcpy(&cur->value, data + off, cur->len);
      off += cur->len;
    }
  } else {
    debug_msg("Warning not found parameters in nal unit buffer.");
  }

  *len = off;
  return h;
}

void free_own_nal_unit(own_nal_unit_t * nal_unit) {
  if (!nal_unit) {
    debug_perror("free_own_nal_unit", EINVAL);
    return;
  }

  if (nal_unit->parametrs) {
    free(nal_unit->parametrs);
    nal_unit->parametrs = NULL;
  }
  free(nal_unit);
}

uint8_t* create_sps_nal_unit(len_value_t* raw_sps, uint32_t * len) {
  *len = 0;
  if (!raw_sps) {
    debug_perror("create_sps_nal_unit", EINVAL);
    return NULL;
  }

  DCHECK((raw_sps->value[0] & 0x1F) == NAL_UNIT_TYPE_SPS);
  if ((raw_sps->value[0] & 0x1F) != NAL_UNIT_TYPE_SPS) {
    return NULL;
  }

  *len = sizeof(sps_header) + raw_sps->len;
  uint8_t* sps = (uint8_t*)(calloc(*len, sizeof(uint8_t)));
  memcpy(sps, sps_header, sizeof(sps_header));
  memcpy(sps + sizeof(sps_header), raw_sps->value, raw_sps->len);

  return sps;
}

uint8_t* create_pps_nal_unit(len_value_t* raw_pps, uint32_t * len) {
  *len = 0;
  if (!raw_pps) {
    debug_perror("create_pps_nal_unit", EINVAL);
    return NULL;
  }

  DCHECK((raw_pps->value[0] & 0x1F) == NAL_UNIT_TYPE_PPS);
  if ((raw_pps->value[0] & 0x1F) != NAL_UNIT_TYPE_PPS) {
    return NULL;
  }

  *len = sizeof(pps_header) + raw_pps->len;
  uint8_t* pps = (uint8_t*)(calloc(*len, sizeof(uint8_t)));
  memcpy(pps, pps_header, sizeof(pps_header));
  memcpy(pps + sizeof(pps_header), raw_pps->value, raw_pps->len);

  return pps;
}

header_enc_frame_t * alloc_header_enc_frame_from_string(const uint8_t* data, uint32_t * olen) {
  *olen = 0;
  if (!data) {
    debug_perror("alloc_header_enc_frame_from_string", EINVAL);
    return NULL;
  }

  header_enc_frame_t * h = (header_enc_frame_t*)(calloc(1, sizeof(header_enc_frame_t)));
  MEMCPY_VAR(header_enc_frame_t, h, frametype, data);
  DCHECK(h->frametype == OWN_FRAME_TYPE);
  MEMCPY_VAR(header_enc_frame_t, h, t1, data);
  MEMCPY_VAR(header_enc_frame_t, h, decode_ts, data);
  MEMCPY_VAR(header_enc_frame_t, h, duration, data);
  uint32_t off = sizeof(h->frametype) + sizeof(cmtype_t) * 3;

  memcpy(&h->json_attach.len, data + off, sizeof(h->json_attach.len));
  off += sizeof(h->json_attach.len);
  h->json_attach.value = (uint8_t*)data + off;
  off += h->json_attach.len;

  memcpy(&h->frame_data.len, data + off, sizeof(h->frame_data.len));
  off += sizeof(h->frame_data.len);
  h->frame_data.data = (uint8_t*)data + off;
  off += h->frame_data.len;

  *olen = off;

  return h;
}

void free_header_enc_frame(header_enc_frame_t * hencfr) {
  if (!hencfr) {
    debug_perror("free_header_enc_frame", EINVAL);
    return;
  }

  free(hencfr);
}

uint8_t* create_non_idr_nal_unit(frame_data_t* raw_slice, uint32_t * len) {
  *len = 0;
  if (!raw_slice) {
    debug_perror("create_non_idr_nal_unit", EINVAL);
    return NULL;
  }

  uint8_t* nonidr = raw_slice->data + 4;
  uint32_t nonidr_len = raw_slice->len - 4;
  DCHECK((nonidr[0] & 0x1F) == NAL_UNIT_TYPE_CODED_SLICE_NON_IDR);

  // DCHECK((raw_idr->data[0] & 0x1F) == NAL_UNIT_TYPE_CODED_SLICE_IDR);
  *len = sizeof(slice_header) + nonidr_len;
  uint8_t* idr = (uint8_t*)(calloc(*len, sizeof(uint8_t)));
  memcpy(idr, slice_header, sizeof(slice_header));
  memcpy(idr + sizeof(slice_header), nonidr, nonidr_len);
  // idr[*len - 1] = 0x80;
  return idr;
}

int is_key_frame(uint8_t* raw_idr, int32_t len) {
  if (!raw_idr) {
    debug_perror("is_key_frame", EINVAL);
    return 0;
  }

  return (raw_idr[4] & 0x1F) == NAL_UNIT_TYPE_CODED_SLICE_IDR;
}


uint8_t* create_sps_pps_key_frame(own_nal_unit_t * nal_u, uint8_t* raw_idr, int32_t raw_idr_len,
                                  uint32_t* olen) {
  *olen = 0;
  if (!nal_u || !raw_idr) {
    debug_perror("create_non_idr_nal_unit", EINVAL);
    return NULL;
  }

  uint32_t sps_len = nal_u->parametrs[0].len;
  uint8_t* sps = nal_u->parametrs[0].value;
  DCHECK((sps[0] & 0x1F) == NAL_UNIT_TYPE_SPS);
  uint32_t pps_len = nal_u->parametrs[1].len;
  uint8_t* pps = nal_u->parametrs[1].value;
  DCHECK((pps[0] & 0x1F) == NAL_UNIT_TYPE_PPS);

  uint8_t* ridr = raw_idr + 4;
  uint32_t ridr_len = raw_idr_len - 4;

  int nal_type = ridr[0] & 0x1F;

  if (nal_type != NAL_UNIT_TYPE_CODED_SLICE_IDR && nal_type != NAL_UNIT_TYPE_CODED_SLICE_NON_IDR) {
    debug_warning("WARNING UNKNOWN FRAME_TYPE: %d\n", nal_type);
    return NULL;
  }

  *olen = sizeof(sps_header) + sps_len + sizeof(pps_header) + pps_len +
      sizeof(idr_header) + ridr_len;

  uint8_t* key_frame = (uint8_t*)(calloc(*olen, sizeof(uint8_t)));

  uint32_t offset = 0;
  memcpy(key_frame + offset, sps_header, sizeof(sps_header));
  offset += sizeof(sps_header);
  memcpy(key_frame + offset, sps, sps_len);
  offset += sps_len;

  memcpy(key_frame + offset, pps_header, sizeof(pps_header));
  offset += sizeof(pps_header);
  memcpy(key_frame + offset, pps, pps_len);
  offset += pps_len;

  memcpy(key_frame + offset, idr_header, sizeof(idr_header));
  offset += sizeof(idr_header);
  memcpy(key_frame + offset, ridr, ridr_len);
  offset += ridr_len;

  DCHECK(offset == *olen);
  return key_frame;
}
