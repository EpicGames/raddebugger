// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ PDB Hash Table Reading

internal U64
pdb_read_bit_vector_string(String8 data, U64 offset, U32Array *bits_out)
{
  MemoryZeroStruct(bits_out);
  U32 word_count = 0;
  if(offset > data.size || data.size - offset < sizeof(word_count))
  {
    return 0;
  }
  str8_deserial_read_struct(data, offset, &word_count);
  U64 word_data_size = (U64)word_count*sizeof(U32);
  if(word_data_size > data.size - offset - sizeof(word_count))
  {
    return 0;
  }
  bits_out->count = word_count;
  bits_out->v = (U32 *)(data.str + offset + sizeof(word_count));
  return sizeof(word_count) + word_data_size;
}

internal U64
pdb_hash_table_compute_load_factor(U64 count)
{
  // PDB/include/map.h:cdrLoadMax()
  return count*2/3 + 1;
}

internal void
pdb_hash_table_release(PDB_HashTable *ht)
{
  ProfBeginFunction();
  arena_release(ht->arena);
  MemoryZeroStruct(ht);
  ProfEnd();
}

internal U32
pdb_hash_table_hash(String8 key)
{
  return (U16)pdb_hash_v1(key);
}

internal B32
pdb_hash_table_get(PDB_HashTable *ht, String8 key, String8 *value_out)
{
  ProfBeginFunction();
  B32 is_get_ok = 0;
  U32 best_ibucket = pdb_hash_table_hash(key) % ht->max;
  U32 ibucket = best_ibucket;
  do {
    B32 is_present = pdb_hash_table_is_present(ht, ibucket);
    if (is_present) {
      PDB_HashTableBucket *bucket = &ht->bucket_arr[ibucket];
      B32 is_match = str8_match(bucket->key, key, 0);
      if (is_match) {
        *value_out = bucket->value;
        is_get_ok = 1;
        break;
      }
    } else {
      break;
    }
    ibucket = (ibucket + 1) % ht->max;
  } while (ibucket != best_ibucket);
  ProfEnd();
  return is_get_ok;
}

internal B32
pdb_hash_table_is_present(PDB_HashTable *ht, U32 k)
{
  Assert(k < ht->max);
  return bit_array_is_set32(ht->present_bits, k);
}

internal B32
pdb_hash_table_is_deleted(PDB_HashTable *ht, U32 k)
{
  Assert(k < ht->max);
  return bit_array_is_set32(ht->deleted_bits, k);
}

internal PDB_HashTableBucket **
pdb_hash_table_get_present_buckets(Arena *arena, PDB_HashTable *ht)
{
  U64 result_count = 0;
  PDB_HashTableBucket **result = push_array(arena, PDB_HashTableBucket *, ht->count);
  for EachIndex(bucket_idx, ht->max) {
    if (bit_array_is_set32(ht->present_bits, bucket_idx)) {
      PDB_HashTableBucket *bucket = &ht->bucket_arr[bucket_idx];
      Assert(result_count < ht->count);
      result[result_count++] = bucket;
    }
  }
  return result;
}

internal void
pdb_hash_table_get_present_keys_and_values(Arena *arena, PDB_HashTable *ht, String8Array *keys_out, String8Array *values_out)
{
  *keys_out   = str8_array_reserve(arena, ht->count);
  *values_out = str8_array_reserve(arena, ht->count);
  for EachIndex(bucket_idx, ht->max) {
    if (bit_array_is_set32(ht->present_bits, bucket_idx)) {
      PDB_HashTableBucket *bucket = &ht->bucket_arr[bucket_idx];
      Assert(keys_out->count < ht->count);
      keys_out->v[keys_out->count++] = bucket->key;
      values_out->v[values_out->count++] = bucket->value;
    }
  }
}

internal PDB_HashTableParseError
pdb_hash_table_entries_from_data(Arena *arena, String8 data, B32 has_local_data,
                                 PDB_HashTableUnpackFunc *unpack_func, void *unpack_ud,
                                 PDB_HashTableEntries *entries_out)
{
  U64 cursor = 0;
  String8 local_data = {0};
  if(has_local_data)
  {
    U32 local_data_size = 0;
    if(data.size < sizeof(local_data_size))
    {
      return PDB_HashTableParseError_OUT_OF_BYTES;
    }
    cursor += str8_deserial_read_struct(data, cursor, &local_data_size);
    if(local_data_size > data.size - cursor)
    {
      return PDB_HashTableParseError_OUT_OF_BYTES;
    }
    cursor += str8_deserial_read_block(data, cursor, local_data_size, &local_data);
  }

  U32 count = 0;
  U32 max = 0;
  if(data.size - cursor < sizeof(count) + sizeof(max))
  {
    return PDB_HashTableParseError_OUT_OF_BYTES;
  }
  cursor += str8_deserial_read_struct(data, cursor, &count);
  cursor += str8_deserial_read_struct(data, cursor, &max);
  U32Array present_bits = {0};
  U32Array deleted_bits = {0};
  U64 present_size = pdb_read_bit_vector_string(data, cursor, &present_bits);
  if(present_size == 0)
  {
    return PDB_HashTableParseError_OUT_OF_BYTES;
  }
  cursor += present_size;
  U64 deleted_size = pdb_read_bit_vector_string(data, cursor, &deleted_bits);
  if(deleted_size == 0)
  {
    return PDB_HashTableParseError_OUT_OF_BYTES;
  }
  cursor += deleted_size;

  U64 word_count = ((U64)max + 31)/32;
  if(count >= max || count >= pdb_hash_table_compute_load_factor(max) ||
     present_bits.count > word_count || deleted_bits.count > word_count)
  {
    return PDB_HashTableParseError_CORRUPTED;
  }

  // validate all bitmap words before allocating or invoking callbacks
  U64 present_count = 0;
  for(U64 i = 0; i < Max(present_bits.count, deleted_bits.count); i += 1)
  {
    U32 present = 0;
    U32 deleted = 0;
    if(i < present_bits.count) { MemoryCopy(&present, present_bits.v + i, sizeof(present)); }
    if(i < deleted_bits.count) { MemoryCopy(&deleted, deleted_bits.v + i, sizeof(deleted)); }
    if((present & deleted) != 0 ||
       (i + 1 == word_count && max%32 != 0 && ((present | deleted) >> (max%32)) != 0))
    {
      return PDB_HashTableParseError_CORRUPTED;
    }
    for(; present != 0; present &= present - 1) { present_count += 1; }
  }
  if(present_count != count)
  {
    return PDB_HashTableParseError_CORRUPTED;
  }

  // every successful unpack must consume at least one byte
  if(count > data.size - cursor)
  {
    return PDB_HashTableParseError_OUT_OF_BYTES;
  }

  PDB_HashTableEntries result = {0};
  result.max = max;
  result.count = count;
  result.present_bits = present_bits;
  result.deleted_bits = deleted_bits;
  if(count != 0) { result.v = push_array(arena, PDB_HashTableBucket, count); }

  for(U32 i = 0; i < count; i += 1)
  {
    PDB_HashTableBucket *entry = &result.v[i];
    U64 entry_start = cursor;
    if(unpack_func(unpack_ud, local_data, data, &cursor, &entry->key, &entry->value) ||
       cursor <= entry_start || cursor > data.size)
    {
      return PDB_HashTableParseError_CORRUPTED;
    }
    entry->insert_idx = i;
  }

  result.read_bytes = cursor;
  *entries_out = result;
  return PDB_HashTableParseError_OK;
}

