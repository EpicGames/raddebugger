// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

//
// Registry
//

internal void
cfm_recycle_registry(CFM_Context *ctx, CFM_Registry *registry)
{
  Assert(registry != ctx->registry && ins_atomic_u64_eval(&registry->refs) == 0);
  CFM_RegistryNode *node = CastFromMember(CFM_RegistryNode, v, registry);
  SLLStackPush(ctx->free_registry, node);
}

internal void
cfm_reclaim_registries(CFM_Context *ctx)
{
  for (CFM_RegistryNode **link = &ctx->retired_registry; *link;) {
    CFM_RegistryNode *node = *link;
    if (ins_atomic_u64_eval(&node->v.refs) == 0) {
      *link = node->next;
      cfm_recycle_registry(ctx, &node->v);
    } else {
      link = &node->next;
    }
  }
}

internal CFM_Registry *
cfm_make_registry(CFM_Context *ctx, U64 capacity)
{
  cfm_reclaim_registries(ctx);

  CFM_RegistryNode *n = ctx->free_registry;
  if (n) { SLLStackPop(ctx->free_registry); }

  if (n == 0) {
    n = push_array(ctx->arena, CFM_RegistryNode, 1);
    n->v.arena = arena_alloc();
    SLLStackPush_N(ctx->all_registry, n, all_next);
  }
  n->next = 0;

  CFM_Registry *result = &n->v;
  Assert(ins_atomic_u64_eval(&result->refs) == 0);
  arena_clear(result->arena);
  result->capacity     = capacity;
  result->count        = 0;
  result->maps         = capacity ? push_array_no_zero(result->arena, CFM_Map *, capacity) : 0;
  result->ranges.v     = capacity ? push_array_no_zero(result->arena, Rng1U64, capacity) : 0;
  result->ranges.count = 0;

  return result;
}

internal void
cfm_release_registry(CFM_Context *ctx, CFM_Registry *registry)
{
  NotImplemented;
}

internal CFM_Map *
cfm_find_locked(CFM_Context *ctx, void *address)
{
  CFM_Map *map = NULL;
  if (ctx->registry) {
    U64 range_idx = rng1u64_array_num_from_value__binary_search(&ctx->registry->ranges, IntFromPtr(address));
    map = range_idx ? ctx->registry->maps[range_idx - 1] : NULL;
  }
  return map;
}

//
// Map
//

internal B32
cfm_raw_view(CFM_Map *map, Rng1U64 range, String8 *out)
{
  if (out == NULL) { return 0; }
  *out = str8_zero();

  if (map == NULL || range.min > range.max || range.max > map->compressor_info.v.raw_size) {
    return 0;
  }

  if (range.min == range.max) {
    return 1;
  }

  if (map->codec == NULL || map->codec->raw_view == NULL) {
    return 0;
  }

  String8 view = {0};
  if ( ! map->codec->raw_view(map->encoded, &map->compressor_info, range, &view)) {
    return 0;
  }

  U64 base = IntFromPtr(map->encoded.str), address = IntFromPtr(view.str);
  if (view.size != range.max - range.min ||
      address < base ||
      address - base > map->encoded.size ||
      view.size > map->encoded.size - (address - base)) {
    return 0;
  }

  *out = view;
  return 1;
}

