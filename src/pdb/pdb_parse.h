// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef PDB_PARSE_H
#define PDB_PARSE_H

////////////////////////////////
//~ PDB Hash Table Types

typedef struct PDB_HashTableBucket
{
  String8 key;
  String8 value;
  U32 key_offset;
  U32 insert_idx;
} PDB_HashTableBucket;

typedef struct PDB_HashTable
{
  Arena *arena;
  PDB_HashTableBucket *bucket_arr;
  U32Array present_bits;
  U32Array deleted_bits;
  U32 max;
  U32 count;
} PDB_HashTable;

typedef struct PDB_HashTableEntries
{
  PDB_HashTableBucket *v;
  U32 count;
  U32 max;
  U32Array present_bits;
  U32Array deleted_bits;
  U64 read_bytes;
} PDB_HashTableEntries;

// Unpack callbacks return nonzero on failure.
#define PDB_HASH_TABLE_UNPACK_FUNC(name) B32 name(void *ud, String8 local_data, String8 key_value_data, U64 *key_value_cursor, String8 *key_out, String8 *value_out)
typedef PDB_HASH_TABLE_UNPACK_FUNC(PDB_HashTableUnpackFunc);

typedef enum
{
  PDB_HashTableParseError_OK,
  PDB_HashTableParseError_OUT_OF_BYTES,
  PDB_HashTableParseError_CORRUPTED
} PDB_HashTableParseError;

////////////////////////////////
//~ PDB Parser String Table Types

typedef struct PDB_Strtbl
{
  String8 data;
  U32 bucket_count;
  U32 strblock_min;
  U32 strblock_max;
  U32 buckets_min;
  U32 buckets_max;
} PDB_Strtbl;

////////////////////////////////
//~ PDB Parser Info Types

typedef enum PDB_NamedStream
{
  PDB_NamedStream_HeaderBlock,
  PDB_NamedStream_StringTable,
  PDB_NamedStream_LinkInfo,
  PDB_NamedStream_Count
} PDB_NamedStream;

typedef struct PDB_NamedStreamTable
{
  MSF_StreamNumber sn[PDB_NamedStream_Count];
} PDB_NamedStreamTable;

typedef struct PDB_InfoNode
{
  struct PDB_InfoNode *next;
  String8 string;
  MSF_StreamNumber sn;
} PDB_InfoNode;

typedef struct PDB_Info
{
  PDB_InfoNode *first;
  PDB_InfoNode *last;
  Guid auth_guid;
  PDB_FeatureFlags features;
} PDB_Info;

typedef struct PDB_InfoHeader
{
  PDB_InfoVersion version;
  U32 time;
  U32 age;
} PDB_InfoHeader;

typedef struct PDB_InfoParse
{
  PDB_InfoVersion version;
  COFF_TimeStamp time_stamp;
  U32 age;
  Guid guid;
  String8 extra_info;
} PDB_InfoParse;

////////////////////////////////
//~ PDB Parser DBI Types

//  (this is not "literally" defined by the format - but helpful to have)
typedef enum PDB_DbiRange
{
  PDB_DbiRange_ModuleInfo,
  PDB_DbiRange_SecCon,
  PDB_DbiRange_SecMap,
  PDB_DbiRange_FileInfo,
  PDB_DbiRange_TSM,
  PDB_DbiRange_EcInfo,
  PDB_DbiRange_DbgHeader,
  PDB_DbiRange_COUNT
} PDB_DbiRange;

//  (this is not "literally" defined by the format - but helpful to have)
typedef enum
{
  PDB_DbiCompUnitRange_Symbols,
  PDB_DbiCompUnitRange_C11,
  PDB_DbiCompUnitRange_C13,
  PDB_DbiCompUnitRange_COUNT
} PDB_DbiCompUnitRange;

typedef struct PDB_DbiParsed
{
  String8 data;
  COFF_MachineType machine_type;
  MSF_StreamNumber gsi_sn;
  MSF_StreamNumber psi_sn;
  MSF_StreamNumber sym_sn;
  U64 range_off[(U64)(PDB_DbiRange_COUNT) + 1];
  MSF_StreamNumber dbg_streams[PDB_DbiStream_COUNT];
} PDB_DbiParsed;

