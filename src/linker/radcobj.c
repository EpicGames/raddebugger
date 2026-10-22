// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#include "radcobj.h"
#include <string.h>

int
radcobj_match_magic(const void *data, uint64_t size)
{
  return data != 0 && size >= sizeof(uint64_t) && memcmp(data, RADCOBJ_MAGIC, sizeof(uint64_t)) == 0;
}

static const char *
radcobj_validate_header(const void *data, uint64_t size, RADCOBJ_Header *header_out)
{
  if (header_out) { memset(header_out, 0, sizeof(*header_out)); }

  if (data == NULL || size < sizeof(RADCOBJ_Header) || size > SIZE_MAX) {
    return "header is truncated or container is too large";
  }

  if ( ! radcobj_match_magic(data, size)) {
    return "unrecognized RADCOBJ signature";
  }

  RADCOBJ_Header header;
  memcpy(&header, data, sizeof(header));

  if (header.version != RADCOBJ_VERSION) {
    return "unsupported RADCOBJ version";
  }
  if (header.compressor.sdk_type != RADCOBJ_SDK_OODLE2) {
    return "unsupported compression SDK";
  }

  uint64_t raw_size = header.compressor.raw_size;
  uint32_t segment_size = header.compressor.segment_size;
  if (raw_size > SIZE_MAX) {
    return "raw size exceeds the address-space limit";
  }
  if (segment_size < RADCOBJ_MIN_SEGMENT_SIZE || (segment_size & (segment_size - 1)) != 0) {
    return "invalid segment size";
  }

  uint64_t count          = header.compressor.segment_count;
  uint64_t expected_count = raw_size / segment_size + (raw_size % segment_size != 0);
  if (count != expected_count || count > (size - sizeof(header)) / sizeof(RADCOBJ_Segment)) {
    return "invalid segment count or truncated segment directory";
  }

  for (unsigned i = 0; i < RADCOBJ_DirectoryKind_Count; i += 1) {
    RADCOBJ_Directory directory = header.dirs[i];
    if (directory.offset > size || directory.size > size - directory.offset) {
      return "auxiliary directory is out of bounds";
    }
  }

  if (header_out) { *header_out = header; }
  return 0;
}

static const char *
radcobj_validate_segments(const void *data, uint64_t size, const RADCOBJ_Header *header, uint64_t first, uint64_t count)
{
  if (first > header->compressor.segment_count || count > header->compressor.segment_count - first) {
    return "segment range is out of bounds";
  }

  uint64_t directory_end = sizeof(*header) + header->compressor.segment_count * sizeof(RADCOBJ_Segment);
  const uint8_t *bytes = (const uint8_t *)data;
  for (uint64_t i = first; i < first + count; i += 1) {
    RADCOBJ_Segment segment;
    memcpy(&segment, bytes + sizeof(*header) + i * sizeof(segment), sizeof(segment));
    if (segment.size == 0              ||
        segment.offset < directory_end ||
        segment.offset > size          ||
        segment.size > size - segment.offset) {
      return "segment payload is empty or out of bounds";
    }
  }

  return 0;
}

const char *
radcobj_validate(const void *data, uint64_t size, RADCOBJ_Header *header_out)
{
  if (header_out) { memset(header_out, 0, sizeof(*header_out)); }

  RADCOBJ_Header header;
  const char *error = radcobj_validate_header(data, size, &header);
  if (error) { return error; }

  error = radcobj_validate_segments(data, size, &header, 0, header.compressor.segment_count);
  if (error) { return error; }

  if (header_out) { *header_out = header; }
  return 0;
}

