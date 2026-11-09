// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////

#pragma once

////////////////////////////////

#define CFM_Status_XList \
  X(Ok)                  \
  X(Unknown)             \
  X(NotEncoded)          \
  X(MixedSegmentSizes)   \
  X(InvalidFormat)       \
  X(Unsupported)         \
  X(InvalidRange)        \
  X(RangeTooLarge)       \
  X(OutOfMemory)         \
  X(DecodeFailed)        \
  X(Busy)                \
  X(Closing)             \
  X(MapFailure)          \
  X(SdkFailure)          \
  /* end */

typedef enum CFM_Status
{
#define X(n, ...) CFM_##n,
  CFM_Status_XList
#undef X
} CFM_Status;

////////////////////////////////

typedef struct CMP_Ctx CMP_Ctx;

#define CMP_OPEN(name)    char* name(Arena *arena, String8 library_path, CMP_Ctx **ctx_out)
#define CMP_CLOSE(name)   B32   name(CMP_Ctx *ctx)
#define CMP_SCRATCH(name) U64   name(CMP_Ctx *ctx, U64 size)
#define CMP_ENCODE(name)  U64   name(CMP_Ctx *ctx, String8 src, String8 dst)
#define CMP_DECODE(name)  B32   name(CMP_Ctx *ctx, String8 src, String8 dst, String8 scratch)
#define CMP_BOUND(name)   U64   name(CMP_Ctx *ctx, U64 size)

typedef CMP_OPEN(CMP_Open);
typedef CMP_CLOSE(CMP_Close);
typedef CMP_SCRATCH(CMP_Scratch);
typedef CMP_ENCODE(CMP_Encode);
typedef CMP_DECODE(CMP_Decode);

#define CMP_Sdk_XList \
  X(Oodle2)           \
  /* end */

typedef U32 CMP_SdkType;
typedef enum
{
  CMP_SdkType_Null,
#define X(n,...) CMP_SdkType_##n,
  CMP_Sdk_XList
#undef X
  CMP_SdkType_Count,
} CMP_SdkTypeEnum;

struct CMP_Ctx
{
  CMP_SdkType  sdk_type;
  Library      library;
  void        *api;
};

typedef struct CMP_PackedSegment
{
  U32 offset;
  U32 size;
  U32 flags; // codec-defined segment flags
} CMP_PackedSegment;

typedef struct
{
  U64         raw_size;
  U64         segment_count;
  U32         segment_size;
  CMP_SdkType sdk_type;
} CMP_CompressorInfo;

typedef struct
{
  CMP_CompressorInfo  v;
  CMP_PackedSegment  *segments;
} CMP_CompressorInfoUnpacked;

////////////////////////////////
// Codec

typedef struct
{
  U64   id;
  void *user;
  B32 (*matches        )(String8 encoded);
  B32 (*compressor_info)(String8 encoded, CMP_CompressorInfoUnpacked *info_out);
  B32 (*decode         )(CMP_Ctx *sdk, CMP_CompressorInfoUnpacked *compressor_info, Rng1U64 segment_range, String8 raw_file, String8 dst, String8 scratch);
  B32 (*raw_view       )(String8 encoded, CMP_CompressorInfoUnpacked *compressor_info, Rng1U64 range, String8 *out);
} CFM_Codec;

////////////////////////////////

// Codec
internal char * cfm_status_string(CFM_Status status);

internal char * cmp_open(Arena *arena, CMP_SdkType sdk_type, String8 sdk_path, CMP_Ctx **ctx_out);
internal B32    cmp_close(CMP_Ctx *ctx);
internal U64    cmp_scratch(CMP_Ctx *ctx, U64 size); 
internal B32    cmp_decode(CMP_Ctx *ctx, String8 src, String8 dst, String8 scratch);
internal U64    cmp_bound(CMP_Ctx *ctx, U64 size);