typedef struct PDB_CompUnit
{
  MSF_StreamNumber sn;
  U32 range_off[(U32)(PDB_DbiCompUnitRange_COUNT) + 1];
  
  String8 obj_name;
  String8 group_name;
} PDB_CompUnit;

typedef struct PDB_CompUnitNode
{
  struct PDB_CompUnitNode *next;
  PDB_CompUnit unit;
} PDB_CompUnitNode;

typedef struct PDB_CompUnitArray
{
  PDB_CompUnit **units;
  U64 count;
} PDB_CompUnitArray;

typedef struct PDB_CompUnitContribution
{
  U32 mod;
  U64 voff_first;
  U64 voff_opl;
} PDB_CompUnitContribution;

typedef struct PDB_CompUnitContributionArray
{
  PDB_CompUnitContribution *contributions;
  U64 count;
} PDB_CompUnitContributionArray;


////////////////////////////////
//~ PDB Parser TPI/IPI Types

typedef struct PDB_HashAdj
{
  U32 name_off;
  CV_TypeId type_id;
} PDB_HashAdj;

typedef struct PDB_HashAdjArray
{
  U64 count;
  PDB_HashAdj *v;
} PDB_HashAdjArray;

typedef struct PDB_TpiParsed
{
  String8 data;
  
  // leaf info
  U64 leaf_first;
  U64 leaf_opl;
  U32 itype_first;
  U32 itype_opl;
  
  // hash info
  MSF_StreamNumber hash_sn;
  MSF_StreamNumber hash_sn_aux;
  U32 hash_key_size;
  U32 hash_bucket_count;
  U32 hash_vals_off;
  U32 hash_vals_size;
  U32 itype_off;
  U32 itype_size;
  U32 hash_adj_off;
  U32 hash_adj_size;
  
} PDB_TpiParsed;

typedef struct PDB_TpiHashBlock
{
  struct PDB_TpiHashBlock *next;
  U32 local_count;
  CV_TypeId itypes[13]; // 13 = (64 - 12)/4
} PDB_TpiHashBlock;

typedef struct PDB_TpiHashParsed
{
  String8 data;
  PDB_TpiHashBlock **buckets;
  U32 bucket_count;
  U32 bucket_mask;
} PDB_TpiHashParsed;

////////////////////////////////
//~ PDB Parser GSI Types

typedef struct PDB_GsiBucket
{
  U32 *offs;
  U64 count;
} PDB_GsiBucket;

typedef struct PDB_GsiParsed
{
  PDB_GsiBucket buckets[4096];
} PDB_GsiParsed;

////////////////////////////////
//~ rjf: Globals

read_only global PDB_CompUnit pdb_comp_unit_nil = {0};

////////////////////////////////
//~ PDB Parser Functions

internal U64 pdb_read_bit_vector_string(String8 data, U64 offset, U32Array *bits_out);
internal U64 pdb_hash_table_compute_load_factor(U64 count);
internal void pdb_hash_table_release(PDB_HashTable *ht);
internal U32 pdb_hash_table_hash(String8 key);
internal B32 pdb_hash_table_get(PDB_HashTable *ht, String8 key, String8 *value_out);
internal B32 pdb_hash_table_is_present(PDB_HashTable *ht, U32 k);
internal B32 pdb_hash_table_is_deleted(PDB_HashTable *ht, U32 k);
internal PDB_HashTableBucket **pdb_hash_table_get_present_buckets(Arena *arena, PDB_HashTable *ht);
internal void pdb_hash_table_get_present_keys_and_values(Arena *arena, PDB_HashTable *ht, String8Array *keys_out, String8Array *values_out);
internal PDB_HashTableParseError pdb_hash_table_entries_from_data(Arena *arena, String8 data, B32 has_local_data, PDB_HashTableUnpackFunc *unpack_func, void *unpack_ud, PDB_HashTableEntries *entries_out);
internal PDB_HashTableParseError pdb_hash_table_from_data(PDB_HashTable *ht, String8 data, B32 has_local_data, PDB_HashTableUnpackFunc *unpack_func, void *unpack_ud, U64 *read_bytes_out);
internal PDB_HashTableParseError pdb_named_stream_ht_from_data(PDB_HashTable *ht, String8 data, U64 *read_bytes_out);
internal PDB_HASH_TABLE_UNPACK_FUNC(pdb_hash_adj_raw_ht_unpack);
internal PDB_HashAdjArray pdb_hash_adj_array_from_data(Arena *arena, String8 data);
internal void pdb_info_parse_from_data(String8 data, PDB_InfoParse *parse_out);
internal B32 pdb_tpi_header_from_data(String8 data, PDB_TpiHeader *header_out);
internal B32 pdb_extract_type_server_info(String8 raw_msf, MSF_RawStreamTable *st, MSF_StreamNumber sn, Rng1U64 *ti_range_out, Rng1U64 *leaf_range_out);

