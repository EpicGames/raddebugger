// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#pragma once

////////////////////////////////
// Debug Section

typedef CV_C13Stream CV_DebugS;

typedef struct CV_DebugH
{
  U64  count;
  U64 *v;
} CV_DebugH;

typedef struct CV_DebugT
{
  CV_LeafStream leaf_stream;
  U32Array      ipi_bits; // one bit per leaf: 0=TPI, 1=IPI; null v uses leaf-kind lookup

  // type server
  U64     source_counts [CV_TypeIndexSource_COUNT];
  U64     source_offsets[CV_TypeIndexSource_COUNT];
  U32     ti_base       [CV_TypeIndexSource_COUNT];
  Rng1U64 ti_ranges     [CV_TypeIndexSource_COUNT];

  // PCH
  B8             has_pch;
  CV_PrecompInfo precomp;
  Rng1U64        pch_ti_range[CV_TypeIndexSource_COUNT];
  U32            pch_obj_idx;
} CV_DebugT;

////////////////////////////////
// String Table Merge

typedef struct CV_StringTableRange CV_StringTableRange;
struct CV_StringTableRange
{
  CV_StringTableRange *next;
  Rng1U64              range;
  U64                  debug_s_idx;
};

typedef struct CV_StringBucket
{
  String8 string;
  union {
    struct {
      U32 idx0;
      U32 idx1;
    };
    U64 offset;
  } u;
} CV_StringBucket;

typedef struct CV_StringHashTable
{
  U64               total_string_size;
  U64               total_insert_count;
  U64               bucket_cap;
  CV_StringBucket **buckets;
} CV_StringHashTable;

typedef struct CV_StringHashTableResult
{
  U64               string_count;
  CV_StringBucket **buckets;
} CV_StringHashTableResult;

typedef struct CV_StringTableMergeCounts
{
  U64 string_size; // excludes null terminators
  U64 insert_count;
} CV_StringTableMergeCounts;

////////////////////////////////
// Symbol Deduper

typedef enum CV_SymbolInsertResult
{
  CV_SymbolInsertResult_Inserted,
  CV_SymbolInsertResult_Duplicate,
  CV_SymbolInsertResult_Full,
} CV_SymbolInsertResult;

////////////////////////////////
// Type Merging

typedef U64 CV_LeafRef;

typedef struct
{
  U64         count;
  CV_LeafRef *v;
} CV_LeafRefArray;

#define CV_LEAF_REF_NULL    max_U64
#define CV_TYPE_SOURCE_NONE max_U32

typedef enum CV_TypeHashKind
{
  CV_TypeHashKind_BLAKE3,
  CV_TypeHashKind_XXHash,
} CV_TypeHashKind;

typedef enum CV_TypeSourceMappingKind
{
  CV_TypeSourceMapping_Local,    // CV_DebugT ranges and ordinary PCH metadata
  CV_TypeSourceMapping_Direct,   // TI is in target_source's ranges
  CV_TypeSourceMapping_Remapped, // original TI -> table entry -> target_source
} CV_TypeSourceMappingKind;

typedef struct CV_TypeSourceMapping
{
  CV_TypeSourceMappingKind kind;
  U32                      target_source;
  Rng1U64                  ti_ranges[CV_TypeIndexSource_COUNT];
  CV_TypeIndex            *ti_map; // indexed by TI - range.min - precompiled size
  U64                      ti_map_count;
  Rng1U64                  precompiled_ranges[CV_TypeIndexSource_COUNT];
  U32                      precompiled_source; // index of the PCH mapping
} CV_TypeSourceMapping;

typedef struct CV_TypeHashGroup
{
  U32Array sources;
  B32      deep;
} CV_TypeHashGroup;

typedef struct CV_TypeMergeInput
{
  U64                   count;
  CV_DebugT            *debug_t_arr;
  CV_DebugH            *debug_h_arr;
  U32Array             *is_leaf_discarded;
  CV_TypeSourceMapping *mappings; // null means all Local
  CV_TypeIndex          min_type_indices[CV_TypeIndexSource_COUNT]; // patch basic/complex thresholds

  CV_TypeHashKind   hash_kind;
  U64               hash_group_count;
  CV_TypeHashGroup *hash_groups;
  U32               seed_source;
} CV_TypeMergeInput;

typedef struct CV_AssignedTiHash
{
  U64           cap;
  CV_TypeIndex *ti_arr;
  CV_TypeIndex  min_ti;
  CV_LeafRef   *leaf_refs;
} CV_AssignedTiHash;

typedef enum CV_TypeMergeErrorKind
{
  CV_TypeMergeError_InvalidLeaf,
  CV_TypeMergeError_InvalidTypeIndex,
  CV_TypeMergeError_ForwardReference,
} CV_TypeMergeErrorKind;

