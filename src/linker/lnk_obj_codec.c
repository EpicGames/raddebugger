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

internal void
lnk_radcobj_no_free(void *user, void *memory)
{
}

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
lnk_obj_codec_debug_t(Arena *arena, LNK_Obj *obj, String8 data, B32 is_debug_p, CV_DebugT *out)
{
  if (obj->input == NULL || obj->input->cfm_map == NULL || data.str == NULL) { return 0; }
  
  // CFM validates the encoded container before publishing the map
  String8 encoded = obj->input->cfm_map->encoded;
  RADCOBJ_Header header;
  if (radcobj_validate_header(encoded.str, encoded.size, &header)) {
    return 0;
  }
  
  RADCOBJ_Directory hd = header.dirs[RADCOBJ_DirectoryKind_DebugT_Header + is_debug_p];
  RADCOBJ_Directory od = header.dirs[RADCOBJ_DirectoryKind_DebugT_LeafOffsets + is_debug_p];
  RADCOBJ_Directory bd = header.dirs[RADCOBJ_DirectoryKind_DebugT_IpiBits + is_debug_p];
  if (hd.size == 0) {
    return 0;
  }

  RADCOBJ_TypesHeader types;
  MemoryCopy(&types, encoded.str + hd.offset, sizeof(types));

  // validate against the actual section without scanning/decompressing leaves
  for EachIndex(i, types.leaf_count) {
    U32 offset;
    MemoryCopy(&offset, encoded.str + od.offset + i * sizeof(offset), sizeof(offset));
    if (offset > data.size || data.size - offset < sizeof(CV_LeafHeader)) {
      return 0;
    }
  }

  U32 *offsets = push_array_no_zero(arena, U32, types.leaf_count);
  MemoryCopy(offsets, encoded.str + od.offset, od.size);
  U64 counts[CV_TypeIndexSource_COUNT] = {
    types.leaf_count - types.source_counts[0] - types.source_counts[1],
    types.source_counts[0],
    types.source_counts[1]
  };

  B32 result = cv_debug_t_from_data_checked(arena, data, CV_LeafAlign, offsets, counts, out);
  if (result && bd.size) {
    out->ipi_bits.count = bd.size / sizeof(U32);
    out->ipi_bits.v     = push_array_no_zero(arena, U32, out->ipi_bits.count);
    MemoryCopy(out->ipi_bits.v, encoded.str + bd.offset, bd.size);
  }
  return result;
}

internal B32
lnk_obj_codec_matches(String8 encoded)
{
  return radcobj_match_magic(encoded.str, encoded.size);
}

internal B32
lnk_obj_codec_compressor_info(String8 encoded, CMP_CompressorInfoUnpacked *info_out)
{
  StaticAssert(sizeof(CMP_PackedSegment) == sizeof(RADCOBJ_Segment), radcobj_segment_layout);
  StaticAssert(OffsetOf(CMP_PackedSegment, flags) == OffsetOf(RADCOBJ_Segment, flags), radcobj_segment_flags_layout);

  MemoryZeroStruct(info_out);

  RADCOBJ_Header header;
  if (radcobj_validate(encoded.str, encoded.size, &header)) { return 0; }

  info_out->v.raw_size      = header.compressor.raw_size;
  info_out->v.segment_count = header.compressor.segment_count;
  info_out->v.segment_size  = header.compressor.segment_size;
  info_out->v.sdk_type      = header.compressor.sdk_type;
  info_out->segments        = (CMP_PackedSegment *)(encoded.str + sizeof(header));

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

internal B32
lnk_obj_codec_raw_view(String8 encoded, CMP_CompressorInfoUnpacked *compressor_info, Rng1U64 range, String8 *out)
{
  *out = str8_zero();
  if (range.min > range.max) {
    return 0;
  }

  const void *view = 0;
  if (radcobj_raw_view(encoded.str, encoded.size, range.min, range.max - range.min, &view)) {
    return 0;
  }

  *out = str8((U8 *)view, range.max - range.min);
  return 1;
}

internal CFM_Codec *
lnk_cobj_codec(void)
{
  local_persist CFM_Codec result = {
    .matches         = lnk_obj_codec_matches,
    .compressor_info = lnk_obj_codec_compressor_info,
    .decode          = lnk_obj_codec_decode,
    .raw_view        = lnk_obj_codec_raw_view,
  };
  return &result;
}

#endif // !LNK_OBJ_CODEC_ENCODER_ONLY
