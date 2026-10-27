// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#include "radcobj.h"
#include <string.h>

////////////////////////////////

int
radcobj_match_magic(const void *data, uint64_t size)
{
  return data != 0 && size >= sizeof(uint64_t) && memcmp(data, RADCOBJ_MAGIC, sizeof(uint64_t)) == 0;
}

internal const char *
radcobj_validate_header(const void *data, uint64_t size, RADCOBJ_Header *header_out)
{
  if (header_out) { MemoryZeroStruct(header_out); }

  if (data == NULL || size < sizeof(RADCOBJ_Header) || size > SIZE_MAX) {
    return "header is truncated or container is too large";
  }

  if ( ! radcobj_match_magic(data, size)) {
    return "unrecognized RADCOBJ signature";
  }

  RADCOBJ_Header header;
  MemoryCopy(&header, data, sizeof(header));

  if (header.version != RADCOBJ_VERSION) {
    return "unsupported RADCOBJ version";
  }
  if (header.compressor.sdk_type != RADCOBJ_SDK_OODLE2) {
    return "unsupported compression SDK";
  }

  uint64_t raw_size     = header.compressor.raw_size;
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

internal const char *
radcobj_validate_segments(const void *data, uint64_t size, const RADCOBJ_Header *header, uint64_t first, uint64_t count)
{
  if (first > header->compressor.segment_count || count > header->compressor.segment_count - first) {
    return "segment range is out of bounds";
  }

  uint64_t directory_end = sizeof(*header) + header->compressor.segment_count * sizeof(RADCOBJ_Segment);
  const uint8_t *bytes = (const uint8_t *)data;
  for (uint64_t i = first; i < first + count; i += 1) {
    RADCOBJ_Segment segment;
    MemoryCopy(&segment, bytes + sizeof(*header) + i * sizeof(segment), sizeof(segment));
    if (segment.flags & ~RADCOBJ_SegmentFlag_Uncompressed) {
      return "unsupported segment flags";
    }

    U64 raw_size = Min(header->compressor.segment_size, header->compressor.raw_size - i * header->compressor.segment_size);
    if ((segment.flags & RADCOBJ_SegmentFlag_Uncompressed) && segment.size != raw_size) {
      return "uncompressed segment size does not match its logical size";
    }

    if (segment.size == 0              ||
        segment.offset < directory_end ||
        segment.offset > size          ||
        segment.size > size - segment.offset) {
      return "segment payload is empty or out of bounds";
    }

    for (unsigned dir_idx = 0; dir_idx < RADCOBJ_DirectoryKind_Count; dir_idx += 1) {
      RADCOBJ_Directory dir = header->dirs[dir_idx];
      if (dir.size && segment.offset < (U64)dir.offset + dir.size && dir.offset < (U64)segment.offset + segment.size) {
        return "segment payload overlaps auxiliary directory";
      }
    }
  }

  return 0;
}

const char *
radcobj_validate(const void *data, uint64_t size, RADCOBJ_Header *header_out)
{
  if (header_out) { MemoryZeroStruct(header_out); }

  RADCOBJ_Header header;
  const char *error = radcobj_validate_header(data, size, &header);
  if (error) { return error; }

  error = radcobj_validate_segments(data, size, &header, 0, header.compressor.segment_count);
  if (error) { return error; }

  U64 directory_end = sizeof(header) + header.compressor.segment_count * sizeof(RADCOBJ_Segment);
  for (U64 i = 1; i < RADCOBJ_DirectoryKind_Count; i += 1) {
    RADCOBJ_Directory a = header.dirs[i];
    if (a.size && a.offset < directory_end) {
      return "type directory overlaps container header";
    }
    for (U64 j = i + 1; j < RADCOBJ_DirectoryKind_Count; j += 1) {
      RADCOBJ_Directory b = header.dirs[j];
      if (a.size && b.size && a.offset < (U64)b.offset + b.size && b.offset < (U64)a.offset + a.size) {
        return "overlapping type directories";
      }
    }
  }

  for EachIndex(i, 2) {
    RADCOBJ_Directory hd = header.dirs[RADCOBJ_DirectoryKind_DebugT_Header + i];
    RADCOBJ_Directory od = header.dirs[RADCOBJ_DirectoryKind_DebugT_LeafOffsets + i];
    RADCOBJ_Directory bd = header.dirs[RADCOBJ_DirectoryKind_DebugT_IpiBits + i];
    if (hd.size == 0 && od.size == 0 && bd.size == 0) { continue; }

    if (hd.size != sizeof(RADCOBJ_TypesHeader)) {
      return "invalid type header size";
    }
    
    RADCOBJ_TypesHeader types;
    MemoryCopy(&types, (const U8 *)data + hd.offset, sizeof(types));

    if (types.leaf_count > od.size / sizeof(U32)  ||
        types.leaf_count * sizeof(U32) != od.size ||
        types.source_counts[0] > types.leaf_count ||
        types.source_counts[1] > types.leaf_count - types.source_counts[0]) {
      return "invalid type counts";
    }

    U32 previous = 0;
    for EachIndex(leaf, types.leaf_count) {
      U32 offset;
      MemoryCopy(&offset, (const U8 *)data + od.offset + leaf * sizeof(offset), sizeof(offset));
      if (offset > header.compressor.raw_size || header.compressor.raw_size - offset < sizeof(CV_LeafHeader) || (leaf && offset <= previous)) {
        return "invalid type leaf offset";
      }
      previous = offset;
    }

    if (bd.size) {
      U64 word_count = CeilIntegerDiv(types.leaf_count, 32);
      if (bd.offset % sizeof(U32) != 0 || bd.size != word_count * sizeof(U32) ||
          types.source_counts[0] != types.leaf_count - types.source_counts[1]) {
        return "invalid type source bitmap size or counts";
      }
      
      U64 ipi_count = 0;
      for EachIndex(word_idx, word_count) {
        U32 word;
        MemoryCopy(&word, (const U8 *)data + bd.offset + word_idx * sizeof(word), sizeof(word));
        if (word_idx + 1 == word_count && types.leaf_count % 32 && (word >> (types.leaf_count % 32))) {
          return "nonzero type source bitmap padding";
        }
        ipi_count += count_bits_set32(word);
      }
      
      if (ipi_count != types.source_counts[1]) {
        return "type source bitmap does not match IPI count";
      }
    }
  }

  RADCOBJ_Directory debug_s_dir = header.dirs[RADCOBJ_DirectoryKind_DebugS_Index];
  if (debug_s_dir.size) {
    if (debug_s_dir.offset % sizeof(U32) || debug_s_dir.size < sizeof(RADCOBJ_DebugSIndexHeader)) {
      return "invalid debug section index header";
    }
    
    U8 *index_data = (U8 *)data + debug_s_dir.offset;
    RADCOBJ_DebugSIndexHeader index;
    MemoryCopy(&index, index_data, sizeof(index));

    U64 subsections_offset = sizeof(index) + (U64)index.section_count * sizeof(RADCOBJ_DebugSSection);
    U64 index_size         = subsections_offset + (U64)index.subsection_count * sizeof(RADCOBJ_DebugSSubsection);
    if (index_size != debug_s_dir.size) {
      return "invalid debug section index size";
    }
    
    U32 previous_section  = 0;
    U64 subsection_cursor = 0;
    for EachIndex(i, index.section_count) {
      RADCOBJ_DebugSSection section;
      MemoryCopy(&section, index_data + sizeof(index) + i * sizeof(section), sizeof(section));
      
      if (section.section_number <= previous_section ||
          section.section_number > header.compressor.raw_size / sizeof(COFF_SectionHeader) ||
          section.subsection_base_idx != subsection_cursor ||
          section.subsection_count > index.subsection_count - subsection_cursor) {
        return "invalid debug section index range";
      }

      U64 expected_offset = sizeof(CV_C13SubSectionHeader);
      for EachIndex(j, section.subsection_count) {
        RADCOBJ_DebugSSubsection subsection;
        MemoryCopy(&subsection, index_data + subsections_offset + (subsection_cursor + j) * sizeof(subsection), sizeof(subsection));
      
        U64 end = (U64)subsection.data_offset + subsection.data_size;
        if (subsection.data_offset != expected_offset ||
            end > header.compressor.raw_size ||
            end > max_U32 - sizeof(CV_Signature)) {
          return "invalid debug subsection payload range";
        }

        expected_offset = AlignPow2(end, CV_C13SubSectionAlign) + sizeof(CV_C13SubSectionHeader);
      }
      
      subsection_cursor += section.subsection_count;
      previous_section = section.section_number;
    }

    if (subsection_cursor != index.subsection_count) {
      return "unreferenced debug subsections";
    }
  }

  if (header_out) { *header_out = header; }
  return 0;
}

const char *
radcobj_encode(CMP_Ctx *cmp, uint32_t segment_size, const void *raw, uint64_t raw_size, void **output_out, uint64_t *output_size, const RADCOBJ_Allocator *allocator)
{
  if (output_out)  { *output_out = 0;  }
  if (output_size) { *output_size = 0; }

  if (!output_out                             ||
      !output_size                            ||
      !allocator                              ||
      !allocator->alloc                       ||
      !allocator->free                        ||
      raw_size > SIZE_MAX                     ||
      (!raw && raw_size)                      ||
      segment_size < RADCOBJ_MIN_SEGMENT_SIZE ||
      !IsPow2(segment_size)) {
    return "invalid encoder input or allocator";
  }

  U64 full_count     = raw_size / segment_size;
  U64 tail_size      = raw_size % segment_size;
  U64 segments_count = full_count + (tail_size != 0);
  if (segments_count > (max_U32 - sizeof(RADCOBJ_Header)) / sizeof(RADCOBJ_Segment)) {
    return "segment directory exceeds the format size limit";
  }

  typedef struct RADCOBJ_Types {
    Arena     *arena;
    B32        present[2];
    CV_DebugT  debug[2];
  } RADCOBJ_Types;

  typedef struct RADCOBJ_DebugSSectionNode {
    struct RADCOBJ_DebugSSectionNode *next;
    RADCOBJ_DebugSSection  v;
    COFF_SectionHeader    *header;
  } RADCOBJ_DebugSSectionNode;
  
  const char *error = 0;

  RADCOBJ_Types             types                 = {0};
  U32Array                  uncompressed_segments = {0};
  RADCOBJ_DebugSIndexHeader debug_s_index         = {0};
  RADCOBJ_DebugSSectionNode *debug_s_first        = 0;
  RADCOBJ_DebugSSectionNode *debug_s_last         = 0;

  // find debug types and segments overlapping hash sections
  {
    String8             raw_obj     = str8((U8 *)raw, raw_size);
    COFF_FileHeaderInfo file_header = coff_file_header_info_from_data(raw_obj);
    if (file_header.header_range.max &&
        file_header.section_table_range.max <= raw_obj.size &&
        file_header.symbol_table_range.max <= raw_obj.size) {
      String8             string_table  = str8_substr(raw_obj, file_header.string_table_range);
      COFF_SectionHeader *section_table = (COFF_SectionHeader *)(raw_obj.str + file_header.section_table_range.min);
      String8             names[]       = { str8_lit_comp(".debug$T"), str8_lit_comp(".debug$P") };
      COFF_SectionHeader *sections[2]   = {0};

      for EachIndex(i, file_header.section_count_no_null) {
        COFF_SectionHeader *header = &section_table[i];
        String8             name   = coff_name_from_section_header(string_table, header);
        for EachElement(k, sections) {
          if (str8_match(names[k], name, 0)) { sections[k] = header; }
        }

        if (str8_match(name, str8_lit(".debug$S"), 0) &&
            header->fsize >= sizeof(CV_Signature) &&
            header->foff <= raw_obj.size &&
            header->fsize <= raw_obj.size - header->foff) {
          String8 section_data = str8_substr(raw_obj, r1u64s(header->foff, header->fsize));
          if (cv_signature_from_debug_s(section_data) == CV_Signature_C13) {
            String8          data             = str8_skip(section_data, sizeof(CV_Signature));
            U64              cursor           = 0;
            U64              subsection_count = 0;
            B32              malformed        = 0;
            CV_C13SubSection subsection;
            while (cv_c13_sub_section_from_data(data, cursor, &subsection)) {
              if (subsection.truncated) {
                malformed = 1;
                break;
              }
              subsection_count += 1;
              cursor = subsection.next_offset;
            }

            // do not cache a malformed prefix: the linker's normal parser must still diagnose the section
            if ( ! malformed) {
              U64 index_size = sizeof(debug_s_index) +
                               ((U64)debug_s_index.section_count + 1) * sizeof(RADCOBJ_DebugSSection) +
                               ((U64)debug_s_index.subsection_count + subsection_count) * sizeof(RADCOBJ_DebugSSubsection);
              
              if (index_size > max_U32) {
                error = "debug section index exceeds the format size limit";
                goto exit;
              }

              if (types.arena == NULL) {
#if RADCOBJ_LIB && OS_WINDOWS
                radcobj_init_system_info();
#endif
                types.arena = arena_alloc(.reserve_size = KB(64), .commit_size = KB(64));
              }

              RADCOBJ_DebugSSectionNode *node = push_array(types.arena, RADCOBJ_DebugSSectionNode, 1);
              node->v = (RADCOBJ_DebugSSection){
                (U32)(i + 1),
                debug_s_index.subsection_count,
                (U32)subsection_count
              };
              node->header = header;
              SLLQueuePush(debug_s_first, debug_s_last, node);
              
              debug_s_index.section_count += 1;
              debug_s_index.subsection_count += (U32)subsection_count;
            }
          }
        }

        if (str8_match(name, str8_lit(".debug$H"), 0) &&
            header->fsize &&
            header->foff <= raw_obj.size &&
            header->fsize <= raw_obj.size - header->foff) {
          if (types.arena == NULL) {
#if RADCOBJ_LIB && OS_WINDOWS
            radcobj_init_system_info();
#endif
            types.arena = arena_alloc(.reserve_size = KB(64), .commit_size = KB(64));
          }

          if (uncompressed_segments.v == NULL) {
            uncompressed_segments = bit_array_init32(types.arena, segments_count);
          }

          U64 first_segment = header->foff / segment_size;
          U64 last_segment  = ((U64)header->foff + header->fsize - 1) / segment_size;
          for (U64 segment_idx = first_segment; segment_idx <= last_segment; segment_idx += 1) {
            bit_array_set_bit32(uncompressed_segments, segment_idx, 1);
          }
        }
      }

      for EachElement(k, sections) {
        COFF_SectionHeader *header = sections[k];
        if (header == NULL                              ||
            header->foff > raw_obj.size                 ||
            header->fsize > raw_obj.size - header->foff ||
            header->fsize < sizeof(CV_Signature)) {
          continue;
        }

        String8 data = str8_substr(raw_obj, r1u64s(header->foff, header->fsize));
        if (cv_signature_from_debug_s(data) != CV_Signature_C13) { continue; }

        if (types.arena == NULL) {
#if RADCOBJ_LIB && OS_WINDOWS
          radcobj_init_system_info();
#endif
          types.arena = arena_alloc(.reserve_size = KB(64), .commit_size = KB(64));
        }

        data = str8_skip(data, sizeof(CV_Signature));
        if ( ! cv_debug_t_from_data_checked(types.arena, data, CV_LeafAlign, 0, 0, &types.debug[k])) {
          error = "failed to allocate type leaf offsets";
          goto exit;
        }

        if (types.debug[k].count > max_U32 / sizeof(U32)) {
          error = "type offsets exceed the format size limit";
          goto exit;
        }

        types.present[k] = 1;
      }
    }
  }

#if OODLE_STATIC
  CMP_Ctx static_sdk = {0};
  static_sdk.sdk_type = CMP_SdkType_Oodle2;
  if (cmp == 0) { cmp = &static_sdk; }
#endif

  if (cmp == 0 || cmp->sdk_type != CMP_SdkType_Oodle2) {
    error = "invalid encoder input or compression SDK";
    goto exit;
  }

  U64            directory_end = sizeof(RADCOBJ_Header) + segments_count * sizeof(RADCOBJ_Segment);
  U64            metadata_end  = directory_end;
  RADCOBJ_Header header        = {0};
  for EachElement(i, types.debug) {
    if (types.present[i]) {
      metadata_end = AlignPow2(metadata_end, 8);

      U64 offsets_size = types.debug[i].count * sizeof(U32);
      if (metadata_end > max_U32 - sizeof(RADCOBJ_TypesHeader) || offsets_size > max_U32 - metadata_end - sizeof(RADCOBJ_TypesHeader)) {
        error = "type metadata exceeds the format size limit";
        goto exit;
      }

      header.dirs[RADCOBJ_DirectoryKind_DebugT_Header + i] = (RADCOBJ_Directory){ (U32)metadata_end, sizeof(RADCOBJ_TypesHeader) };
      metadata_end += sizeof(RADCOBJ_TypesHeader);

      header.dirs[RADCOBJ_DirectoryKind_DebugT_LeafOffsets + i] = (RADCOBJ_Directory){ (U32)metadata_end, (U32)offsets_size };
      metadata_end += offsets_size;

      if (types.debug[i].ipi_bits.v) {
        U64 bitmap_size = types.debug[i].ipi_bits.count * sizeof(U32);
        if (bitmap_size > max_U32 - metadata_end) {
          error = "type source bitmap exceeds the format size limit";
          goto exit;
        }
        header.dirs[RADCOBJ_DirectoryKind_DebugT_IpiBits + i] = (RADCOBJ_Directory){ (U32)metadata_end, (U32)bitmap_size };
        metadata_end += bitmap_size;
      }
    }
  }

  if (debug_s_index.section_count) {
    metadata_end = AlignPow2(metadata_end, 4);
    
    U64 index_size = sizeof(debug_s_index) +
                     (U64)debug_s_index.section_count * sizeof(RADCOBJ_DebugSSection) +
                     (U64)debug_s_index.subsection_count * sizeof(RADCOBJ_DebugSSubsection);
    
    if (metadata_end > max_U32 || index_size > max_U32 - metadata_end) {
      error = "debug section index exceeds the format size limit";
      goto exit;
    }
    
    header.dirs[RADCOBJ_DirectoryKind_DebugS_Index] = (RADCOBJ_Directory){ (U32)metadata_end, (U32)index_size };
    metadata_end += index_size;
  }

  U64 capacity   = metadata_end;
  U64 full_bound = full_count ? cmp_bound(cmp, segment_size) : 0;
  if (full_count) {
    if (full_bound == 0) {
      error = "failed to size encoded segment";
      goto exit;
    }

    // raw payloads are 8-byte aligned; full raw segments remain contiguous
    full_bound = Max(full_bound, (U64)segment_size + 7);
    if (full_bound > (max_U32 - capacity) / full_count) {
      error = "encoded object exceeds the format size limit";
      goto exit;
    }
    
    capacity += full_count * full_bound;
  }

  U64 tail_bound = tail_size ? cmp_bound(cmp, tail_size) : 0;
  if (tail_size) {
    if (tail_bound == 0) {
      error = "failed to size encoded tail segment";
      goto exit;
    }
    tail_bound = Max(tail_bound, tail_size + 7);
    if (tail_bound > max_U32 - capacity) {
      error = "encoded object exceeds the format size limit";
      goto exit;
    }
    capacity += tail_bound;
  }

  if (capacity > SIZE_MAX) {
    error = "encoded object exceeds the address-space limit";
    goto exit;
  }

  U8 *output = allocator->alloc(allocator->user, (size_t)capacity);
  if (output == 0) {
    error = "failed to allocate encoded object";
    goto exit;
  }

  U64 src_address = IntFromPtr(raw), dst_address = IntFromPtr(output);
  B32 overlaps    = src_address <= dst_address
                    ? dst_address - src_address < raw_size
                    : src_address - dst_address < capacity;
  if (raw_size && overlaps) {
    allocator->free(allocator->user, output);
    error = "encoder allocation overlaps the input";
    goto exit;
  }

  U64 cursor = metadata_end;
  MemoryZero(output, metadata_end);
  for EachElement(i, types.debug) {
    if (types.present[i]) {
      CV_DebugT *debug = &types.debug[i];
      RADCOBJ_TypesHeader type_header = { {debug->source_counts[CV_TypeIndexSource_TPI], debug->source_counts[CV_TypeIndexSource_IPI]}, debug->count };
      MemoryCopy(output + header.dirs[RADCOBJ_DirectoryKind_DebugT_Header + i].offset, &type_header, sizeof(type_header));
      MemoryCopy(output + header.dirs[RADCOBJ_DirectoryKind_DebugT_LeafOffsets + i].offset, debug->offsets, debug->count * sizeof(U32));
      if (debug->ipi_bits.v) {
        RADCOBJ_Directory bitmap = header.dirs[RADCOBJ_DirectoryKind_DebugT_IpiBits + i];
        MemoryCopy(output + bitmap.offset, debug->ipi_bits.v, bitmap.size);
      }
    }
  }

  if (debug_s_index.section_count) {
    U8 *index_data = output + header.dirs[RADCOBJ_DirectoryKind_DebugS_Index].offset;
    MemoryCopy(index_data, &debug_s_index, sizeof(debug_s_index));

    U64 section_cursor = sizeof(debug_s_index);
    U64 subsection_cursor = section_cursor + (U64)debug_s_index.section_count * sizeof(RADCOBJ_DebugSSection);
    
    // write descriptors directly to the output
    for EachNode(node, RADCOBJ_DebugSSectionNode, debug_s_first) {
      MemoryCopy(index_data + section_cursor, &node->v, sizeof(node->v));
      section_cursor += sizeof(node->v);

      String8 data   = str8_substr(str8((U8 *)raw, raw_size), r1u64s((U64)node->header->foff + sizeof(CV_Signature), node->header->fsize - sizeof(CV_Signature)));
      U64     cursor = 0;
      for EachIndex(i, node->v.subsection_count) {
        CV_C13SubSection parsed_subsection;
        B32              parsed = cv_c13_sub_section_from_data(data, cursor, &parsed_subsection);
        AssertAlways(parsed && ! parsed_subsection.truncated);

        RADCOBJ_DebugSSubsection subsection = { parsed_subsection.kind, (U32)parsed_subsection.data_offset, parsed_subsection.declared_size };
        MemoryCopy(index_data + subsection_cursor, &subsection, sizeof(subsection));
        
        subsection_cursor += sizeof(subsection);
        cursor = parsed_subsection.next_offset;
      }
    }
    Assert(subsection_cursor == header.dirs[RADCOBJ_DirectoryKind_DebugS_Index].size);
  }

  for EachIndex(segment_idx, segments_count) {
    U64     data_cursor     = segment_idx * segment_size;
    U64     src_size        = Min(segment_size, raw_size - data_cursor);
    U64     bound_size      = src_size == segment_size ? full_bound : tail_bound;
    String8 src             = { (U8 *)raw + data_cursor, src_size };
    B32     store_raw       = uncompressed_segments.v && bit_array_get_bit32(uncompressed_segments, segment_idx);
    U64     encode_size     = src_size;
    if ( ! store_raw) {
      String8 encoded_segment = { output + cursor, bound_size };
      encode_size = cmp_encode(cmp, src, encoded_segment);
      if (encode_size == 0 || encode_size > bound_size) {
        allocator->free(allocator->user, output);
        error = "failed to encode RADCOBJ segment";
        goto exit;
      }
      store_raw = encode_size >= src_size;
    }

    RADCOBJ_Segment segment = { (U32)cursor, (U32)encode_size, 0 };
    if (store_raw) {
      U64 raw_cursor = AlignPow2(cursor, 8);
      MemoryZero(output + cursor, raw_cursor - cursor);
      MemoryCopy(output + raw_cursor, src.str, src.size);
      segment = (RADCOBJ_Segment){ (U32)raw_cursor, (U32)src_size, RADCOBJ_SegmentFlag_Uncompressed };
    }
    MemoryCopy(output + sizeof(RADCOBJ_Header) + segment_idx*sizeof(segment), &segment, sizeof(segment));
    cursor = (U64)segment.offset + segment.size;
  }

  MemoryCopy(&header.magic, RADCOBJ_MAGIC, sizeof(header.magic));
  header.version                  = RADCOBJ_VERSION;
  header.compressor.raw_size      = raw_size;
  header.compressor.segment_size  = segment_size;
  header.compressor.segment_count = segments_count;
  header.compressor.sdk_type      = cmp->sdk_type;
  MemoryCopy(output, &header, sizeof(header));

  *output_out = output;
  *output_size = cursor;

exit:

  if (types.arena) {
    arena_release(types.arena);
  }

  return error;
}

internal const char *
radcobj_decode_range_from_header(CMP_Ctx *cmp, const void *data, uint64_t size, const RADCOBJ_Header *header, uint64_t first, uint64_t count, void *output, uint64_t output_capacity, const RADCOBJ_Allocator *allocator)
{
  const char *error = radcobj_validate_segments(data, size, header, first, count);
  if (error) { return error; }

#if OODLE_STATIC
  CMP_Ctx static_sdk = {0};
  static_sdk.sdk_type = CMP_SdkType_Oodle2;
  if (!cmp) { cmp = &static_sdk; }
#endif

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

  String8 scratch = {0};
  const U8 *bytes = data;
  for (U64 i = first; i < first + count; i += 1) {
    RADCOBJ_Segment segment;
    MemoryCopy(&segment, bytes + sizeof(*header) + i*sizeof(segment), sizeof(segment));

    U64     raw_offset   = i * header->compressor.segment_size;
    U64     segment_size = Min(header->compressor.segment_size, header->compressor.raw_size - raw_offset);
    String8 src          = { (U8 *)bytes + segment.offset, segment.size };
    String8 dst          = { (U8 *)output + (raw_offset - first_offset), segment_size };
    if (segment.flags & RADCOBJ_SegmentFlag_Uncompressed) {
      MemoryCopy(dst.str, src.str, dst.size);
    } else {
      // raw-only ranges need neither an SDK context nor a scratch allocator
      if (scratch.str == NULL) {
        if (cmp == NULL || cmp->sdk_type != header->compressor.sdk_type) {
          error = "compression SDK does not match the object";
          break;
        }
        if (allocator == NULL || allocator->alloc == NULL || allocator->free == NULL) {
          error = "invalid decoder scratch allocator";
          break;
        }
        U64 scratch_size = cmp_scratch(cmp, header->compressor.segment_size);
        if (scratch_size == 0 || scratch_size > SIZE_MAX) {
          error = "failed to size decoder scratch";
          break;
        }
        scratch.str = allocator->alloc(allocator->user, (size_t)scratch_size);
        if (scratch.str == NULL) {
          error = "failed to allocate decoder scratch";
          break;
        }
        scratch.size = scratch_size;
      }
      if ( ! cmp_decode(cmp, src, dst, scratch)) {
        error = "failed to decode RADCOBJ segment";
        break;
      }
    }
  }

  if (scratch.str) { allocator->free(allocator->user, scratch.str); }
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

const char *
radcobj_raw_view(const void *data, uint64_t size, uint64_t raw_offset, uint64_t raw_size, const void **output)
{
  if (output == NULL) { return "invalid raw view output pointer"; }
  *output = NULL;

  RADCOBJ_Header header;
  const char *error = radcobj_validate_header(data, size, &header);
  if (error) {
    return error;
  }
  if (raw_offset > header.compressor.raw_size || raw_size > header.compressor.raw_size - raw_offset) {
    return "raw view range is out of bounds";
  }
  if (raw_size == 0) { return 0; }

  U64 first = raw_offset / header.compressor.segment_size;
  U64 last  = (raw_offset + raw_size - 1) / header.compressor.segment_size;
  error = radcobj_validate_segments(data, size, &header, first, last - first + 1);
  if (error) { return error; }

  U64 view_offset = 0;
  U64 previous_end = 0;
  for (U64 i = first; i <= last; i += 1) {
    RADCOBJ_Segment segment;
    MemoryCopy(&segment, (const U8 *)data + sizeof(header) + i * sizeof(segment), sizeof(segment));

    if ( ! (segment.flags & RADCOBJ_SegmentFlag_Uncompressed)) {
      return "raw view includes a compressed segment";
    }
    if (i != first && segment.offset != previous_end) {
      return "raw view segments are not contiguous";
    }
    if (i == first) {
      view_offset = segment.offset + raw_offset % header.compressor.segment_size;
    }

    previous_end = (U64)segment.offset + segment.size;
  }

  *output = (const U8 *)data + view_offset;
  return 0;
}

