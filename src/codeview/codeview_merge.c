// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
// Include

#ifndef BLAKE3_API
# define BLAKE3_API static
# define BLAKE3_PRIVATE static
#endif

#include "third_party/blake3/blake3.h"
#ifndef XXH_STATIC_LINKING_ONLY
# define XXH_STATIC_LINKING_ONLY
#endif
#include "third_party/xxHash/xxhash.h"

////////////////////////////////
// Type References

internal CV_LeafRef
cv_leaf_ref_make(U64 source_ordinal, U64 leaf_idx)
{
  CV_LeafRef result = Compose64Bit((U32)source_ordinal, (U32)leaf_idx);
  return result;
}

internal U32
cv_leaf_ref_source(CV_LeafRef ref)
{
  return (U32)(ref >> 32);
}

internal U32
cv_leaf_ref_leaf_idx(CV_LeafRef ref)
{
  return (U32)ref;
}

internal int
cv_leaf_ref_compare(CV_LeafRef a, CV_LeafRef b)
{
  return a < b ? -1 : a > b ? +1 : 0;
}

internal B32
cv_direct_leaf_ref_from_ti(CV_TypeMergeInput *input, U32 source_ordinal, CV_TypeIndexSource source, CV_TypeIndex ti, CV_LeafRef *ref_out)
{
  if (source_ordinal >= input->count) { return 0; }

  CV_DebugT *debug_t      = &input->debug_t_arr[source_ordinal];
  Rng1U64    ti_range     = debug_t->ti_ranges[source];
  Rng1U64    pch_ti_range = debug_t->pch_ti_range[source];
  if ( ! contains_1u64(ti_range, ti) || pch_ti_range.min > pch_ti_range.max || contains_1u64(pch_ti_range, ti)) { return 0; }

  // strip type index range
  U64 leaf_idx = ti - ti_range.min;
  if (leaf_idx < dim_1u64(pch_ti_range)) { return 0; }

  // strip PCH indirection
  leaf_idx -= dim_1u64(pch_ti_range);
  if (debug_t->source_offsets[source] > max_U64 - leaf_idx) { return 0; }
  leaf_idx += debug_t->source_offsets[source];

  if (leaf_idx >= debug_t->leaf_stream.count || leaf_idx > max_U32) { return 0; }
  *ref_out = cv_leaf_ref_make(source_ordinal, leaf_idx);
  return 1;
}

internal B32
cv_leaf_ref_from_ti(CV_TypeMergeInput *input, U32 source_ordinal, CV_TypeIndexSource source, CV_TypeIndex ti, CV_LeafRef *ref_out)
{
  // ti range: external source
  CV_TypeSourceMapping *mapping = input->mappings ? &input->mappings[source_ordinal] : 0;
  if (mapping && mapping->kind != CV_TypeSourceMapping_Local) {
    // Keep the original external target across precompiled remapping. A PCH
    // descriptor supplies the original table/ranges, not a new target server.
    U32 target_source = mapping->target_source;

    // type index map indirection
    if (mapping->kind == CV_TypeSourceMapping_Remapped) {
      // precompiled source indirection
      if (contains_1u64(mapping->precompiled_ranges[source], ti)) {
        if (mapping->precompiled_source >= input->count) { return 0; }
        mapping = &input->mappings[mapping->precompiled_source];
      }

      if ( ! contains_1u64(mapping->ti_ranges[source], ti)) { return 0; }
      Rng1U64 pch_ti_range = mapping->precompiled_ranges[source];
      if (pch_ti_range.min > pch_ti_range.max) { return 0; }

      // translate type index to original leaf index
      U64 leaf_idx_og = ti - mapping->ti_ranges[source].min;
      if (leaf_idx_og < dim_1u64(pch_ti_range)) { return 0; }
      leaf_idx_og -= dim_1u64(pch_ti_range);

      if (mapping->ti_map) {
        if (leaf_idx_og >= mapping->ti_map_count) { return 0; }
        ti = memory_read32(mapping->ti_map + leaf_idx_og);
      }
    }

    return cv_direct_leaf_ref_from_ti(input, target_source, source, ti, ref_out);
  }

  CV_DebugT *debug_t = &input->debug_t_arr[source_ordinal];

  // ti range: PCH
  if (contains_1u64(debug_t->pch_ti_range[source], ti)) {
    source_ordinal = debug_t->pch_obj_idx;
  }

  // ti range: local source
  return cv_direct_leaf_ref_from_ti(input, source_ordinal, source, ti, ref_out);
}

internal U64
cv_hash_from_leaf_ref(CV_TypeMergeInput *input, CV_LeafRef leaf_ref)
{
  return input->debug_h_arr[cv_leaf_ref_source(leaf_ref)].v[cv_leaf_ref_leaf_idx(leaf_ref)];
}

internal B32
cv_match_leaf_ref(CV_TypeMergeInput *input, CV_LeafRef a, CV_LeafRef b)
{
  U64 a_hash = cv_hash_from_leaf_ref(input, a);
  U64 b_hash = cv_hash_from_leaf_ref(input, b);
  return a_hash == b_hash;
}

typedef struct CV_TypeHasher
{
  CV_TypeHashKind kind;
  union {
    blake3_hasher blake3;
    XXH3_state_t  xxhash;
  } u;
} CV_TypeHasher;

internal void
cv_type_hasher_init(CV_TypeHasher *hasher, CV_TypeHashKind kind)
{
  hasher->kind = kind;
  switch (kind) {
  case CV_TypeHashKind_BLAKE3: blake3_hasher_init(&hasher->u.blake3); break;
  case CV_TypeHashKind_XXHash: XXH3_64bits_reset(&hasher->u.xxhash);  break;
  }
}

internal void
cv_type_hasher_update(CV_TypeHasher *hasher, void *data, U64 size)
{
  switch (hasher->kind) {
  case CV_TypeHashKind_BLAKE3: blake3_hasher_update(&hasher->u.blake3, data, size); break;
  case CV_TypeHashKind_XXHash: XXH3_64bits_update(&hasher->u.xxhash, data, size);   break;
  }
}

internal U64
cv_type_hasher_digest64(CV_TypeHasher *hasher)
{
  U64 hash = 0;
  switch (hasher->kind) {
  case CV_TypeHashKind_BLAKE3: blake3_hasher_finalize(&hasher->u.blake3, (U8 *)&hash, sizeof(hash)); break;
  case CV_TypeHashKind_XXHash: hash = XXH3_64bits_digest(&hasher->u.xxhash); break;
  }
  return hash;
}

internal void
cv_type_merge_error(Arena *arena, CV_TypeMergeErrorList *errors, CV_TypeMergeErrorKind kind, CV_LeafRef leaf_ref, CV_TypeIndex ti, U64 offset)
{
  CV_TypeMergeError *error = push_array(arena, CV_TypeMergeError, 1);
  error->kind             = kind;
  error->leaf_ref         = leaf_ref;
  error->ti               = ti;
  error->offset           = offset;

  SLLQueuePush(errors->first, errors->last, error);
  errors->count += 1;
}

