// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Build Options

#define BUILD_TITLE "The RAD Debugger Server"
#define BUILD_CONSOLE_INTERFACE 1

////////////////////////////////
//~ rjf: Includes

//- rjf: [h]
#include "base/base_inc.h"
#include "x64/x64_inc.h"
#include "arch/arch_inc.h"
#include "socket/socket_inc.h"
#include "coff/coff.h"
#include "coff/coff_parse.h"
#include "pe/pe.h"
#include "elf/elf.h"
#include "gnu/gnu.h"
#include "gnu/gnu_parse.h"
#include "elf/elf_parse.h"
#include "demon/demon_inc.h"
#include "raddbg_server/raddbg_server.h"

//- rjf: [c]
#include "base/base_inc.c"
#include "x64/x64_inc.c"
#include "arch/arch_inc.c"
#include "socket/socket_inc.c"
#include "coff/coff.c"
#include "coff/coff_parse.c"
#include "pe/pe.c"
#include "elf/elf.c"
#include "gnu/gnu.c"
#include "gnu/gnu_parse.c"
#include "elf/elf_parse.c"
#include "demon/demon_inc.c"
#include "raddbg_server/raddbg_server.c"

////////////////////////////////
//~ rjf: Globals

typedef struct RDS_State RDS_State;
struct RDS_State
{
  Arena *arena;
  SOCK_Session sock_session;
  
  //- rjf: client -> demon messages
  RWMutex c2d_rw_mutex;
  CondVar c2d_cv;
  Arena *c2d_arena;
  String8List c2d_msgs;
  
  //- rjf: demon run state
  B32 demon_is_running;
};

global RDS_State *rds_state = 0;

////////////////////////////////
//~ rjf: Demon Thread

internal void
dmn_thread__entry_point(void *p)
{
  ThreadNameF("dmn_thread");
  DMN_CtrlCtx *ctrl_ctx = dmn_ctrl_begin();
  for(;;)
  {
    Temp scratch = scratch_begin(0, 0);
    
    ////////////////////////////
    //- rjf: get next messages
    //
    String8List msgs = {0};
    RWMutexScope(rds_state->c2d_rw_mutex, 1) for(;;)
    {
      if(rds_state->c2d_msgs.node_count != 0)
      {
        msgs = str8_list_copy(scratch.arena, &rds_state->c2d_msgs);
        MemoryZeroStruct(&rds_state->c2d_msgs);
        arena_clear(rds_state->c2d_arena);
        break;
      }
      cond_var_wait_rw_w(rds_state->c2d_cv, rds_state->c2d_rw_mutex, max_U64);
    }
    
    ////////////////////////////
    //- rjf: do messages
    //
    for(String8Node *msg_n = msgs.first; msg_n != 0; msg_n = msg_n->next)
    {
      RDS_Msg msg = rds_msg_from_data(scratch.arena, msg_n->string);
      switch(msg.kind)
      {
        default:{}break;
        
        //- rjf: debuggee attaching/detaching ops
        
        case RDS_MsgKind_Launch:{}break;
        case RDS_MsgKind_Attach:{}break;
        case RDS_MsgKind_Kill:{}break;
        case RDS_MsgKind_Detach:{}break;
        
        //- rjf: running ops
        
        case RDS_MsgKind_Run:{}break;
        case RDS_MsgKind_SingleStep:{}break;
      }
    }
    
    scratch_end(scratch);
  }
}

////////////////////////////////
//~ rjf: Entry Point

internal void
entry_point(CmdLine *cmd_line)
{
  //- rjf: get port argument
  U64 port = 7273;
  {
    U64 port_maybe = 0;
    String8 port_arg_string = cmd_line_string(cmd_line, s("port"));
    if(try_u64_from_str8_c_rules(port_arg_string, &port_maybe))
    {
      port = port_maybe;
    }
  }
  
  //- rjf: set up shared state
  Arena *arena = arena_alloc();
  rds_state = push_array(arena, RDS_State, 1);
  rds_state->arena = arena;
  rds_state->sock_session = sock_session_open((U16)port, 0);
  rds_state->c2d_rw_mutex = rw_mutex_alloc();
  rds_state->c2d_cv = cond_var_alloc();
  rds_state->c2d_arena = arena_alloc();
  
  //- rjf: launch demon thread
  thread_launch(dmn_thread__entry_point, 0);
  
  //- rjf: loop: wait for messages, issue commands to demon
  for(;;)
  {
    Temp scratch = scratch_begin(0, 0);
    
    //- rjf: get next message from client
    fprintf(stderr, "[waiting for message]\n");
    SOCK_Protocol protocol = SOCK_Protocol_TCP;
    SOCK_Endpoint endpoint = {0};
    String8 data = {0};
    sock_recv(scratch.arena, rds_state->sock_session, &protocol, &endpoint, &data, max_U64);
    
    //- rjf: parse magic prefix
    U16 magic_maybe = 0;
    str8_deserial_read_struct(data, 0, &magic_maybe);
    
    //- rjf: log message info
    {
      String8 endpoint_string = sock_string_from_endpoint(scratch.arena, endpoint);
      fprintf(stderr, "[got message]\n");
      fprintf(stderr, "  sender: %.*s\n", str8_varg(endpoint_string));
      fprintf(stderr, "  magic:  0x%x\n", (int)magic_maybe);
    }
    
    //- rjf: try to handle this message on the main thread, before sending to
    // demon. the demon thread trades runtimes with debuggees, and so it is
    // only able to handle messages when it is not running - e.g. launching,
    // attaching, detaching, and so on. other operations, like reading/writing
    // memory/registers, listing processes, and halting, we want to just do
    // on the main thread without interrupting the demon at all.
    RDS_MsgKind peek_kind = RDS_MsgKind_Null;
    if(magic_maybe == RDS_MSG_MAGIC)
    {
      str8_deserial_read_struct(data, 2, &peek_kind);
      if(peek_kind < RDS_MsgKind_DemonThreadHandledFirst || RDS_MsgKind_DemonThreadHandledLast < peek_kind)
      {
        RDS_Msg msg = rds_msg_from_data(scratch.arena, data);
        switch(msg.kind)
        {
          default:{}break;
          case RDS_MsgKind_Halt:
          {
            dmn_halt(0, 0);
          }break;
        }
      }
    }
    
    //- rjf: not handled by main thread? -> okay, send to demon thread, if it's ready.
    if(magic_maybe == RDS_MSG_MAGIC && !rds_state->demon_is_running && (RDS_MsgKind_DemonThreadHandledFirst <= peek_kind && peek_kind <= RDS_MsgKind_DemonThreadHandledLast))
    {
      RWMutexScope(rds_state->c2d_rw_mutex, 1)
      {
        str8_list_push(rds_state->c2d_arena, &rds_state->c2d_msgs, data);
      }
      cond_var_broadcast(rds_state->c2d_cv);
    }
    
    scratch_end(scratch);
  }
}
