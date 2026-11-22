// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef CODEVIEW_PARSE_H
#define CODEVIEW_PARSE_H

////////////////////////////////
// Parser Types

typedef struct CV_Record
{
  CV_RecHeader header;
  String8      data;
  U64          consumed;
} CV_Record;

typedef struct CV_Numeric
{
  CV_NumericKind kind;
  U8            *val;
  U64            encoded_size;
} CV_Numeric;

typedef struct CV_TypeIdArray
{
  CV_TypeId *v;
  U64        count;
} CV_TypeIdArray;

typedef struct CV_C13SubSection
{
  CV_C13SubSectionKind kind;
  U32                  declared_size;
  U64                  data_offset;
  String8              data;
  U64                  next_offset;
  B32                  truncated;
} CV_C13SubSection;

typedef struct CV_C13FileInfo
{
  String8            name;
  CV_C13ChecksumKind checksum_kind;
  String8            checksum;
} CV_C13FileInfo;

typedef struct CV_C13InlineeLinesParsed
{
  CV_ItemId          inlinee;
  U32                file_off;
  String8            file_name;
  CV_C13ChecksumKind checksum_kind;
  String8            checksum;
  U32                first_source_ln;
  U32                extra_file_count;
  U32               *extra_files;
} CV_C13InlineeLinesParsed;

////////////////////////////////
// Record Range

typedef struct CV_RecRange
{
  U32          off;
  CV_RecHeader hdr;
} CV_RecRange;

////////////////////////////////
// C13 Stream

typedef U32 CV_C13SubSectionIdxKind;
enum
{
  CV_C13SubSectionIdxKind_NULL,
#define X(N,c) CV_C13SubSectionIdxKind_##N,
  CV_C13SubSectionKindXList(X)
#undef X
  CV_C13SubSectionIdxKind_COUNT
};

typedef struct CV_C13StreamStats
{
  U64 size[CV_C13SubSectionIdxKind_COUNT];
  U64 count[CV_C13SubSectionIdxKind_COUNT];
  U64 framed_size[CV_C13SubSectionIdxKind_COUNT]; // individual headers + standard C13 padding
  U64 lines_size;                                 // same as framed_size[Lines], including empty subsections
} CV_C13StreamStats;

typedef struct CV_C13StreamSubsection
{
  CV_C13SubSectionKind kind;
  U32                  data_offset;
  U32                  data_size;
} CV_C13StreamSubsection;

typedef struct CV_C13StreamBlockIndex
{
  U32 subsection_base_idx; // max_U32 means raw parsing; count==0 can be cached
  U32 subsection_count;
} CV_C13StreamBlockIndex;

typedef struct CV_C13StreamIndex
{
  String8                 subsections;
  U64                     block_count;
  CV_C13StreamBlockIndex *blocks;
  String8Node            *last_block;
  CV_C13StreamStats       stats;
  String8                 string_table;
  String8                 file_chksms;
} CV_C13StreamIndex;

typedef struct CV_C13Stream
{
  String8List        data; // complete subsections, headers and padding, no signature
  U32                ignored_kinds;
  CV_C13StreamIndex *index;
} CV_C13Stream;

////////////////////////////////
// Symbol and Leaf Stream Results

typedef struct CV_SymTopLevelInfo
{
  CV_Arch     arch;
  CV_Language language;
  String8     compiler_name;
} CV_SymTopLevelInfo;

typedef struct CV_SymbolStream
{
  // source information
  String8 data;
  U64     sym_align;

  // physical framed records in the valid prefix, including short known kinds
  U64 count;

  // top-level info derived from the syms
  CV_SymTopLevelInfo info;
} CV_SymbolStream;

typedef struct CV_LeafStream
{
  String8  data;
  U64      count;
  U32     *offsets;
  U64      align;
} CV_LeafStream;

typedef struct CV_SymbolNode CV_SymbolNode;
struct CV_SymbolNode
{
  CV_SymbolNode *next;
  CV_SymbolNode *prev;
  CV_Symbol      data;
};