internal U64
cv_hash_leaf(Arena *error_arena, CV_TypeMergeErrorList *errors, CV_TypeMergeInput *input, CV_LeafRef leaf_ref, CV_TypeIndexInfoList ti_info_list, B32 discard_cycles)
{
  U32                source_ordinal = cv_leaf_ref_source(leaf_ref);
  U32                leaf_idx       = cv_leaf_ref_leaf_idx(leaf_ref);
  CV_DebugT         *debug_t        = &input->debug_t_arr[source_ordinal];
  CV_Leaf            leaf           = cv_debug_t_get_leaf(debug_t, leaf_idx);
  CV_TypeIndexSource curr_ti_source = cv_type_index_source_from_leaf_kind(leaf.kind);
  CV_TypeIndex       curr_ti        = cv_ti_from_leaf_idx(debug_t, curr_ti_source, leaf_idx);

  // init hasher
  CV_TypeHasher hasher;
  cv_type_hasher_init(&hasher, input->hash_kind);

  // hash bytes around indices
  {
    U64 last_ti_off = 0;
    for EachNode(ti_info, CV_TypeIndexInfo, ti_info_list.first) {
      U8 *bytes = leaf.data.str + last_ti_off;
      U64 size  = ti_info->offset - last_ti_off;
      cv_type_hasher_update(&hasher, bytes, size);
      last_ti_off = ti_info->offset + sizeof(CV_TypeIndex);
    }

    U8 *bytes = leaf.data.str + last_ti_off;
    U64 size  = leaf.data.size - last_ti_off;
    cv_type_hasher_update(&hasher, bytes, size);
  }

  // mix-in sub leaf hashes
  for EachNode(sub_ti_n, CV_TypeIndexInfo, ti_info_list.first) {
    CV_TypeIndex sub_ti = memory_read32(leaf.data.str + sub_ti_n->offset);

    // simple indices are stable across compile units
    if (sub_ti < debug_t->ti_ranges[sub_ti_n->source].min) {
      cv_type_hasher_update(&hasher, &sub_ti, sizeof(sub_ti));
      continue;
    }

    // discard type with a cyclic-ref or an invalid type index
    CV_LeafRef sub_ref;
    B32 is_type_graph_cyclic = discard_cycles && sub_ti > 0 && sub_ti > curr_ti;
    if (is_type_graph_cyclic || !cv_leaf_ref_from_ti(input, source_ordinal, sub_ti_n->source, sub_ti, &sub_ref)) {
      // discard type
      bit_array_set_bit32(input->is_leaf_discarded[source_ordinal], leaf_idx, 1);

      // reset hasher
      cv_type_hasher_init(&hasher, input->hash_kind);

      // report error
      cv_type_merge_error(error_arena, errors, is_type_graph_cyclic ? CV_TypeMergeError_ForwardReference : CV_TypeMergeError_InvalidTypeIndex, leaf_ref, sub_ti, sub_ti_n->offset);
      break;
    }

    // mix-in sub-type hash
    U64 sub_hash = cv_hash_from_leaf_ref(input, sub_ref);
    cv_type_hasher_update(&hasher, &sub_hash, sizeof(sub_hash));
  }

  // hash leaf header
  CV_LeafHeader *leaf_header_ptr = cv_debug_t_get_leaf_header(debug_t, leaf_idx);
  cv_type_hasher_update(&hasher, leaf_header_ptr, sizeof(*leaf_header_ptr));

  // finalize the type hash
  U64 hash = cv_type_hasher_digest64(&hasher);

  Assert(hash != 0);
  Assert(input->debug_h_arr[source_ordinal].v[leaf_idx] == 0 ||
         input->debug_h_arr[source_ordinal].v[leaf_idx] == 1);
  input->debug_h_arr[source_ordinal].v[leaf_idx] = hash;

  return hash;
}

internal void
cv_hash_leaf_deep(Arena                 *arena,
                  Arena                 *error_arena,
                  CV_TypeMergeErrorList *errors,
                  CV_TypeMergeInput     *input,
                  CV_LeafRef             root_leaf_ref,
                  CV_TypeIndexInfoList   root_ti_info_list)
{
  Temp temp = temp_begin(arena);

  typedef struct HashStack {
    struct HashStack    *next;
    CV_LeafRef           ref;
    CV_TypeIndexInfoList indices;
    CV_TypeIndexInfo    *cursor;
    CV_Leaf              leaf;
  } HashStack;

  // set up root frame
  U32        root_source = cv_leaf_ref_source(root_leaf_ref);
  HashStack *root_frame  = push_array(temp.arena, HashStack, 1);
  root_frame->ref        = root_leaf_ref;
  root_frame->indices    = root_ti_info_list;
  root_frame->cursor     = root_ti_info_list.first;
  root_frame->leaf       = cv_debug_t_get_leaf(&input->debug_t_arr[root_source], cv_leaf_ref_leaf_idx(root_leaf_ref));

  // mark the root just like a child before following dependencies, so a
  // reference back to it cannot enqueue and hash the root a second time
  input->debug_h_arr[root_source].v[cv_leaf_ref_leaf_idx(root_leaf_ref)] = 1;

  HashStack *stack = root_frame;
  while (stack) {
    while (stack->cursor) {
      CV_TypeIndexInfo *ti_info = stack->cursor;

      // advance iterator
      stack->cursor = ti_info->next;

      // get type index info
      CV_TypeIndex ti = memory_read32(stack->leaf.data.str + ti_info->offset);

      // resolve against the original root source (external graphs are in one source's TI space),
      // as in the original deep traversal
      if ( ! contains_1u64(input->debug_t_arr[root_source].ti_ranges[ti_info->source], ti)) { continue; }

      // skip hashed types
      CV_LeafRef leaf_ref;
      if ( ! cv_leaf_ref_from_ti(input, root_source, ti_info->source, ti, &leaf_ref)) {
        cv_type_merge_error(error_arena, errors, CV_TypeMergeError_InvalidTypeIndex, stack->ref, ti, ti_info->offset);
        continue;
      }

      U32 source_ordinal = cv_leaf_ref_source(leaf_ref);
      U32 leaf_idx       = cv_leaf_ref_leaf_idx(leaf_ref);
      if (input->debug_h_arr[source_ordinal].v[leaf_idx] != 0) { continue; }

      CV_Leaf             leaf         = cv_debug_t_get_leaf(&input->debug_t_arr[source_ordinal], leaf_idx);
      CV_TypeIndexInfoList ti_info_list = {0};
      if ( ! cv_get_leaf_type_index_offsets(temp.arena, leaf.kind, leaf.data, &ti_info_list)) {
        cv_type_merge_error(error_arena, errors, CV_TypeMergeError_InvalidLeaf, leaf_ref, 0, 0);
        bit_array_set_bit32(input->is_leaf_discarded[source_ordinal], leaf_idx, 1);
        continue;
      }

      // visiting marker for cyclic graphs
      input->debug_h_arr[source_ordinal].v[leaf_idx] = 1;

      // recurse down to sub types
      HashStack *frame = push_array(temp.arena, HashStack, 1);
      frame->ref       = leaf_ref;
      frame->leaf      = leaf;
      frame->indices   = ti_info_list;
      frame->cursor    = ti_info_list.first;
      SLLStackPush(stack, frame);
      break;
    }

    // no more type indices, pop frame
    if ( ! stack->cursor) {
      cv_hash_leaf(error_arena, errors, input, stack->ref, stack->indices, 0);
      SLLStackPop(stack);
    }
  }

  temp_end(temp);
}

internal CV_TypeIndex
cv_assigned_ti_hash_search(CV_AssignedTiHash *ht, CV_TypeMergeInput *input, CV_LeafRef leaf_ref)
{
  if (ht->cap == 0) { return 0; }

  CV_DebugH *debug_h = &input->debug_h_arr[cv_leaf_ref_source(leaf_ref)];

  U64 hash     = debug_h->v[cv_leaf_ref_leaf_idx(leaf_ref)];
  U64 best_idx = hash % ht->cap;
  U64 idx      = best_idx;
  do {
    CV_TypeIndex ti = ht->ti_arr[idx];
    if (ti == 0) { break; }
    if (cv_hash_from_leaf_ref(input, ht->leaf_refs[ti - ht->min_ti]) == hash) { return ti; }
    idx = (idx + 1) == ht->cap ? 0 : idx + 1;
  } while (idx != best_idx);

  return 0;
}

////////////////////////////////
// Type Merge

// must be power of two
#define CV_LEAF_HT_MIN_CAP 16

typedef struct CV_LeafHashTable
{
  Arena      *arena;
  U64         cap;
  U64         count;
  U64         initial_cap;
  U64         grow_count;
  CV_LeafRef *bucket_arr;
} CV_LeafHashTable;

typedef struct CV_LeafDedupWork
{
  U32 source;
  U64 cursor;
  U64 opl;
} CV_LeafDedupWork;

typedef enum CV_LeafInsert
{
  CV_LeafInsert_Full,
  CV_LeafInsert_Existing,
  CV_LeafInsert_New,
} CV_LeafInsert;

typedef struct CV_MergeTypesTask
{
  CV_TypeMergeInput     *input;
  CV_LeafHashTable       leaf_ht_arr[CV_TypeIndexSource_COUNT];
  U32Array               indices;
  B32                    deep;

  CV_TypeMergeErrorList *errors; // per worker
  TP_Arena              *error_arenas;

  CV_LeafDedupWork      *dedup_work;
  B32                    skip_dedup;
  U32                    grow_mask;
  CV_TypeIndexSource     ti_source;
  U64                    range_item_count;

  // extract present buckets
  U64 *counts;
  U64 *offsets;

  CV_MergedTypes result;
} CV_MergeTypesTask;