internal CFM_Status
cfm_acquire_locked(CFM_Context *ctx, CFM_Map *map, U64 segment_idx, CFM_Lease *lease_out)
{
  MemoryZeroStruct(lease_out);

  retry:;

  // was the segment already decoded?
  CFM_SegmentKey   segment_key = { map, segment_idx };
  CFM_SegmentInfo *segment     = hash_map_search_string_raw(&ctx->segment_tracker, str8_struct(&segment_key));

  if (segment == NULL) {
    CFM_SegmentNode *segment_node = NULL;
    void            *evict_target = NULL;
    U64              evict_size   = 0;

    // find empty segment
    for (;;) {
      // try free backings
      segment_node = ctx->free_segments;
      if (segment_node) {
        SLLStackPop(ctx->free_segments);
        DLLPushBack(ctx->first_segment, ctx->last_segment, segment_node);
        goto segment_found;
      }

      // try creating new backing if segment fits in the cache
      U64 new_cache_size = ctx->cache_size + map->compressor_info.v.segment_size;
      if (new_cache_size <= ctx->cache_cap) {
        ProfBegin("CFM allocate backing");
        segment_node = push_array(ctx->arena, CFM_SegmentNode, 1);
        segment_node->v.size        = map->compressor_info.v.segment_size;
        segment_node->v.memory      = shared_memory_alloc(map->compressor_info.v.segment_size, str8_zero());
        segment_node->v.write       = shared_memory_view_open(segment_node->v.memory, r1u64s(0, map->compressor_info.v.segment_size));
        segment_node->v.ready_mutex = rw_mutex_alloc();
        segment_node->v.ready_cond  = cond_var_alloc();
        AssertAlways( ! MemoryIsZeroStruct(&segment_node->v.memory));
        AssertAlways(segment_node->v.write != NULL);

        ctx->cache_size += map->compressor_info.v.segment_size;
        AssertAlways(ctx->cache_size <= ctx->cache_cap);

        DLLPushBack(ctx->first_segment, ctx->last_segment, segment_node);

        ProfEnd();
        goto segment_found;
      }

      // try evicting resident backings without a reference
      ProfBegin("CFM evict");
      for EachNode(n, CFM_SegmentNode, ctx->first_segment) {
        if (n->v.refs == 0) {
          segment = &n->v;
          AssertAlways(segment->state == CFM_SegmentState_Live);

          evict_target = segment->map->data.str + segment->index * segment->map->compressor_info.v.segment_size;
          evict_size   = segment->map->compressor_info.v.segment_size;

          ProfBegin("CFM purge tracker");
          B32 is_purged = hash_map_purge_string(&ctx->segment_tracker, str8_struct((&(CFM_SegmentKey){ segment->map, segment->index })));
          ProfEnd();
          AssertAlways(is_purged);

          segment->state = CFM_SegmentState_Null;

          // reused segment becomes the newest resident entry
          DLLRemove(ctx->first_segment, ctx->last_segment, n);
          DLLPushBack(ctx->first_segment, ctx->last_segment, n);

          segment_node = n;
          ProfEnd();
          goto segment_found;
        }
      }
      ProfEnd();

      // no backing available and every resident is referenced
      ProfBegin("CFM wait cache");
      ctx->cache_waiters += 1;
      cond_var_wait_rw_w(ctx->work, ctx->mutex, max_U64);
      ctx->cache_waiters -= 1;
      ProfEnd();
      goto retry;
    }
    segment_found:;

    segment = &segment_node->v;

    // record resident segment info; the backing is pinned (refs > 0) until the caller
    // releases its lease, so the decoder worker may safely operate on it lock-free
    ins_atomic_u32_eval_assign(&segment->state, CFM_SegmentState_Pending);
    segment->refs  = 1;
    segment->index = segment_idx;
    segment->map   = map;

    hash_map_push_string_raw(ctx->arena, &ctx->segment_tracker, str8_struct(&segment_key), segment);

    // hand decode request to the caller, which enqueues it after dropping ctx->mutex
    lease_out->has_job      = 1;
    lease_out->job.map      = map;
    lease_out->job.segment  = segment;
    lease_out->evict_target = evict_target;
    lease_out->evict_size   = evict_size;
  } else {
    // segment already was decoded or being decoded -- increment ref count and leave
    segment->refs += 1;
  }

  lease_out->segment = segment;
  return CFM_Ok;
}

internal void
cfm_lease_release(CFM_Context *ctx, CFM_Lease *lease)
{
  AssertAlways(lease->has_job == 0);
  ProfBegin("CFM release lock wait");
  MutexScopeW(ctx->mutex) {
    ProfEnd();
    CFM_SegmentInfo *segment = lease->segment;

    AssertAlways(segment->refs > 0);
    segment->refs -= 1;

    // this backing has become eligible for eviction; wake threads starved for cache only
    if (segment->refs == 0 && ctx->cache_waiters > 0) {
      cond_var_broadcast(ctx->work);
    }

    MemoryZeroStruct(lease);
  }
}