typedef struct CV_SymbolList
{
  U64            count;
  CV_Signature   signature;
  CV_SymbolNode *first;
  CV_SymbolNode *last;
} CV_SymbolList;

////////////////////////////////
// Iterators

typedef enum
{
  CV_RecordKind_Null,
  CV_RecordKind_Leaf,
  CV_RecordKind_Symbol,
} CV_RecordKind;

typedef struct CV_RecIter
{
  String8       data;
  CV_RecordKind kind;
  U64           cursor;
  U64           align;
} CV_RecIter;

typedef struct CV_FieldIter
{
  U64         next_offset;
  B32         malformed;
  CV_LeafKind kind;
  String8     data;
  String8     name;
  CV_Numeric  numeric[2];
  U32         numeric_count;
  U32         virtual_offset;
  U32         type_offset_count;
  U64         type_offsets[2];
} CV_FieldIter;

typedef struct CV_C13LinesIter
{
  String8                 data;
  CV_C13SubSecLinesHeader header;
  CV_C13File              file;
  U64                     cursor;
  U64                     file_offset;
  U64                     line_array_off;
  U64                     col_array_off;
  U64                     col_count; // column pairs (one per line)
  B32                     malformed;
} CV_C13LinesIter;

typedef struct CV_C13InlineeIter
{
  String8                  data;
  CV_C13InlineeLinesSig    signature;
  U64                      cursor;
  U64                      record_offset;
  CV_C13InlineeLinesParsed v;
  B32                      malformed;
} CV_C13InlineeIter;

typedef struct CV_C13StreamIter
{
  CV_C13Stream        *stream;
  String8Node         *next_block;
  String8              block;
  U64                  cursor;
  B32                  malformed;
  CV_C13SubSectionKind kind;
  String8              data;
  U64                  data_offset; // relative to the original block, for source relocations
  CV_C13StreamIndex   *index;
  U64                  block_idx;
  U64                  subsection_idx;
  U64                  subsection_opl;
  B32                  cached_block;
} CV_C13StreamIter;

typedef struct CV_C13InlineSiteDecoder
{
  U64                cursor;
  U64                parent_voff;
  U64Array           separated_code_voffs;
  U64                code_offset_base;
  B32                code_chunk_changed;
  B32                error;
  CV_InlineRangeKind range_kind;
  U32                code_length;
  U32                code_offset;
  U32                file_off;
  S32                ln;
  S32                cn;
  U64                code_offset_lo;
  B32                code_offset_changed;
  B32                code_offset_lo_changed;
  B32                code_length_changed;
  B32                ln_changed;
  B32                file_off_changed;
  Rng1U64            last_range;
  U32                file_count;
  Rng1U64            file_last_range;
  U64                file_line_count;
  U64                file_last_ln;
} CV_C13InlineSiteDecoder;

typedef U32 CV_C13InlineSiteDecoderStepFlags;
enum
{
  CV_C13InlineSiteDecoderStepFlag_EmitRange       = (1 << 0),
  CV_C13InlineSiteDecoderStepFlag_ExtendLastRange = (1 << 1),
  CV_C13InlineSiteDecoderStepFlag_EmitFile        = (1 << 2),
  CV_C13InlineSiteDecoderStepFlag_EmitLine        = (1 << 3),
  CV_C13InlineSiteDecoderStepFlag_Error           = (1 << 4),
};

typedef struct CV_C13InlineSiteDecoderStep
{
  CV_C13InlineSiteDecoderStepFlags flags;
  Rng1U64                          range;
  U64                              line_voff;
  U64                              line_voff_end;
  U64                              ln;
  U64                              cn;
  U32                              file_off;
} CV_C13InlineSiteDecoderStep;

////////////////////////////////
// C13 Parse

typedef struct CV_C13LinesParsed
{
  // raw info
  U32 sec_idx;
  U32 file_off;
  U64 secrel_base_off;

  // parsed info
  String8            file_name;
  CV_C13ChecksumKind checksum_kind;
  String8            checksum;
  U64               *voffs;     // [line_count + 1]
  U32               *line_nums; // [line_count]
  U16               *col_nums;  // [2*line_count]
  U32                line_count;
} CV_C13LinesParsed;

