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
  RADCOBJ_DirectoryKind_DebugT_Header,      // RADCOBJ_TypesHeader
  RADCOBJ_DirectoryKind_DebugP_Header,
  RADCOBJ_DirectoryKind_DebugT_LeafOffsets, // uint32_t array of offsets
  RADCOBJ_DirectoryKind_DebugP_LeafOffsets,
  RADCOBJ_DirectoryKind_DebugT_IpiBits,     // uint32_t bit array: 0=TPI, 1=IPI
  RADCOBJ_DirectoryKind_DebugP_IpiBits,
  RADCOBJ_DirectoryKind_DebugS_Index,       // RADCOBJ_DebugSIndexHeader, sections, subsections
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

typedef uint32_t RADCOBJ_SegmentFlags;
enum
{
  RADCOBJ_SegmentFlag_Uncompressed = (1u << 0),
};

typedef struct RADCOBJ_Segment
{
  uint32_t offset;
  uint32_t size; // stored payload size; raw segments have their exact logical size
  RADCOBJ_SegmentFlags flags;
} RADCOBJ_Segment;

typedef struct RADCOBJ_TypesHeader
{
  uint64_t source_counts[2]; // [TPI, IPI]
  uint64_t leaf_count;       // must match number of leaf offsets
} RADCOBJ_TypesHeader;

// DebugS_Index is an uncompressed, 4-byte-aligned blob containing this header,
// then section_count section records, then subsection_count subsection records.
// All integers are little endian; only valid C13 sections are indexed.
typedef struct RADCOBJ_DebugSIndexHeader
{
  uint32_t section_count;
  uint32_t subsection_count;
} RADCOBJ_DebugSIndexHeader;

typedef struct RADCOBJ_DebugSSection
{
  uint32_t section_number;      // original 1-based COFF section number, ascending
  uint32_t subsection_base_idx; // index in the object-wide subsection array
  uint32_t subsection_count;
} RADCOBJ_DebugSSection;

typedef struct RADCOBJ_DebugSSubsection
{
  uint32_t kind;        // original C13 kind, including IgnoreFlag
  uint32_t data_offset; // relative to section data AFTER the CodeView signature
  uint32_t data_size;   // payload bytes, excluding header and padding
} RADCOBJ_DebugSSubsection;

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
const char *radcobj_raw_view(const void *data, uint64_t size, uint64_t raw_offset, uint64_t raw_size, const void **output);

#ifdef __cplusplus
}
#endif

#endif // RADCOBJ_H