internal PDB_HashTableParseError
pdb_hash_table_from_data(PDB_HashTable *ht, String8 data, B32 has_local_data,
                         PDB_HashTableUnpackFunc *unpack_func, void *unpack_ud,
                         U64 *read_bytes_out)
{
  Temp scratch = scratch_begin(0, 0);
  PDB_HashTableEntries entries = {0};
  PDB_HashTableParseError error = pdb_hash_table_entries_from_data(scratch.arena, data, has_local_data,
                                                                 unpack_func, unpack_ud, &entries);
  if(error == PDB_HashTableParseError_OK)
  {
    U64 word_count = ((U64)entries.max + 31)/32;
    U64 dense_size = (U64)entries.max*sizeof(PDB_HashTableBucket) + 2*word_count*sizeof(U32);

    // limit this reader to 256 MiB so a tiny sparse serialization cannot force enormous allocation
    if(dense_size > (256ull << 20))
    {
      error = PDB_HashTableParseError_CORRUPTED;
    }
    else
    {
      Arena *arena = arena_alloc();
      PDB_HashTable result = {0};
      result.arena = arena;
      result.max = entries.max;
      result.count = entries.count;
      result.bucket_arr = push_array(arena, PDB_HashTableBucket, entries.max);
      result.present_bits = bit_array_init32(arena, entries.max);
      result.deleted_bits = bit_array_init32(arena, entries.max);
      MemoryCopyTyped(result.present_bits.v, entries.present_bits.v, entries.present_bits.count);
      MemoryCopyTyped(result.deleted_bits.v, entries.deleted_bits.v, entries.deleted_bits.count);

      U32 entry_idx = 0;
      for(U64 i = 0; i < entries.present_bits.count; i += 1)
      {
        U32 present = result.present_bits.v[i];
        for(U32 bit = 0; present != 0; bit += 1, present >>= 1)
        {
          if(present & 1)
          {
            result.bucket_arr[i*32 + bit] = entries.v[entry_idx++];
          }
        }
      }
      *ht = result;
      if(read_bytes_out != 0) { *read_bytes_out = entries.read_bytes; }
    }
  }

  scratch_end(scratch);
  return error;
}

internal
PDB_HASH_TABLE_UNPACK_FUNC(pdb_hash_adj_raw_ht_unpack)
{
  U64 cursor = *key_value_cursor;
  if(cursor > key_value_data.size || key_value_data.size - cursor < sizeof(U32) + sizeof(CV_TypeId))
  {
    return 1;
  }
  *key_out = str8(key_value_data.str + cursor, sizeof(U32));
  *value_out = str8(key_value_data.str + cursor + sizeof(U32), sizeof(CV_TypeId));
  *key_value_cursor = cursor + sizeof(U32) + sizeof(CV_TypeId);
  return 0;
}

internal
PDB_HASH_TABLE_UNPACK_FUNC(pdb_named_stream_ht_unpack)
{
  String8 raw_name_off = {0};
  String8 stream_number = {0};
  // both named streams and adjustments serialize two U32s per entry
  if(pdb_hash_adj_raw_ht_unpack(0, local_data, key_value_data, key_value_cursor, &raw_name_off, &stream_number))
  {
    return 1;
  }
  U32 name_off = 0;
  MemoryCopy(&name_off, raw_name_off.str, sizeof(name_off));
  if(name_off >= local_data.size)
  {
    return 1;
  }
  String8 name = str8_cstring_capped(local_data.str + name_off, local_data.str + local_data.size);
  if(name.size == local_data.size - name_off)
  {
    return 1;
  }
  *key_out = name;
  *value_out = stream_number;
  return 0;
}

internal PDB_HashTableParseError
pdb_named_stream_ht_from_data(PDB_HashTable *ht, String8 data, U64 *read_bytes_out)
{
  return pdb_hash_table_from_data(ht, data, 1, pdb_named_stream_ht_unpack, 0, read_bytes_out);
}

