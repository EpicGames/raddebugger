// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Generated Code

#include "generated/dbg_engine.meta.c"

////////////////////////////////
//~ rjf: @eval2 Bundled Evaluation Path

thread_static D_EvalCache *d_eval_cache = 0;

internal D_EvalCache *
d_eval_cache_alloc(void)
{
  Arena *arena = arena_alloc();
  D_EvalCache *cache = push_array(arena, D_EvalCache, 1);
  cache->arena = arena;
  cache->arena_eval_start_pos = arena_pos(arena);
  cache->top_parent = &cache->nil_parent;
  cache->cons_type_map = e2_cons_type_map_alloc();
  return cache;
}

internal void
d_eval_cache_select(D_EvalCache *cache, U64 async_endt_us)
{
  d_eval_cache = cache;
  cache->async_endt_us = async_endt_us;
  arena_pop_to(cache->arena, cache->arena_eval_start_pos);
  e2_select_cons_type_map(cache->cons_type_map);
  cache->slots_count = 4096;
  cache->string_slots = push_array(cache->arena, D_EvalSlot, cache->slots_count);
  cache->id_slots = push_array(cache->arena, D_EvalSlot, cache->slots_count);
}

internal D_EvalID
d_eval_push_parent(D_EvalID id)
{
  D_EvalID previous_top = d_eval_cache->top_parent->v;
  D_EvalIDNode *n = d_eval_cache->free_parent;
  if(n != 0)
  {
    SLLStackPop(d_eval_cache->free_parent);
  }
  else
  {
    n = push_array(d_eval_cache->arena, D_EvalIDNode, 1);
  }
  SLLStackPush(d_eval_cache->top_parent, n);
  n->v = id;
  return previous_top;
}

internal D_EvalID
d_eval_pop_parent(void)
{
  D_EvalID result = 0;
  if(d_eval_cache->top_parent != &d_eval_cache->nil_parent)
  {
    result = d_eval_cache->top_parent->v;
    D_EvalIDNode *n = d_eval_cache->top_parent;
    SLLStackPop(d_eval_cache->top_parent);
    SLLStackPush(d_eval_cache->free_parent, n);
  }
  return result;
}