//
// Worker
//

internal void
cfm_worker(void *raw_ctx)
{
  Temp scratch = scratch_begin(0,0);

  CFM_Worker  *worker = raw_ctx;
  CFM_Context *ctx    = worker->ctx;

  set_thread_namef("CFM Worker [ID %llu]", worker->id);

  Temp decode_temp = temp_begin(scratch.arena);
  U64  decode_scratch_size = 0;
  U8  *decode_scratch_ptr  = 0;

  while (ctx->stop == 0) {
    // read incoming decode job
    RingGuard job_ring = guarded_ring_open(worker->job_ring);
    CFM_Job job = {0};
    if ( ! guarded_ring_read_struct_or_wait(&job_ring, &job, max_U64)) { InvalidPath; }
    guarded_ring_close(&job_ring);

    ProfBegin("CFM job");

    // pick decoder SDK for the job
    CMP_Ctx *sdk = ctx->sdks[job.map->compressor_info.v.sdk_type];

    // estimate decoder scratch memory size & grow on demand
    U64 needed_decode_scratch_size = cmp_scratch(sdk, job.map->compressor_info.v.segment_size);
    if (decode_scratch_size < needed_decode_scratch_size) {
      temp_end(decode_temp);
      decode_scratch_size = needed_decode_scratch_size;
      decode_scratch_ptr  = push_array(decode_temp.arena, U8, decode_scratch_size);
    }

    // compute destination segment size and handle tail segment case
    U64 dst_size = job.segment->index+1 == job.map->compressor_info.v.segment_count
                   ? job.map->compressor_info.v.raw_size - (job.segment->index * job.map->compressor_info.v.segment_size)
                   : job.map->compressor_info.v.segment_size;

    // decode segment
    ProfBegin("Decompressing Segment");
    B32 is_decoded = job.map->codec->decode(sdk,
                                            &job.map->compressor_info,
                                            r1u64s(job.segment->index, 1),
                                            job.map->encoded,
                                            str8(job.segment->write, dst_size),
                                            str8(decode_scratch_ptr, decode_scratch_size));
    AssertAlways(is_decoded); // TODO: handle failed decodes
    ProfEnd();

    // replace placeholder segment with decoded data
    ProfBegin("CFM map decoded");
    void *target = job.map->data.str + job.segment->index * job.map->compressor_info.v.segment_size;

    // first decode of this segment: carve its placeholder out of the map's reservation.
    // this worker is the segment's sole owner until it goes live, so its bit is only read
    // and set by the owner; the set happens under carve_mutex, which also serializes the
    // splits (the kernel fails concurrent splits of the same placeholder) and keeps
    // neighbouring bits written by other workers from racing on the same word
    if ( ! bit_array_get_bit32(job.map->carved, job.segment->index)) {
      ProfBegin("CFM carve placeholder");
      B32 is_carved = 0;
      MutexScope(ctx->carve_mutex) {
        is_carved = carve_memory_placeholder(target, job.map->compressor_info.v.segment_size);
        if (is_carved) {
          bit_array_set_bit32(job.map->carved, job.segment->index, 1);
        }
      }
      ProfEnd();
      AssertAlways(is_carved); // TODO: handle failed placeholder split
    }

    void *replace_result = shared_memory_view_replace_placeholder(job.segment->memory, target, r1u64(0, job.map->compressor_info.v.segment_size), AccessFlag_Read);
    if (target != replace_result) {
      // TODO: handle failed placeholder replace
      NotImplemented;
    }
    ProfEnd();

    // finalize job
    ProfBegin("CFM finalize job");
    rw_mutex_take_w(job.segment->ready_mutex);
    ins_atomic_u32_eval_assign(&job.segment->state, CFM_SegmentState_Live);
    cond_var_broadcast(job.segment->ready_cond);
    rw_mutex_drop_w(job.segment->ready_mutex);
    ProfEnd();

    ProfEnd();
  }

  scratch_end(scratch);
}

