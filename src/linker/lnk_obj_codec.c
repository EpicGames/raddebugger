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

typedef struct LNK_DebugSIndex
{
  U8     *sections;
  U64     section_count;
  U64     section_cursor;
  String8 subsections;
  U64     raw_size;
} LNK_DebugSIndex;

internal B32
lnk_obj_codec_debug_s_index(LNK_Obj *obj, LNK_DebugSIndex *out)
{
  StaticAssert(sizeof(CV_DebugS_Subsection) == sizeof(RADCOBJ_DebugSSubsection), debug_s_subsection_size);
  StaticAssert(OffsetOf(CV_DebugS_Subsection, kind) == OffsetOf(RADCOBJ_DebugSSubsection, kind), debug_s_kind_offset);
  StaticAssert(OffsetOf(CV_DebugS_Subsection, data_offset) == OffsetOf(RADCOBJ_DebugSSubsection, data_offset), debug_s_data_offset);
  StaticAssert(OffsetOf(CV_DebugS_Subsection, data_size) == OffsetOf(RADCOBJ_DebugSSubsection, data_size), debug_s_size_offset);

  MemoryZeroStruct(out);
  
  if (obj->input == NULL || obj->input->cfm_map == NULL) {
    return 0;
  }
  
  String8 encoded = obj->input->cfm_map->encoded;
  RADCOBJ_Header header;
  if (radcobj_validate_header(encoded.str, encoded.size, &header)) {
    return 0;
  }
  
  RADCOBJ_Directory dir = header.dirs[RADCOBJ_DirectoryKind_DebugS_Index];
  if (dir.size < sizeof(RADCOBJ_DebugSIndexHeader)) {
    return 0;
  }
  
  RADCOBJ_DebugSIndexHeader index;
  MemoryCopy(&index, encoded.str + dir.offset, sizeof(index));

  U64 sections_size    = (U64)index.section_count * sizeof(RADCOBJ_DebugSSection);
  U64 subsections_size = (U64)index.subsection_count * sizeof(RADCOBJ_DebugSSubsection);
  if (sizeof(index) + sections_size + subsections_size != dir.size) {
    return 0;
  }
  
  out->sections      = encoded.str + dir.offset + sizeof(index);
  out->section_count = index.section_count;
  out->subsections   = str8(out->sections + sections_size, subsections_size);
  out->raw_size      = header.compressor.raw_size;
  return 1;
}

internal B32
lnk_obj_codec_debug_s_block(LNK_DebugSIndex *index, U32 section_number, U64 block_size, CV_DebugS_BlockIndex *out)
{
  *out = (CV_DebugS_BlockIndex){ max_U32, 0 };

  // the selected section list and encoded section table are both ordered
  while (index->section_cursor < index->section_count) {
    RADCOBJ_DebugSSection section;
    MemoryCopy(&section, index->sections + index->section_cursor * sizeof(section), sizeof(section));
  
    if (section.section_number > section_number) {
      return 0;
    }

    index->section_cursor += 1;
  
    if (section.section_number < section_number) {
      continue;
    }
  
    U64 count = index->subsections.size / sizeof(CV_DebugS_Subsection);
    if (section.subsection_base_idx > count || section.subsection_count > count - section.subsection_base_idx) {
      return 0;
    }

    // CFM already validated descriptor progression; checking the last record
    // bounds all preceding records against this actual COFF section as well
    U64 cursor = 0;
    if (section.subsection_count) {
      U64                  last_idx = (U64)section.subsection_base_idx + section.subsection_count - 1;
      CV_DebugS_Subsection last;
      MemoryCopy(&last, index->subsections.str + last_idx * sizeof(last), sizeof(last));

      cursor = (U64)last.data_offset + last.data_size;
      if (cursor > block_size) {
        return 0;
      }
      cursor = Min(AlignPow2(cursor, CV_C13SubSectionAlign), block_size);
    }

    // a complete index may leave trailing bytes, but not another whole header
    if (block_size - cursor >= sizeof(CV_C13SubSectionHeader)) {
      return 0;
    }
    *out = (CV_DebugS_BlockIndex){ section.subsection_base_idx, section.subsection_count };
    
    return 1;
  }
  
  return 0;
}

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