typedef struct CV_TypeMergeError CV_TypeMergeError;
struct CV_TypeMergeError
{
  CV_TypeMergeError    *next;
  CV_TypeMergeErrorKind kind;
  CV_LeafRef            leaf_ref;
  CV_TypeIndex          ti;
  U64                   offset;
};

typedef struct CV_TypeMergeErrorList
{
  U64                count;
  CV_TypeMergeError *first;
  CV_TypeMergeError *last;
} CV_TypeMergeErrorList;

typedef struct CV_TypeMergeStats
{
  U64 unique_count;
  U64 capacity;
  U64 initial_capacity;
  U64 grow_count;
} CV_TypeMergeStats;

typedef enum CV_MergeTypeFlags
{
  CV_MergeTypeFlag_BuildSourceTiMap = (1 << 0),
  CV_MergeTypeFlag_ExportHashes     = (1 << 2),
} CV_MergeTypeFlags;

typedef struct CV_MergedTypes
{
  CV_TypeMergeInput input;

  CV_TypeIndex      min_type_indices[CV_TypeIndexSource_COUNT];
  U64               count           [CV_TypeIndexSource_COUNT];
  CV_AssignedTiHash assigned_ti_arr [CV_TypeIndexSource_COUNT];
  CV_LeafRefArray   leaf_refs       [CV_TypeIndexSource_COUNT];

  U64           *hashes[CV_TypeIndexSource_COUNT];
  CV_TypeIndex **source_ti_maps; // [input.count][source leaf count], optional

  CV_TypeMergeStats     stats[CV_TypeIndexSource_COUNT];
  CV_TypeMergeErrorList errors; // nodes owned by result arena, no scratch pointers
} CV_MergedTypes;

////////////////////////////////
// Patcher

typedef enum CV_TypeIndexPatchStatus
{
  CV_TypeIndexPatchStatus_Ok,
  CV_TypeIndexPatchStatus_InvalidTypeIndex,
  CV_TypeIndexPatchStatus_UnassignedTypeIndex,
} CV_TypeIndexPatchStatus;

typedef struct CV_TypeIndexPatchResult
{
  CV_TypeIndexPatchStatus status;
  CV_TypeIndexInfo       *info; // failing entry; null on success
} CV_TypeIndexPatchResult;

typedef enum CV_ChecksumPatchStatus
{
  CV_ChecksumPatchStatus_Ok,
  CV_ChecksumPatchStatus_InvalidRecord,
  CV_ChecksumPatchStatus_InvalidName,
  CV_ChecksumPatchStatus_MissingString,
  CV_ChecksumPatchStatus_OffsetOverflow,
} CV_ChecksumPatchStatus;

typedef struct CV_ChecksumPatchResult
{
  CV_ChecksumPatchStatus status;
  U64                    offset;   // failing record's byte offset in raw checksums
  U32                    name_off; // original name offset; zero for InvalidRecord
} CV_ChecksumPatchResult;

////////////////////////////////
// Debug Sections

internal B32                cv_debug_t_from_data_checked(Arena *arena, String8 data, U64 align, U32 *offsets, U64 *source_counts, CV_DebugT *out);
internal CV_DebugT          cv_debug_t_from_data(Arena *arena, String8 data, U64 align);
internal B32                cv_debug_t_get_leaf_checked(CV_DebugT *debug_t, U64 leaf_idx, CV_Leaf *leaf_out);
internal CV_Leaf            cv_debug_t_get_leaf(CV_DebugT *debug_t, U64 leaf_idx);
internal CV_TypeIndexSource cv_type_index_source_from_debug_t(CV_DebugT *debug_t, U64 leaf_idx);
internal U64                cv_leaf_idx_from_ti(CV_DebugT *debug_t, CV_TypeIndexSource source, CV_TypeIndex ti);
internal CV_TypeIndex       cv_ti_from_leaf_idx(CV_DebugT *debug_t, CV_TypeIndexSource source, U64 leaf_idx);
internal CV_Leaf            cv_debug_t_get_leaf_from_ti(CV_DebugT *debug_t, CV_TypeIndexSource source, CV_TypeIndex ti);
internal String8            cv_debug_t_get_raw_leaf(CV_DebugT *debug_t, U64 leaf_idx);
internal CV_LeafHeader *    cv_debug_t_get_leaf_header(CV_DebugT *debug_t, U64 leaf_idx);
internal CV_TypeIndex       cv_debug_t_get_type_index(CV_DebugT *debug_t, CV_TypeIndexSource ti_source, U64 leaf_idx);
internal U64                cv_debug_t_get_leaf_index(CV_DebugT *debug_t, CV_TypeIndexSource ti_source, CV_TypeIndex ti);
internal B32                cv_debug_t_is_pch(CV_DebugT *debug_t);
internal B32                cv_debug_t_is_type_server_ref(CV_DebugT *debug_t);