internal CFM_Context *
cfm_alloc(U64        cache_cap,
          U64        prefetch_bytes,
          U64        worker_count,
          U64        codecs_count,
          CFM_Codec *codecs,
          String8    sdk_paths[CMP_SdkType_Count])
{
  AssertAlways(memory_placeholders_supported());
  AssertAlways(cache_cap >= CFM_MIN_CACHE_SIZE);
  AssertAlways(worker_count > 0);
  ProfMsg("CFM config cache=%llu workers=%llu prefetch=%llu", cache_cap, worker_count, prefetch_bytes);

  // alloc context
  Arena *arena = arena_alloc();
  CFM_Context *ctx = push_array(arena, CFM_Context, 1);
  ctx->cache_cap      = cache_cap;
  ctx->prefetch_bytes = prefetch_bytes;
  ctx->worker_count   = worker_count;
  ctx->codecs_count   = codecs_count;
  ctx->codecs         = codecs;
  MemoryCopyArray(ctx->sdk_paths, sdk_paths);
  ctx->arena          = arena;
  ctx->mutex          = rw_mutex_alloc();
  ctx->carve_mutex    = mutex_alloc();
  ctx->work           = cond_var_alloc();
  ctx->workers_ctxs   = push_array(arena, CFM_Worker, worker_count);

  // load compression SDKs
  for EachIndex(sdk_type, CMP_SdkType_Count) {
    char *open_error = cmp_open(ctx->arena, sdk_type, sdk_paths[sdk_type], &ctx->sdks[sdk_type]);
    AssertAlways(open_error == NULL);
  }

  // create background decode workers
  for EachIndex(i, worker_count) {
    CFM_Worker *worker  = &ctx->workers_ctxs[i];
    worker->ctx      = ctx;
    worker->job_ring = guarded_ring_alloc(ctx->arena, sizeof(CFM_Job) * 128);
    worker->id       = i;
    worker->handle   = thread_launch(cfm_worker, worker);
    AssertAlways( ! MemoryIsZeroStruct(&worker->handle));
  }

  return ctx;
}

internal ASAN_NO_ADDR B32
cfm_read_fault(void *address, void *user_data)
{
  ProfBegin("CFM fault");
  B32 was_memory_paged_in = 0;

  CFM_Context *ctx = user_data;

  // acquire segment lease under the global lock; no syscalls or waits happen while it's held
  CFM_Lease  lease  = {0};
  CFM_Status status = CFM_Unknown;
  ProfBegin("CFM fault lock wait");
  rw_mutex_take_w(ctx->mutex);
  ProfEnd();
  {
    CFM_Map *map = cfm_find_locked(ctx, address);
    if (map) {
      // acquire segment lease, which forces the segment to be decoded if it's not ready
      U64 offset      = IntFromPtr(address) - IntFromPtr(map->data.str);
      U64 segment_idx = offset / map->compressor_info.v.segment_size;
      ProfBegin("CFM acquire");
      status = cfm_acquire_locked(ctx, map, segment_idx, &lease);
      ProfEnd();
    }
  }
  rw_mutex_drop_w(ctx->mutex);

  if (status == CFM_Ok) {
    // send decode request if we were the first to touch the segment
    if (lease.has_job) {
      AssertAlways(lease.job.segment->refs > 0);

      if (lease.evict_target) {
        ProfBegin("CFM unmap");
        B32 is_unmapped = unmap_memory_preserve_placeholder(lease.evict_target, lease.evict_size);
        ProfEnd();
        AssertAlways(is_unmapped);
        lease.evict_target = 0;
        lease.evict_size   = 0;
      }

      // round-robin jobs between workers
      U64         worker_idx = ins_atomic_u64_inc_eval(&ctx->next_job_worker);
      CFM_Worker *worker     = &ctx->workers_ctxs[worker_idx % ctx->worker_count];

      ProfBegin("CFM enqueue");
      RingGuard guard = guarded_ring_open(worker->job_ring);
      B32 is_queued = guarded_ring_write_struct_or_wait(&guard, &lease.job, max_U64);
      guarded_ring_close(&guard);
      AssertAlways(is_queued);
      ProfEnd();

      lease.has_job = 0;
    }

    // wait for the segment to be decoded
    {
      CFM_SegmentInfo *segment = lease.segment;
      if (ins_atomic_u32_eval(&segment->state) != CFM_SegmentState_Live) {
        ProfBegin("CFM wait decode");
        rw_mutex_take_r(segment->ready_mutex);
        while (ins_atomic_u32_eval(&segment->state) != CFM_SegmentState_Live) {
          cond_var_wait_rw_r(segment->ready_cond, segment->ready_mutex, max_U64);
        }
        rw_mutex_drop_r(segment->ready_mutex);
        ProfEnd();
      }
    }

    // release the segment lease
    cfm_lease_release(ctx, &lease);
    was_memory_paged_in = 1;
  }

  ProfEnd();
  return was_memory_paged_in;
}