THREAD_POOL_TASK_FUNC(cv_hash_debug_t_task)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(&arena, 1);

  CV_MergeTypesTask     *task           = raw_task;
  U32                    source_ordinal = task->indices.v[task_id];
  CV_DebugT             *debug_t        = &task->input->debug_t_arr[source_ordinal];
  Arena                 *error_arena    = task->error_arenas->v[worker_id];
  CV_TypeMergeErrorList *errors         = &task->errors[worker_id];

  for EachIndex(leaf_idx, debug_t->leaf_stream.count) {
    if (task->deep && task->input->debug_h_arr[source_ordinal].v[leaf_idx] != 0) { continue; }
    Temp temp = temp_begin(scratch.arena);

    CV_LeafRef           leaf_ref = cv_leaf_ref_make(source_ordinal, leaf_idx);
    CV_Leaf              leaf;
    CV_TypeIndexInfoList ti_list = {0};
    if ( ! cv_debug_t_get_leaf_checked(debug_t, leaf_idx, &leaf) ||
         ! cv_get_leaf_type_index_offsets(temp.arena, leaf.kind, leaf.data, &ti_list)) {
      cv_type_merge_error(error_arena, errors, CV_TypeMergeError_InvalidLeaf, leaf_ref, 0, 0);
      bit_array_set_bit32(task->input->is_leaf_discarded[source_ordinal], leaf_idx, 1);
    } else if (task->deep) {
      cv_hash_leaf_deep(temp.arena, error_arena, errors, task->input, leaf_ref, ti_list);
    } else {
      cv_hash_leaf(error_arena, errors, task->input, leaf_ref, ti_list, 1);
    }

    temp_end(temp);
  }

  scratch_end(scratch);
  ProfEnd();
}

internal U64
cv_leaf_hash_table_double_cap(U64 cap)
{
  AssertAlways(cap <= (max_U64 - ARENA_HEADER_SIZE) / (2 * sizeof(CV_LeafRef)));
  return cap * 2;
}

internal CV_LeafHashTable
cv_leaf_hash_table_alloc(U64 cap)
{
  AssertAlways(IsPow2(cap) && cap >= CV_LEAF_HT_MIN_CAP && cap <= (max_U64 - ARENA_HEADER_SIZE) / sizeof(CV_LeafRef));
  U64    size  = ARENA_HEADER_SIZE + cap * sizeof(CV_LeafRef);
  Arena *arena = arena_alloc(.name = "LEAF_BUCKETS", .reserve_size = size, .commit_size = size);

  CV_LeafHashTable result = {
    .arena       = arena,
    .cap         = cap,
    .initial_cap = cap,
    .bucket_arr  = push_array_no_zero(arena, CV_LeafRef, cap)
  };
  MemorySet(result.bucket_arr, 0xff, cap * sizeof(CV_LeafRef));

  return result;
}

internal CV_LeafInsert
cv_leaf_hash_table_insert(CV_TypeMergeInput *input, CV_LeafHashTable *ht, CV_LeafRef ref, U64 hash, B32 dedup)
{
  U64 best_idx = hash & (ht->cap - 1);
  U64 idx      = best_idx;
  do {
    CV_LeafRef curr = ins_atomic_u64_eval(&ht->bucket_arr[idx]);
    while (curr == CV_LEAF_REF_NULL || (dedup && cv_hash_from_leaf_ref(input, curr) == hash)) {
      if (curr != CV_LEAF_REF_NULL && cv_leaf_ref_compare(ref, curr) >= 0) {
        return CV_LeafInsert_Existing;
      }
      CV_LeafRef cmp = ins_atomic_u64_eval_cond_assign(&ht->bucket_arr[idx], ref, curr);
      if (cmp == curr) {
        return curr == CV_LEAF_REF_NULL ? CV_LeafInsert_New : CV_LeafInsert_Existing;
      }
      curr = cmp;
    }
    idx = (idx + 1) & (ht->cap - 1);
  } while (idx != best_idx);
  return CV_LeafInsert_Full;
}

internal void
cv_leaf_hash_table_count_inserts(CV_MergeTypesTask *task, CV_TypeIndexSource source, U64 count)
{
  if (count) {
    CV_LeafHashTable *ht = &task->leaf_ht_arr[source];
    U64 total = ins_atomic_u64_add_eval(&ht->count, count);
    if (total >= ht->cap - ht->cap / 4) {
      ins_atomic_u32_or(&task->grow_mask, 1u << source);
    }
  }
}

THREAD_POOL_TASK_FUNC(cv_leaf_dedup_task)
{
  CV_MergeTypesTask *task           = raw_task;
  CV_LeafDedupWork  *work           = &task->dedup_work[task_id];
  U32                source_ordinal = work->source;
  CV_DebugT         *debug_t        = &task->input->debug_t_arr[source_ordinal];
  CV_DebugH         *debug_h        = &task->input->debug_h_arr[source_ordinal];
  U32Array           is_discarded   = task->input->is_leaf_discarded[source_ordinal];

  U64 inserted[CV_TypeIndexSource_COUNT] = {0};
  U64 leaf_idx = work->cursor;
  for (; leaf_idx < work->opl; ++leaf_idx) {
    if ((leaf_idx & 63) == 0 && ins_atomic_u32_eval(&task->grow_mask)) { break; }
    if ( ! task->skip_dedup && bit_array_is_set32(is_discarded, leaf_idx)) { continue; }

    CV_TypeIndexSource leaf_source = cv_type_index_source_from_debug_t(debug_t, leaf_idx);
    CV_LeafHashTable  *leaf_ht     = &task->leaf_ht_arr[leaf_source];
    CV_LeafRef         leaf_ref    = cv_leaf_ref_make(source_ordinal, leaf_idx);
    CV_LeafInsert      result      = cv_leaf_hash_table_insert(task->input, leaf_ht, leaf_ref, debug_h->v[leaf_idx], !task->skip_dedup);

    if (result == CV_LeafInsert_Full) {
      ins_atomic_u32_or(&task->grow_mask, 1u << leaf_source);
      break; // retry this leaf after growth, including in release builds
    }

    if (result == CV_LeafInsert_New && ++inserted[leaf_source] == 64) {
      cv_leaf_hash_table_count_inserts(task, leaf_source, inserted[leaf_source]);
      inserted[leaf_source] = 0;
    }
  }
  work->cursor = leaf_idx;

  for EachIndex(source, CV_TypeIndexSource_COUNT) {
    cv_leaf_hash_table_count_inserts(task, source, inserted[source]);
  }
}

typedef struct CV_RehashLeaves
{
  CV_TypeMergeInput *input;
  CV_LeafHashTable  *src;
  CV_LeafHashTable  *dst;
} CV_RehashLeaves;

THREAD_POOL_TASK_FUNC(cv_rehash_leaves_task)
{
  CV_RehashLeaves *task = raw_task;
  Rng1U64 leaf_range = tp_work_range(task->src->cap, tp->worker_count, task_id);
  for EachInRange(i, leaf_range) {
    CV_LeafRef ref = task->src->bucket_arr[i];
    if (ref == CV_LEAF_REF_NULL) { continue; }
    U64           hash   = cv_hash_from_leaf_ref(task->input, ref);
    CV_LeafInsert result = cv_leaf_hash_table_insert(task->input, task->dst, ref, hash, 0);
    AssertAlways(result == CV_LeafInsert_New);
  }
}

internal void
cv_grow_leaf_hash_table(TP_Context *tp, CV_TypeMergeInput *input, CV_LeafHashTable *ht)
{
  CV_LeafHashTable grown = cv_leaf_hash_table_alloc(cv_leaf_hash_table_double_cap(ht->cap));

  CV_RehashLeaves rehash = {
    input,
    ht,
    &grown
  };
  tp_for_parallel(tp, 0, tp->worker_count, cv_rehash_leaves_task, &rehash);

  grown.count       = ht->count;
  grown.initial_cap = ht->initial_cap;
  grown.grow_count  = ht->grow_count + 1;

  arena_release(ht->arena);

  *ht = grown;
}

internal void
cv_dedup_leaf_work(TP_Context *tp, CV_MergeTypesTask *task, U64 count, CV_LeafDedupWork *work, B32 skip_dedup)
{
  task->dedup_work = work;
  task->skip_dedup = skip_dedup;
  for (;;) {
    task->grow_mask = 0;
    tp_for_parallel(tp, 0, count, cv_leaf_dedup_task, task);
    if (!task->grow_mask) { break; }

    B32 pending = 0;
    for EachIndex(i, count) { pending |= work[i].cursor != work[i].opl; }
    if (!pending) { break; }

    for EachIndex(source, CV_TypeIndexSource_COUNT) {
      if (task->grow_mask & (1u << source)) {
        cv_grow_leaf_hash_table(tp, task->input, &task->leaf_ht_arr[source]);
      }
    }
  }
}