typedef struct CV_C13LinesParsedNode CV_C13LinesParsedNode;
struct CV_C13LinesParsedNode
{
  CV_C13LinesParsedNode *next;
  CV_C13LinesParsed      v;
};

typedef struct CV_C13InlineeLinesParsedNode CV_C13InlineeLinesParsedNode;
struct CV_C13InlineeLinesParsedNode
{
  CV_C13InlineeLinesParsedNode *next;
  CV_C13InlineeLinesParsed      v;
};

typedef struct CV_C13InlineeLinesParsedList
{
  CV_C13InlineeLinesParsedNode *first;
  CV_C13InlineeLinesParsedNode *last;
  U64                           count;
} CV_C13InlineeLinesParsedList;

typedef struct CV_Checksum
{
  CV_C13Checksum *header; // borrowed, allows adapter-owned in-place patching
  String8         value;
} CV_Checksum;

typedef struct CV_ChecksumNode
{
  struct CV_ChecksumNode *next;
  CV_Checksum             data;
} CV_ChecksumNode;

typedef struct CV_ChecksumList
{
  U64              count;
  CV_ChecksumNode *first;
  CV_ChecksumNode *last;
} CV_ChecksumList;

typedef struct CV_LineArray
{
  U32  file_off;
  U64  line_count;
  U64  col_count; // column pairs, one per line when present
  U64 *voffs;     // [line_count + 1]
  U32 *line_nums; // [line_count]
  U16 *col_nums;  // [line_count * 2]
} CV_LineArray;

typedef struct CV_File
{
  U32          file_off;
  CV_LineArray lines;
} CV_File;

typedef struct CV_C13LinesHeader
{
  U64 header_off;
  U64 sec_idx;
  U64 sec_off_lo;
  U64 sec_off_hi;
  U64 file_off;
  U64 line_count;
  U64 col_count;
  U64 line_array_off;
  U64 col_array_off;
} CV_C13LinesHeader;

typedef struct CV_C13LinesHeaderNode CV_C13LinesHeaderNode;
struct CV_C13LinesHeaderNode
{
  CV_C13LinesHeaderNode *next;
  CV_C13LinesHeader      v;
};

typedef struct CV_C13LinesHeaderList
{
  CV_C13LinesHeaderNode *first;
  CV_C13LinesHeaderNode *last;
  U64                    count;
} CV_C13LinesHeaderList;

typedef struct CV_InlineBinaryAnnotsParsed
{
  U64           lines_count;
  CV_LineArray *lines;
  Rng1U64List   code_ranges;
} CV_InlineBinaryAnnotsParsed;

////////////////////////////////
// Accelerators

typedef struct CV_Line
{
  U64 voff;
  U32 file_off;
  U32 line_num;
  U16 col_num;
} CV_Line;

typedef struct CV_LinesAccel
{
  U64      map_count;
  CV_Line *map;
} CV_LinesAccel;

typedef struct CV_InlineeLinesAccel
{
  U64                        bucket_count;
  U64                        bucket_max;
  CV_C13InlineeLinesParsed **buckets;
} CV_InlineeLinesAccel;

////////////////////////////////
// Helpers

internal U64 cv_hash_from_item_id(CV_ItemId item_id);

// C13 Subsection Kinds
internal CV_C13SubSectionKind    cv_c13_sub_section_kind_from_idx(CV_C13SubSectionIdxKind idx);
internal CV_C13SubSectionIdxKind cv_c13_sub_section_idx_from_kind(CV_C13SubSectionKind kind);

////////////////////////////////
// Parsers

// numeric
internal CV_Numeric cv_numeric_from_data_range(U8 *first, U8 *opl);
internal U64        cv_read_numeric(String8 data, U64 offset, CV_Numeric *out);
internal B32 cv_numeric_fits_in_u64(CV_Numeric *num);
internal B32 cv_numeric_fits_in_s64(CV_Numeric *num);
internal B32 cv_numeric_fits_in_f64(CV_Numeric *num);
internal U64 cv_u64_from_numeric(CV_Numeric *num);
internal S64 cv_s64_from_numeric(CV_Numeric *num);
internal F64 cv_f64_from_numeric(CV_Numeric *num);