internal B32
cfm_install_fault_handler(CFM_Context *ctx)
{
  if ( ! ctx->handler_installed) {
    ctx->handler_installed = memory_read_fault_handler_set(cfm_read_fault, ctx);
  }
  return ctx->handler_installed;
}

internal CFM_Map *
cfm_map_from_address(CFM_Context *ctx, void *address)
{
  if (ctx == NULL) { return 0; }
  rw_mutex_take_r(ctx->mutex);
  CFM_Map *map = cfm_find_locked(ctx, address);
  rw_mutex_drop_r(ctx->mutex);
  return map;
}

internal force_inline int
cfm_map_address_is_before(void *a, void *b)
{
  CFM_Map *map_a = *(CFM_Map **)a, *map_b = *(CFM_Map **)b;
  if (map_a == 0 || map_b == 0) { return map_a == 0 && map_b != 0; }
  return IntFromPtr(map_a->data.str) < IntFromPtr(map_b->data.str);
}

internal CFM_Status
cfm_map_prepare(CFM_Context *ctx, File file, CFM_Map **out)
{
  CFM_Status status = CFM_Unknown;
  *out = 0;

  // open file
  FileMap storage = file_map_open(AccessFlag_Read, file);
  if (MemoryIsZeroStruct(&storage)) {
    status = CFM_Unknown;
    goto close_storage;
  }

  // create file view
  ProfBegin("CFM prepare: view");
  FileProperties file_props = properties_from_file(file);
  void *file_view = file_map_view_open(storage, AccessFlag_Read, r1u64(0, file_props.size));
  ProfEnd();
  if (file_view == NULL) {
    status = CFM_MapFailure;
    goto close_storage;
  }
  String8 file_raw = str8(file_view, file_props.size);

  // infer CFM codec from file view
  CFM_Codec *codec = 0;
  for EachIndex(codec_idx, ctx->codecs_count) {
    CFM_Codec *candidate = ctx->codecs + codec_idx;
    if (candidate->matches(file_raw)) {
      codec = candidate;
      break;
    }
  }
  if (codec == 0) {
    status = CFM_InvalidFormat;
    goto close_view;
  }

  // unpack compressor info
  ProfBegin("CFM prepare: validate");
  CMP_CompressorInfoUnpacked compressor_info = {0};
  B32 is_compressor_ok = codec->compressor_info(file_raw, &compressor_info);
  ProfEnd();
  if ( ! is_compressor_ok) {
    status = CFM_InvalidFormat;
    goto close_view;
  }

  // verify segment size
  if (compressor_info.v.segment_size == 0 || ( ! IsPow2(compressor_info.v.segment_size))) {
    status = CFM_InvalidFormat;
    goto close_view;
  }
  ins_atomic_u64_eval_cond_assign(&ctx->first_segment_size, compressor_info.v.segment_size, 0);
  if (ins_atomic_u64_eval(&ctx->first_segment_size) != compressor_info.v.segment_size) {
    status = CFM_MixedSegmentSizes;
    goto close_view;
  }
  if (compressor_info.v.segment_size > ctx->cache_cap) {
    status = CFM_OutOfMemory;
    goto close_view;
  }

  U64 granularity = get_system_info()->allocation_granularity;
  AssertAlways(compressor_info.v.segment_size % granularity == 0); // TODO: error handling

  // reserve the decompressed file's address range as a single placeholder; segments are
  // carved out of it on first decode (each split is a serialized kernel call, and only a
  // fraction of segments is ever touched)
  ProfMsg("CFM map raw=%llu encoded=%llu segments=%llu segment_size=%u", compressor_info.v.raw_size, file_raw.size, compressor_info.v.segment_count, compressor_info.v.segment_size);
  U64      placeholders_size = compressor_info.v.segment_count * compressor_info.v.segment_size;
  ProfBegin("CFM prepare: reserve placeholders");
  void    *placeholders_ptr  = reserve_memory_placeholders(placeholders_size, placeholders_size);
  ProfEnd();
  String8  placeholders_data = str8(placeholders_ptr, placeholders_size);
  AssertAlways(placeholders_ptr); // TODO: error handling

  MutexScopeW(ctx->mutex) {
    CFM_MapNode *map_node = ctx->free_maps;
    if (map_node) { SLLStackPop(ctx->free_maps);                       }
    else          { map_node = push_array(ctx->arena, CFM_MapNode, 1); }
    MemoryZeroStruct(map_node);

    CFM_Map *map = &map_node->v;
    map->context         = ctx;
    map->storage         = storage;
    map->encoded         = file_raw;
    map->codec           = codec;
    map->reserve_size    = placeholders_size;
    map->data            = placeholders_data;
    map->carved          = bit_array_init32(ctx->arena, compressor_info.v.segment_count);
    map->compressor_info = compressor_info;

    *out = map;
  }

  return CFM_Ok;

  close_view:;
  file_map_view_close(storage, file_view, r1u64(0, file_props.size));
  close_storage:;
  file_map_close(storage);
  return status;
}