THREAD_POOL_TASK_FUNC(cv_count_present_type_buckets_task)
{
  CV_MergeTypesTask *task = raw_task;
  CV_LeafHashTable  *ht   = &task->leaf_ht_arr[task->ti_source];

  Rng1U64 range = tp_work_range(ht->cap, tp->worker_count, task_id);
  for EachInRange(bucket_idx, range) {
    task->counts[task_id] += ht->bucket_arr[bucket_idx] != CV_LEAF_REF_NULL;
  }
}

THREAD_POOL_TASK_FUNC(cv_get_present_type_buckets_task)
{
  CV_MergeTypesTask *task = raw_task;

  CV_LeafHashTable *ht               = &task->leaf_ht_arr[task->ti_source];
  CV_LeafRef       *unique_leaf_refs = task->result.leaf_refs[task->ti_source].v;
  U64              cursor           = task->offsets[task_id];

  Rng1U64 range = tp_work_range(ht->cap, tp->worker_count, task_id);
  for EachInRange(bucket_idx, range) {
    if (ht->bucket_arr[bucket_idx] != CV_LEAF_REF_NULL) {
      unique_leaf_refs[cursor++] = ht->bucket_arr[bucket_idx];
    }
  }
}

THREAD_POOL_TASK_FUNC(cv_assign_type_indices_task)
{
  CV_MergeTypesTask *task = raw_task;

  CV_AssignedTiHash *assigned         = &task->result.assigned_ti_arr[task->ti_source];
  CV_LeafRefArray    unique_leaf_refs = task->result.leaf_refs[task->ti_source];

  Rng1U64 range = tp_work_range(unique_leaf_refs.count, tp->worker_count, task_id);
  for EachInRange(i, range) {
    CV_TypeIndex type_index = assigned->min_ti + i;

    U64 hash     = cv_hash_from_leaf_ref(task->input, unique_leaf_refs.v[i]);
    U64 best_idx = hash % assigned->cap;
    U64 idx      = best_idx;

    B32 is_inserted = 0;
    do {
      CV_TypeIndex curr_type_index = ins_atomic_u32_eval(&assigned->ti_arr[idx]);
      if (curr_type_index == 0) {
        CV_TypeIndex cmp_type_index = ins_atomic_u32_eval_cond_assign(&assigned->ti_arr[idx], type_index, 0);
        if (cmp_type_index == 0) {
          is_inserted = 1;
          break;
        }
      }
      // advance
      idx = (idx + 1) == assigned->cap ? 0 : idx + 1;
    } while (idx != best_idx);
    AssertAlways(is_inserted);
  }
}

THREAD_POOL_TASK_FUNC(cv_export_type_hashes_task)
{
  CV_MergeTypesTask *task = raw_task;
  CV_LeafRefArray unique_leaf_refs = task->result.leaf_refs[task->ti_source];
  Rng1U64 range = tp_work_range(unique_leaf_refs.count, tp->worker_count, task_id);
  for EachInRange(i, range) {
    CV_LeafRef leaf_ref = unique_leaf_refs.v[i];
    task->result.hashes[task->ti_source][i] = cv_hash_from_leaf_ref(task->input, leaf_ref);
  }
}

THREAD_POOL_TASK_FUNC(cv_build_source_ti_map_task)
{
  CV_MergeTypesTask *task = raw_task;

  CV_DebugT    *debug_t       = &task->input->debug_t_arr[task_id];
  CV_TypeIndex *source_ti_map = task->result.source_ti_maps[task_id];

  for EachIndex(leaf_idx, debug_t->leaf_stream.count) {
    CV_TypeIndexSource source   = cv_type_index_source_from_debug_t(debug_t, leaf_idx);
    CV_AssignedTiHash *assigned = &task->result.assigned_ti_arr[source];
    CV_LeafRef         leaf_ref = cv_leaf_ref_make(task_id, leaf_idx);
    source_ti_map[leaf_idx] = cv_assigned_ti_hash_search(assigned, task->input, leaf_ref);
  }
}

internal CV_MergedTypes
cv_merge_types(TP_Context *tp, Arena *arena, CV_TypeMergeInput *input, CV_MergeTypeFlags merge_flags)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(&arena, 1);

  CV_MergeTypesTask task = { .input = input };
  task.result.input = *input;
  task.error_arenas = tp_arena_alloc(tp);
  task.errors       = push_array(scratch.arena, CV_TypeMergeErrorList, tp->worker_count);

  // produce hashes
  {
    // allocate every hash array before traversing any graph
    U32Array *hash_indices = push_array(scratch.arena, U32Array, input->hash_group_count);
    for EachIndex(group_idx, input->hash_group_count) {
      CV_TypeHashGroup *hash_group = &input->hash_groups[group_idx];

      // reserve array for source ordinals that need hashing
      hash_indices[group_idx].v = push_array(scratch.arena, U32, hash_group->sources.count);

      for EachIndex(i, hash_group->sources.count) {
        U32 source_ordinal = hash_group->sources.v[i];

        CV_DebugT *debug_t = &input->debug_t_arr[source_ordinal];
        CV_DebugH *debug_h = &input->debug_h_arr[source_ordinal];

        if (debug_h->count == 0) {
          // alloc hashes
          debug_h->count = debug_t->leaf_stream.count;
          debug_h->v     = push_array(arena, U64, debug_h->count);

          // schedule source types to be hashed
          hash_indices[group_idx].v[hash_indices[group_idx].count++] = source_ordinal;
        }
      }
    }

    for EachIndex(group_idx, input->hash_group_count) {
      task.indices = hash_indices[group_idx];
      task.deep    = input->hash_groups[group_idx].deep;
      tp_for_parallel(tp, 0, task.indices.count, cv_hash_debug_t_task, &task);
    }
  }

  // copy report nodes before releasing worker arenas
  for EachIndex(i, tp->worker_count) {
    for EachNode(error, CV_TypeMergeError, task.errors[i].first) {
      cv_type_merge_error(arena, &task.result.errors, error->kind, error->leaf_ref, error->ti, error->offset);
    }
  }
  tp_arena_release(&task.error_arenas);

  // leaf hash table init
  for EachIndex(ti_source, CV_TypeIndexSource_COUNT) {
    // largest individual type set is a cheap starting hint
    U64 largest_type_count = 0;
    for EachIndex(source_ordinal, input->count) {
      largest_type_count = Max(largest_type_count, input->debug_t_arr[source_ordinal].source_counts[ti_source]);
    }

    // choose capacity that can hold largest_type_count at no more than 75% occupancy
    U64 cap = CV_LEAF_HT_MIN_CAP;
    while (cap - cap / 4 < largest_type_count) { cap = cv_leaf_hash_table_double_cap(cap); }

    task.leaf_ht_arr[ti_source] = cv_leaf_hash_table_alloc(cap);
  }

  // prepopulate hash table with the selected premerged type set
  if (input->seed_source != CV_TYPE_SOURCE_NONE) {
    CV_LeafDedupWork *work = push_array(scratch.arena, CV_LeafDedupWork, tp->worker_count);
    for EachIndex(i, tp->worker_count) {
      Rng1U64 range = tp_work_range(input->debug_t_arr[input->seed_source].leaf_stream.count, tp->worker_count, i);
      work[i] = (CV_LeafDedupWork){ input->seed_source, range.min, range.max };
    }
    cv_dedup_leaf_work(tp, &task, tp->worker_count, work, 1);
  }

  // leaf dedup
  for EachIndex(group_idx, input->hash_group_count) {
    U32Array          indices = input->hash_groups[group_idx].sources;
    CV_LeafDedupWork *work    = push_array(scratch.arena, CV_LeafDedupWork, indices.count);
    U64               count   = 0;
    for EachIndex(i, indices.count) {
      U32 source_ordinal = indices.v[i];
      if (source_ordinal == input->seed_source) { continue; }
      work[count++] = (CV_LeafDedupWork){ source_ordinal, 0, input->debug_t_arr[source_ordinal].leaf_stream.count };
    }
    cv_dedup_leaf_work(tp, &task, count, work, 0);
  }

  // extract present buckets from the leaf hash tables and assign type indices
  for EachIndex(ti_source, CV_TypeIndexSource_COUNT) {
    task.ti_source = ti_source;
    CV_LeafHashTable *ht = &task.leaf_ht_arr[ti_source];

    // count present buckets
    task.counts = push_array(scratch.arena, U64, tp->worker_count);
    tp_for_parallel(tp, 0, tp->worker_count, cv_count_present_type_buckets_task, &task);

    CV_LeafRefArray *unique_leaf_refs = &task.result.leaf_refs[ti_source];
    unique_leaf_refs->count = sum_array_u64(tp->worker_count, task.counts);
    Assert(unique_leaf_refs->count == ht->count);
    AssertAlways(unique_leaf_refs->count <= max_U32 - CV_MinComplexTypeIndex);
    unique_leaf_refs->v = push_array_no_zero(arena, CV_LeafRef, unique_leaf_refs->count);
    task.offsets       = offsets_from_counts_array_u64(scratch.arena, task.counts, tp->worker_count);

    // copy present buckets
    tp_for_parallel(tp, 0, tp->worker_count, cv_get_present_type_buckets_task, &task);
    task.result.stats[ti_source] = (CV_TypeMergeStats){ ht->count, ht->cap, ht->initial_cap, ht->grow_count };
    arena_release(ht->arena);
    MemoryZeroStruct(ht);

    // sort output leaves based on { source ordinal, leaf index } to guarantee determinism
    u64_array_sort_radix_parallel(tp, unique_leaf_refs->count, unique_leaf_refs->v);

    // assign type indices
    CV_AssignedTiHash *assigned = &task.result.assigned_ti_arr[ti_source];
    assigned->cap       = unique_leaf_refs->count * 13 / 10;
    assigned->ti_arr    = push_array(arena, CV_TypeIndex, assigned->cap);
    assigned->min_ti    = CV_MinComplexTypeIndex;
    assigned->leaf_refs = unique_leaf_refs->v;
    tp_for_parallel(tp, 0, tp->worker_count, cv_assign_type_indices_task, &task);
    task.result.count[ti_source] = unique_leaf_refs->count;

    // export hashes
    if (merge_flags & CV_MergeTypeFlag_ExportHashes) {
      task.result.hashes[ti_source] = push_array_no_zero(arena, U64, unique_leaf_refs->count);
      tp_for_parallel(tp, 0, tp->worker_count, cv_export_type_hashes_task, &task);
    }
  }

  // build per-source type index maps
  if (merge_flags & CV_MergeTypeFlag_BuildSourceTiMap) {
    task.result.source_ti_maps = push_array(arena, CV_TypeIndex *, input->count);
    for EachIndex(source_ordinal, input->count) {
      task.result.source_ti_maps[source_ordinal] = push_array_no_zero(arena, CV_TypeIndex, input->debug_t_arr[source_ordinal].leaf_stream.count);
    }
    tp_for_parallel(tp, 0, input->count, cv_build_source_ti_map_task, &task);
  }

  MemoryCopyArray(task.result.min_type_indices, input->min_type_indices);

  scratch_end(scratch);
  tp_release_scratch_free_blocks(tp);
  ProfEnd();
  return task.result;
}