internal void           cv_symbol_list_push_node(CV_SymbolList *list, CV_SymbolNode *node);
internal CV_SymbolNode* cv_symbol_list_push(Arena *arena, CV_SymbolList *list, CV_Symbol v);

////////////////////////////////
// String Table Merge

internal U64                cv_string_hash_table_hash(String8 string);
internal int                cv_string_bucket_is_before(void *raw_a, void *raw_b);
internal CV_StringBucket *  cv_string_hash_table_insert_or_update(CV_StringBucket **buckets, U64 cap, U64 hash, CV_StringBucket *new_bucket);

internal CV_StringTableMergeCounts cv_merge_string_table(Arena *arena, String8 table, U64 source_ordinal, CV_StringBucket **buckets, U64 bucket_cap);
internal CV_StringHashTable        cv_dedup_string_tables(TP_Arena *arena, TP_Context *tp, U64 count, CV_DebugS *arr);
internal void                      cv_string_hash_table_assign_buffer_offsets(TP_Context *tp, CV_StringHashTable string_ht);
internal CV_StringBucket *         cv_string_hash_table_lookup(CV_StringHashTable ht, String8 string);

internal void    cv_pack_string_hash_table_range(String8 buffer, CV_StringHashTable string_ht, Rng1U64 range);
internal void    cv_pack_string_hash_table_into(TP_Context *tp, CV_StringHashTable string_ht, String8 buffer);
internal String8 cv_pack_string_hash_table(Arena *arena, TP_Context *tp, CV_StringHashTable string_ht);


////////////////////////////////
// Symbol Deduper

// Bucket capacity is nonzero; raw symbols are complete records.
internal CV_SymbolInsertResult cv_symbol_deduper_insert_only(void **buckets, U64 bucket_cap, String8 raw);
internal void * cv_symbol_deduper_insert_or_update(void **buckets, U64 bucket_cap, U64 hash, void *symbol_ptr);

////////////////////////////////
// Type Merge

internal CV_LeafRef   cv_leaf_ref_make(U64 source_ordinal, U64 leaf_idx);
internal U32          cv_leaf_ref_source(CV_LeafRef ref);
internal U32          cv_leaf_ref_leaf_idx(CV_LeafRef ref);
internal int          cv_leaf_ref_compare(CV_LeafRef a, CV_LeafRef b);
internal B32          cv_leaf_ref_from_ti(CV_TypeMergeInput *input, U32 source_ordinal, CV_TypeIndexSource source, CV_TypeIndex ti, CV_LeafRef *ref_out);
internal U64          cv_hash_from_leaf_ref(CV_TypeMergeInput *input, CV_LeafRef leaf_ref);
internal B32          cv_match_leaf_ref(CV_TypeMergeInput *input, CV_LeafRef a, CV_LeafRef b);
internal U64          cv_hash_leaf(Arena *error_arena, CV_TypeMergeErrorList *errors, CV_TypeMergeInput *input, CV_LeafRef leaf_ref, CV_TypeIndexInfoList ti_info_list, B32 discard_cycles);
internal void         cv_hash_leaf_deep(Arena *arena, Arena *error_arena, CV_TypeMergeErrorList *errors, CV_TypeMergeInput *input, CV_LeafRef root_leaf_ref, CV_TypeIndexInfoList root_ti_info_list);
internal CV_TypeIndex cv_assigned_ti_hash_search(CV_AssignedTiHash *ht, CV_TypeMergeInput *input, CV_LeafRef leaf_ref);

internal CV_MergedTypes cv_merge_types(TP_Context *tp, Arena *arena, CV_TypeMergeInput *input, CV_MergeTypeFlags merge_flags);

////////////////////////////////
// Patcher

internal CV_TypeIndexPatchResult cv_patch_type_indices(CV_MergedTypes *types, U32 source_ordinal, String8 data, CV_TypeIndexInfoList indices);
internal CV_ChecksumPatchResult cv_c13_patch_string_offsets_in_checksum_data(String8 checksums, String8 string_data, U64 string_data_base_offset, CV_StringHashTable string_ht);
internal B32  cv_patch_symbol_tree_offsets(String8 raw_symbols, U64 base_offset, U64 align, U64 *tree_size_out);
internal void cv_c13_patch_string_offsets_in_checksum_list(CV_ChecksumList checksum_list, String8 string_data, U64 string_data_base_offset, CV_StringHashTable string_ht);
internal void cv_c13_patch_checksum_offsets_in_line_data_list(String8List line_data, U64 checksum_rebase);
internal void cv_c13_patch_checksum_offsets_in_frame_data_list(String8List frame_data, U32 checksum_rebase);