// leaf/symbol record reader
internal B32 cv_read_record(String8 data, U64 offset, U64 align, CV_Record *out);

// $$Leaf
internal U64     cv_read_leaf(String8 data, U64 offset, U64 align, CV_Leaf *out);
internal CV_Leaf cv_leaf_from_string(String8 data);
internal CV_Leaf cv_leaf_from_ptr(U8 *ptr);
internal U64     cv_leaf_size_from_ptr(U8 *ptr);
internal String8 cv_raw_leaf_from_ptr(U8 *ptr);

// $$Symbol
internal U64       cv_read_symbol(String8 data, U64 offset, U64 align, CV_Symbol *out);
internal CV_Symbol cv_symbol_from_string(String8 data);
internal CV_Symbol cv_symbol_from_ptr(U8 *ptr);
internal String8   cv_raw_from_symbol(void *ptr);

// C13 raw helpers
internal B32          cv_c13_sub_section_from_data(String8 data, U64 offset, CV_C13SubSection *out);
internal B32          cv_c13_checksum_from_data(String8 data, U64 offset, CV_C13Checksum *header, String8 *value, U64 *next_offset);
internal B32          cv_c13_name_from_string_table(String8 data, U64 offset, String8 *out);
internal B32          cv_c13_file_info_from_checksum(String8 checksums, String8 strings, U64 offset, CV_C13FileInfo *out);
internal CV_Signature cv_c13_stream_signature_from_data(String8 data);

internal void cv_sym_top_level_info_from_symbol(Arena *arena, CV_Symbol symbol, CV_SymTopLevelInfo *info);

////////////////////////////////
// Iterators

internal CV_RecIter cv_record_iter_init_ex(String8 data, CV_RecordKind kind, U64 align);
internal CV_RecIter cv_record_iter_init(String8 data, CV_RecordKind kind);
internal B32        cv_record_iter_next(CV_RecIter *iter, CV_Record *record_out);

internal B32 cv_field_next(String8 data, CV_FieldIter *iter);
internal B32 cv_method_next(String8 data, CV_FieldIter *iter);

internal CV_C13LinesIter   cv_c13_lines_iter_init(String8 data);
internal B32               cv_c13_lines_iter_next(CV_C13LinesIter *it);
internal CV_C13InlineeIter cv_c13_inlinee_iter_init(String8 data);
internal B32               cv_c13_inlinee_iter_next(CV_C13InlineeIter *it);

internal CV_C13InlineSiteDecoder     cv_c13_inline_site_decoder_init(U32 file_off, U32 first_source_ln, U64 parent_voff);
internal CV_C13InlineSiteDecoderStep cv_c13_inline_site_decoder_step(CV_C13InlineSiteDecoder *decoder, String8 binary_annots);

internal CV_C13StreamIter cv_c13_stream_iter_init(CV_C13Stream *stream);
internal B32              cv_c13_stream_iter_next_block(CV_C13StreamIter *it);
internal B32              cv_c13_stream_iter_next_raw(CV_C13StreamIter *it);
internal B32              cv_c13_stream_iter_next(CV_C13StreamIter *it);
internal B32              cv_c13_stream_iter_next_kind(CV_C13StreamIter *it, CV_C13SubSectionKind kind);

////////////////////////////////
// Stream Builders

// symbol parse
internal CV_SymbolStream * cv_symbol_stream_from_data(Arena *arena, String8 sym_data, U64 sym_align);

// leaf parse
internal B32 cv_leaf_stream_from_data_checked(Arena *arena, String8 data, U64 align, CV_LeafStream *out);
internal B32 cv_make_leaf_stream_checked(String8 data, U64 align, U64 count, U32 *offsets, CV_LeafStream *out);

