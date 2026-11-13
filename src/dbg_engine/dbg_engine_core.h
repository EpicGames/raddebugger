// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef DBG_ENGINE_CORE_H
#define DBG_ENGINE_CORE_H

////////////////////////////////
//~ NOTE(rjf): On entity IDs & handles:
//
// Debug entities can be uniquely referred to in a number of ways, and within
// different contexts. A D_Handle is a way to uniquely refer to an entity across
// all connected servers / controllers, and to contain a payload which can be
// sent directly to servers / controllers. Controllers communicate unique entity
// IDs via 64-bit payloads, e.g. DMN_Handles. D_Handles just bundle those handles,
// and the necessary server/controller IDs, so that you can pick a server, and
// communicate the 64-bit payload (e.g. DMN_Handle) to the server directly.
// You don't need any debug entity state for this, because all of the information
// is just in the handle payload itself.
//
// This is distinct from a D_EntityID, which is a 32-bit value, which uniquely
// identifies all entities across all servers, *but* only for the debug engine.
// These IDs are not meaningful to servers / controllers, they are just
// monotonically increasing IDs that the debug engine assigns to entities, so that
// these entities can be uniquely referred to in a much lighter way than storing
// a whole handle. As such, if you have a D_EntityID, and you need to find a
// D_Handle, you need debug entity state. These are used for uniquely identifying
// memory / register spaces during evaluation, for example.
//
// Both of these are distinct from entity OS IDs, which are basically just TID/PIDs -
// they uniquely refer to entities within a single kernel, and they are useful to
// report, but they don't solve either of the above two cases, so they aren't used
// for much in the debug engine.
//
// In short:
//
// DMN_Handle -> uniquely refers to an entity to one controller instance - may collide across controllers
// D_Handle -> uniquely refers to an entity across all controllers - heavier, does not require entity state
// D_EntityID -> uniquely refers to an entity across all controllers - lighter, does require entity state
// U64 os_id -> TIDs/PIDs, uniquely refers to an entity on one kernel

////////////////////////////////
//~ rjf: ID Types

typedef U64 D_MsgID;
typedef U32 D_ServerID;
typedef U32 D_ControllerKind;
typedef enum D_ControllerKindEnum
{
  D_ControllerKind_Demon,
  D_ControllerKind_Dump,
}
D_ControllerKindEnum;

#define D_ServerID_LocalHost (1)

#if 0
// TODO(rjf): instead of moving to controller/server IDs, move to single ID, something like this.
// some decisions need to be made about how this shows up in the entity tree - e.g. local demon DLLs
// can't be detected as being on another server.
typedef U32 D_ControllerID;
typedef enum D_ControllerIDEnum
{
  D_ControllerID_LocalDemon,
  D_ControllerID_DumpFile,
  D_ControllerID_FirstDynamic, // all servers and/or control DLLs start here
}
D_ControllerIDEnum;
#endif

#define d_packet_msg_id(ring_id, lower32) (((U64)(ring_id)<<32)|(lower32&0xffffffff))
#define d_packet_ring_id_from_msg_id(msg_id) (((msg_id)&0xffffffff00000000ull)>>32)

////////////////////////////////
//~ rjf: Entity Handle Types

typedef struct D_Handle D_Handle;
struct D_Handle
{
  D_ServerID server_id;
  D_ControllerKind controller_kind;
  U64 control_id;
};

typedef struct D_HandleNode D_HandleNode;
struct D_HandleNode
{
  D_HandleNode *next;
  D_Handle v;
};

typedef struct D_HandleList D_HandleList;
struct D_HandleList
{
  D_HandleNode *first;
  D_HandleNode *last;
  U64 count;
};

typedef struct D_HandleArray D_HandleArray;
struct D_HandleArray
{
  D_Handle *v;
  U64 count;
};

////////////////////////////////
//~ rjf: @eval2 Evaluation Types

typedef U8 D_SpaceKind;
typedef enum D_SpaceKindEnum
{
  D_SpaceKind_Null,
  D_SpaceKind_Memory,
  D_SpaceKind_Registers,
  D_SpaceKind_ContentKey,
  D_SpaceKind_COUNT
}
D_SpaceKindEnum;

