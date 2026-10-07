// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef DBG_SERVER_H
#define DBG_SERVER_H

////////////////////////////////
//~ rjf: Messages (Client -> Server)

#define DS_MSG_MAGIC 0x7273

typedef enum DS_MsgKind
{
  DS_MsgKind_Null,
  
  //- rjf: debuggee attaching/detaching ops
  DS_MsgKind_Launch,
  DS_MsgKind_Attach,
  DS_MsgKind_Kill,
  DS_MsgKind_Detach,
  
  //- rjf: running ops
  DS_MsgKind_Run,
  DS_MsgKind_SingleStep,
  
  //- rjf: halting
  DS_MsgKind_Halt,
  
  //- rjf: memory ops
  DS_MsgKind_MemoryReserve,
  DS_MsgKind_MemoryCommit,
  DS_MsgKind_MemoryDecommit,
  DS_MsgKind_MemoryRelease,
  DS_MsgKind_MemoryProtect,
  DS_MsgKind_MemoryRead,
  DS_MsgKind_MemoryWrite,
  
  //- rjf: thread ops
  DS_MsgKind_ThreadRegBlockRead,
  DS_MsgKind_ThreadRegBlockWrite,
  DS_MsgKind_ThreadGetModuleTLSVAddr,
  
  //- rjf: system ops
  DS_MsgKind_ListSystemProcesses,
}
DS_MsgKind;

typedef U32 DS_MsgFlags;
enum
{
  DS_MsgFlag_InheritEnv              = (1<<0),
  DS_MsgFlag_DebugSubprocesses       = (1<<1),
  DS_MsgFlag_IgnorePreviousException = (1<<2),
  DS_MsgFlag_RunEntitiesAreUnfrozen  = (1<<3),
  DS_MsgFlag_RunEntitiesAreProcesses = (1<<4),
};

typedef struct DS_Msg DS_Msg;
struct DS_Msg
{
  DS_MsgKind kind;
  DS_MsgFlags flags;
  U64 id;
  
  //- rjf: general parameters
  Rng1U64 vaddr_range;
  AccessFlags access_flags;
  DMN_Handle entity;
  U64 pid;
  
  //- rjf: process launching parameters
  String8List command_line;
  String8 path;
  String8List env;
  
  //- rjf: running parameters
  DMN_TrapChunkList traps;
  DMN_HandleArray run_entities;
};

////////////////////////////////
//~ rjf: Server Instance

typedef enum DS_InstKind
{
  DS_InstKind_Host,
  DS_InstKind_Remote,
}
DS_InstKind;

typedef struct DS_Inst DS_Inst;
struct DS_Inst
{
  DS_InstKind kind;
  U64 u64[1];
};

////////////////////////////////
//~ rjf: Responses (Server -> Client responses to messages)

typedef struct DS_Response DS_Response;
struct DS_Response
{
  U64 id;
  DS_Inst inst;
  String8 data;
  DMN_Handle entity;
  U64 pid;
  DMN_EventList events;
};

////////////////////////////////
//~ rjf: Message Parsing

internal DS_Msg ds_msg_from_serialized(Arena *arena, String8 data);
internal String8 ds_serialized_from_msg(Arena *arena, DS_Msg msg);

#endif // DBG_SERVER_H