////////////////////////////////
// String Table Merge

typedef struct
{
  CV_DebugS            *arr;
  CV_StringTableRange **range_lists;
  U64                  *string_counts;
  U64                   bucket_cap;
  CV_StringBucket     **buckets;
  U64                   total_string_size;
  U64                   total_insert_count;
} CV_DedupStringTablesTask;

internal U64
cv_string_hash_table_hash(String8 string)
{
  return u64_hash_from_str8(string);
}

internal int
cv_string_bucket_is_before(void *raw_a, void *raw_b)
{
  CV_StringBucket **a = raw_a;
  CV_StringBucket **b = raw_b;

  int is_before;

  if ((*a)->u.idx0 == (*b)->u.idx0) {
    is_before = (*a)->u.idx1 < (*b)->u.idx1;
  } else {
    is_before = (*a)->u.idx0 < (*b)->u.idx0;
  }

  return is_before;
}

internal CV_StringBucket *
cv_string_hash_table_insert_or_update(CV_StringBucket **buckets, U64 cap, U64 hash, CV_StringBucket *new_bucket)
{
  CV_StringBucket *result                         = 0;
  B32              was_bucket_inserted_or_updated = 0;

  U64 best_idx = hash % cap;
  U64 idx      = best_idx;

  do {
    retry:;
    CV_StringBucket *curr_bucket = ins_atomic_ptr_eval_cond_assign(&buckets[idx], 0, 0);

    if (curr_bucket == 0) {
      CV_StringBucket *compare_bucket = ins_atomic_ptr_eval_cond_assign(&buckets[idx], new_bucket, curr_bucket);

      if (compare_bucket == curr_bucket) {
        // success, bucket was inserted
        was_bucket_inserted_or_updated = 1;
        break;
      }

      // another thread took the bucket...
      goto retry;
    } else if (str8_match(curr_bucket->string, new_bucket->string, 0)) {
      if (cv_string_bucket_is_before(&curr_bucket, &new_bucket)) {
        // return back bucket to the caller so it can be reused
        result = new_bucket;

        // don't need to update, more recent leaf is in the bucket
        was_bucket_inserted_or_updated = 1;

        break;
      }

      CV_StringBucket *compare_bucket = ins_atomic_ptr_eval_cond_assign(&buckets[idx], new_bucket, curr_bucket);

      if (compare_bucket == curr_bucket) {

        // Return the displaced bucket, but concurrent readers may still hold
        // it: callers must keep its allocation and contents alive until join.
        result = compare_bucket;

        // new bucket is in the hash table, exit
        was_bucket_inserted_or_updated = 1;
        break;
      }

      // another thread took the bucket...
      goto retry;
    }

    // advance
    idx = (idx + 1) % cap;
  } while (idx != best_idx);

  // are there enough free buckets?
  AssertAlways(was_bucket_inserted_or_updated);

  return result;
}

THREAD_POOL_TASK_FUNC(cv_count_strings_in_debug_s_arr_task)
{
  ProfBeginFunction();
  CV_DedupStringTablesTask *task          = raw_task;
  CV_StringTableRange      *range_list    = task->range_lists[task_id];

  for (CV_StringTableRange *range_n = range_list; range_n != 0; range_n = range_n->next) {
    CV_DebugS debug_s       = task->arr[range_n->debug_s_idx];
    String8   string_buffer = cv_c13_stream_string_table(&debug_s);

    Assert(range_n->range.min <= range_n->range.max);
    Assert(range_n->range.min <= string_buffer.size);
    Assert(range_n->range.max <= string_buffer.size);

    U64 count = 0;
    for (U64 i = range_n->range.min; i < range_n->range.max; ++i) {
      U8 b = string_buffer.str[i];
      if (b == '\0') {
        count += 1;
      }
    }

    // splitter also accepts a final nonempty, unterminated field
    if (range_n->range.max == string_buffer.size && string_buffer.size &&
        string_buffer.str[string_buffer.size - 1] != 0) {
      count += 1;
    }

    ins_atomic_u64_add_eval(&task->string_counts[range_n->debug_s_idx], count);
  }

  ProfEnd();
}

internal CV_StringTableMergeCounts
cv_merge_string_table(Arena *arena, String8 table, U64 source_ordinal, CV_StringBucket **buckets, U64 bucket_cap)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(&arena, 1);
  String8List strings_list = str8_split_by_string_chars(scratch.arena, table, str8_lit("\0"), 0);

  CV_StringBucket *bucket = 0;
  CV_StringTableMergeCounts result = {0};

  U64 string_idx = 0;

  for (String8Node *string_n = strings_list.first; string_n != 0; string_n = string_n->next, ++string_idx) {
    if (bucket == 0) {
      bucket = push_array_no_zero(arena, CV_StringBucket, 1);
    }

    AssertAlways(string_idx <= max_U32);
    bucket->u.idx0 = (U32)source_ordinal;
    bucket->u.idx1 = (U32)string_idx;
    bucket->string = string_n->string;

    U64              hash             = cv_string_hash_table_hash(string_n->string);
    CV_StringBucket *insert_or_update = cv_string_hash_table_insert_or_update(buckets, bucket_cap, hash, bucket);

    if (insert_or_update == 0) {
      result.string_size  += string_n->string.size;
      result.insert_count += 1;
    }

    if (insert_or_update != bucket) {
      // only reuse a rejected or never-published bucket
      bucket = 0;
    }
  }

  scratch_end(scratch);
  ProfEnd();
  return result;
}

THREAD_POOL_TASK_FUNC(cv_dedup_strings_in_debug_s_arr_task)
{
  CV_DedupStringTablesTask *task = raw_task;
  String8 table = cv_c13_stream_string_table(&task->arr[task_id]);
  CV_StringTableMergeCounts counts = cv_merge_string_table(arena, table, task_id, task->buckets, task->bucket_cap);
  ins_atomic_u64_add_eval(&task->total_string_size, counts.string_size);
  ins_atomic_u64_add_eval(&task->total_insert_count, counts.insert_count);
}