#define d_space_kind_from_id(id)             (((id)&0xff00000000000000ull)>>56)
#define d_index_from_space_id(id)            (d_space_kind_from_id(id) == D_SpaceKind_Registers ? (((id)&0x00ffffff00000000ull)>>32) : 0)
#define d_entity_id_from_space_id(id)        ((d_space_kind_from_id(id) == D_SpaceKind_Registers || d_space_kind_from_id(id) == D_SpaceKind_Memory) ? ((id)&0xffffffffull) : 0)
#define d_content_key_id_from_space_id(id)   (d_space_kind_from_id(id) == D_SpaceKind_ContentKey ? ((id)&0x00ffffffffffffffull) : 0)

#define d_memory_space_id(entity_id) ((((U64)D_SpaceKind_Memory)<<56)|(entity_id&0xffffffffull))
#define d_registers_space_id(entity_id, unwind_index) ((((U64)D_SpaceKind_Registers)<<56)|(((unwind_index)&0xffffffull)<<32)|((entity_id)&0xffffffffull))
#define d_content_key_space_id(content_key_id) ((((U64)D_SpaceKind_ContentKey)<<56)|((content_key_id)&0x00ffffffffffffffull))

typedef enum D_EvalCtxIDKind
{
  D_EvalCtxIDKind_DebugEntitiesMetadata,
  D_EvalCtxIDKind_DebugEntities,
  D_EvalCtxIDKind_Registers,
}
D_EvalCtxIDKind;

typedef U64 D_EvalID;

typedef struct D_EvalIDNode D_EvalIDNode;
struct D_EvalIDNode
{
  D_EvalIDNode *next;
  D_EvalID v;
};

typedef struct D_Eval D_Eval;
struct D_Eval
{
  D_EvalID id;
  D_EvalID parent_id;
  String8 string;
  E2_Expr *expr;
  E2_IRNode *irtree;
  String8 bytecode;
  E2_CtxID ctx_id;
  E2_Val val;
  E2_MsgList msgs;
};

typedef struct D_EvalNode D_EvalNode;
struct D_EvalNode
{
  D_EvalNode *string_next;
  D_EvalNode *id_next;
  D_Eval eval;
};

typedef struct D_EvalSlot D_EvalSlot;
struct D_EvalSlot
{
  D_EvalNode *first;
  D_EvalNode *last;
};

typedef struct D_EvalCache D_EvalCache;
struct D_EvalCache
{
  Arena *arena;
  
  // rjf: per evaluation phase context
  U64 async_endt_us;
  D_Handle selected_thread;
  U64 selected_unwind_idx;
  
  // rjf: evaluation phase reset marker
  U64 arena_eval_start_pos;
  
  // rjf: per evaluation phase dynamic state
  D_EvalID id_gen;
  E2_SpaceID space_id_gen;
  U64 slots_count;
  D_EvalSlot *string_slots;
  D_EvalSlot *id_slots;
  D_EvalIDNode nil_parent;
  D_EvalIDNode *top_parent;
  D_EvalIDNode *free_parent;
  E2_ConsTypeMap *cons_type_map;
};

////////////////////////////////
//~ rjf: Generated Code

#include "generated/dbg_engine.meta.h"

////////////////////////////////
//~ rjf: User Breakpoint Types

typedef U32 D_BreakpointFlags;
enum
{
  D_BreakpointFlag_BreakOnWrite            = (1<<0),
  D_BreakpointFlag_BreakOnRead             = (1<<1),
  D_BreakpointFlag_BreakOnExecute          = (1<<2),
  D_BreakpointFlag_BreakSelectedThreadOnly = (1<<3),
};

typedef struct D_Breakpoint D_Breakpoint;
struct D_Breakpoint
{
  D_BreakpointFlags flags;
  U64 id;
  String8 file_path;
  TxtPt pt;
  String8 vaddr_expr;
  String8 condition;
  U64 size;
};

typedef struct D_BreakpointArray D_BreakpointArray;
struct D_BreakpointArray
{
  D_Breakpoint *v;
  U64 count;
};

typedef struct D_BreakpointNode D_BreakpointNode;
struct D_BreakpointNode
{
  D_BreakpointNode *next;
  D_Breakpoint v;
};

typedef struct D_BreakpointList D_BreakpointList;
struct D_BreakpointList
{
  D_BreakpointNode *first;
  D_BreakpointNode *last;
  U64 count;
};

////////////////////////////////
//~ rjf: Entity Types

typedef U32 D_EntityID;

