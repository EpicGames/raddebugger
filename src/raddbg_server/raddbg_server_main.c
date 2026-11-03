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
#include "stap/stap_parse.h"
#include "demon/demon_inc.h"
#include "dbg_server/dbg_server.h"

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
#include "stap/stap_parse.c"
#include "demon/demon_inc.c"
#include "dbg_server/dbg_server.c"

////////////////////////////////
//~ rjf: Globals

typedef struct RDS_Msg RDS_Msg;
struct RDS_Msg
{
  RDS_Msg *next;
  SOCK_Endpoint endpoint;
  String8 data;
};

typedef struct RDS_Client RDS_Client;
struct RDS_Client
{
  RDS_Client *order_next;
  RDS_Client *order_prev;
  RDS_Client *hash_next;
  RDS_Client *hash_prev;
  SOCK_Endpoint endpoint;
};

typedef struct RDS_ClientSlot RDS_ClientSlot;
struct RDS_ClientSlot
{
  RDS_Client *first;
  RDS_Client *last;
};

typedef struct RDS_DeployNode RDS_DeployNode;
struct RDS_DeployNode
{
  RDS_DeployNode *path_next;
  RDS_DeployNode *hash_next;
  String8 path;
  U128 hash;
};

typedef struct RDS_State RDS_State;
struct RDS_State
{
  Arena *arena;
  B32 quiet;
  U64 msg_log_flags[(DS_MsgKind_COUNT+63)/64];
  U64 log_counter;
  SOCK_Session sock_session;
  RWMutex sock_send_mutex;
  
  //- rjf: deploy state
  String8 deploy_cache_path;
  RWMutex deploy_mutex;
  RDS_DeployNode **deploy_path_slots;
  RDS_DeployNode **deploy_hash_slots;
  U64 deploy_slots_count;
  
  //- rjf: connected clients
  RDS_ClientSlot *client_slots;
  U64 client_slots_count;
  RDS_Client *first_client;
  RDS_Client *last_client;
  RDS_Client *free_client;
  
  //- rjf: client -> demon messages
  RWMutex c2d_rw_mutex;
  CondVar c2d_cv;
  Arena *c2d_arena;
  RDS_Msg *c2d_msg_first;
  RDS_Msg *c2d_msg_last;
};

global RDS_State *rds_state = 0;

////////////////////////////////
//~ rjf: Response Logger

internal void
rds_log(String8 string)
{
  if(!rds_state->quiet)
  {
    fprintf(stderr, "%.*s", str8_varg(string));
  }
}

internal void
rds_logf(char *fmt, ...)
{
  Temp scratch = scratch_begin(0, 0);
  va_list args;
  va_start(args, fmt);
  String8 string = str8fv(scratch.arena, fmt, args);
  rds_log(string);
  va_end(args);
  scratch_end(scratch);
}