const char *
radcobj_encode(CMP_Ctx *cmp, uint32_t segment_size, const void *raw, uint64_t raw_size, void **output_out, uint64_t *output_size, const RADCOBJ_Allocator *allocator)
{
  if (output_out)  { *output_out = 0; }
  if (output_size) { *output_size = 0; }

  if (output_out == NULL || output_size == 0) {
    return "invalid encode output pointers";
  }
  if (allocator == NULL || allocator->alloc == NULL || allocator->free == NULL) {
    return "invalid encoder allocator";
  }

#if OODLE_STATIC
  CMP_Ctx static_sdk = {0};
  static_sdk.sdk_type = CMP_SdkType_Oodle2;
  if (cmp == 0) { cmp = &static_sdk; }
#endif

  if (cmp == 0                                ||
      cmp->sdk_type != CMP_SdkType_Oodle2     ||
      segment_size < RADCOBJ_MIN_SEGMENT_SIZE ||
      IsPow2(segment_size) == 0               ||
      raw_size > SIZE_MAX                     ||
      (raw == 0 && raw_size != 0)) {
    return "invalid encoder input or compression SDK";
  }

  U64 full_count     = raw_size / segment_size;
  U64 tail_size      = raw_size % segment_size;
  U64 segments_count = full_count + (tail_size != 0);
  if (segments_count > (max_U32 - sizeof(RADCOBJ_Header)) / sizeof(RADCOBJ_Segment)) {
    return "segment directory exceeds the format size limit";
  }

  U64 directory_end = sizeof(RADCOBJ_Header) + segments_count * sizeof(RADCOBJ_Segment);
  U64 capacity      = directory_end;
  U64 full_bound    = full_count ? cmp_bound(cmp, segment_size) : 0;
  if (full_count) {
    if (full_bound == 0) {
      return "failed to size encoded segment";
    }
    if (full_bound > (max_U32 - capacity) / full_count) {
      return "encoded object exceeds the format size limit";
    }
    capacity += full_count * full_bound;
  }

  U64 tail_bound = tail_size ? cmp_bound(cmp, tail_size) : 0;
  if (tail_size) {
    if (tail_bound == 0) {
      return "failed to size encoded tail segment";
    }
    if (tail_bound > max_U32 - capacity) {
      return "encoded object exceeds the format size limit";
    }
    capacity += tail_bound;
  }

  if (capacity > SIZE_MAX) {
    return "encoded object exceeds the address-space limit";
  }

  U8 *output = allocator->alloc(allocator->user, (size_t)capacity);
  if (output == 0) { return "failed to allocate encoded object"; }

  U64 src_address = IntFromPtr(raw), dst_address = IntFromPtr(output);
  B32 overlaps    = src_address <= dst_address
                    ? dst_address - src_address < raw_size
                    : src_address - dst_address < capacity;
  if (raw_size && overlaps) {
    allocator->free(allocator->user, output);
    return "encoder allocation overlaps the input";
  }

  U64 cursor = directory_end;
  MemoryZero(output, directory_end);
  for EachIndex(segment_idx, segments_count) {
    U64     data_cursor     = segment_idx * segment_size;
    U64     src_size        = Min(segment_size, raw_size - data_cursor);
    U64     bound_size      = src_size == segment_size ? full_bound : tail_bound;
    String8 src             = { (U8 *)raw + data_cursor, src_size };
    String8 encoded_segment = { output + cursor, bound_size };
    U64     encode_size     = cmp_encode(cmp, src, encoded_segment);
    if (!encode_size || encode_size > bound_size) {
      allocator->free(allocator->user, output);
      return "failed to encode RADCOBJ segment";
    }

    RADCOBJ_Segment segment = { (U32)cursor, (U32)encode_size };
    MemoryCopy(output + sizeof(RADCOBJ_Header) + segment_idx*sizeof(segment), &segment, sizeof(segment));
    cursor += encode_size;
  }

  RADCOBJ_Header header = {0};
  MemoryCopy(&header.magic, RADCOBJ_MAGIC, sizeof(header.magic));
  header.version                  = RADCOBJ_VERSION;
  header.compressor.raw_size      = raw_size;
  header.compressor.segment_size  = segment_size;
  header.compressor.segment_count = segments_count;
  header.compressor.sdk_type      = cmp->sdk_type;
  MemoryCopy(output, &header, sizeof(header));

  *output_out = output;
  *output_size = cursor;
  return 0;
}