internal CV_StringHashTable
cv_dedup_string_tables(TP_Arena *arena, TP_Context *tp, U64 count, CV_DebugS *arr)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(arena->v, arena->count);

  ProfBegin("Compute Total Weight");
  U64 total_weight = 0;
  for EachIndex(i, count) {
    String8 string_table = cv_c13_stream_string_table(&arr[i]);
    total_weight += string_table.size;
  }
  ProfEnd();

  U64                   per_task_weight = CeilIntegerDiv(total_weight, tp->worker_count);
  U64                   task_weight     = 0;
  U64                   task_id         = 0;
  CV_StringTableRange **range_lists     = push_array(scratch.arena, CV_StringTableRange *, tp->worker_count);

  ProfBegin("Divide Work");
  for EachIndex(debug_s_idx, count) {
    String8 string_table = cv_c13_stream_string_table(&arr[debug_s_idx]);

    for (U64 cursor = 0; cursor < string_table.size; cursor += per_task_weight) {
      if (task_weight >= per_task_weight) {
        task_id     = (task_id + 1) % tp->worker_count;
        task_weight = 0;
      }

      U64 max_range_weight = Min(per_task_weight, string_table.size - cursor);

      CV_StringTableRange *node = push_array(scratch.arena, CV_StringTableRange, 1);
      node->range               = rng_1u64(cursor, cursor + max_range_weight);
      node->debug_s_idx         = debug_s_idx;

      SLLStackPush(range_lists[task_id], node);
      task_weight += max_range_weight;
    }
  }
  ProfEnd();

  ProfBegin("Count");
  CV_DedupStringTablesTask task = {0};
  task.arr                      = arr;
  task.range_lists              = range_lists;
  task.string_counts            = push_array(scratch.arena, U64, count);
  tp_for_parallel(tp, 0, tp->worker_count, cv_count_strings_in_debug_s_arr_task, &task);
  ProfEnd();

  ProfBegin("Dedup");
  U64 total_string_count = sum_array_u64(count, task.string_counts);
  task.bucket_cap = Max(1, (U64)((F64)total_string_count * 1.3));
  task.buckets    = push_array(arena->v[0], CV_StringBucket *, task.bucket_cap);
  tp_for_parallel(tp, arena, count, cv_dedup_strings_in_debug_s_arr_task, &task);
  ProfEnd();

  CV_StringHashTable string_ht = {0};
  string_ht.total_string_size  = task.total_string_size;
  string_ht.total_insert_count = task.total_insert_count;
  string_ht.bucket_cap         = task.bucket_cap;
  string_ht.buckets            = task.buckets;

  scratch_end(scratch);
  ProfEnd();
  return string_ht;
}

internal void
cv_string_hash_table_assign_buffer_offsets(TP_Context *tp, CV_StringHashTable string_ht)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(0,0);

  ProfBegin("Count Strings");
  U64 string_count = 0;
  for EachIndex(i, string_ht.bucket_cap) {
    if (string_ht.buckets[i] != 0) {
      string_count += 1;
    }
  }
  ProfEnd();

  ProfBegin("Push");
  CV_StringBucket **strings = push_array_no_zero(scratch.arena, CV_StringBucket *, string_count);
  ProfEnd();

  ProfScope("Copy Present Buckets") {
    U64 string_idx = 0;
    for EachIndex(i, string_ht.bucket_cap) {
      if (string_ht.buckets[i] != 0) {
        strings[string_idx++] = string_ht.buckets[i];
      }
    }
  }

  ProfBegin("Sort");
  radsort(strings, string_count, cv_string_bucket_is_before);
  ProfEnd();

  ProfScope("Assign Offsets") {
    U64 offset_cursor = 0;
    for EachIndex(i, string_count) {
      CV_StringBucket *s = strings[i];
      s->u.offset = offset_cursor;
      offset_cursor += s->string.size + 1;
    }
  }

  scratch_end(scratch);
  ProfEnd();
}

internal CV_StringBucket *
cv_string_hash_table_lookup(CV_StringHashTable ht, String8 string)
{
  if (ht.bucket_cap == 0) { return 0; }

  U64 hash     = cv_string_hash_table_hash(string);
  U64 best_idx = hash % ht.bucket_cap;
  U64 idx      = best_idx;

  do {
    if (ht.buckets[idx] == 0) {
      break;
    }

    if (str8_match(ht.buckets[idx]->string, string, 0)) {
      return ht.buckets[idx];
    }

    idx = (idx + 1) % ht.bucket_cap;
  } while (idx != best_idx);

  return 0;
}

typedef struct
{
  String8            buffer;
  CV_StringHashTable string_ht;
} CV_PackStringHashTableTask;

internal void
cv_pack_string_hash_table_range(String8 buffer, CV_StringHashTable string_ht, Rng1U64 range)
{
  for EachInRange(bucket_idx, range) {
    CV_StringBucket *bucket = string_ht.buckets[bucket_idx];
    if (bucket) {
      if (bucket->string.size) { MemoryCopy(buffer.str + bucket->u.offset, bucket->string.str, bucket->string.size); }
      buffer.str[bucket->u.offset + bucket->string.size] = '\0';
    }
  }
}

THREAD_POOL_TASK_FUNC(cv_pack_string_hash_table_task)
{
  ProfBeginFunction();
  CV_PackStringHashTableTask *task = raw_task;
  Rng1U64 range = tp_work_range(task->string_ht.bucket_cap, tp->worker_count, task_id);
  cv_pack_string_hash_table_range(task->buffer, task->string_ht, range);
  ProfEnd();
}

internal void
cv_pack_string_hash_table_into(TP_Context *tp, CV_StringHashTable string_ht, String8 buffer)
{
  CV_PackStringHashTableTask task = {0};
  task.string_ht = string_ht;
  task.buffer    = buffer;
  tp_for_parallel(tp, 0, tp->worker_count, cv_pack_string_hash_table_task, &task);
}

internal String8
cv_pack_string_hash_table(Arena *arena, TP_Context *tp, CV_StringHashTable string_ht)
{
  ProfBeginFunction();

  AssertAlways(string_ht.total_string_size <= max_U64 - string_ht.total_insert_count);
  U64  buffer_size = string_ht.total_string_size + /* nulls: */ string_ht.total_insert_count;
  U8  *buffer      = push_array(arena, U8, buffer_size);
  cv_pack_string_hash_table_into(tp, string_ht, str8(buffer, buffer_size));

  String8 result = str8(buffer, buffer_size);
  ProfEnd();
  return result;
}

////////////////////////////////
// Symbol Deduper

internal CV_SymbolInsertResult
cv_symbol_deduper_insert_only(void **buckets, U64 bucket_cap, String8 raw)
{
  U64       idx    = u64_hash_from_str8(raw) % bucket_cap;
  CV_Symbol symbol = cv_symbol_from_ptr(raw.str);

  for (U64 probes = 0; probes < bucket_cap; ) {
    void *curr = ins_atomic_ptr_eval_cond_assign(&buckets[idx], 0, 0);
    if (curr == 0) {
      if (ins_atomic_ptr_eval_cond_assign(&buckets[idx], raw.str, 0) != 0) { continue; }
      return CV_SymbolInsertResult_Inserted;
    }

    if (cv_symbol_match(cv_symbol_from_ptr(curr), symbol)) {
      return CV_SymbolInsertResult_Duplicate;
    }

    idx = (idx + 1) == bucket_cap ? 0 : idx + 1;
    probes += 1;
  }

  return CV_SymbolInsertResult_Full;
}

internal void *
cv_symbol_deduper_insert_or_update(void **buckets, U64 bucket_cap, U64 hash, void *symbol_ptr)
{
  CV_Symbol  symbol                 = cv_symbol_from_ptr(symbol_ptr);
  U8        *result                 = 0;
  B32        is_inserted_or_updated = 0;
  U64        best_idx               = hash % bucket_cap;
  U64        bucket_idx             = best_idx;
  do {
    retry:;
    void *curr_ptr  = buckets[bucket_idx];
    Assert(curr_ptr != symbol_ptr);

    if (curr_ptr == 0) {
      U8 *cmp = ins_atomic_ptr_eval_cond_assign(&buckets[bucket_idx], symbol_ptr, curr_ptr);
      if (cmp == curr_ptr) {
        // success, bucket was inserted
        is_inserted_or_updated = 1;
        break;
      }
      // another thread took the bucket...
      goto retry;
    } else {
      CV_Symbol curr = cv_symbol_from_ptr(curr_ptr);

      if (cv_symbol_match(curr, symbol)) {
        if (curr_ptr < symbol_ptr) {
          // do not update, more recent symbol is in the bucket
          result = symbol_ptr;
          is_inserted_or_updated = 1;
          break;
        }

        void *cmp = ins_atomic_ptr_eval_cond_assign(&buckets[bucket_idx], symbol_ptr, curr_ptr);
        if (cmp == curr_ptr) {
          result = cmp;
          is_inserted_or_updated = 1;
          break;
        }

        // another thread took the bucket...
        goto retry;
      }
    }

    // advance
    bucket_idx = (bucket_idx + 1) == bucket_cap ? 0 : bucket_idx + 1;
  } while (bucket_idx != best_idx);
  Assert(is_inserted_or_updated);
  return result;
}