internal D_Eval
d_eval_from_leaf_identifier(String8 identifier, String8List qualifiers)
{
  D_Eval eval = {.expr = &e2_expr_nil, .irtree = &e2_irnode_nil};
  {
    Temp scratch = scratch_begin(0, 0);
    
    //- rjf: identifier / qualifiers -> expr string
    String8 expr_string = identifier;
    if(qualifiers.node_count != 0)
    {
      String8List strings = str8_list_copy(scratch.arena, &qualifiers);
      str8_list_push(scratch.arena, &strings, identifier);
      StringJoin join = {.sep = s("!")};
      expr_string = str8_list_join(scratch.arena, &strings, &join);
    }
    
    //- rjf: expr string -> evaluation
    U64 hash = u64_hash_from_str8(expr_string);
    U64 string_slot_idx = hash%d_eval_cache->slots_count;
    B32 found = 0;
    for(D_EvalNode *n = d_eval_cache->string_slots[string_slot_idx].first; n != 0; n = n->string_next)
    {
      if(str8_match(n->eval.string, identifier, 0) && n->eval.parent_id == d_eval_cache->top_parent->v)
      {
        found = 1;
        eval = n->eval;
        break;
      }
    }
    
    //- rjf: no existing evaluation -> compute & cache
    if(!found)
    {
      B32 resolved = 0;
      E2_TypeKey type_key = {E2_TypeKeyKind_Null};
      E2_IRNode *irtree = &e2_irnode_nil;
      
      //- rjf: resolve as a debug info match
      if(!resolved)
      {
        // rjf: get debug info match from string / params
        U32 match_idx = 0;
        U32 unit_idx = 0;
        B32 allow_other_dbg_infos = 1;
        DI_Key preferred_dbgi_key = {0};
        DI_Match match = di_match_from_string(identifier, match_idx, unit_idx, allow_other_dbg_infos, preferred_dbgi_key, d_eval_cache->async_endt_us);
        
        // rjf: dbgi key -> debug info ID
        //
        // TODO(rjf): @eval2 we need to support non-linear lookups, (dbgi key -> dbgi ID)
        //
        E2_DbgInfoID dbgi_id = 0;
        RDI_Parsed *resolved_rdi = &rdi_parsed_nil;
        for EachIndex(idx, e2_assets->dbg_infos_count)
        {
          if(di_key_match(e2_assets->dbg_infos[idx].dbgi_key, match.key))
          {
            dbgi_id = e2_assets->dbg_infos[idx].id;
            resolved_rdi = e2_assets->dbg_infos[idx].rdi;
            break;
          }
        }
        
        // rjf: resolve to type info / IR from match
        RDI_Location resolved_location = 0;
        if(match.section_kind != RDI_SectionKind_NULL)
        {
          resolved = 1;
          switch(match.section_kind)
          {
            default:{}break;
            case RDI_SectionKind_GlobalVariables:
            case RDI_SectionKind_ThreadVariables:
            {
              RDI_Symbol *symbol = rdi_section_raw_element_from_kind_idx(resolved_rdi, match.section_kind, match.idx);
              RDI_TypeNode *type_node = rdi_element_from_name_idx(resolved_rdi, TypeNodes, symbol->type_idx);
              resolved_location = symbol->location;
              type_key = e2_type_key_dbgi(e2_type_kind_from_rdi(type_node->kind), dbgi_id, symbol->type_idx);
            }break;
            case RDI_SectionKind_Procedures:
            {
              RDI_Symbol *symbol = rdi_section_raw_element_from_kind_idx(resolved_rdi, match.section_kind, match.idx);
              U64 root_scope_voff = rdi_first_voff_from_procedure(resolved_rdi, symbol);
              RDI_TypeNode *type_node = rdi_element_from_name_idx(resolved_rdi, TypeNodes, symbol->type_idx);
              resolved_location = rdi_location_voff(root_scope_voff);
              type_key = e2_type_key_dbgi(e2_type_kind_from_rdi(type_node->kind), dbgi_id, symbol->type_idx);
            }break;
            case RDI_SectionKind_TypeNodes:
            {
              RDI_TypeNode *type_node = rdi_element_from_name_idx(resolved_rdi, TypeNodes, match.idx);
              type_key = e2_type_key_dbgi(e2_type_kind_from_rdi(type_node->kind), dbgi_id, match.idx);
            }break;
          }
        }
        
        // rjf: resolved location is a set? -> pick by voff, replace resolved location
        if(rdi_kind_from_location(resolved_location) == RDI_LocationKind_Set)
        {
          // TODO(rjf)
        }
        
        // rjf: have a resolved location -> produce corresponding IR tree
        {
          B32 need_extra_memread = 0;
          switch((RDI_LocationKindEnum)rdi_kind_from_location(resolved_location))
          {
            case RDI_LocationKind_NULL:
            case RDI_LocationKind_Set:{}break;
            
            //- rjf: bytecode
            case RDI_LocationKind_AddrBytecodeStream:
            case RDI_LocationKind_ValBytecodeStream:
            {
              // TODO(rjf)
            }break;
            
            //- rjf: reg + off
            case RDI_LocationKind_AddrRegPlusOff: goto reg_plus_off;
            case RDI_LocationKind_AddrAddrRegPlusOff: need_extra_memread = 1; goto reg_plus_off;
            reg_plus_off:;
            {
              RDI_TopLevelInfo *tli = rdi_element_from_name_idx(resolved_rdi, TopLevelInfo, 0);
              Arch arch = arch_from_rdi_arch(tli->arch);
              U64 arch_addr_bytesize = byte_size_from_arch(arch);
              RDI_RegCode regcode = rdi_regcode_from_location(resolved_location);
              S64 reg_off = rdi_regoff_from_location(resolved_location);
              // e_oplist_push_op(arena, &result, RDI_EvalOp_RegRead, e_value_u64(RDI_EncodeRegReadParam(regcode, arch_addr_bytesize, 0)));
              // e_oplist_push_sconst(arena, &result, reg_off);
              // e_oplist_push_op(arena, &result, RDI_EvalOp_Add, e_value_u64(0));
              if(need_extra_memread)
              {
                // e_oplist_push_op(arena, &result, RDI_EvalOp_MemRead, e_value_u64(arch_addr_bytesize));
              }
            }break;
            
            
            //- rjf: reg
            case RDI_LocationKind_ValReg:
            {
              RDI_TopLevelInfo *tli = rdi_element_from_name_idx(resolved_rdi, TopLevelInfo, 0);
              Arch arch = arch_from_rdi_arch(tli->arch);
              ARCH_Info *arch_info = arch_info_from_arch(arch);
              RDI_RegCode rdi_regcode = rdi_regcode_from_location(resolved_location);
              ARCH_RegCode reg_code = arch_reg_code_from_rdi(arch, rdi_regcode);
              Rng1U16 reg_rng = arch_info->reg_code_rng_table[reg_code];
              U64 byte_size = (U64)dim_1u16(reg_rng);
              // e_oplist_push_op(arena, &result, RDI_EvalOp_RegRead, e_value_u64(RDI_EncodeRegReadParam(rdi_regcode, byte_size, 0)));
            }break;
            
            //- rjf: space offsets
            case RDI_LocationKind_ModuleOff:
            {
              U64 voff = rdi_voff_from_location(resolved_location);
              irtree = e2_irnode_const_u64_or_smaller(d_eval_cache->arena, voff);
              irtree->mode = E2_Mode_Address;
              irtree->type_key = type_key;
            }break;
            case RDI_LocationKind_TLSOff:
            {
              // U64 toff = rdi_toff_from_location(loc);
              // e_oplist_push_op(arena, &result, RDI_EvalOp_TLSOff, e_value_u64(toff));
            }break;
            case RDI_LocationKind_ConstantDataOff:
            {
              // U64 constant_data_off = rdi_constant_data_off_from_location(loc);
              // e_oplist_push_uconst(arena, &result, constant_data_off);
            }break;
          }
        }
      }
      
      //- rjf: resolve as a built-in constant
      if(!resolved)
      {
        if(str8_match(identifier, s("true"), 0))
        {
          irtree = e2_irnode_const_u64_or_smaller(d_eval_cache->arena, 1);
          irtree->type_key = e2_type_key_basic(E2_TypeKind_Bool);
          resolved = 1;
        }
        else if(str8_match(identifier, s("false"), 0))
        {
          irtree = e2_irnode_const_u64_or_smaller(d_eval_cache->arena, 0);
          irtree->type_key = e2_type_key_basic(E2_TypeKind_Bool);
          resolved = 1;
        }
      }
      
      //- rjf: resolve as a built-in type
      if(!resolved)
      {
        if(0){}
#define BuiltInType_XList \
BasicCase("uint8", U8)\
BasicCase("uint8_t", U8)\
BasicCase("uchar", UChar8)\
BasicCase("uchar8", UChar8)\
BasicCase("uint16", U16)\
BasicCase("uint16_t", U16)\
BasicCase("uchar16", UChar16)\
BasicCase("uint32", U32)\
BasicCase("uint32_t", U32)\
BasicCase("uchar32", UChar32)\
BasicCase("uint64", U64)\
BasicCase("uint64_t", U64)\
BasicCase("uint128", U128)\
BasicCase("uint128_t", U128)\
BasicCase("uint256", U256)\
BasicCase("uint256_t", U256)\
BasicCase("uint512", U512)\
BasicCase("uint512_t", U512)\
BasicCase("int8", S8)\
BasicCase("int8_t", S8)\
BasicCase("char", Char8)\
BasicCase("char8", Char8)\
BasicCase("int16", S16)\
BasicCase("int16_t", S16)\
BasicCase("char16", Char16)\
BasicCase("int32", S32)\
BasicCase("int32_t", S32)\
BasicCase("char32", Char32)\
BasicCase("int64", S64)\
BasicCase("int64_t", S64)\
BasicCase("int128", S128)\
BasicCase("int128_t", S128)\
BasicCase("int256", S256)\
BasicCase("int256_t", S256)\
BasicCase("int512", S512)\
BasicCase("int512_t", S512)\
BasicCase("void", Void)\
BasicCase("bool", Bool)\
BasicCase("float", F32)\
BasicCase("float32", F32)\
BasicCase("double", F64)\
BasicCase("float64", F64)
#define BasicCase(str, kind) else if(str8_match(identifier, s(str), 0)) {resolved = 1; type_key = e2_type_key_basic(E2_TypeKind_##kind);}
        BuiltInType_XList
#undef BasicCase
      }
      
      //- rjf: given a resolved identifier, evaluate it & cache
      if(resolved)
      {
        // rjf: construct tree for this identifier
        E2_Expr *expr = e2_expr(d_eval_cache->arena, ((!e2_type_key_match(irtree->type_key, e2_type_key_zero()) && irtree->mode == E2_Mode_Type) ? E2_ExprKind_TypeIdentifier : E2_ExprKind_Identifier));
        expr->string = identifier;
        expr->qualifiers = qualifiers;
        
        // rjf: create irtree if needed (type evaluations)
        if(type_key.kind != E2_TypeKeyKind_Null && irtree == &e2_irnode_nil)
        {
          irtree = e2_irnode_type(d_eval_cache->arena, type_key);
        }
        
        // rjf: irtree -> bytecode
        String8 bytecode = e2_bytecode_from_irnode(d_eval_cache->arena, irtree);
        
        // rjf: bytecode -> value
        E2_Val val = {0};
        E2_MsgList msgs = {0};
        {
          E2_InterpState state = {0};
          E2_SpaceMap space_map = {0};
          for(;;)
          {
            E2_Interp interp = e2_interp_from_bytecode(d_eval_cache->arena, &state, &space_map, bytecode);
            e2_msg_list_concat_in_place(&msgs, &interp.msgs);
            val = interp.val;
            if(e2_interp_status_is_terminal(interp.status))
            {
              break;
            }
          }
        }
        
        // rjf: generate an ID for this string
        d_eval_cache->id_gen += 1;
        D_EvalID id = d_eval_cache->id_gen;
        U64 id_slot_idx = id%d_eval_cache->slots_count;
        
        // rjf: cache
        {
          D_EvalNode *node = push_array(d_eval_cache->arena, D_EvalNode, 1);
          node->eval.id                = id;
          node->eval.parent_id         = d_eval_cache->top_parent->v;
          node->eval.string            = str8_copy(d_eval_cache->arena, expr_string);
          node->eval.expr              = expr;
          node->eval.irtree            = irtree;
          node->eval.bytecode          = bytecode;
          node->eval.val               = val;
          node->eval.msgs              = msgs;
          SLLQueuePush_N(d_eval_cache->string_slots[string_slot_idx].first, d_eval_cache->string_slots[string_slot_idx].last, node, string_next);
          SLLQueuePush_N(d_eval_cache->id_slots[id_slot_idx].first, d_eval_cache->id_slots[id_slot_idx].last, node, id_next);
          eval = node->eval;
        }
      }
    }
    
    scratch_end(scratch);
  }
  return eval;
}