internal void
rds_log_response(DS_Response response)
{
  rds_logf("%I64d: [response]\n", rds_state->log_counter);
  rds_logf("  msg_id: 0x%llx\n", response.msg_id);
  rds_logf("  good: %i\n", response.good);
  if(response.vaddr != 0)
  {
    rds_logf("  vaddr: 0x%llx\n", response.vaddr);
  }
  if(response.pid != 0)
  {
    rds_logf("  pid: %llu\n", response.pid);
  }
  if(response.data.size != 0)
  {
    rds_logf("  data: {%llu bytes}\n", response.data.size);
  }
  rds_state->log_counter += 1;
}

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
    RDS_Msg *msg_first = 0;
    RDS_Msg *msg_last = 0;
    RWMutexScope(rds_state->c2d_rw_mutex, 1) for(;;)
    {
      if(rds_state->c2d_msg_first != 0)
      {
        for EachNode(src, RDS_Msg, rds_state->c2d_msg_first)
        {
          RDS_Msg *dst = push_array(scratch.arena, RDS_Msg, 1);
          SLLQueuePush(msg_first, msg_last, dst);
          dst->endpoint = src->endpoint;
          dst->data = str8_copy(scratch.arena, src->data);
        }
        arena_clear(rds_state->c2d_arena);
        rds_state->c2d_msg_first = rds_state->c2d_msg_last = 0;
        break;
      }
      cond_var_wait_rw_w(rds_state->c2d_cv, rds_state->c2d_rw_mutex, max_U64);
    }
    
    ////////////////////////////
    //- rjf: do messages, push responses
    //
    for(RDS_Msg *msg_n = msg_first; msg_n != 0; msg_n = msg_n->next)
    {
      //- rjf: unpack message
      DS_Msg msg = ds_msg_from_data(scratch.arena, msg_n->data);
      DS_LaunchParams launch_params = {0};
      if(msg.launch_params != 0)
      {
        MemoryCopyStruct(&launch_params, msg.launch_params);
        
        // rjf: try mapping executable path -> deployed hash
        if(msg.launch_params->command_line.first != 0)
        {
          String8 exe_path = str8_list_first(&msg.launch_params->command_line);
          U64 hash = u64_hash_from_str8(exe_path);
          U64 slot_idx = hash%rds_state->deploy_slots_count;
          B32 is_deployed = 0;
          U128 deployed_hash = {0};
          RWMutexScope(rds_state->deploy_mutex, 0)
          {
            for(RDS_DeployNode *n = rds_state->deploy_path_slots[slot_idx]; n != 0; n = n->path_next)
            {
              if(path_match_normalized(n->path, exe_path))
              {
                is_deployed = 1;
                deployed_hash = n->hash;
                break;
              }
            }
          }
          if(is_deployed)
          {
            String8List command_line = {0};
            String8 deployed_exe_path = str8f(scratch.arena, "%S/%I64x_%I64x%S", rds_state->deploy_cache_path, deployed_hash.u64[0], deployed_hash.u64[1], program_ext_postfix_from_os(OperatingSystem_CURRENT, 1));
            str8_list_push(scratch.arena, &command_line, deployed_exe_path);
            for EachNode(n, String8Node, msg.launch_params->command_line.first->next)
            {
              str8_list_push(scratch.arena, &command_line, n->string);
            }
            launch_params.command_line = command_line;
          }
        }
      }
      
      //- rjf: do action, get response
      DS_Response response = {.good = 1, .msg_id = msg.msg_id};
      DMN_EventList events = {0};
      switch(msg.kind)
      {
        default:{}break;
        
        //- rjf: launching
        case DS_MsgKind_Launch:
        {
          response.pid = ds_ctrl_launch(ctrl_ctx, &launch_params);
        }break;
        
        //- rjf: attaching
        case DS_MsgKind_Attach:
        {
          response.good = ds_ctrl_attach(ctrl_ctx, msg.pid);
        }break;
        
        //- rjf: killing
        case DS_MsgKind_Kill:
        {
          response.good = ds_ctrl_kill(ctrl_ctx, msg.entity, (U32)msg.code);
        }break;
        
        //- rjf: detaching
        case DS_MsgKind_Detach:
        {
          response.good = ds_ctrl_detach(ctrl_ctx, msg.entity);
        }break;
        
        //- rjf: running ops
        case DS_MsgKind_Run:
        case DS_MsgKind_SingleStep:
        {
          DMN_RunCtrls ctrls = {0};
          {
            ctrls.priority_thread            = msg.entity;
            ctrls.single_step_thread         = (msg.kind == DS_MsgKind_SingleStep) ? msg.entity : dmn_handle_zero();
            ctrls.run_entities               = msg.run_entities.handles;
            ctrls.run_entity_count           = msg.run_entities.count;
            ctrls.traps                      = msg.traps;
            ctrls.ignore_previous_exception  = !!(msg.flags & DS_MsgFlag_IgnorePreviousException);
            ctrls.run_entities_are_unfrozen  = !!(msg.flags & DS_MsgFlag_RunEntitiesAreUnfrozen);
            ctrls.run_entities_are_processes = !!(msg.flags & DS_MsgFlag_RunEntitiesAreProcesses);
          };
          response.good = 1;
          events = ds_ctrl_run(scratch.arena, ctrl_ctx, &ctrls);
        }break;
      }
      
      //- rjf: map event module paths -> client
      for EachNode(n, DMN_EventNode, events.first)
      {
        if(n->v.kind == DMN_EventKind_LoadModule)
        {
          B32 is_deployed_path = str8_match(rds_state->deploy_cache_path, n->v.module_info->module_path, StringMatchFlag_RightSideSloppy);
          if(is_deployed_path)
          {
            String8 hash_string = str8_chop_last_dot(str8_skip_last_slash(n->v.module_info->module_path));
            U64 underscore_pos = str8_find_needle(hash_string, 0, s("_"), 0);
            String8 u640_string = str8_prefix(hash_string, underscore_pos);
            String8 u641_string = str8_skip(hash_string, underscore_pos+1);
            U128 hash = u128_make(u64_from_str8(u640_string, 16), u64_from_str8(u641_string, 16));
            U64 slot_idx = hash.u64[0]%rds_state->deploy_slots_count;
            RWMutexScope(rds_state->deploy_mutex, 0)
            {
              for(RDS_DeployNode *deploy_n = rds_state->deploy_hash_slots[slot_idx]; deploy_n != 0; deploy_n = deploy_n->hash_next)
              {
                if(u128_match(hash, deploy_n->hash))
                {
                  n->v.string = str8_copy(scratch.arena, deploy_n->path);
                  n->v.module_info->module_path = n->v.string;
                }
              }
            }
          }
        }
      }
      
      //- rjf: log events
      for EachNode(n, DMN_EventNode, events.first)
      {
        rds_logf("%I64d: [%S event]\n", rds_state->log_counter, lower_from_str8(scratch.arena, dmn_event_kind_string_table[n->v.kind]));
        rds_state->log_counter += 1;
      }
      
      //- rjf: pack events & send to all connected client 
      if(events.count != 0)
      {
        String8 data = ds_data_from_dmn_events(scratch.arena, events);
        String8List packets = ds_packets_from_data(scratch.arena, DS_PacketKind_Event, data);
        for(RDS_Client *c = rds_state->first_client; c != 0; c = c->order_next)
        {
          RWMutexScope(rds_state->sock_send_mutex, 1) for EachNode(n, String8Node, packets.first)
          {
            sock_send(rds_state->sock_session, c->endpoint, SOCK_Protocol_TCP, n->string, max_U64);
          }
        }
      }
      
      //- rjf: pack response & send back to the originating endpoint of the message
      if(msg_n->endpoint.port != 0)
      {
        String8 data = ds_data_from_response(scratch.arena, &response);
        String8List packets = ds_packets_from_data(scratch.arena, DS_PacketKind_Response, data);
        RWMutexScope(rds_state->sock_send_mutex, 1)
        {
          if(!!(rds_state->msg_log_flags[msg.kind/64] & (1ull<<(msg.kind%64))))
          {
            rds_log_response(response);
          }
          for EachNode(n, String8Node, packets.first)
          {
            sock_send(rds_state->sock_session, msg_n->endpoint, SOCK_Protocol_TCP, n->string, max_U64);
          }
        }
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
  Temp scratch = scratch_begin(0, 0);
  
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
  
  //- rjf: get launch targets from command line
  typedef struct Target Target;
  struct Target
  {
    Target *next;
    String8List cmd_line;
  };
  Target *first_target = 0;
  Target *last_target = 0;
  {
    B32 past_first_non_flag = 0;
    for(U64 idx = 1; idx < cmd_line->argc; idx += 1)
    {
      String8 arg = str8_cstring(cmd_line->argv[idx]);
      B32 is_flag = (str8_match(str8_prefix(arg, 1), s("-"), 0) ||
                     str8_match(str8_prefix(arg, 1), s("--"), 0) ||
                     str8_match(str8_prefix(arg, 1), s("/"), 0));
      past_first_non_flag = (past_first_non_flag || !is_flag);
      if(past_first_non_flag)
      {
        // rjf: create new target after ;s, or if we don't have one
        if(str8_match(arg, s(";"), 0) || first_target == 0)
        {
          Target *t = push_array(scratch.arena, Target, 1);
          SLLQueuePush(first_target, last_target, t);
        }
        
        // rjf: push this string to the most recently added target
        if(last_target != 0)
        {
          str8_list_push(scratch.arena, &last_target->cmd_line, arg);
        }
      }
    }
  }
  
  //- rjf: set up shared state
  Arena *arena = arena_alloc();
  rds_state = push_array(arena, RDS_State, 1);
  rds_state->arena = arena;
  rds_state->quiet = (cmd_line_has_flag(cmd_line, s("quiet")) || cmd_line_has_flag(cmd_line, s("q")));
  for EachElement(idx, rds_state->msg_log_flags)
  {
    rds_state->msg_log_flags[idx] = max_U64;
  }
  rds_state->sock_session = sock_session_open((U16)port, 0, DS_PACKET_SIZE);
  rds_state->sock_send_mutex = rw_mutex_alloc();
  String8 user_cache_data_path = get_process_info()->user_program_cache_data_path;
  rds_state->deploy_cache_path = str8f(arena, "%S/raddbg_server", user_cache_data_path);
  make_directory(rds_state->deploy_cache_path);
  rds_state->deploy_mutex = rw_mutex_alloc();
  rds_state->deploy_slots_count = 256;
  rds_state->deploy_path_slots = push_array(arena, RDS_DeployNode *, rds_state->deploy_slots_count);
  rds_state->deploy_hash_slots = push_array(arena, RDS_DeployNode *, rds_state->deploy_slots_count);
  rds_state->c2d_rw_mutex = rw_mutex_alloc();
  rds_state->c2d_cv = cond_var_alloc();
  rds_state->c2d_arena = arena_alloc();
  rds_state->client_slots_count = 64;
  rds_state->client_slots = push_array(arena, RDS_ClientSlot, rds_state->client_slots_count);
  
  //- rjf: filter msg kind logs
  {
    MemoryZeroArray(rds_state->msg_log_flags);
    String8List msg_kinds = cmd_line_strings(cmd_line, s("log_msgs"));
    for EachNode(n, String8Node, msg_kinds.first)
    {
      DS_MsgKind msg_kind = DS_MsgKind_Null;
      for EachNonZeroEnumVal(DS_MsgKind, k)
      {
        if(str8_match(n->string, ds_name_from_msg_kind_table[k], StringMatchFlag_CaseInsensitive))
        {
          msg_kind = k;
          break;
        }
      }
      if(msg_kind != DS_MsgKind_Null)
      {
        rds_logf("[logging %S messages]\n", lower_from_str8(scratch.arena, ds_name_from_msg_kind_table[msg_kind]));
        rds_state->msg_log_flags[msg_kind/64] |= 1ull<<(msg_kind%64);
      }
    }
  }
  
  //- rjf: launch demon thread
  thread_launch(dmn_thread__entry_point, 0);
  
  //- rjf: do initial launch of all command-line-specified targets
  {
    String8 current_path = get_current_path(scratch.arena);
    for EachNode(t, Target, first_target)
    {
      rds_logf("[launching command line target]\n");
      rds_logf(" ");
      for EachNode(n, String8Node, t->cmd_line.first)
      {
        rds_logf(" %S", n->string);
      }
      rds_logf("\n");
      DS_LaunchParams launch_params =
      {
        .command_line = t->cmd_line,
        .path         = current_path,
        .inherit_env  = 1,
      };
      DS_Msg msg =
      {
        .kind          = DS_MsgKind_Launch,
        .launch_params = &launch_params,
      };
      String8 data = ds_data_from_msg(scratch.arena, &msg);
      RWMutexScope(rds_state->c2d_rw_mutex, 1)
      {
        RDS_Msg *msg = push_array(rds_state->c2d_arena, RDS_Msg, 1);
        msg->data = str8_copy(rds_state->c2d_arena, data);
        SLLQueuePush(rds_state->c2d_msg_first, rds_state->c2d_msg_last, msg);
      }
      cond_var_broadcast(rds_state->c2d_cv);
    }
  }
  
  //- rjf: loop: wait for messages, issue commands to demon
  rds_logf("[waiting for message]\n");
  for(;;)
  {
    Temp scratch = scratch_begin(0, 0);
    
    //- rjf: get next set of packets from client
    SOCK_Protocol protocol = SOCK_Protocol_TCP;
    SOCK_Endpoint endpoint = {0};
    String8List packets = {0};
    for(B32 done = 0; !done;)
    {
      // rjf: receive next packet
      String8 packet = {0};
      packet.size = DS_PACKET_SIZE;
      packet.str = push_array(scratch.arena, U8, packet.size);
      sock_recv(rds_state->sock_session, &endpoint, &protocol, packet.str, max_U64);
      
      // rjf: extract header
      DS_PacketHeader header = {0};
      if(packet.size >= sizeof(header))
      {
        MemoryCopy(&header, packet.str, sizeof(header));
      }
      
      // rjf: valid header? -> push
      if(header.magic == DS_PACKET_MAGIC && header.version == DS_PACKET_VERSION)
      {
        str8_list_push(scratch.arena, &packets, packet);
      }
      
      // rjf: terminal? -> done
      if(header.flags & DS_PacketFlag_Terminal)
      {
        done = 1;
      }
    }
    
    //- rjf: store this client, if we don't have it
    {
      U64 hash = u64_hash_from_str8(str8_struct(&endpoint));
      U64 slot_idx = hash%rds_state->client_slots_count;
      RDS_Client *client = 0;
      for(RDS_Client *c = rds_state->client_slots[slot_idx].first; c != 0; c = c->hash_next)
      {
        if(MemoryMatchStruct(&c->endpoint, &endpoint))
        {
          client = c;
          break;
        }
      }
      if(client == 0)
      {
        client = rds_state->free_client;
        if(client != 0)
        {
          SLLStackPop_N(rds_state->free_client, order_next);
        }
        else
        {
          client = push_array(rds_state->arena, RDS_Client, 1);
        }
        DLLPushBack_NP(rds_state->client_slots[slot_idx].first, rds_state->client_slots[slot_idx].last, client, hash_next, hash_prev);
        DLLPushBack_NP(rds_state->first_client, rds_state->last_client, client, order_next, order_prev);
        client->endpoint = endpoint;
      }
    }
    
    //- rjf: packets -> data
    DS_PacketKind kind = DS_PacketKind_Null;
    String8 data = ds_data_from_packets(scratch.arena, packets, &kind);
    
    //- rjf: data -> msg kind
    DS_MsgKind msg_kind = DS_MsgKind_Null;
    if(kind == DS_PacketKind_Msg)
    {
      str8_deserial_read_struct(data, 0, &msg_kind);
    }
    
    //- rjf: log message info
    if(kind != DS_PacketKind_Null && (msg_kind == DS_MsgKind_Null || !!(rds_state->msg_log_flags[msg_kind/64] & (1ull<<(msg_kind%64)))))
    {
      String8 endpoint_string = sock_string_from_endpoint(scratch.arena, endpoint);
      String8 packet_kind_name = lower_from_str8(scratch.arena, ds_name_from_packet_kind_table[kind]);
      rds_logf("%I64d: [%S]\n", rds_state->log_counter, packet_kind_name);
      rds_logf("  sender: %S\n", endpoint_string);
      if(msg_kind != DS_MsgKind_Null)
      {
        U64 msg_id = 0;
        str8_deserial_read_struct(data, sizeof(DS_MsgKind) + sizeof(DS_MsgFlags), &msg_id);
        String8 msg_kind_name = lower_from_str8(scratch.arena, ds_name_from_msg_kind_table[msg_kind]);
        rds_logf("  msg_kind: %S\n", msg_kind_name);
        rds_logf("  msg_id: 0x%llx\n", msg_id);
      }
      rds_state->log_counter += 1;
    }
    
    //- rjf: try to handle this message on the main thread, before sending to
    // demon. the demon thread trades runtimes with debuggees, and so it is
    // only able to handle messages when it is not running - e.g. launching,
    // attaching, detaching, and so on. other operations, like reading/writing
    // memory/registers, listing processes, and halting, we want to just do
    // on the main thread without interrupting the demon at all.
    if(kind != DS_PacketKind_Null && (msg_kind < DS_MsgKind_Launch || DS_MsgKind_SingleStep < msg_kind))
    {
      DS_Msg msg = ds_msg_from_data(scratch.arena, data);
      
      // rjf: form base response info
      DS_Response response = {0};
      response.msg_id = msg.msg_id;
      response.good = 1;
      
      // rjf: do message specifics
      String8 deploy_path = {0};
      U128 deploy_hash = {0};
      switch(msg.kind)
      {
        default:{}break;
        
        //- rjf: response
        case DS_MsgKind_Handshake:
        {
          // TODO(rjf): send initial state
        }break;
        
        //- rjf: deploying files
        case DS_MsgKind_DeployStatus:
        {
          String8 hash_path = str8f(scratch.arena, "%S/%I64x_%I64x%S", rds_state->deploy_cache_path, msg.hash.u64[0], msg.hash.u64[1], program_ext_postfix_from_os(OperatingSystem_CURRENT, 1));
          FileProperties props = properties_from_file_path(hash_path);
          response.good = (props.modified != 0);
          if(response.good)
          {
            deploy_path = msg.path;
            deploy_hash = msg.hash;
          }
        }break;
        case DS_MsgKind_Deploy:
        {
          U128 hash = u128_hash_from_str8(msg.data);
          String8 hash_path = str8f(scratch.arena, "%S/%I64x_%I64x%S", rds_state->deploy_cache_path, hash.u64[0], hash.u64[1], program_ext_postfix_from_os(OperatingSystem_CURRENT, 1));
          response.good = write_data_to_file_path(hash_path, msg.data);
          deploy_path = msg.path;
          deploy_hash = hash;
        }break;
        
        //- rjf: halting
        case DS_MsgKind_Halt:
        {
          dmn_halt(0, 0);
        }break;
        
        //- rjf: memory ops
        case DS_MsgKind_MemoryReserve:
        {
          DMN_Handle process = msg.entity;
          response.vaddr = dmn_process_memory_reserve(process, msg.vaddr_range.min, dim_1u64(msg.vaddr_range));
        }break;
        case DS_MsgKind_MemoryCommit:
        {
          DMN_Handle process = msg.entity;
          dmn_process_memory_commit(process, msg.vaddr_range.min, dim_1u64(msg.vaddr_range));
        }break;
        case DS_MsgKind_MemoryDecommit:
        {
          DMN_Handle process = msg.entity;
          dmn_process_memory_decommit(process, msg.vaddr_range.min, dim_1u64(msg.vaddr_range));
        }break;
        case DS_MsgKind_MemoryRelease:
        {
          DMN_Handle process = msg.entity;
          dmn_process_memory_release(process, msg.vaddr_range.min, dim_1u64(msg.vaddr_range));
        }break;
        case DS_MsgKind_MemoryProtect:
        {
          DMN_Handle process = msg.entity;
          dmn_process_memory_protect(process, msg.vaddr_range.min, dim_1u64(msg.vaddr_range), msg.access_flags);
        }break;
        case DS_MsgKind_MemoryRead:
        {
          DMN_Handle process = msg.entity;
          U64 read_size_desired = dim_1u64(msg.vaddr_range);
          U8 *data = push_array_no_zero(scratch.arena, U8, read_size_desired);
          U64 read_size_actual = dmn_process_read(process, msg.vaddr_range, data);
          response.data.size = read_size_actual;
          response.data.str  = data;
        }break;
        case DS_MsgKind_MemoryWrite:
        {
          DMN_Handle process = msg.entity;
          response.good = dmn_process_write(process, r1u64(msg.vaddr_range.min, msg.vaddr_range.min + msg.data.size), msg.data.str);
        }break;
        
        //- rjf: thread ops
        case DS_MsgKind_ThreadRegBlockRead:
        {
          ARCH_Info *arch_info = arch_info_from_arch(Arch_CURRENT);
          U8 *reg_block = push_array(scratch.arena, U8, arch_info->reg_block_size);
          response.good = dmn_thread_read_reg_block(msg.entity, reg_block);
          response.data = str8(reg_block, arch_info->reg_block_size);
        }break;
        case DS_MsgKind_ThreadRegBlockWrite:
        {
          ARCH_Info *arch_info = arch_info_from_arch(Arch_CURRENT);
          U8 *reg_block = push_array(scratch.arena, U8, arch_info->reg_block_size);
          MemoryCopy(reg_block, msg.data.str, Min(msg.data.size, arch_info->reg_block_size));
          response.good = dmn_thread_write_reg_block(msg.entity, reg_block);
        }break;
        case DS_MsgKind_ThreadGetModuleTLSVAddr:
        {
          response.good = dmn_thread_get_module_tls_vaddr(msg.entity, msg.module, &response.vaddr);
        }break;
        
        //- rjf: system ops
        case DS_MsgKind_ListSystemProcesses:{}break;
      }
      
      // rjf: store (source path -> deployed hash) correllation
      if(deploy_path.size != 0) RWMutexScope(rds_state->deploy_mutex, 1)
      {
        U64 hash_slot_idx = deploy_hash.u64[0]%rds_state->deploy_slots_count;
        U64 path_hash = u64_hash_from_str8(deploy_path);
        U64 path_slot_idx = path_hash%rds_state->deploy_slots_count;
        RDS_DeployNode *n = push_array(rds_state->arena, RDS_DeployNode, 1);
        SLLStackPush_N(rds_state->deploy_path_slots[path_slot_idx], n, path_next);
        SLLStackPush_N(rds_state->deploy_hash_slots[hash_slot_idx], n, hash_next);
        n->path = str8_copy(rds_state->arena, deploy_path);
        n->hash = deploy_hash;
      }
      
      // rjf: send response
      {
        String8 data = ds_data_from_response(scratch.arena, &response);
        String8List packets = ds_packets_from_data(scratch.arena, DS_PacketKind_Response, data);
        RWMutexScope(rds_state->sock_send_mutex, 1)
        {
          if(!!(rds_state->msg_log_flags[msg.kind/64] & (1ull<<(msg.kind%64))))
          {
            rds_log_response(response);
          }
          for EachNode(n, String8Node, packets.first)
          {
            sock_send(rds_state->sock_session, endpoint, SOCK_Protocol_TCP, n->string, max_U64);
          }
        }
      }
    }
    
    //- rjf: not handled by main thread? -> okay, send to demon thread, if it's ready.
    if(kind != DS_PacketKind_Null && (DS_MsgKind_Launch <= msg_kind && msg_kind <= DS_MsgKind_SingleStep))
    {
      RWMutexScope(rds_state->c2d_rw_mutex, 1)
      {
        RDS_Msg *msg = push_array(rds_state->c2d_arena, RDS_Msg, 1);
        msg->endpoint = endpoint;
        msg->data = str8_copy(rds_state->c2d_arena, data);
        SLLQueuePush(rds_state->c2d_msg_first, rds_state->c2d_msg_last, msg);
      }
      cond_var_broadcast(rds_state->c2d_cv);
    }
    
    scratch_end(scratch);
  }
  
  scratch_end(scratch);
}