typedef struct D_Entity D_Entity;
struct D_Entity
{
  D_Entity *first;
  D_Entity *last;
  D_Entity *next;
  D_Entity *prev;
  D_Entity *parent;
  D_EntityKind kind;
  D_EntityID id;
  Arch arch;
  B32 is_frozen;
  B32 is_soloed;
  U32 rgba;
  D_Handle handle;
  U64 os_id;
  Rng1U64 vaddr_range;
  U64 stack_base;
  U64 timestamp;
  D_BreakpointFlags bp_flags;
  String8 string;
  U64 tls_root_vaddr;
  OperatingSystem os;
  D_MsgID src_msg_id;
};

typedef struct D_EntityNode D_EntityNode;
struct D_EntityNode
{
  D_EntityNode *next;
  D_Entity *v;
};

typedef struct D_EntityList D_EntityList;
struct D_EntityList
{
  D_EntityNode *first;
  D_EntityNode *last;
  U64 count;
};

typedef struct D_EntityArray D_EntityArray;
struct D_EntityArray
{
  D_Entity **v;
  U64 count;
};

typedef struct D_EntityRec D_EntityRec;
struct D_EntityRec
{
  D_Entity *next;
  S32 push_count;
  S64 pop_count;
};

typedef struct D_EntityHashNode D_EntityHashNode;
struct D_EntityHashNode
{
  D_EntityHashNode *next;
  D_EntityHashNode *prev;
  D_Entity *entity;
};

typedef struct D_EntityHashSlot D_EntityHashSlot;
struct D_EntityHashSlot
{
  D_EntityHashNode *first;
  D_EntityHashNode *last;
};

typedef struct D_EntityStringChunkNode D_EntityStringChunkNode;
struct D_EntityStringChunkNode
{
  D_EntityStringChunkNode *next;
  U64 size;
};

read_only global U64 d_entity_string_bucket_chunk_sizes[] =
{
  16,
  64,
  256,
  1024,
  4096,
  16384,
  65536,
  0xffffffffffffffffull,
};

typedef struct D_EntityCtx D_EntityCtx;
struct D_EntityCtx
{
  D_Entity *root;
  U64 hash_slots_count;
  D_EntityHashSlot *hash_slots;
  U64 id_slots_count;
  D_EntityHashSlot *id_slots;
  U64 entity_kind_counts[D_EntityKind_COUNT];
  U64 entity_kind_alloc_gens[D_EntityKind_COUNT];
};

typedef struct D_EntityCtxRWStore D_EntityCtxRWStore;
struct D_EntityCtxRWStore
{
  Arena *arena;
  D_EntityCtx ctx;
  D_Entity *free;
  D_EntityID entity_id_gen;
  D_EntityHashNode *hash_node_free;
  D_EntityStringChunkNode *free_string_chunks[ArrayCount(d_entity_string_bucket_chunk_sizes)];
};

typedef struct D_EntityCtxLookupAccel D_EntityCtxLookupAccel;
struct D_EntityCtxLookupAccel
{
  Arena *arena;
  Arena *entity_kind_arrays_arenas[D_EntityKind_COUNT];
  D_EntityArray entity_kind_arrays[D_EntityKind_COUNT];
  U64 entity_kind_arrays_gens[D_EntityKind_COUNT];
};

////////////////////////////////
//~ rjf: Tick Input Types

typedef struct D_Target D_Target;
struct D_Target
{
  String8 exe;
  String8 args;
  String8 working_directory;
  String8 custom_entry_point_name;
  String8 stdout_path;
  String8 stderr_path;
  String8 stdin_path;
  String8 remote_address;
  D_MsgID msg_id;
  B32 debug_subprocesses;
  B32 deploy;
  String8List env;
};

typedef struct D_TargetArray D_TargetArray;
struct D_TargetArray
{
  D_Target *v;
  U64 count;
};

typedef struct D_PathMap D_PathMap;
struct D_PathMap
{
  String8 src;
  String8 dst;
};

typedef struct D_PathMapArray D_PathMapArray;
struct D_PathMapArray
{
  D_PathMap *v;
  U64 count;
};

////////////////////////////////
//~ rjf: Trap Types

