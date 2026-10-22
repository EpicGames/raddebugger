// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////

#pragma once

////////////////////////////////

#define CFM_MIN_CACHE_SIZE   KB(64)
#define CFM_MIN_SEGMENT_SIZE KB(4)

////////////////////////////////

typedef struct CFM_Context CFM_Context;

typedef struct
{
  CFM_Context *context;
  FileMap      storage;
  String8      encoded;
  String8      data;
  U64          reserve_size;
  CFM_Codec   *codec;
  CMP_CompressorInfoUnpacked compressor_info;
} CFM_Map;

typedef U32 CFM_SegmentState;
typedef enum
{
  CFM_SegmentState_Null,
  CFM_SegmentState_Pending,
  CFM_SegmentState_Live,
} CFM_SegmentStateEnum;

typedef struct
{
  CFM_SegmentState  state;
  U64               index;
  CondVar          *ready_cond;
  SharedMemory      memory;
  U8               *write;
  U64               size;
  U64               refs;
  CFM_Map          *map;
} CFM_SegmentInfo;

typedef struct CFM_SegmentNode CFM_SegmentNode;
struct CFM_SegmentNode
{
  CFM_SegmentInfo  v;
  CFM_SegmentNode *next;
  CFM_SegmentNode *prev;
};

typedef struct
{
  CFM_SegmentInfo *segment;
} CFM_Lease;

typedef struct
{
  Arena         *arena;
  U64            refs;
  U64            capacity;
  U64            count;
  CFM_Map      **maps;
  Rng1U64Array   ranges;
} CFM_Registry;

typedef struct CFM_RegistryNode CFM_RegistryNode;
struct CFM_RegistryNode
{
  CFM_RegistryNode *next;
  CFM_RegistryNode *all_next;
  CFM_Registry      v;
};

typedef struct CFM_MapNode CFM_MapNode;
struct CFM_MapNode
{
  CFM_Map v;
  CFM_MapNode *next;
};

typedef struct
{
  CFM_Map         *map;
  CFM_SegmentInfo *segment;
} CFM_Job;

typedef struct CFM_CondVar CFM_CondVar;
struct CFM_CondVar
{
  CFM_CondVar *next;
  CondVar v;
};

typedef struct
{
  CFM_Map *map;
  U64      segment_idx;
} CFM_SegmentKey;

typedef struct
{
  CFM_Context *ctx;
  GuardedRing *job_ring;
  U64          id;
  Thread       handle;
} CFM_Worker;

struct CFM_Context
{
  U64         cache_size;
  U64         cache_cap;
  U64         prefetch_bytes;
  U64         worker_count;
  U64         codecs_count;
  CFM_Codec  *codecs;
  String8     sdk_paths[CMP_SdkType_Count];
  CMP_Ctx    *sdks[CMP_SdkType_Count];
  Arena      *arena;
  Mutex       mutex;
  CFM_Worker *workers_ctxs;
  CondVar     work;

  U64 next_job_worker;
  B32 stop;
  B32 handler_installed;

  CFM_CondVar *free_cond_vars;

  CFM_Registry     *registry;
  CFM_RegistryNode *all_registry;
  CFM_RegistryNode *retired_registry;
  CFM_RegistryNode *free_registry;

  CFM_MapNode *free_maps;
  CFM_MapNode *maps_first;
  CFM_MapNode *maps_last;

  CFM_SegmentNode *first_segment;
  CFM_SegmentNode *last_segment;
  CFM_SegmentNode *free_segments;

  HashMap segment_tracker; // (CFM_Map, segment_index) -> CFM_Segment

  U64 first_segment_size; // first observed segment size
};

internal CFM_Context * cfm_alloc(U64 cache_cap, U64 prefetch_bytes, U64 worker_count, U64 codecs_count, CFM_Codec *codecs, String8 sdk_paths[CMP_SdkType_Count]);
internal B32 cfm_install_fault_handler(CFM_Context *ctx);

internal void cfm_lease_release(CFM_Context *ctx, CFM_Lease *lease);