internal PDB_HashAdjArray
pdb_hash_adj_array_from_data(Arena *arena, String8 data)
{
  PDB_HashAdjArray result = {0};
  Temp scratch = scratch_begin(&arena, 1);
  PDB_HashTableEntries entries = {0};
  if(pdb_hash_table_entries_from_data(scratch.arena, data, 0, pdb_hash_adj_raw_ht_unpack, 0, &entries) == PDB_HashTableParseError_OK)
  {
    if(entries.count != 0)
    {
      result.v = push_array_no_zero(arena, PDB_HashAdj, entries.count);

      // entries are serialized in ascending bucket order
      for EachIndex(i, entries.count)
      {
        PDB_HashAdj *adj = &result.v[result.count++];
        MemoryCopy(&adj->name_off, entries.v[i].key.str, sizeof(adj->name_off));
        MemoryCopy(&adj->type_id, entries.v[i].value.str, sizeof(adj->type_id));
      }
    }
  }
  scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ PDB Parser Functions

internal void
pdb_info_parse_from_data(String8 data, PDB_InfoParse *parse_out)
{
  MemoryZeroStruct(parse_out);
  PDB_InfoHeader header = {0};
  if(data.size < sizeof(header)) { return; }
  U64 cursor = str8_deserial_read_struct(data, 0, &header);
  B32 has_guid = 0;
  switch(header.version)
  {
    case PDB_InfoVersion_VC2:
    case PDB_InfoVersion_VC4:
    case PDB_InfoVersion_VC41:
    case PDB_InfoVersion_VC50:
    case PDB_InfoVersion_VC98: break;
    case PDB_InfoVersion_VC70_DEP:
    case PDB_InfoVersion_VC70:
    case PDB_InfoVersion_VC80:
    case PDB_InfoVersion_VC110:
    case PDB_InfoVersion_VC140: has_guid = 1; break;
    default: return;
  }
  Guid guid = {0};
  if(has_guid)
  {
    if(data.size - cursor < sizeof(guid)) { return; }
    cursor += str8_deserial_read_struct(data, cursor, &guid);
  }
  parse_out->version = header.version;
  parse_out->time_stamp = header.time;
  parse_out->age = header.age;
  parse_out->guid = guid;
  parse_out->extra_info = str8_skip(data, cursor);
}

internal PDB_Info*
pdb_info_from_data(Arena *arena, String8 data)
{
  ProfBegin("pdb_info_from_data");
  PDB_Info *result = push_array(arena, PDB_Info, 1);
  
  PDB_InfoParse info = {0};
  pdb_info_parse_from_data(data, &info);
  if(info.version != 0)
  {
    result->auth_guid = info.guid;
    Temp scratch = scratch_begin(&arena, 1);
    PDB_HashTableEntries entries = {0};
    if(pdb_hash_table_entries_from_data(scratch.arena, info.extra_info, 1, pdb_named_stream_ht_unpack, 0, &entries) == PDB_HashTableParseError_OK)
    {
      for(U64 i = 0; i < entries.count; i += 1)
      {
        PDB_HashTableBucket *entry = &entries.v[i];
        U32 sn = 0;
        MemoryCopy(&sn, entry->value.str, sizeof(sn));
        PDB_InfoNode *node = push_array(arena, PDB_InfoNode, 1);
        SLLQueuePush(result->first, result->last, node);
        node->string = entry->key;
        node->sn = (MSF_StreamNumber)sn;
      }

      U64 record_off = entries.read_bytes;
      for(; record_off + sizeof(PDB_FeatureSig) <= info.extra_info.size;)
      {
        PDB_FeatureSig sig = 0;
        record_off += str8_deserial_read_struct(info.extra_info, record_off, &sig);
        switch(sig)
        {
          case PDB_FeatureSig_NULL: break;
          case PDB_FeatureSig_VC140:              result->features |= PDB_FeatureFlag_HAS_ID_STREAM;    break;
          case PDB_FeatureSig_NO_TYPE_MERGE:      result->features |= PDB_FeatureFlag_NO_TYPE_MERGE;    break;
          case PDB_FeatureSig_MINIMAL_DEBUG_INFO: result->features |= PDB_FeatureFlag_MINIMAL_DBG_INFO; break;
        }
      }
    }
    scratch_end(scratch);
  }
  
  ProfEnd();
  return(result);
}

internal PDB_NamedStreamTable*
pdb_named_stream_table_from_info(Arena *arena, PDB_Info *info){
  ProfBegin("pdb_named_stream_table_from_info");
  
  // mapping "NamedStream" indexes to strings
  struct StreamNameIndexPair{
    PDB_NamedStream index;
    String8 name;
  };
  struct StreamNameIndexPair pairs[] = {
    {PDB_NamedStream_HeaderBlock, str8_lit("/src/headerblock")},
    {PDB_NamedStream_StringTable, str8_lit("/names")},
    {PDB_NamedStream_LinkInfo,    str8_lit("/LinkInfo")},
  };
  
  // build baked table
  PDB_NamedStreamTable *result = push_array(arena, PDB_NamedStreamTable, 1);
  struct StreamNameIndexPair *p = pairs;
  for (U64 i = 0; i < ArrayCount(pairs); i += 1, p += 1){
    String8 name = p->name;
    
    // get info node with this name
    PDB_InfoNode *match = 0;
    for (PDB_InfoNode *node = info->first;
         node != 0;
         node = node->next){
      if (str8_match(name, node->string, 0)){
        match = node;
        break;
      }
    }
    
    // if match found save stream number
    if (match != 0){
      result->sn[p->index] = match->sn;
    }
    else{
      result->sn[p->index] = 0xFFFF;
    }
  }
  
  ProfEnd();
  
  return(result);
}

internal PDB_Strtbl*
pdb_strtbl_from_data(Arena *arena, String8 data){
  ProfBegin("pdb_strtbl_from_data");
  
  // get header
  PDB_StringTableHeader *header = 0;
  if (sizeof(*header) <= data.size){
    header = (PDB_StringTableHeader *)data.str;
  }
  
  PDB_Strtbl *result = push_array(arena, PDB_Strtbl, 1);
  if (header != 0 && header->magic == PDB_StringTableHeader_MAGIC && header->version == 1){
    U32 strblock_size_off = sizeof(*header);
    U32 strblock_size = 0;
    if (strblock_size_off + 4 <= data.size){
      strblock_size = *(U32*)(data.str + strblock_size_off);
    }
    U32 strblock_off = strblock_size_off + 4;
    
    U32 bucket_count_off = strblock_off + strblock_size;
    U32 bucket_count = 0;
    if (bucket_count_off + 4 <= data.size){
      bucket_count = *(U32*)(data.str + bucket_count_off);
    }
    
    U32 bucket_array_off = bucket_count_off + 4;
    U32 bucket_array_size = bucket_count*sizeof(PDB_StringIndex);
    
    if (bucket_array_off + bucket_array_size <= data.size){
      result->data = data;
      result->bucket_count = bucket_count;
      result->strblock_min = strblock_off;
      result->strblock_max = strblock_off + strblock_size;
      result->buckets_min = bucket_array_off;
      result->buckets_max = bucket_array_off + bucket_array_size;
    }
  }
  
  ProfEnd();
  
  return(result);
}

internal PDB_DbiParsed *
pdb_dbi_from_data(Arena *arena, String8 data)
{
  ProfBeginFunction();
  PDB_DbiParsed *result = push_array(arena, PDB_DbiParsed, 1);;
  
  // rjf: extract header
  PDB_DbiHeader *header = 0;
  if(sizeof(*header) <= data.size)
  {
    header = (PDB_DbiHeader*)data.str;
  }
  
  // rjf: parse
  if(header != 0 && header->sig == PDB_DbiHeaderSignature_V1)
  {
    // extract range sizes
    U64 range_size[PDB_DbiRange_COUNT];
    range_size[PDB_DbiRange_ModuleInfo] = header->module_info_size;
    range_size[PDB_DbiRange_SecCon]     = header->sec_con_size;
    range_size[PDB_DbiRange_SecMap]     = header->sec_map_size;
    range_size[PDB_DbiRange_FileInfo]   = header->file_info_size;
    range_size[PDB_DbiRange_TSM]        = header->tsm_size;
    range_size[PDB_DbiRange_EcInfo]     = header->ec_info_size;
    range_size[PDB_DbiRange_DbgHeader]  = header->dbg_header_size;
    
    // fill result
    result->data = data;
    result->machine_type = header->machine;
    result->gsi_sn = header->gsi_sn;
    result->psi_sn = header->psi_sn;
    result->sym_sn = header->sym_sn;
    
    
    // fill result's range offsets
    {
      U64 cursor = sizeof(*header);
      for(U64 i = 0; i < (U64)(PDB_DbiRange_COUNT); i += 1)
      {
        result->range_off[i] = cursor;
        cursor += range_size[i];
        cursor = ClampTop(cursor, data.size);
      }
      result->range_off[PDB_DbiRange_COUNT] = cursor;
    }
    
    // fill result's debug streams
    U64 dbg_streams_min = result->range_off[PDB_DbiRange_DbgHeader];
    U64 dbg_streams_max = result->range_off[PDB_DbiRange_DbgHeader + 1];
    U64 dbg_streams_size_raw = dbg_streams_max - dbg_streams_min;
    U64 dbg_streams_size = ClampTop(dbg_streams_size_raw, sizeof(result->dbg_streams));
    MemoryCopy(result->dbg_streams, data.str + dbg_streams_min, dbg_streams_size);
    if(dbg_streams_size < sizeof(result->dbg_streams))
    {
      U64 filled_count = dbg_streams_size/sizeof(MSF_StreamNumber);
      MemorySet(result->dbg_streams + filled_count, 0xff, (ArrayCount(result->dbg_streams) - filled_count)*sizeof(MSF_StreamNumber));
    }
  }
  ProfEnd();
  return result;
}

internal B32
pdb_tpi_header_from_data(String8 data, PDB_TpiHeader *header_out)
{
  MemoryZeroStruct(header_out);
  PDB_TpiHeader header = {0};
  if(data.size < sizeof(header)) { return 0; }
  str8_deserial_read_struct(data, 0, &header);
  if(header.version != PDB_TpiVersion_IMPV80 || header.header_size < sizeof(header))
  {
    return 0;
  }
  *header_out = header;
  return 1;
}

internal B32
pdb_extract_type_server_info(String8 raw_msf, MSF_RawStreamTable *st, MSF_StreamNumber sn,
                             Rng1U64 *ti_range_out, Rng1U64 *leaf_range_out)
{
  B32 is_ok = 0;
  if(sn < st->stream_count)
  {
    Temp scratch = scratch_begin(0, 0);
    // Read only the fixed header, not the potentially large leaf stream.
    String8 data = msf_data_from_stream_number_ex(scratch.arena, raw_msf, st, sn,
                                                 r1u64(0, sizeof(PDB_TpiHeader)), 8);
    PDB_TpiHeader header = {0};
    if(pdb_tpi_header_from_data(data, &header))
    {
      U64 leaf_opl = (U64)header.header_size + header.leaf_data_size;
      if(leaf_opl <= st->streams[sn].size)
      {
        *ti_range_out = r1u64(header.ti_lo, header.ti_hi);
        *leaf_range_out = r1u64(header.header_size, leaf_opl);
        is_ok = 1;
      }
    }
    scratch_end(scratch);
  }
  return is_ok;
}

internal PDB_TpiParsed *
pdb_tpi_from_data(Arena *arena, String8 data)
{
  ProfBeginFunction();
  PDB_TpiParsed *result = push_array(arena, PDB_TpiParsed, 1);
  
  PDB_TpiHeader decoded_header = {0};
  if(pdb_tpi_header_from_data(data, &decoded_header))
  {
    PDB_TpiHeader *header = &decoded_header;
    U64 leaf_first_raw = header->header_size;
    U64 leaf_first     = ClampTop(leaf_first_raw, data.size);
    U64 leaf_opl_raw   = leaf_first + header->leaf_data_size;
    U64 leaf_opl       = ClampTop(leaf_opl_raw, data.size);
    
    result->data = data;
    
    result->leaf_first  = leaf_first;
    result->leaf_opl    = leaf_opl;
    result->itype_first = header->ti_lo;
    result->itype_opl   = header->ti_hi;
    
    result->hash_sn           = header->hash_sn;
    result->hash_sn_aux       = header->hash_sn_aux;
    result->hash_key_size     = header->hash_key_size;
    result->hash_bucket_count = header->hash_bucket_count;
    result->hash_vals_off     = header->hash_vals.off;
    result->hash_vals_size    = header->hash_vals.size;
    result->itype_off         = header->itype_offs.off;
    result->itype_size        = header->itype_offs.size;
    result->hash_adj_off      = header->hash_adj.off;
    result->hash_adj_size     = header->hash_adj.size;
  }
  
  ProfEnd();
  return result;
}

internal PDB_TpiHashParsed*
pdb_tpi_hash_from_data(Arena *arena, PDB_Strtbl *strtbl, PDB_TpiParsed *tpi, String8 data)
{
  ProfBeginFunction();
  PDB_TpiHashParsed *result = push_array(arena, PDB_TpiHashParsed, 1);
  U32 stride = tpi->hash_key_size;
  U32 bucket_count = tpi->hash_bucket_count;
  if(1 <= stride && stride <= 8 && bucket_count > 0 && data.str != 0)
  {
    // allocate buckets
    PDB_TpiHashBlock **buckets = push_array(arena, PDB_TpiHashBlock*, bucket_count);
    
    // extract "hash" array
    U8 *hashes = data.str + tpi->hash_vals_off;
    U8 *hash_opl = hashes + tpi->hash_vals_size;
    
    // for each index in the array...
    CV_TypeId itype = tpi->itype_first;
    U8 *hash_cursor = hashes;
    for (;hash_cursor + stride <= hash_opl;){
      
      // read index
      U64 bucket_idx = 0;
      MemoryCopy(&bucket_idx, hash_cursor, stride);
      
      // save to map
      if (bucket_idx < bucket_count){
        PDB_TpiHashBlock *block = buckets[bucket_idx];
        if (block == 0 || block->local_count == ArrayCount(block->itypes)){
          block = push_array(arena, PDB_TpiHashBlock, 1);
          SLLStackPush(buckets[bucket_idx], block);
        }
        if(block->local_count != 0)
        {
          MemoryCopy(block->itypes+1, block->itypes, sizeof(CV_TypeId)*block->local_count);
        }
        block->itypes[0] = itype;
        block->local_count += 1;
      }
      
      // advance cursor
      hash_cursor += stride;
      itype += 1;
    }
    
    //- rjf: compute bucket mask
    U32 bucket_mask = 0;
    if(IsPow2OrZero(bucket_count))
    {
      bucket_mask = bucket_count-1;
    }
    
    //- rjf: apply hash adjustments, to pull correct type IDs to the front of
    // the chains
    if(tpi->hash_adj_size != 0)
    {
      Temp scratch = scratch_begin(&arena, 1);
      PDB_HashAdjArray adjs = {0};
      if((U64)tpi->hash_adj_off + tpi->hash_adj_size <= data.size)
      {
        String8 adj_data = str8(data.str + tpi->hash_adj_off, tpi->hash_adj_size);
        adjs = pdb_hash_adj_array_from_data(scratch.arena, adj_data);
      }
      for(U64 pair_idx = 0; pair_idx < adjs.count; pair_idx += 1)
      {
        U32 name_off = adjs.v[pair_idx].name_off;
        CV_TypeId type_id = adjs.v[pair_idx].type_id;
        String8 string = pdb_strtbl_string_from_off(strtbl, name_off);
        U32 hash = pdb_hash_v1(string);
        U32 bucket_idx = ((bucket_mask != 0) ? hash&bucket_mask : hash%bucket_count);
        PDB_TpiHashBlock *prev_block = 0;
        for(PDB_TpiHashBlock *block = buckets[bucket_idx];
            block != 0;
            prev_block = block, block = block->next)
        {
          for(U32 local_idx = 0;
              local_idx < block->local_count && local_idx < ArrayCount(block->itypes);
              local_idx += 1)
          {
            if(block->itypes[local_idx] == type_id)
            {
              if(prev_block != 0)
              {
                prev_block->next = block->next;
                block->next = buckets[bucket_idx];
                buckets[bucket_idx] = block;
              }
              if(local_idx != 0)
              {
                Swap(CV_TypeId, block->itypes[0], block->itypes[local_idx]);
              }
              break;
            }
          }
        }
      }
      scratch_end(scratch);
    }
    
    // fill result
    result->data = data;
    result->buckets = buckets;
    result->bucket_count = bucket_count;
    result->bucket_mask = bucket_mask;
  }
  ProfEnd();
  return result;
}

internal PDB_GsiParsed *
pdb_gsi_from_data(Arena *arena, String8 data)
{
  ProfBeginFunction();
  PDB_GsiParsed *result = push_array(arena, PDB_GsiParsed, 1);
  
  // rjf: extract header
  PDB_GsiHeader *header = 0;
  if(sizeof(*header) <= data.size)
  {
    header = (PDB_GsiHeader*)data.str;
  }
  
  // rjf: parse
  if(header != 0 && header->signature == PDB_GsiSignature_Basic &&
     header->version == PDB_GsiVersion_V70 && header->bucket_data_size != 0)
  {
    Temp scratch = scratch_begin(&arena, 1);
    
    // hash offset
    U32 hash_record_array_off = sizeof(*header);
    
    // bucket count
    U32 slot_count = 4097;
    
    // array offsets
    U32 bitmask_u32_count = CeilIntegerDiv(slot_count, 32);
    U32 bitmask_byte_size = bitmask_u32_count*4;
    U32 bitmask_off       = hash_record_array_off + header->hash_record_arr_size;
    U32 offsets_off       = bitmask_off + bitmask_byte_size;
    
    // get bitmask & packed offset arrays
    U32Array  bitmask = {0};
    U32      *packed_offsets      = 0;
    U64       packed_offset_count = 0;
    if(bitmask_off + bitmask_byte_size <= data.size)
    {
      bitmask        = (U32Array){ .v = (U32 *)(data.str + bitmask_off), .count = bitmask_u32_count };
      packed_offsets = (U32 *)(data.str + offsets_off);
      packed_offset_count = (data.size - offsets_off)/4;
    }
    
    // unpack
    U32 *unpacked_offsets = 0;
    if(packed_offsets != 0)
    {
      unpacked_offsets = push_array_no_zero(scratch.arena, U32, slot_count);
      MemorySet(unpacked_offsets, 0xff, sizeof(unpacked_offsets[0]) * slot_count);
      
      U32 *off_ptr = (U32 *)packed_offsets;
      U32 *off_opl = off_ptr + packed_offset_count;
      for (U64 slot_idx = 0; ; slot_idx += 1) {
        slot_idx = bit_array_scan_left_to_right32(bitmask, slot_idx, slot_count, 1);
        if (slot_idx >= slot_count) { break; }
        if (off_ptr >= off_opl)     { Assert(0 && "corrupted GSI"); break; }
        unpacked_offsets[slot_idx] = *off_ptr;
        off_ptr += 1;
      }
    }
    
    // construct table
    B32 bad_table = 0;
    if(unpacked_offsets != 0)
    {
      // hash records
      PDB_GsiHashRecord *hash_records = (PDB_GsiHashRecord*)(data.str + hash_record_array_off);
      U32 hash_record_count = header->hash_record_arr_size/sizeof(PDB_GsiHashRecord);
      
      // * We unpack hash records into the the table by scanning backwards through the
      // * hash records. Neighboring values in unpacked_offsets *sort of* form counts, but we 
      // * have to skip the max-U32s (sloppy PDB nonsense).
      
      // * PDBs put one extra slot at the beginning of the encoded buckets that is mean
      // * to be padding for modifying the buffer in place. After decoding there are 4096 buckets, 
      // * in the encoded buckets there are 4097. We are meant to drop the first one.
      
      // build table
      PDB_GsiHashRecord *hash_record_ptr = hash_records + hash_record_count - 1;
      U32 prev_n = hash_record_count;
      for(U32 i = slot_count; i > 1;)
      {
        i -= 1;
        if(unpacked_offsets[i] != 0xFFFFFFFF)
        {
          // determine hash record range to use
          // * The "12" here is the result of some really sloppy PDB magic.
          U32 n = unpacked_offsets[i]/12;
          if(n > prev_n)
          {
            bad_table = 1;
            break;
          }
          U32 num_steps = prev_n - n;
          
          // fill this bucket
          U32 *bucket_offs = push_array_aligned(arena, U32, num_steps, 4);
          for(U32 j = num_steps; j > 0;)
          {
            j -= 1;
            // * The "- 1" is more sloppy PDB magic.
            bucket_offs[j] = hash_record_ptr->symbol_off - 1;
            hash_record_ptr -= 1;
          }
          PDB_GsiBucket *bucket = &result->buckets[i];
          bucket->count = num_steps;
          bucket->offs = bucket_offs;
          
          // update prev_n
          prev_n = n;
        }
      }
    }
    scratch_end(scratch);
  }
  
  ProfEnd();
  return result;
}

internal U64
pdb_gsi_symbol_from_string(PDB_GsiParsed *gsi, String8 symbol_data, String8 string)
{
  U64 result = max_U64;
  
  U32           hash       = pdb_hash_v1(string);
  U32           bucket_idx = hash % ArrayCount(gsi->buckets);
  PDB_GsiBucket bucket     = gsi->buckets[bucket_idx];
  
  for(U64 i = 0; i < bucket.count; ++i)
  {
    U32 off = bucket.offs[i];
    if(off + sizeof(CV_RecHeader) <= symbol_data.size)
    {
      CV_RecHeader *sym_header = (CV_RecHeader *)(symbol_data.str + off);
      
      if(sym_header->size >= sizeof(sym_header->kind))
      {
        U64  opl_off = off + sizeof(sym_header->size) + sym_header->size;
        U8  *sym_opl = (U8*)sym_header;
        if(opl_off <= symbol_data.size)
        {
          sym_opl = symbol_data.str + opl_off;
        }
        
        Rng1U64 raw_symbol_range = rng_1u64(off + sizeof(*sym_header), opl_off);
        String8 raw_symbol       = str8_substr(symbol_data, raw_symbol_range);
        String8 sym_name         = cv_name_from_symbol(sym_header->kind, raw_symbol);
        
        if(str8_match(sym_name, string, 0))
        {
          result = off;
          goto exit;
        }
      }
    }
  }
  
  exit:;
  return result;
}

internal COFF_SectionHeaderArray
pdb_coff_section_array_from_data(Arena *arena, String8 data){
  COFF_SectionHeaderArray result = {0};
  result.count = data.size/sizeof(COFF_SectionHeader);
  result.v = (COFF_SectionHeader*)data.str;
  return(result);
}

internal PDB_CompUnitArray*
pdb_comp_unit_array_from_data(Arena *arena, String8 data){
  PDB_CompUnitNode *first = 0;
  PDB_CompUnitNode *last = 0;
  U64 count = 0;
  
  U64 cursor = 0;
  for (;cursor + sizeof(PDB_DbiCompUnitHeader) <= data.size;){
    // get header
    PDB_DbiCompUnitHeader *header = (PDB_DbiCompUnitHeader*)(data.str + cursor);
    
    // get names
    U64 name_off = cursor + sizeof(*header);
    String8 name = str8_cstring_capped((char *)(data.str + name_off), (char *)(data.str + data.size));
    
    U64 name2_off = name_off + name.size + 1;
    String8 name2 = str8_cstring_capped((char *)(data.str + name2_off), (char *)(data.str + data.size));
    
    U64 after_name2_off = name2_off + name2.size + 1;
    
    // save mod info
    PDB_CompUnitNode *node = push_array_no_zero(arena, PDB_CompUnitNode, 1);
    SLLQueuePush(first, last, node);
    count += 1;
    node->unit.sn = header->sn;
    node->unit.obj_name = name;
    node->unit.group_name = name2;
    
    // fill range offsets
    U32 *range_buf = node->unit.range_off;
    {
      // fill the buffer with size of each range
      range_buf[PDB_DbiCompUnitRange_Symbols] = header->symbols_size;
      range_buf[PDB_DbiCompUnitRange_C11] = header->c11_lines_size;
      range_buf[PDB_DbiCompUnitRange_C13] = header->c13_lines_size;
      Assert(PDB_DbiCompUnitRange_C13 + 1 == PDB_DbiCompUnitRange_COUNT);
      
      // in-place sizes -> offs conversion
      U64 i = 0;
      U32 range_cursor = 0;
      for (; i < (U64)(PDB_DbiCompUnitRange_COUNT); i += 1){
        U64 adv = range_buf[i];
        range_buf[i] = range_cursor;
        range_cursor += adv;
      }
      range_buf[i] = range_cursor;
      
      // skip 4 byte signature in symbols range
      if (range_buf[1] >= 4){
        range_buf[0] += 4;
      }
    }
    
    // update cursor
    cursor = AlignPow2(after_name2_off, 4);
  }
  
  
  // fill result
  PDB_CompUnit **units = push_array_no_zero(arena, PDB_CompUnit*, count);
  {
    U64 idx = 0;
    for (PDB_CompUnitNode *node = first;
         node != 0;
         node = node->next, idx += 1){
      units[idx] = &node->unit;
    }
  }
  
  PDB_CompUnitArray *result = push_array(arena, PDB_CompUnitArray, 1);
  result->units = units;
  result->count = count;
  
  return(result);
}

internal PDB_CompUnitContributionArray
pdb_comp_unit_contribution_array_from_data(Arena *arena, String8 data, COFF_SectionHeaderArray sections)
{
  PDB_CompUnitContribution *contributions = 0;
  U64 count = 0;
  if(data.size >= sizeof(PDB_DbiSCVersion))
  {
    PDB_DbiSCVersion *version = (PDB_DbiSCVersion*)data.str;
    
    // determine array layout from version
    U32 item_size = 0;
    U32 array_off = 0;
    switch(*version)
    {
      default:
      {
        // TODO(allen): do we have a test case for this?
        item_size = sizeof(PDB_DbiSC40);
      }break;
      case PDB_DbiSCVersion_1:
      {
        item_size = sizeof(PDB_DbiSC);
        array_off = sizeof(*version);
      }break;
      case PDB_DbiSCVersion_2:
      {
        item_size = sizeof(PDB_DbiSC2);
        array_off = sizeof(*version);
      }break;
    }
    
    // allocate ranges
    U64 max_count = (data.size - array_off)/item_size;
    contributions = push_array_no_zero(arena, PDB_CompUnitContribution, max_count);
    
    // binary section info
    U64 section_count = sections.count;
    COFF_SectionHeader* section_headers = sections.v;
    
    // fill array
    PDB_CompUnitContribution *contribution_ptr = contributions;
    U64 cursor = array_off;
    for(; cursor + item_size <= data.size; cursor += item_size)
    {
      PDB_DbiSC40 *sc = (PDB_DbiSC40*)(data.str + cursor);
      if(sc->size > 0 && 1 <= sc->sec && sc->sec <= section_count)
      {
        U64 voff = section_headers[sc->sec - 1].voff + sc->sec_off;
        contribution_ptr->mod        = sc->mod;
        contribution_ptr->voff_first = voff;
        contribution_ptr->voff_opl   = voff + sc->size;
        contribution_ptr += 1;
      }
    }
    count = (U64)(contribution_ptr - contributions);
  }
  
  // fill result
  PDB_CompUnitContributionArray result = {0};
  result.contributions = contributions;
  result.count = count;
  return result;
}

internal PDB_CompUnitContribution *
pdb_comp_unit_contribution_from_voff__binary_search(PDB_CompUnitContributionArray *array, U64 voff)
{
  PDB_CompUnitContribution *result = 0;
  if(array->count != 0)
  {
    U64 first_idx = 0;
    U64 last_idx = array->count-1;
    for(;first_idx < last_idx;)
    {
      U64 mid_idx = (last_idx + first_idx) / 2;
      U64 mid_voff_first = array->contributions[mid_idx].voff_first;
      U64 mid_voff_opl = array->contributions[mid_idx].voff_opl;
      if(voff < mid_voff_first)
      {
        last_idx = mid_idx;
      }
      else if(mid_voff_opl <= voff)
      {
        first_idx = mid_idx+1;
      }
      else
      {
        result = &array->contributions[mid_idx];
        break;
      }
    }
  }
  return result;
}

////////////////////////////////
//~ PDB Dbi Functions

internal String8
pdb_data_from_dbi_range(PDB_DbiParsed *dbi, PDB_DbiRange range){
  String8 result = {0};
  if (range < PDB_DbiRange_COUNT){
    U64 first = dbi->range_off[range];
    U64 opl   = dbi->range_off[range + 1];
    result.str = dbi->data.str + first;
    result.size = opl - first;
  }
  return(result);
}

internal String8
pdb_data_from_unit_range(MSF_Parsed *msf, PDB_CompUnit *unit, PDB_DbiCompUnitRange range){
  String8 result = {0};
  if (range < PDB_DbiCompUnitRange_COUNT){
    String8 full_stream_data = msf_data_from_stream(msf, unit->sn);
    
    U64 first_raw = unit->range_off[range];
    U64 opl_raw = unit->range_off[range + 1];
    U64 opl = ClampTop(opl_raw, full_stream_data.size);
    U64 first = ClampTop(first_raw, opl);
    
    result.str = full_stream_data.str + first;
    result.size = opl - first;
  }
  return(result);
}

////////////////////////////////
//~ PDB Tpi Functions

internal String8
pdb_leaf_data_from_tpi(PDB_TpiParsed *tpi){
  String8 data = tpi->data;
  U8 *first = data.str + tpi->leaf_first;
  U8 *opl   = data.str + tpi->leaf_opl;
  String8 result = str8_range(first, opl);
  return(result);
}

internal CV_TypeIdArray
pdb_tpi_itypes_from_name(Arena *arena, PDB_TpiHashParsed *tpi_hash, CV_LeafStream *leaf, CV_TypeId itype_first, String8 name, B32 compare_unique_name, U32 output_cap)
{
  CV_TypeIdArray result = {0};
  if(tpi_hash->bucket_count != 0)
  {
    U32 hash = pdb_hash_v1(name);
    U32 bucket_idx = ((tpi_hash->bucket_mask != 0) ?
                      hash&tpi_hash->bucket_mask :
                      hash%tpi_hash->bucket_count);
    
    String8 data = leaf->data;
    
    Temp scratch = scratch_begin(&arena, 1);
    struct Chain
    {
      struct Chain *next;
      CV_TypeId itype;
    };
    struct Chain *first = 0;
    struct Chain *last = 0;
    U32 count = 0;
    
    for (PDB_TpiHashBlock *block = tpi_hash->buckets[bucket_idx];
         block != 0;
         block = block->next){
      U32 local_count = block->local_count;
      CV_TypeId *itype_ptr = block->itypes;
      for (U32 i = 0; i < local_count; i += 1, itype_ptr += 1){
        
        String8 extracted_name = {0};
        
        CV_TypeId itype = *itype_ptr;
        if (itype_first <= itype && (U64)(itype - itype_first) < leaf->count){
          CV_RecRange  range_value = cv_leaf_stream_rec_range(leaf, itype - itype_first);
          CV_RecRange *range       = &range_value;

          if(range->hdr.size < 2) { continue; }

          if (range->off + range->hdr.size <= data.size) {
            U8 *first = data.str + range->off + 2;
            U64 cap = range->hdr.size - 2;
            
            switch (range->hdr.kind) {
              default:break;
              
              case CV_LeafKind_CLASS:
              case CV_LeafKind_STRUCTURE:
              {
                if (sizeof(CV_LeafStruct) <= cap){
                  CV_LeafStruct *lf_struct = (CV_LeafStruct*)first;
                  
                  if (!(lf_struct->props & CV_TypeProp_FwdRef)){
                    // size
                    U8 *numeric_ptr = (U8*)(lf_struct + 1);
                    CV_Numeric size = cv_numeric_from_data_range(numeric_ptr, first + cap);
                    
                    // name
                    U8 *name_ptr = numeric_ptr + size.encoded_size;
                    String8 name = str8_cstring_capped((char*)name_ptr, (char *)(first + cap));
                    
                    // unique name
                    if (compare_unique_name){
                      if (lf_struct->props & CV_TypeProp_HasUniqueName) {
                        U8 *unique_name_ptr = name_ptr + name.size + 1;
                        String8 unique_name = str8_cstring_capped((char*)unique_name_ptr, (char *)(first + cap));
                        extracted_name = unique_name;
                      }
                    }
                    else{
                      extracted_name = name;
                    }
                  }
                }
              }break;
              
              case CV_LeafKind_CLASS2:
              case CV_LeafKind_STRUCT2:
              {
                if (sizeof(CV_LeafStruct2) <= cap){
                  CV_LeafStruct2 *lf_struct = (CV_LeafStruct2*)first;
                  
                  if (!(lf_struct->props & CV_TypeProp_FwdRef)){
                    // size
                    U8 *numeric_ptr = (U8*)(lf_struct + 1);
                    CV_Numeric size = cv_numeric_from_data_range(numeric_ptr, first + cap);
                    
                    // name
                    U8 *name_ptr = numeric_ptr + size.encoded_size;
                    String8 name = str8_cstring_capped((char*)name_ptr, (char *)(first + cap));
                    
                    // unique name
                    if (compare_unique_name){
                      if (lf_struct->props & CV_TypeProp_HasUniqueName) {
                        U8 *unique_name_ptr = name_ptr + name.size + 1;
                        String8 unique_name = str8_cstring_capped((char*)unique_name_ptr, (char *)(first + cap));
                        extracted_name = unique_name;
                      }
                    }
                    else{
                      extracted_name = name;
                    }
                  }
                }
              }break;
              
              case CV_LeafKind_UNION:
              {
                if (sizeof(CV_LeafUnion) <= cap){
                  CV_LeafUnion *lf_union = (CV_LeafUnion*)first;
                  
                  if (!(lf_union->props & CV_TypeProp_FwdRef)){
                    // size
                    U8 *numeric_ptr = (U8*)(lf_union + 1);
                    CV_Numeric size = cv_numeric_from_data_range(numeric_ptr, first + cap);
                    
                    // name
                    U8 *name_ptr = numeric_ptr + size.encoded_size;
                    String8 name = str8_cstring_capped((char*)name_ptr, (char *)(first + cap));
                    
                    // unique name
                    if (compare_unique_name){
                      if (lf_union->props & CV_TypeProp_HasUniqueName) {
                        U8 *unique_name_ptr = name_ptr + name.size + 1;
                        String8 unique_name = str8_cstring_capped((char*)unique_name_ptr, (char *)(first + cap));
                        extracted_name = unique_name;
                      }
                    }
                    else{
                      extracted_name = name;
                    }
                  }
                }
              }break;
              
              case CV_LeafKind_ENUM:
              {
                if (sizeof(CV_LeafEnum) <= cap){
                  CV_LeafEnum *lf_enum = (CV_LeafEnum*)first;
                  
                  if (!(lf_enum->props & CV_TypeProp_FwdRef)){
                    // name
                    U8 *name_ptr = (U8*)(lf_enum + 1);
                    String8 name = str8_cstring_capped((char*)name_ptr, (char *)(first + cap));
                    
                    // unique name
                    if (compare_unique_name){
                      if (lf_enum->props & CV_TypeProp_HasUniqueName) {
                        U8 *unique_name_ptr = name_ptr + name.size + 1;
                        String8 unique_name = str8_cstring_capped((char*)unique_name_ptr, (char *)(first + cap));
                        extracted_name = unique_name;
                      }
                    }
                    else{
                      extracted_name = name;
                    }
                  }
                }
              }break;
            }
          }
        }
        
        if (str8_match(extracted_name, name, 0)){
          struct Chain *chain = push_array(scratch.arena, struct Chain, 1);
          SLLQueuePush(first, last, chain);
          count += 1;
          chain->itype = itype;
          if (count == output_cap){
            goto dblbreak;
          }
        }
      }
    }
    
    dblbreak:;
    
    
    // assemble result
    CV_TypeId *itypes = push_array_aligned(arena, CV_TypeId, count, 8);
    {
      CV_TypeId *itype_ptr = itypes;
      for (struct Chain *node = first;
           node != 0;
           node = node->next, itype_ptr += 1){
        *itype_ptr = node->itype;
      }
    }
    result.v = itypes;
    result.count = count;
    
    scratch_end(scratch);
  }
  return result;
}

internal CV_TypeId
pdb_tpi_first_itype_from_name(PDB_TpiHashParsed *tpi_hash, CV_LeafStream *tpi_leaf, CV_TypeId itype_first, String8 name, B32 compare_unique_name)
{
  Temp scratch = scratch_begin(0, 0);
  CV_TypeIdArray array = pdb_tpi_itypes_from_name(scratch.arena, tpi_hash, tpi_leaf, itype_first, name, compare_unique_name, 1);
  CV_TypeId result = 0;
  if(array.count > 0)
  {
    result = array.v[0];
  }
  scratch_end(scratch);
  return(result);
}

////////////////////////////////
//~ PDB Strtbl Functions

internal String8
pdb_strtbl_string_from_off(PDB_Strtbl *strtbl, U32 off){
  U32 strblock_max = strtbl->strblock_max;
  U32 full_off_raw = strtbl->strblock_min + off;
  U32 full_off = ClampTop(full_off_raw, strblock_max);
  String8 result = str8_cstring_capped((char*)(strtbl->data.str + full_off),
                                       (char*)(strtbl->data.str + strblock_max));
  return(result);
}

internal String8
pdb_strtbl_string_from_index(PDB_Strtbl *strtbl, PDB_StringIndex idx){
  String8 result = {0};
  if (idx < strtbl->bucket_count){
    U32 off = *(U32*)(strtbl->data.str + strtbl->buckets_min + idx*4);
    result = pdb_strtbl_string_from_off(strtbl, off);
  }
  return(result);
}

internal U32
pdb_strtbl_off_from_string(PDB_Strtbl *strtbl, String8 string)
{
  U32 result = max_U32;
  
  U32 hash            = pdb_hash_v1(string);
  U32 best_bucket_idx = hash % strtbl->bucket_count;
  U32 bucket_idx      = best_bucket_idx;
  
  do
  {
    String8 test_string = pdb_strtbl_string_from_index(strtbl, bucket_idx);
    
    if(test_string.size == 0)
    {
      break;
    }
    
    if(str8_match(test_string, string, 0))
    {
      result = bucket_idx;
      break;
    }
    
    bucket_idx = (bucket_idx+1) % strtbl->buckets_max;
  } while (bucket_idx != best_bucket_idx);
  
  return result;
}

////////////////////////////////
//~ rjf: Thin Lookup Fast Paths

internal B32
pdb_has_symbol_ref(String8 msf_data, String8List symbol_list, MSF_RawStreamTable *st)
{
  Temp scratch = scratch_begin(0,0);
  
  B32 has_ref = 0;
  
  String8        dbi_data = msf_data_from_stream_number(scratch.arena, msf_data, st, PDB_FixedStream_Dbi);
  PDB_DbiParsed *dbi      = pdb_dbi_from_data(scratch.arena, dbi_data);
  if(dbi)
  {
    String8        gsi_data   = msf_data_from_stream_number(scratch.arena, msf_data, st, dbi->gsi_sn);
    PDB_GsiParsed *gsi_parsed = pdb_gsi_from_data(scratch.arena, gsi_data);
    if(gsi_parsed)
    {
      String8 symbol_data = msf_data_from_stream_number(scratch.arena, msf_data, st, dbi->sym_sn);
      
      for(String8Node *symbol_n = symbol_list.first; symbol_n != 0; symbol_n = symbol_n->next)
      {
        U64 symbol_off = pdb_gsi_symbol_from_string(gsi_parsed, symbol_data, symbol_n->string);
        if(symbol_off < symbol_data.size)
        {
          has_ref = 1;
          break;
        }
      }
    }
  }
  
  scratch_end(scratch);
  return has_ref;
}

internal B32
pdb_has_file_ref(String8 msf_data, String8List file_list, MSF_RawStreamTable *st)
{
  Temp scratch = scratch_begin(0,0);
  
  B32 has_ref = 0;
  
  String8   info_data = msf_data_from_stream_number(scratch.arena, msf_data, st, PDB_FixedStream_Info);
  PDB_Info *info      = pdb_info_from_data(scratch.arena, info_data);
  if(info)
  {
    PDB_NamedStreamTable *named_streams = pdb_named_stream_table_from_info(scratch.arena, info);
    if(named_streams)
    {
      MSF_StreamNumber  strtbl_sn   = named_streams->sn[PDB_NamedStream_StringTable];
      String8           strtbl_data = msf_data_from_stream_number(scratch.arena, msf_data, st, strtbl_sn);
      PDB_Strtbl       *strtbl      = pdb_strtbl_from_data(scratch.arena, strtbl_data);
      if(strtbl->bucket_count != 0)
      {
        for EachIndex(idx, strtbl->bucket_count)
        {
          String8 stored_string = pdb_strtbl_string_from_index(strtbl, idx);
          for(String8Node *file_n = file_list.first; file_n != 0; file_n = file_n->next)
          {
            if(str8_match(file_n->string, stored_string, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive))
            {
              has_ref = 1;
              goto dbl_break;
            }
          }
        }
        dbl_break:;
      }
    }
  }
  
  scratch_end(scratch);
  return has_ref;
}

internal B32
pdb_has_symbol_or_file_ref(String8 msf_data, String8List symbol_list, String8List file_list)
{
  Temp scratch = scratch_begin(0,0);
  
  B32 has_ref = 0;
  
  MSF_RawStreamTable *st = msf_raw_stream_table_from_data(scratch.arena, msf_data);
  
  if(!has_ref && symbol_list.node_count)
  {
    has_ref = pdb_has_symbol_ref(msf_data, symbol_list, st);
  }
  
  if(!has_ref && file_list.node_count)
  {
    has_ref = pdb_has_file_ref(msf_data, file_list, st);
  }
  
  scratch_end(scratch);
  return has_ref;
}