internal void
cfm_map_register_batch(CFM_Context *ctx, U64 count, CFM_Map **maps)
{
  if (count == 0) { return; }

  MutexScopeW(ctx->mutex) {
    if (ctx->registry == NULL) {
      U64 capacity = Max(count * 2, 256);
      ctx->registry = cfm_make_registry(ctx, capacity);
    }

    if (ctx->registry->refs > 0) {
      // TODO: registry is pinned by a thread and should be released after thread finishes using it
      NotImplemented;
    }

    CFM_Registry *registry = ctx->registry;
    U64 new_count = registry->count + count;
    if (new_count > registry->capacity) {
      CFM_Registry *old = registry;
      AssertAlways(old->refs == 0);

      U64 capacity = old->capacity <= max_U64 / 2
        ? Max(new_count, old->capacity * 2)
        : new_count;

      AssertAlways(capacity <= max_U64 / sizeof(CFM_Map *));
      AssertAlways(capacity <= max_U64 / sizeof(Rng1U64));

      registry = cfm_make_registry(ctx, capacity);
      MemoryCopyTyped(registry->maps, old->maps, old->count);
      registry->count = old->count;

      ctx->registry = registry;
      cfm_recycle_registry(ctx, old);
    }

    // append batch & sort file maps by address
    MemoryCopyTyped(registry->maps + registry->count, maps, count);
    registry->count += count;
    radsort(registry->maps, registry->count, cfm_map_address_is_before);

    // fill out file maps lookup ranges
    registry->ranges.count = registry->count;
    for EachIndex(i, registry->count) {
      registry->ranges.v[i] = r1u64s(IntFromPtr(registry->maps[i]->data.str), registry->maps[i]->data.size);
    }
  }
}

internal CFM_Map **
cfm_map_open_batch(Arena *arena, CFM_Context *ctx, U64 file_count, File *files, CFM_Status *out_status)
{
  Temp scratch = scratch_begin(&arena, 1);

  CFM_Map **result = push_array(arena, CFM_Map *, file_count);
  CFM_Map **batch  = push_array(scratch.arena, CFM_Map *, file_count);
  U64 batch_count = 0;

  for EachIndex(file_idx, file_count) {
    out_status[file_idx] = cfm_map_prepare(ctx, files[file_idx], &result[file_idx]);
    if (out_status[file_idx] == CFM_Ok) {
      batch[batch_count++] = result[file_idx];
    }
  }

  cfm_map_register_batch(ctx, batch_count, batch);

  scratch_end(scratch);
  return result;
}