// C13 stream
internal B32          cv_c13_stream_from_data_c13_checked(Arena *arena, String8 data, CV_C13Stream *out);
internal CV_C13Stream cv_c13_stream_from_data_c13(Arena *arena, String8 data);
internal B32          cv_c13_stream_from_data_checked(Arena *arena, String8 data, CV_C13Stream *out);
internal CV_C13Stream cv_c13_stream_from_data(Arena *arena, String8 data);
internal void         cv_c13_stream_concat_in_place(CV_C13Stream *dst, CV_C13Stream *src);

// C13 parse
internal void                         cv_parse_checksum_data(Arena *arena, CV_ChecksumList *list, String8 checksum_data);
internal CV_ChecksumList              cv_c13_parse_checksum_data_list(Arena *arena, String8List checksum_data_list);
internal String8List                  cv_c13_collect_source_file_names(Arena *arena, CV_ChecksumList checksum_list, String8 string_data);
internal CV_C13LinesHeaderList        cv_c13_lines_from_sub_sections(Arena *arena, String8 c13_data, Rng1U64 ss_range);
internal CV_LineArray                 cv_c13_line_array_from_data(Arena *arena, String8 c13_data, U64 sec_base, CV_C13LinesHeader parsed_lines);
internal void                         cv_c13_lines_header_list_concat_in_place(CV_C13LinesHeaderList *list, CV_C13LinesHeaderList *to_concat);
internal CV_C13InlineeLinesParsedList cv_c13_inlinee_lines_from_sub_sections(Arena *arena, CV_C13Stream *stream);
internal CV_InlineBinaryAnnotsParsed  cv_c13_parse_inline_binary_annots(Arena *arena, U64 parent_voff, CV_C13InlineeLinesParsed *inlinee_parsed, String8 binary_annots, U64Array separated_code_voffs);

// accelerators
internal CV_LinesAccel *        cv_c13_make_lines_accel(Arena *arena, U64 lines_count, CV_LineArray *lines);
internal B32                    cv_c13_inlinee_lines_accel_push(CV_InlineeLinesAccel *accel, CV_C13InlineeLinesParsed *parsed);
internal CV_InlineeLinesAccel * cv_c13_make_inlinee_lines_accel(Arena *arena, CV_C13InlineeLinesParsedList inlinee_lines);

////////////////////////////////
// Lookups

internal B32         cv_leaf_stream_read_record(CV_LeafStream *stream, U64 ordinal, CV_Record *out);
internal B32         cv_leaf_stream_get_leaf(CV_LeafStream *stream, U64 ordinal, CV_Leaf *out);
internal CV_RecRange cv_leaf_stream_rec_range(CV_LeafStream *stream, U64 ordinal);

// C13 stream data and statistics
internal CV_C13StreamIndex * cv_c13_stream_index(CV_C13Stream *stream);
internal B32                 cv_c13_stream_find_first(CV_C13Stream *stream, CV_C13SubSectionKind kind, String8 *out);
internal CV_C13StreamStats   cv_c13_stream_stats(CV_C13Stream *stream);
internal String8             cv_c13_stream_string_table(CV_C13Stream *stream);
internal String8             cv_c13_stream_file_chksms(CV_C13Stream *stream);

// line and inlinee accelerators
internal U64                       cv_nearest_line(CV_Line *arr, U64 count, U64 value);
internal CV_Line*                  cv_line_from_voff(CV_LinesAccel *accel, U64 voff, U64 *out_line_count);
internal CV_C13InlineeLinesParsed* cv_c13_inlinee_lines_accel_find(CV_InlineeLinesAccel *accel, CV_ItemId inlinee);

////////////////////////////////
// C13 Sonstruction and Serialization

internal String8     cv_c13_stream_alloc_subsection(Arena *arena, CV_C13Stream *stream, CV_C13SubSectionKind kind, U64 size);
internal void        cv_c13_stream_ignore_kind(CV_C13Stream *stream, CV_C13SubSectionKind kind);
internal void        cv_c13_stream_stats_accumulate(CV_C13StreamStats *stats, CV_C13SubSectionKind kind, U64 size);
internal U64         cv_c13_stream_size(CV_C13Stream *stream);
internal String8List cv_c13_stream_data(Arena *arena, CV_C13Stream *stream, B32 write_sig);

#endif // CODEVIEW_PARSE_H