internal D_Eval
d_eval_from_string(String8 string)
{
  D_Eval eval = {0};
  {
    E2_MsgList msgs = {0};
    
    ////////////////////////////
    //- rjf: string -> expr
    //
    E2_Expr *expr = &e2_expr_nil;
    {
      E2_ParseState state = {0};
      B32 identifier_is_type = 0;
      for(;;)
      {
        E2_Parse parse = e2_parse_from_string(d_eval_cache->arena, &state, identifier_is_type, E2_LangKind_CLike, string);
        identifier_is_type = 0;
        expr = parse.expr;
        switch(parse.status)
        {
          default:{}break;
          
          //- rjf: identifier -> look up evaluation, check if type
          case E2_ParseStatus_CheckIdentifierIsType:
          {
            D_Eval identifier_eval = d_eval_from_leaf_identifier(parse.identifier, parse.qualifiers);
            identifier_is_type = (!e2_type_key_match(identifier_eval.irtree->type_key, e2_type_key_zero()) && identifier_eval.irtree->mode == E2_Mode_Type);
          }break;
        }
        e2_msg_list_concat_in_place(&msgs, &parse.msgs);
        if(e2_parse_status_is_terminal(parse.status))
        {
          break;
        }
      }
    }
    
    ////////////////////////////
    //- rjf: expr -> irtree
    //
    E2_IRNode *irtree = &e2_irnode_nil;
    {
      E2_IRNode *resolve_result = &e2_irnode_nil;
      E2_Val compile_time_eval_result = {0};
      E2_CompileState state = {0};
      for(;;)
      {
        E2_Compile compile = e2_compile_from_expr(d_eval_cache->arena, &state, resolve_result, compile_time_eval_result, expr);
        resolve_result = &e2_irnode_nil;
        MemoryZeroStruct(&compile_time_eval_result);
        irtree = compile.irtree;
        e2_msg_list_concat_in_place(&msgs, &compile.msgs);
        switch(compile.status)
        {
          default:{}break;
          case E2_CompileStatus_MissedIdentifierResolution:
          {
            D_Eval identifier_eval = d_eval_from_leaf_identifier(compile.identifier, compile.qualifiers);
            if(!e2_type_key_match(identifier_eval.irtree->type_key, e2_type_key_zero()))
            {
              resolve_result = e2_irnode(d_eval_cache->arena);
              resolve_result->val = identifier_eval.val;
              resolve_result->type_key = identifier_eval.irtree->type_key;
              resolve_result->mode = identifier_eval.irtree->mode;
              if(identifier_eval.val.string.size != 0)
              {
                resolve_result->op = RDI_EvalOp_ConstString;
              }
              else if(identifier_eval.irtree->mode != E2_Mode_Type)
              {
                resolve_result->op = RDI_EvalOp_ConstU512;
              }
            }
          }break;
          case E2_CompileStatus_NewIdentifierDefinition:
          {
          }break;
          case E2_CompileStatus_MemberAccess:
          {
          }break;
          case E2_CompileStatus_IndexAccess:
          {
          }break;
          case E2_CompileStatus_Call:
          {
          }break;
          case E2_CompileStatus_CompileTimeEval:
          {
          }break;
        }
        if(e2_compile_status_is_terminal(compile.status))
        {
          break;
        }
      }
    }
    
    ////////////////////////////
    //- rjf: ir tree -> bytecode
    //
    String8 bytecode = e2_bytecode_from_irnode(d_eval_cache->arena, irtree);
    
    ////////////////////////////
    //- rjf: bytecode -> value
    //
    E2_Val val = {0};
    {
      E2_InterpState state = {0};
      E2_SpaceMap space_map = {0};
      for(;;)
      {
        E2_Interp interp = e2_interp_from_bytecode(d_eval_cache->arena, &state, &space_map, bytecode);
        val = interp.val;
        e2_msg_list_concat_in_place(&msgs, &interp.msgs);
        if(e2_interp_status_is_terminal(interp.status))
        {
          break;
        }
      }
    }
    
    ////////////////////////////
    //- rjf: fill
    //
    eval.string   = string;
    eval.expr     = expr;
    eval.irtree   = irtree;
    eval.bytecode = bytecode;
    eval.val      = val;
    eval.msgs     = msgs;
  }
  return eval;
}