internal CFM_Status
cfm_map_open(CFM_Context *ctx, File file, CFM_Map **out)
{
  Temp scratch = scratch_begin(0,0);
  CFM_Status status;
  CFM_Map **maps = cfm_map_open_batch(scratch.arena, ctx, 1, &file, &status);
  *out = *maps;
  scratch_end(scratch);
  return status;
}

internal void
cfm_map_close(CFM_Map *map)
{
  Temp scratch = scratch_begin(0,0);

  CFM_Context *ctx           = map->context;
  U64          segment_size  = map->compressor_info.v.segment_size;
  U64          segment_count = map->compressor_info.v.segment_count;

  // detach resident segments from the cache & unregister the map
  void **views      = push_array_no_zero(scratch.arena, void *, segment_count);
  U64    view_count = 0;
  MutexScopeW(ctx->mutex) {
    for EachIndex(segment_idx, segment_count) {
      CFM_SegmentKey   key     = { map, segment_idx };
      CFM_SegmentInfo *segment = hash_map_search_string_raw(&ctx->segment_tracker, str8_struct(&key));
      if (segment) {
        AssertAlways(segment->refs == 0);
        AssertAlways(segment->state == CFM_SegmentState_Live);

        B32 is_purged = hash_map_purge_string(&ctx->segment_tracker, str8_struct(&key));
        AssertAlways(is_purged);

        // backing goes to the free list and keeps its shared memory & sync objects for reuse
        ins_atomic_u32_eval_assign(&segment->state, CFM_SegmentState_Null);
        segment->map   = 0;
        segment->index = 0;
        CFM_SegmentNode *node = CastFromMember(CFM_SegmentNode, v, segment);
        DLLRemove(ctx->first_segment, ctx->last_segment, node);
        SLLStackPush(ctx->free_segments, node);

        views[view_count++] = map->data.str + segment_idx * segment_size;
      }
    }

    // remove from the address registry so a stray fault in this range can no longer resolve to the map
    CFM_Registry *registry = ctx->registry;
    if (registry) {
      if (registry->refs > 0) {
        // TODO: registry is pinned by a thread and should be released after thread finishes using it
        NotImplemented;
      }
      U64 num = rng1u64_array_num_from_value__binary_search(&registry->ranges, IntFromPtr(map->data.str));
      if (num > 0 && registry->maps[num-1] == map) {
        U64 idx  = num - 1;
        U64 tail = registry->count - (idx + 1);
        MemoryCopyTyped(registry->maps + idx,     registry->maps + idx + 1,     tail);
        MemoryCopyTyped(registry->ranges.v + idx, registry->ranges.v + idx + 1, tail);
        registry->count         -= 1;
        registry->ranges.count  -= 1;
      }
    }
  }

  // tear down the decoded address range: mapped views back to placeholders, then release the reservation
  ProfBegin("CFM close unmap views");
  for EachIndex(i, view_count) {
    B32 is_unmapped = unmap_memory_preserve_placeholder(views[i], segment_size);
    AssertAlways(is_unmapped);
  }
  ProfEnd();

  ProfBegin("CFM close release placeholders");
  B32 is_released = release_memory_placeholders(map->data.str, map->reserve_size, segment_size);
  AssertAlways(is_released);
  ProfEnd();

  // close the encoded file view & mapping
  ProfBegin("CFM close storage");
  file_map_view_close(map->storage, map->encoded.str, r1u64(0, map->encoded.size));
  file_map_close(map->storage);
  ProfEnd();

  // recycle map node
  MutexScopeW(ctx->mutex) {
    CFM_MapNode *map_node = CastFromMember(CFM_MapNode, v, map);
    MemoryZeroStruct(&map_node->v);
    SLLStackPush(ctx->free_maps, map_node);
  }

  scratch_end(scratch);
}

