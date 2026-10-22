// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#include "radcobj.c"

typedef struct LNK_RadCObjArena
{
  Arena *arena;
  U64    allocation_size;
} LNK_RadCObjArena;

internal void *
lnk_radcobj_arena_alloc(void *user, size_t size)
{
  LNK_RadCObjArena *state = user;
  state->allocation_size = size;
  return arena_push(state->arena, size, 16, 0);
}

internal void lnk_radcobj_no_free(void *user, void *memory) {}

internal String8
lnk_obj_codec_encode(Arena *arena, CMP_Ctx *cmp, U64 segment_size, String8 data)
{
  Temp               temp      = temp_begin(arena);
  String8            result    = str8_zero();
  LNK_RadCObjArena   state     = { arena };
  RADCOBJ_Allocator  allocator = { &state, lnk_radcobj_arena_alloc, lnk_radcobj_no_free };
  void              *output    = 0;
  uint64_t           size      = 0;
  const char *error = radcobj_encode(cmp, (U32)segment_size, data.str, data.size, &output, &size, &allocator);
  if (error == NULL) {
    arena_pop(arena, state.allocation_size - size);
    result = str8(output, size);
  } else {
    temp_end(temp);
  }
  return result;
}

#if !LNK_OBJ_CODEC_ENCODER_ONLY

internal B32
lnk_obj_codec_matches(String8 encoded)
{
  return radcobj_match_magic(encoded.str, encoded.size);
}

internal B32
lnk_obj_codec_compressor_info(String8 encoded, CMP_CompressorInfoUnpacked *info_out)
{
  MemoryZeroStruct(info_out);
  RADCOBJ_Header header;
  if (radcobj_validate(encoded.str, encoded.size, &header)) { return 0; }
  info_out->v.raw_size      = header.compressor.raw_size;
  info_out->v.segment_count = header.compressor.segment_count;
  info_out->v.segment_size  = header.compressor.segment_size;
  info_out->v.sdk_type      = header.compressor.sdk_type;
  info_out->segments = (CMP_PackedSegment *)(encoded.str + sizeof(header));
  return 1;
}

internal void *
lnk_radcobj_scratch_alloc(void *user, size_t size)
{
  String8 *scratch = user;
  return size <= scratch->size ? scratch->str : 0;
}

internal B32
lnk_obj_codec_decode(CMP_Ctx *sdk, CMP_CompressorInfoUnpacked *compressor_info, Rng1U64 range, String8 encoded, String8 dst, String8 scratch)
{
  RADCOBJ_Allocator allocator = { &scratch, lnk_radcobj_scratch_alloc, lnk_radcobj_no_free };
  return radcobj_decode_range(sdk, encoded.str, encoded.size, range.min, dim_1u64(range), dst.str, dst.size, &allocator) == 0;
}

internal CFM_Codec *
lnk_cobj_codec(void)
{
  local_persist CFM_Codec result = {
    .matches         = lnk_obj_codec_matches,
    .compressor_info = lnk_obj_codec_compressor_info,
    .decode          = lnk_obj_codec_decode,
  };
  return &result;
}

#endif // !LNK_OBJ_CODEC_ENCODER_ONLY