////////////////////////////////
// Helpers

internal B32
cv_debug_t_from_data_checked(Arena *arena, String8 data, U64 align, U32 *offsets, U64 *source_counts, CV_DebugT *out)
{
  MemoryZeroStruct(out);

  CV_DebugT debug_t = {0};
  if ( ! cv_make_leaf_stream_checked(data, align, 0, 0, &debug_t.leaf_stream)) {
    return 0;
  }

  U64 parse_start_offset = 0;

  // strip LF_PRECOMP from .debug$T to exclude it from the leaf arrays;
  // (we don't need to adjust type indices because compiler already did this for us)
  {
    CV_Leaf leaf        = {0};
    U64     leaf_size   = cv_read_leaf(data, 0, align, &leaf);
    if (leaf_size && cv_is_leaf_pch(leaf.kind)) {
      if ( ! cv_precomp_info_from_leaf(leaf, &debug_t.precomp)) {
        return 0;
      }
      debug_t.has_pch = 1;
      parse_start_offset = leaf_size;
    }
  }

  //
  // init from cache
  //
  if (offsets || source_counts) {
    MemoryCopy(debug_t.source_counts, source_counts, sizeof(debug_t.source_counts));

    U64 count = 0;
    for EachElement(i, debug_t.source_counts) {
      if (debug_t.source_counts[i] > (data.size - parse_start_offset)/sizeof(CV_RecHeader) - count) {
        return 0;
      }
      count += debug_t.source_counts[i];
    }

    if( ! cv_make_leaf_stream_checked(data, align, count, offsets, &debug_t.leaf_stream)) {
      return 0;
    }
  }
  //
  // full parse
  //
  else {
    CV_LeafStream *stream = &debug_t.leaf_stream;
    if ( ! cv_leaf_stream_from_data_checked(arena, data, align, stream)) {
      return 0;
    }

    // keep offsets relative to the original object section, including PRECOMP.
    if (parse_start_offset) {
      stream->offsets += 1;
      stream->count   -= 1;
    }
    if (stream->count) {
      debug_t.ipi_bits = bit_array_init32(arena, stream->count);
    }

    for EachIndex(idx, stream->count) {
      CV_Record record;
      if ( ! cv_leaf_stream_read_record(stream, idx, &record)) {
        return 0;
      }

      CV_TypeIndexSource source = cv_type_index_source_from_leaf_kind(record.header.kind);
      debug_t.source_counts[source] += 1;

      if (source == CV_TypeIndexSource_IPI) {
        bit_array_set_bit32(debug_t.ipi_bits, idx, 1);
      }
    }

    if (debug_t.source_counts[CV_TypeIndexSource_NULL]) {
      debug_t.ipi_bits = (U32Array){0};
    }
  }

  // assign type index ranges
  CV_TypeId first       = CV_MinComplexTypeIndex;
  U64       input_count = debug_t.leaf_stream.count + debug_t.precomp.leaf_count;
  if (input_count > max_U32 - first) {
    return 0;
  }

  for EachElement(i, debug_t.ti_ranges) {
    debug_t.ti_ranges[i] = r1u64s(first, input_count);
  }

  // assign PCH type index ranges
  for EachElement(i, debug_t.pch_ti_range) {
    debug_t.pch_ti_range[i] = r1u64s(debug_t.precomp.start_index, debug_t.precomp.leaf_count);
  }

  *out = debug_t;
  return 1;
}

internal CV_DebugT
cv_debug_t_from_data(Arena *arena, String8 data, U64 align)
{
  CV_DebugT result = {0};
  B32 ok = cv_debug_t_from_data_checked(arena, data, align, 0, 0, &result);
  AssertAlways(ok);
  return result;
}

internal B32
cv_debug_t_get_leaf_checked(CV_DebugT *debug_t, U64 leaf_idx, CV_Leaf *leaf_out)
{
  return cv_leaf_stream_get_leaf(&debug_t->leaf_stream, leaf_idx, leaf_out);
}

internal CV_Leaf
cv_debug_t_get_leaf(CV_DebugT *debug_t, U64 leaf_idx)
{
  CV_Leaf result;
  AssertAlways(cv_debug_t_get_leaf_checked(debug_t, leaf_idx, &result));
  return result;
}

internal CV_TypeIndexSource
cv_type_index_source_from_debug_t(CV_DebugT *debug_t, U64 leaf_idx)
{
  if (debug_t->ipi_bits.v) {
    return bit_array_get_bit32(debug_t->ipi_bits, leaf_idx) ? CV_TypeIndexSource_IPI : CV_TypeIndexSource_TPI;
  }
  CV_Leaf leaf;
  if(!cv_debug_t_get_leaf_checked(debug_t, leaf_idx, &leaf)) { return CV_TypeIndexSource_NULL; }
  return cv_type_index_source_from_leaf_kind(leaf.kind);
}

internal U64
cv_leaf_idx_from_ti(CV_DebugT *debug_t, CV_TypeIndexSource source, CV_TypeIndex ti)
{
  // validate TI
  Assert(contains_1u64(debug_t->ti_ranges[source], ti));

  // no PCH indirection
  Assert(!contains_1u64(debug_t->pch_ti_range[source], ti));

  U64 leaf_idx = ti;
  Assert(leaf_idx >= debug_t->ti_ranges[source].min);

  // strip type index range
  leaf_idx -= debug_t->ti_ranges[source].min;
  Assert(leaf_idx >= dim_1u64(debug_t->pch_ti_range[source]));

  // strip PCH indirection
  leaf_idx -= dim_1u64(debug_t->pch_ti_range[source]);
  leaf_idx += debug_t->source_offsets[source];

  Assert(leaf_idx < debug_t->leaf_stream.count);
  return leaf_idx;
}

internal CV_TypeIndex
cv_ti_from_leaf_idx(CV_DebugT *debug_t, CV_TypeIndexSource source, U64 leaf_idx)
{
  U64          pch_count = dim_1u64(debug_t->pch_ti_range[source]);
  CV_TypeIndex ti        = debug_t->ti_ranges[source].min + pch_count + leaf_idx - debug_t->source_offsets[source];
  return ti;
}

internal CV_Leaf
cv_debug_t_get_leaf_from_ti(CV_DebugT *debug_t, CV_TypeIndexSource source, CV_TypeIndex ti)
{
  U64 leaf_idx = cv_leaf_idx_from_ti(debug_t, source, ti);
  return cv_debug_t_get_leaf(debug_t, leaf_idx);
}

internal String8
cv_debug_t_get_raw_leaf(CV_DebugT *debug_t, U64 leaf_idx)
{
  CV_Record record;
  if(!cv_leaf_stream_read_record(&debug_t->leaf_stream, leaf_idx, &record)) { return str8_zero(); }
  return str8(debug_t->leaf_stream.data.str + debug_t->leaf_stream.offsets[leaf_idx],
              sizeof(CV_RecHeader) + record.data.size);
}

internal CV_LeafHeader *
cv_debug_t_get_leaf_header(CV_DebugT *debug_t, U64 leaf_idx)
{
  String8 raw = cv_debug_t_get_raw_leaf(debug_t, leaf_idx);
  return (CV_LeafHeader *)raw.str;
}

internal CV_TypeIndex
cv_debug_t_get_type_index(CV_DebugT *debug_t, CV_TypeIndexSource ti_source, U64 leaf_idx)
{
  return cv_ti_from_leaf_idx(debug_t, ti_source, leaf_idx);
}

internal U64
cv_debug_t_get_leaf_index(CV_DebugT *debug_t, CV_TypeIndexSource ti_source, CV_TypeIndex ti)
{
  return cv_leaf_idx_from_ti(debug_t, ti_source, ti);
}