static const char *
radcobj_decode_range_from_header(CMP_Ctx *cmp, const void *data, uint64_t size, const RADCOBJ_Header *header, uint64_t first, uint64_t count, void *output, uint64_t output_capacity, const RADCOBJ_Allocator *allocator)
{
  const char *error = radcobj_validate_segments(data, size, header, first, count);
  if (error) { return error; }

#if OODLE_STATIC
  CMP_Ctx static_sdk = {0};
  static_sdk.sdk_type = CMP_SdkType_Oodle2;
  if (!cmp) { cmp = &static_sdk; }
#endif

  if (cmp == NULL || cmp->sdk_type != header->compressor.sdk_type) {
    return "compression SDK does not match the object";
  }

  if (output_capacity > SIZE_MAX) {
    return "decode output capacity exceeds the address-space limit";
  }
  if (count == 0) { return 0; }

  U64 first_offset = first * header->compressor.segment_size;
  U64 end_offset   = first + count == header->compressor.segment_count
                     ? header->compressor.raw_size
                     : (first + count) * header->compressor.segment_size;
  U64 raw_size     = end_offset - first_offset;

  if (output_capacity < raw_size || (!output && raw_size)) {
    return "invalid or insufficient decode output buffer";
  }

  U64 src_address = IntFromPtr(data), dst_address = IntFromPtr(output);
  B32 overlaps = src_address <= dst_address
                 ? dst_address - src_address < size
                 : src_address - dst_address < raw_size;
  if (raw_size && overlaps) {
    return "decode input and output overlap";
  }
  if (allocator == NULL || allocator->alloc == NULL || allocator->free == NULL) {
    return "invalid decoder scratch allocator";
  }

  U64 scratch_size = cmp_scratch(cmp, header->compressor.segment_size);
  if (scratch_size == 0 || scratch_size > SIZE_MAX) {
    return "failed to size decoder scratch";
  }

  U8 *scratch_memory = allocator->alloc(allocator->user, (size_t)scratch_size);
  if (scratch_memory == NULL) {
    return "failed to allocate decoder scratch";
  }

  String8 scratch = { scratch_memory, scratch_size };
  const U8 *bytes = data;
  for (U64 i = first; i < first + count; i += 1) {
    RADCOBJ_Segment segment;
    MemoryCopy(&segment, bytes + sizeof(*header) + i*sizeof(segment), sizeof(segment));

    U64     raw_offset   = i * header->compressor.segment_size;
    U64     segment_size = Min(header->compressor.segment_size, header->compressor.raw_size - raw_offset);
    String8 src          = { (U8 *)bytes + segment.offset, segment.size };
    String8 dst          = { (U8 *)output + (raw_offset - first_offset), segment_size };
    if ( ! cmp_decode(cmp, src, dst, scratch)) {
      error = "failed to decode RADCOBJ segment";
      break;
    }
  }

  allocator->free(allocator->user, scratch_memory);
  return error;
}

const char *
radcobj_decode_range(CMP_Ctx *cmp, const void *data, uint64_t size, uint64_t first_segment, uint64_t segment_count, void *output, uint64_t output_capacity, const RADCOBJ_Allocator *allocator)
{
  RADCOBJ_Header header;
  const char *error = radcobj_validate_header(data, size, &header);
  if (error) { return error; }
  return radcobj_decode_range_from_header(cmp, data, size, &header, first_segment, segment_count, output, output_capacity, allocator);
}

const char *
radcobj_decode(CMP_Ctx *cmp, const void *data, uint64_t size, void *output, uint64_t output_capacity, const RADCOBJ_Allocator *allocator)
{
  RADCOBJ_Header header;
  const char *error = radcobj_validate_header(data, size, &header);
  if (error) { return error; }
  return radcobj_decode_range_from_header(cmp, data, size, &header, 0, header.compressor.segment_count, output, output_capacity, allocator);
}