typedef U32 D_TrapFlags;
enum
{
  D_TrapFlag_IgnoreStackPointerCheck = (1<<0),
  D_TrapFlag_SingleStepAfterHit      = (1<<1),
  D_TrapFlag_SaveStackPointerBefore  = (1<<2),
  D_TrapFlag_SaveStackPointerAfter   = (1<<3),
  D_TrapFlag_BeginSpoofMode          = (1<<4),
  D_TrapFlag_EndStepping             = (1<<5),
};

typedef struct D_Trap D_Trap;
struct D_Trap
{
  D_TrapFlags flags;
  U64 vaddr;
};

typedef struct D_TrapNode D_TrapNode;
struct D_TrapNode
{
  D_TrapNode *next;
  D_Trap v;
};

typedef struct D_TrapList D_TrapList;
struct D_TrapList
{
  D_TrapNode *first;
  D_TrapNode *last;
  U64 count;
};

typedef struct D_Spoof D_Spoof;
struct D_Spoof
{
  D_Handle process;
  D_Handle thread;
  U64 vaddr;
  U64 new_ip_value;
};

////////////////////////////////
//~ rjf: Trap Nets

typedef struct D_TrapNet D_TrapNet;
struct D_TrapNet
{
  D_TrapList traps;
  B32 good_line_info;
  B32 good_read;
};

////////////////////////////////
//~ rjf: Debug Engine Core Shared State

typedef struct D_String2NumNode D_String2NumNode;
struct D_String2NumNode
{
  D_String2NumNode *next;
  String8 string;
  U64 num;
};

typedef struct D_String2NumTable D_String2NumTable;
struct D_String2NumTable
{
  D_String2NumNode **slots;
  U64 slots_count;
};

typedef struct D_Shared D_Shared;
struct D_Shared
{
  Arena *arena;
  D_String2NumTable arch_string2regcode_tables[Arch_COUNT];
  CFG_SchemaTable entity_schema_table;
  CFG_SchemaTable cfg_schema_table;
};

////////////////////////////////
//~ rjf: Globals

global D_Shared *d_shared = 0;
read_only global D_String2NumTable d_nil_string2num_table = {0};
thread_static D_EvalCache *d_eval_cache = 0;

////////////////////////////////
//~ rjf: @eval2 Bundled Evaluation Path

//- rjf: helpers
internal U64 d_num_from_string(D_String2NumTable *map, String8 string);

//- rjf: async locals map building
internal AC_Artifact d_locals_map_artifact_create(String8 key, B32 *cancel_signal, AC_Status *status_out, U64 *gen_out);
internal void d_locals_map_artifact_destroy(AC_Artifact artifact);
internal D_String2NumTable *d_locals_map_from_dbgi_key_voff(Access *access, DI_Key dbgi_key, U64 voff, U64 endt_us);

//- rjf: eval building helpers
internal E2_Val d_eval_val_from_bytecode(Arena *arena, String8 bytecode, E2_CtxID *ctx_id_out, E2_MsgList *msgs_out);
internal E2_Val d_eval_val_from_irtree(Arena *arena, E2_IRNode *irtree, E2_CtxID *ctx_id_out, E2_MsgList *msgs_out);
internal D_Handle d_handle_from_irtree(Arena *arena, E2_IRNode *irtree, E2_MsgList *msgs_out);
internal ARCH_RegCode d_eval_reg_code_from_arch_string(Arch arch, String8 string);
internal E2_IRNode *d_eval_irtree_from_arch_regcode(Arena *arena, D_Handle thread, U64 frame_idx, Arch arch, ARCH_RegCode reg_code);
internal E2_IRNode *d_eval_irtree_from_location(Arena *arena, RDI_Parsed *rdi, RDI_Location location, U64 ctx_voff);
internal E2_IRNode *d_eval_irtree_from_thread_unwind_idx_local_string(Arena *arena, D_Handle thread, U64 unwind_idx, String8 name);

//- rjf: eval cache operations
internal D_EvalCache *d_eval_cache_alloc(void);
internal void d_eval_cache_select(D_EvalCache *cache, D_Handle selected_thread, U64 selected_unwind_idx, U64 async_endt_us);
internal D_EvalID d_eval_push_parent(D_EvalID id);
internal D_EvalID d_eval_pop_parent(void);
internal D_Eval d_eval_from_leaf_identifier(String8 identifier, String8List qualifiers);
internal D_Eval d_eval_from_string(String8 string);

////////////////////////////////
//~ rjf: Layer Initialization

internal void d_init(void);

#endif // DBG_ENGINE_CORE_H