internal PDB_Info*            pdb_info_from_data(Arena *arena, String8 pdb_info_data);
internal PDB_NamedStreamTable*pdb_named_stream_table_from_info(Arena *arena, PDB_Info *info);
internal PDB_Strtbl*          pdb_strtbl_from_data(Arena *arena, String8 strtbl_data);

internal PDB_DbiParsed*       pdb_dbi_from_data(Arena *arena, String8 dbi_data);
internal PDB_TpiParsed*       pdb_tpi_from_data(Arena *arena, String8 tpi_data);
internal PDB_TpiHashParsed*   pdb_tpi_hash_from_data(Arena *arena, PDB_Strtbl *strtbl, PDB_TpiParsed *tpi, String8 tpi_hash_data);
internal PDB_GsiParsed*       pdb_gsi_from_data(Arena *arena, String8 gsi_data);
internal U64                  pdb_gsi_symbol_from_string(PDB_GsiParsed *gsi, String8 symbol_data, String8 string);

internal COFF_SectionHeaderArray pdb_coff_section_array_from_data(Arena *arena, String8 section_data);

internal PDB_CompUnitArray*   pdb_comp_unit_array_from_data(Arena *arena,
                                                            String8 module_info_data);

internal PDB_CompUnitContributionArray pdb_comp_unit_contribution_array_from_data(Arena *arena, String8 seccontrib_data, COFF_SectionHeaderArray sections);
internal PDB_CompUnitContribution *pdb_comp_unit_contribution_from_voff__binary_search(PDB_CompUnitContributionArray *array, U64 voff);

////////////////////////////////
//~ PDB Dbi Functions

internal String8              pdb_data_from_dbi_range(PDB_DbiParsed *dbi, PDB_DbiRange range);
internal String8              pdb_data_from_unit_range(MSF_Parsed *msf, PDB_CompUnit *unit,
                                                       PDB_DbiCompUnitRange range);

////////////////////////////////
//~ PDB Tpi Functions

internal String8              pdb_leaf_data_from_tpi(PDB_TpiParsed *tpi);

internal CV_TypeIdArray       pdb_tpi_itypes_from_name(Arena *arena,
                                                       PDB_TpiHashParsed *tpi_hash,
                                                       CV_LeafStream *tpi_leaf,
                                                       CV_TypeId itype_first,
                                                       String8 name,
                                                       B32 compare_unique_name,
                                                       U32 output_cap);

internal CV_TypeId            pdb_tpi_first_itype_from_name(PDB_TpiHashParsed *tpi_hash,
                                                            CV_LeafStream *tpi_leaf,
                                                            CV_TypeId itype_first,
                                                            String8 name,
                                                            B32 compare_unique_name);

////////////////////////////////
//~ PDB Strtbl Functions

internal String8              pdb_strtbl_string_from_off(PDB_Strtbl *strtbl, U32 off);
internal String8              pdb_strtbl_string_from_index(PDB_Strtbl *strtbl,
                                                           PDB_StringIndex idx);
internal U32                  pdb_strtbl_off_from_string(PDB_Strtbl *strtbl, String8 string);

////////////////////////////////
//~ rjf: Thin Lookup Fast Paths

internal B32 pdb_has_symbol_ref(String8 msf_data, String8List symbol_list, MSF_RawStreamTable *st);
internal B32 pdb_has_file_ref(String8 msf_data, String8List file_list, MSF_RawStreamTable *st);
internal B32 pdb_has_symbol_or_file_ref(String8 msf_data, String8List symbol_list, String8List file_list);

#endif // PDB_PARSE_H
