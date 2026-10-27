// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RADCOBJ_H
#define RADCOBJ_H

#include <stdint.h>
#include <stddef.h>

#define RADCOBJ_MAGIC "RADCOBJ\0"
#define RADCOBJ_VERSION 1
#define RADCOBJ_MIN_SEGMENT_SIZE 4096
#define RADCOBJ_SDK_OODLE2 1

typedef struct RADCOBJ_CompressorInfo
{
  uint64_t raw_size;
  uint64_t segment_count;
  uint32_t segment_size;
  uint32_t sdk_type;
} RADCOBJ_CompressorInfo;

typedef enum RADCOBJ_DirectoryKind
{
  RADCOBJ_DirectoryKind_Null,
  RADCOBJ_DirectoryKind_DebugT_Header, // RADCOBJ_TypesHeader
  RADCOBJ_DirectoryKind_DebugP_Header,
  RADCOBJ_DirectoryKind_DebugT_LeafOffsets, // uint32_t array of offsets
  RADCOBJ_DirectoryKind_DebugP_LeafOffsets,
  RADCOBJ_DirectoryKind_Count,
} RADCOBJ_DirectoryKind;

typedef struct RADCOBJ_Directory
{
  uint32_t offset;
  uint32_t size;
} RADCOBJ_Directory;

typedef struct RADCOBJ_Header
{
  uint64_t               magic;
  uint64_t               version;
  RADCOBJ_Directory      dirs[RADCOBJ_DirectoryKind_Count];
  RADCOBJ_CompressorInfo compressor;
} RADCOBJ_Header;

typedef struct RADCOBJ_Segment
{
  uint32_t offset;
  uint32_t size;
} RADCOBJ_Segment;

typedef struct RADCOBJ_TypesHeader
{
  uint64_t source_counts[2]; // [TPI, IPI]
  uint64_t leaf_count;       // must match number of leaf offsets
} RADCOBJ_TypesHeader;

typedef char RADCOBJ_HeaderSizeCheck[sizeof(RADCOBJ_Header) == 80 ? 1 : -1];
typedef char RADCOBJ_HeaderCompressorOffsetCheck[offsetof(RADCOBJ_Header, compressor) == 56 ? 1 : -1];
typedef char RADCOBJ_TypesHeaderSizeCheck[sizeof(RADCOBJ_TypesHeader) == 24 ? 1 : -1];
typedef char RADCOBJ_SegmentSizeCheck[sizeof(RADCOBJ_Segment) == 8 ? 1 : -1];

#ifdef __cplusplus
extern "C" {
#endif

struct CMP_Ctx;

typedef struct RADCOBJ_Allocator
{
  void *user;
  void *(*alloc)(void *user, size_t size);
  void  (*free)(void *user, void *memory);
} RADCOBJ_Allocator;

int radcobj_match_magic(const void *data, uint64_t size);
const char *radcobj_validate(const void *data, uint64_t size, RADCOBJ_Header *header_out);
const char *radcobj_encode(struct CMP_Ctx *sdk, uint32_t segment_size, const void *raw, uint64_t raw_size, void **output, uint64_t *output_size, const RADCOBJ_Allocator *allocator);
const char *radcobj_decode(struct CMP_Ctx *sdk, const void *data, uint64_t size, void *output, uint64_t output_capacity, const RADCOBJ_Allocator *allocator);
const char *radcobj_decode_range(struct CMP_Ctx *sdk, const void *data, uint64_t size, uint64_t first_segment, uint64_t segment_count, void *output, uint64_t output_capacity, const RADCOBJ_Allocator *allocator);

#ifdef __cplusplus
}
#endif

#endif // RADCOBJ_H