////////////////////////////////
//~ rjf: Layer Initialization

internal void
d_init(void)
{
  //- rjf: set up ctrl state
  {
    Arena *arena = arena_alloc();
    d_ctrl_state = push_array(arena, D_CtrlState, 1);
    d_ctrl_state->arena = arena;
    d_ctrl_state->sock_session = sock_session_open(0, 0, DS_PACKET_SIZE);
    d_ctrl_state->sock_send_mutex = rw_mutex_alloc();
    d_ctrl_state->sock_event_ring = guarded_ring_alloc(arena, KB(256));
    d_ctrl_state->sock_rings_count = get_system_info()->logical_processor_count;
    d_ctrl_state->sock_rings = push_array(arena, D_SockRingSlot, d_ctrl_state->sock_rings_count);
    for EachIndex(idx, d_ctrl_state->sock_rings_count)
    {
      d_ctrl_state->sock_rings[idx].recv_ring = guarded_ring_alloc(arena, KB(8));
      d_ctrl_state->sock_rings[idx].completion_ring = guarded_ring_alloc(arena, KB(1));
      SLLStackPush(d_ctrl_state->free_sock_ring, &d_ctrl_state->sock_rings[idx]);
    }
    d_ctrl_state->sock_ring_alloc_rw_mutex = rw_mutex_alloc();
    d_ctrl_state->sock_ring_alloc_cv = cond_var_alloc();
    for EachEnumVal(Arch, arch)
    {
      ARCH_Info *arch_info = arch_info_from_arch(arch);
      U64 reg_count = arch_info->reg_code_count;
      String8 *reg_names = arch_info->reg_code_name_table;
      d_ctrl_state->arch_string2reg_tables[arch] = e_string2num_map_make(d_ctrl_state->arena, 256);
      for(U64 idx = 1; idx < reg_count; idx += 1)
      {
        e_string2num_map_insert(d_ctrl_state->arena, &d_ctrl_state->arch_string2reg_tables[arch], reg_names[idx], idx);
      }
    }
    d_ctrl_state->thread_reg_cache.slots_count = 1024;
    d_ctrl_state->thread_reg_cache.slots = push_array(arena, D_ThreadRegCacheSlot, d_ctrl_state->thread_reg_cache.slots_count);
    d_ctrl_state->thread_reg_cache.stripes_count = get_system_info()->logical_processor_count;
    d_ctrl_state->thread_reg_cache.stripes = push_array(arena, D_ThreadRegCacheStripe, d_ctrl_state->thread_reg_cache.stripes_count);
    for(U64 idx = 0; idx < d_ctrl_state->thread_reg_cache.stripes_count; idx += 1)
    {
      d_ctrl_state->thread_reg_cache.stripes[idx].arena = arena_alloc();
      d_ctrl_state->thread_reg_cache.stripes[idx].rw_mutex = rw_mutex_alloc();
    }
    d_ctrl_state->module_info_cache.slots_count = 1024;
    d_ctrl_state->module_info_cache.slots = push_array(arena, D_ModuleInfoCacheSlot, d_ctrl_state->module_info_cache.slots_count);
    d_ctrl_state->module_info_cache.stripes = stripe_array_alloc(arena);
    d_ctrl_state->u2c_ring_size = KB(64);
    d_ctrl_state->u2c_ring_base = push_array_no_zero(arena, U8, d_ctrl_state->u2c_ring_size);
    d_ctrl_state->u2c_ring_mutex = mutex_alloc();
    d_ctrl_state->u2c_ring_cv = cond_var_alloc();
    d_ctrl_state->c2u_ring_size = KB(64);
    d_ctrl_state->c2u_ring_max_string_size = d_ctrl_state->c2u_ring_size/2;
    d_ctrl_state->c2u_ring_base = push_array_no_zero(arena, U8, d_ctrl_state->c2u_ring_size);
    d_ctrl_state->c2u_ring_mutex = mutex_alloc();
    d_ctrl_state->c2u_ring_cv = cond_var_alloc();
    {
      d_ctrl_state->ctrl_thread_log_path = push_str8f(d_ctrl_state->arena, "%S/ctrl_thread.raddbg_log", g_logs_folder);
      write_data_to_file_path(d_ctrl_state->ctrl_thread_log_path, str8_zero());
    }
    d_ctrl_state->ctrl_thread_entity_ctx_rw_mutex = rw_mutex_alloc();
    d_ctrl_state->ctrl_thread_entity_store = d_entity_ctx_rw_store_alloc();
    d_ctrl_state->ctrl_thread_eval_cache = e_cache_alloc();
    d_ctrl_state->ctrl_thread_msg_process_arena = arena_alloc();
    d_ctrl_state->server_slots_count = 16;
    d_ctrl_state->server_slots = push_array(arena, D_ServerSlot, d_ctrl_state->server_slots_count);
    {
      D_ServerState *ss = push_array(arena, D_ServerState, 1);
      ss->server_id = D_ServerID_LocalHost;
      ss->dmn_event_arena = arena_alloc();
      U64 slot_idx = ss->server_id%d_ctrl_state->server_slots_count;
      DLLPushBack_NP(d_ctrl_state->first_server, d_ctrl_state->last_server, ss, order_next, order_prev);
      DLLPushBack_NP(d_ctrl_state->server_slots[slot_idx].first, d_ctrl_state->server_slots[slot_idx].last, ss, hash_next, hash_prev);
      d_ctrl_state->next_dequeue_server = ss;
    }
    d_ctrl_state->user_entry_point_arena = arena_alloc();
    for(D_ExceptionCodeKind k = (D_ExceptionCodeKind)0; k < D_ExceptionCodeKind_COUNT; k = (D_ExceptionCodeKind)(k+1))
    {
      if(d_exception_code_kind_default_enable_table[k])
      {
        d_ctrl_state->exception_code_filters[k/64] |= 1ull<<(k%64);
      }
    }
    d_ctrl_state->ctrl_thread_log = log_alloc();
    d_ctrl_state->ctrl_thread = thread_launch(d_ctrl_thread__entry_point, 0);
    d_ctrl_state->dump_cache.slots_count = 64;
    d_ctrl_state->dump_cache.slots = push_array(arena, D_DumpSlot, d_ctrl_state->dump_cache.slots_count);
    d_ctrl_state->dump_cache.stripes = stripe_array_alloc(arena);
    d_ctrl_state->open_server_cache.arena = arena_alloc();
    d_ctrl_state->open_server_cache.slots_count = 8;
    d_ctrl_state->open_server_cache.slots = push_array(arena, D_OpenServerSlot, d_ctrl_state->open_server_cache.slots_count);
    d_ctrl_state->open_server_cache.rw_mutex = rw_mutex_alloc();
    d_ctrl_state->sock_recv_thread = thread_launch(d_sock_recv_thread_entry_point, 0);
  }
  
  //- rjf: set up user state
  {
    Arena *arena = arena_alloc();
    d_user_state = push_array(arena, D_UserState, 1);
    d_user_state->arena = arena;
    d_user_state->cmds_arena = arena_alloc();
    d_user_state->output_log_root = c_root_alloc();
    d_user_state->ctrl_entity_store = d_entity_ctx_rw_store_alloc();
    d_user_state->ctrl_stop_arena = arena_alloc();
    d_user_state->ctrl_msg_arena = arena_alloc();
    d_user_state->last_ctrl_status_arena = arena_alloc();
    
    // rjf: set up caches
    for(U64 idx = 0; idx < ArrayCount(d_user_state->tls_base_caches); idx += 1)
    {
      d_user_state->tls_base_caches[idx].arena = arena_alloc();
    }
    for(U64 idx = 0; idx < ArrayCount(d_user_state->locals_caches); idx += 1)
    {
      d_user_state->locals_caches[idx].arena = arena_alloc();
    }
    for(U64 idx = 0; idx < ArrayCount(d_user_state->member_caches); idx += 1)
    {
      d_user_state->member_caches[idx].arena = arena_alloc();
    }
    
    // rjf: set up run state
    d_user_state->ctrl_last_run_arena = arena_alloc();
  }
  
  //- rjf: select user state's entity context
  d_select_entity_ctx(&d_user_state->ctrl_entity_store->ctx);
}
