// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RADDBG_SERVER_H
#define RADDBG_SERVER_H

////////////////////////////////
//~ rjf: Messages (Client -> Server)

#define RDS_MSG_MAGIC 0x7273

typedef enum RDS_MsgKind
{
  RDS_MsgKind_Null,
  
  //- rjf: debuggee attaching/detaching ops
  RDS_MsgKind_Launch, RDS_MsgKind_DemonThreadHandledFirst = RDS_MsgKind_Launch,
  RDS_MsgKind_Attach,
  RDS_MsgKind_Kill,
  RDS_MsgKind_Detach,
  
  //- rjf: running ops
  RDS_MsgKind_Run,
  RDS_MsgKind_SingleStep, RDS_MsgKind_DemonThreadHandledLast = RDS_MsgKind_SingleStep,
  
  //- rjf: halting
  RDS_MsgKind_Halt,
  
  //- rjf: memory ops
  RDS_MsgKind_MemoryReserve,
  RDS_MsgKind_MemoryCommit,
  RDS_MsgKind_MemoryDecommit,
  RDS_MsgKind_MemoryRelease,
  RDS_MsgKind_MemoryProtect,
  RDS_MsgKind_MemoryRead,
  RDS_MsgKind_MemoryWrite,
  
  //- rjf: thread ops
  RDS_MsgKind_ThreadRegBlockRead,
  RDS_MsgKind_ThreadRegBlockWrite,
  RDS_MsgKind_ThreadGetModuleTLSVAddr,
  
  //- rjf: system ops
  RDS_MsgKind_ListSystemProcesses,
}
RDS_MsgKind;

typedef U32 RDS_MsgFlags;
enum
{
  RDS_MsgFlag_InheritEnv              = (1<<0),
  RDS_MsgFlag_DebugSubprocesses       = (1<<1),
  RDS_MsgFlag_IgnorePreviousException = (1<<2),
  RDS_MsgFlag_RunEntitiesAreUnfrozen  = (1<<3),
  RDS_MsgFlag_RunEntitiesAreProcesses = (1<<4),
};

typedef struct RDS_Msg RDS_Msg;
struct RDS_Msg
{
  RDS_MsgKind kind;
  RDS_MsgFlags flags;
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
//~ rjf: Responses (Server -> Client responses to messages)

typedef struct RDS_Response RDS_Response;
struct RDS_Response
{
  U64 id;
  String8 data;
  DMN_Handle entity;
  U64 pid;
  U64 demon_event_count;
};

////////////////////////////////
//~ rjf: Message Parsing

internal RDS_Msg rds_msg_from_data(Arena *arena, String8 data);

#endif // RADDBG_SERVER_H