internal B32
cv_debug_t_is_pch(CV_DebugT *debug_t)
{
  CV_Leaf leaf;
  return debug_t->leaf_stream.count && cv_debug_t_get_leaf_checked(debug_t, 0, &leaf) && cv_is_leaf_pch(leaf.kind);
}

internal B32
cv_debug_t_is_type_server_ref(CV_DebugT *debug_t)
{
  B32 is_type_server = 0;
  CV_Leaf leaf;
  if (debug_t->leaf_stream.count && cv_debug_t_get_leaf_checked(debug_t, 0, &leaf)) {
    is_type_server = cv_is_leaf_type_server(leaf.kind);
  }
  return is_type_server;
}

////////////////////////////////
// Patcher

internal void
cv_symbol_list_push_node(CV_SymbolList *list, CV_SymbolNode *node)
{
  node->prev = 0;
  node->next = 0;
  DLLPushBack(list->first, list->last, node);
  list->count += 1;
}

internal CV_SymbolNode *
cv_symbol_list_push(Arena *arena, CV_SymbolList *list, CV_Symbol v)
{
  CV_SymbolNode *node = push_array(arena, CV_SymbolNode, 1);
  node->data = v;
  cv_symbol_list_push_node(list, node);
  return node;
}

internal CV_TypeIndexPatchResult
cv_patch_type_indices(CV_MergedTypes *types, U32 source_ordinal, String8 data, CV_TypeIndexInfoList indices)
{
  CV_TypeIndexPatchResult result = {0};

  for EachNode(n, CV_TypeIndexInfo, indices.first) {
    CV_TypeIndex ti = memory_read32(data.str + n->offset);
    if (ti >= types->input.min_type_indices[n->source]) {
      CV_LeafRef ref;
      if ( ! cv_leaf_ref_from_ti(&types->input, source_ordinal, n->source, ti, &ref)) {
        result.status = CV_TypeIndexPatchStatus_InvalidTypeIndex;
      } else {
        CV_TypeIndex assigned = cv_assigned_ti_hash_search(&types->assigned_ti_arr[n->source], &types->input, ref);
        if (assigned == 0) { result.status = CV_TypeIndexPatchStatus_UnassignedTypeIndex; }
        else               { memory_write32(data.str + n->offset, assigned);              }
      }
    }

    if (result.status != CV_TypeIndexPatchStatus_Ok) {
      result.info = n;
      break;
    }
  }

  return result;
}

internal B32
cv_patch_symbol_tree_offsets(String8 raw_symbols, U64 base_offset, U64 align, U64 *tree_size_out)
{
  Temp scratch = scratch_begin(0, 0);

  struct Stack { struct Stack *next; U64 symbol_pos; };
  struct Stack *stack = 0, *free_list = 0;

  B32 is_patched = 0;
  U64 cursor     = 0;

  for (;;) {
    if(cursor == raw_symbols.size) { break; }
    CV_Record record;
    if(!cv_read_record(raw_symbols, cursor, align, &record)) { goto exit; }

    if (cv_is_scope_symbol(record.header.kind)) {
      // NOTE: We don't patch 'next' offset in PROC symbols because
      // it's not used by visual studio and MSVC leaves the offsets
      // zeroed. LLD is on the same page.
      if(record.data.size < sizeof(U32)*2) { goto exit; }

      // patch parent symbol offset
      if (stack) {
        U32 parent_offset = safe_cast_u32(base_offset + stack->symbol_pos);
        MemoryCopy(raw_symbols.str + cursor + sizeof(CV_SymbolHeader), &parent_offset, sizeof(parent_offset));
      }

      // reuse/alloc frame
      struct Stack *frame = free_list;
      if (frame) { SLLStackPop(free_list);                                     }
      else       { frame = push_array_no_zero(scratch.arena, struct Stack, 1); }

      // push frame to the stack
      frame->symbol_pos = cursor;
      SLLStackPush(stack, frame);

    } else if (cv_is_end_symbol(record.header.kind)) {
      if (stack == 0) { goto exit; }

      // patch symbol end
      U32 end_offset = safe_cast_u32(base_offset + cursor);
      MemoryCopy(raw_symbols.str + stack->symbol_pos + sizeof(CV_SymbolHeader) + sizeof(U32), &end_offset, sizeof(end_offset));

      // recycle frame
      struct Stack *free_frame = stack;
      SLLStackPop(stack);
      SLLStackPush(free_list, free_frame);

    }

    // advance symbol offset
    cursor += record.consumed;
  }

  if (tree_size_out) {
    *tree_size_out = cursor;
  }

  is_patched = 1;
  exit:;
  scratch_end(scratch);
  return is_patched;
}

internal CV_ChecksumPatchStatus
cv_c13_rebase_checksum_name(U32 name_off, String8 string_data, U64 string_data_base_offset, CV_StringHashTable string_ht, U32 *out)
{
  String8 name;
  if ( ! cv_c13_name_from_string_table(string_data, name_off, &name)) {
    return CV_ChecksumPatchStatus_InvalidName;
  }
  CV_StringBucket *bucket = cv_string_hash_table_lookup(string_ht, name);
  if (bucket == 0) { return CV_ChecksumPatchStatus_MissingString; }
  if (string_data_base_offset > max_U32 || bucket->u.offset > max_U32 - string_data_base_offset) {
    return CV_ChecksumPatchStatus_OffsetOverflow;
  }
  *out = (U32)(string_data_base_offset + bucket->u.offset);
  return CV_ChecksumPatchStatus_Ok;
}

internal CV_ChecksumPatchResult
cv_c13_patch_string_offsets_in_checksum_data(String8 checksums, String8 string_data, U64 string_data_base_offset, CV_StringHashTable string_ht)
{
  CV_ChecksumPatchResult result = {0};
  for (U64 cursor = 0; cursor < checksums.size;) {
    CV_C13Checksum header;
    String8 value;
    U64 next_offset;
    if ( ! cv_c13_checksum_from_data(checksums, cursor, &header, &value, &next_offset)) {
      result.status = CV_ChecksumPatchStatus_InvalidRecord;
      result.offset = cursor;
      break;
    }
    U32 rebased = 0;
    result.status = cv_c13_rebase_checksum_name(header.name_off, string_data, string_data_base_offset, string_ht, &rebased);
    if (result.status != CV_ChecksumPatchStatus_Ok) {
      result.offset   = cursor;
      result.name_off = header.name_off;
      break;
    }
    memory_write32(checksums.str + cursor + OffsetOf(CV_C13Checksum, name_off), rebased);
    cursor = next_offset;
  }
  return result;
}

internal void
cv_c13_patch_string_offsets_in_checksum_list(CV_ChecksumList checksum_list, String8 string_data, U64 string_data_base_offset, CV_StringHashTable string_ht)
{
  for EachNode(node, CV_ChecksumNode, checksum_list.first) {
    U8 *name_ptr = (U8 *)node->data.header + OffsetOf(CV_C13Checksum, name_off);
    U32 rebased = 0;
    if (cv_c13_rebase_checksum_name(memory_read32(name_ptr), string_data, string_data_base_offset, string_ht, &rebased) == CV_ChecksumPatchStatus_Ok) {
      memory_write32(name_ptr, rebased);
    }
  }
}

internal void
cv_c13_patch_checksum_offsets_in_line_data_list(String8List line_data, U64 checksum_rebase)
{
  for EachNode(node, String8Node, line_data.first) {
    for (CV_C13LinesIter it = cv_c13_lines_iter_init(node->string); cv_c13_lines_iter_next(&it);) {
      if (checksum_rebase > max_U32 - (U64)it.file.file_off) { continue; }
      U32 rebased_file_off = (U32)(it.file.file_off + checksum_rebase);
      MemoryCopy(node->string.str + it.file_offset + OffsetOf(CV_C13File, file_off), &rebased_file_off, sizeof(rebased_file_off));
    }
  }
}

internal void
cv_c13_patch_checksum_offsets_in_frame_data_list(String8List frame_data, U32 checksum_rebase)
{
  for EachNode(node, String8Node, frame_data.first) {
    String8          raw_data = node->string;
    U64              count    = raw_data.size / sizeof(CV_C13FrameData);
    CV_C13FrameData *arr      = (CV_C13FrameData *)raw_data.str;
    CV_C13FrameData *ptr      = arr;
    CV_C13FrameData *opl      = arr + count;
    for (; ptr < opl; ptr += 1) {
      U64 rebased_frame_func = ptr->frame_func + checksum_rebase;
      ptr->frame_func = safe_cast_u32(rebased_frame_func);
    }
  }
}
