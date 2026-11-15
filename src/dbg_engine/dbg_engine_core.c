// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Generated Code

#include "generated/dbg_engine.meta.c"

////////////////////////////////
//~ rjf: @eval2 Bundled Evaluation Path

//- rjf: helpers

internal void
d_string2num_table_insert(Arena *arena, D_String2NumTable *map, String8 string, U64 num, B32 allow_duplicates)
{
  if(map->slots_count != 0)
  {
    U64 hash = u64_hash_from_str8(string);
    U64 slot_idx = hash%map->slots_count;
    B32 can_insert = 1;
    if(!allow_duplicates)
    {
      for EachNode(n, D_String2NumNode, map->slots[slot_idx].first)
      {
        if(str8_match(n->string, string, 0))
        {
          can_insert = 0;
          break;
        }
      }
    }
    if(can_insert)
    {
      D_String2NumNode *n = push_array(arena, D_String2NumNode, 1);
      SLLQueuePush(map->slots[slot_idx].first, map->slots[slot_idx].last, n);
      n->string = string;
      n->num = num;
    }
  }
}

internal U64
d_num_from_string(D_String2NumTable *map, String8 string)
{
  U64 result = 0;
  if(map->slots_count != 0)
  {
    U64 hash = u64_hash_from_str8(string);
    U64 slot_idx = hash%map->slots_count;
    for EachNode(n, D_String2NumNode, map->slots[slot_idx].first)
    {
      if(str8_match(n->string, string, 0))
      {
        result = n->num;
        break;
      }
    }
  }
  return result;
}

//- rjf: async locals map building

internal AC_Artifact
d_locals_map_artifact_create(String8 key, B32 *cancel_signal, AC_Status *status_out, U64 *gen_out)
{
  AC_Artifact artifact = {0};
  {
    Access *access = access_open();
    
    // rjf: unpack key
    DI_Key dbgi_key = {0};
    U64 voff = 0;
    str8_deserial_read_struct(key, 0, &dbgi_key);
    str8_deserial_read_struct(key, sizeof(dbgi_key), &voff);
    
    // rjf: key -> RDI
    RDI_Parsed *rdi = di_rdi_from_key(access, dbgi_key, 1, 0);
    
    // rjf: RDI / voff -> build locals map
    D_String2NumTable *table = 0;
    Arena *arena = 0;
    if(rdi != &rdi_parsed_nil)
    {
      Temp scratch = scratch_begin(0, 0);
      
      //- rjf: gather all scopes to gather locals from
      typedef struct Task Task;
      struct Task
      {
        Task *next;
        RDI_Scope *scope;
      };
      Task *first_task = 0;
      Task *last_task = 0;
      U64 total_local_count = 0;
      {
        RDI_Scope *seed_scopes[] = 
        {
          rdi_scope_from_voff(rdi, voff),
          rdi_scope_from_voff(rdi, voff-1),
        };
        for EachElement(seed_scope_idx, seed_scopes)
        {
          RDI_Scope *nil_scope = rdi_element_from_name_idx(rdi, Scopes, 0);
          for(RDI_Scope *s = seed_scopes[seed_scope_idx];
              s != 0 && s != nil_scope;
              s = rdi_element_from_name_idx(rdi, Scopes, s->parent_scope_idx))
          {
            total_local_count += s->local_count;
            Task *task = push_array(scratch.arena, Task, 1);
            task->scope = s;
            SLLQueuePush(first_task, last_task, task);
          }
        }
      }
      
      //- rjf: gather locals from all scopes
      if(total_local_count != 0)
      {
        arena = arena_alloc();
        table = push_array(arena, D_String2NumTable, 1);
        table->slots_count = total_local_count + total_local_count/2;
        table->slots = push_array(arena, D_String2NumSlot, table->slots_count);
        for(Task *t = first_task; t != 0; t = t->next)
        {
          RDI_Scope *scope = t->scope;
          for EachIndex(scope_local_idx, scope->local_count)
          {
            U32 local_idx = scope->local_first + scope_local_idx;
            RDI_Symbol *local = rdi_element_from_name_idx(rdi, LocalVariables, local_idx);
            String8 name = str8_from_rdi_string_idx(rdi, local->name_string_idx);
            d_string2num_table_insert(arena, table, name, local_idx, 1);
          }
        }
      }
      
      scratch_end(scratch);
    }
    
    // rjf: locals map -> artifact
    artifact.u64[0] = (U64)table;
    artifact.u64[1] = (U64)arena;
    
    access_close(access);
  }
  return artifact;
}

internal void
d_locals_map_artifact_destroy(AC_Artifact artifact)
{
  Arena *arena = (Arena *)artifact.u64[1];
  if(arena != 0)
  {
    arena_release(arena);
  }
}

internal D_String2NumTable *
d_locals_map_from_dbgi_key_voff(Access *access, DI_Key dbgi_key, U64 voff, U64 endt_us)
{
  D_String2NumTable *result = &d_nil_string2num_table;
  {
    U64 key_data[] =
    {
      dbgi_key.u64[0],
      dbgi_key.u64[1],
      voff,
    };
    String8 key = str8((U8 *)key_data, sizeof(key_data));
    AC_Artifact artifact = ac_artifact_from_key(access, key, d_locals_map_artifact_create, d_locals_map_artifact_destroy, endt_us);
    if(artifact.u64[0] != 0)
    {
      result = (D_String2NumTable *)artifact.u64[0];
    }
  }
  return result;
}

//- rjf: space reads/writes

internal B32
d_space_write(E2_SpaceID space_id, void *in, Rng1U64 range)
{
  B32 result = 0;
  return result;
}

internal B32
d_space_read(E2_SpaceID space_id, void *out, Rng1U64 range)
{
  B32 result = 0;
  switch(d_space_kind_from_id(space_id))
  {
    default:{}break;
    
    ////////////////////////////
    //- rjf: address space reads
    //
    case D_SpaceKind_Memory:
    {
      D_EntityID entity_id = d_entity_id_from_space_id(space_id);
      D_Entity *entity = d_entity_from_id(entity_id);
      D_Entity *process = d_process_from_entity(entity);
    }break;
    
    ////////////////////////////
    //- rjf: register block space reads
    //
    case D_SpaceKind_Registers:
    {
      Temp scratch = scratch_begin(0, 0);
      Access *access = access_open();
      
      // rjf: get architecture / reg block
      Arch arch = Arch_Null;
      void *regs = 0;
      {
        D_EntityID thread_id = d_entity_id_from_space_id(space_id);
        U64 unwind_idx = d_index_from_space_id(space_id);
        D_Entity *thread = d_entity_from_id(thread_id);
        arch = thread->arch;
        if(arch != Arch_Null)
        {
          switch(unwind_idx)
          {
            case 0:
            {
              regs = d_cached_reg_block_from_thread(scratch.arena, thread->handle);
            }break;
            default:
            {
              D_CallStack call_stack = d_call_stack_from_thread(access, thread->handle, 1, d_eval_cache->async_endt_us);
              U64 concrete_frame_idx = unwind_idx;
              if(concrete_frame_idx < call_stack.concrete_frames_count)
              {
                D_CallStackFrame *f = call_stack.concrete_frames[concrete_frame_idx];
                regs = f->regs;
              }
            }break;
          }
        }
      }
      
      // rjf: do read
      if(regs != 0)
      {
        ARCH_Info *arch_info = arch_info_from_arch(arch);
        U64 regs_size = arch_info->reg_block_size;
        Rng1U64 legal_range = r1u64(0, regs_size);
        Rng1U64 read_range = intersect_1u64(legal_range, range);
        U64 read_size = dim_1u64(read_range);
        MemoryCopy(out, (U8 *)regs + read_range.min, read_size);
        result = (read_size == dim_1u64(range));
      }
      
      access_close(access);
      scratch_end(scratch);
    }break;
  }
  return result;
}

//- rjf: eval building helpers

internal E2_Val
d_eval_val_from_bytecode(Arena *arena, String8 bytecode, E2_SpaceID *space_id_out, E2_MsgList *msgs_out)
{
  E2_Val val = {0};
  {
    Temp scratch = scratch_begin(&arena, 1);
    E2_InterpState state = {0};
    E2_SpaceMap space_map = {0};
    for(;;)
    {
      E2_Interp interp = e2_interp_from_bytecode(arena, &state, &space_map, bytecode);
      e2_msg_list_concat_in_place(msgs_out, &interp.msgs);
      val = interp.val;
      switch(interp.status)
      {
        default:{}break;
        
        //- rjf: missed read from space -> do read & insert into space map
        case E2_InterpStatus_MissedSpaceRead:
        {
          E2_SpaceID space_id = interp.space_id;
          Rng1U64 range = interp.missed_read_space_addr_range;
          void *data = push_array(scratch.arena, U8, dim_1u64(range));
          if(d_space_read(space_id, data, range))
          {
            e2_space_map_push(scratch.arena, &space_map, space_id, range, data);
          }
        }break;
        
        //- rjf: changing evaluation context -> unpack space ID, fill out state's ctx accordingly
        case E2_InterpStatus_NewSpaceID:
        {
          if(space_id_out)
          {
            space_id_out[0] = interp.space_id;
          }
          MemoryZeroStruct(&state.selected_ctx);
          switch(d_space_kind_from_id(interp.space_id))
          {
            default:{}break;
            case D_SpaceKind_Memory:
            case D_SpaceKind_Registers:
            {
              D_EntityID entity_id = d_entity_id_from_space_id(interp.space_id);
              D_Entity *entity = d_entity_from_id(entity_id);
              state.selected_ctx.arch = entity->arch;
              if(entity->kind == D_EntityKind_Thread)
              {
                U64 unwind_idx = d_index_from_space_id(interp.space_id);
                U64 ip_vaddr = d_cached_ip_from_thread_unwind(entity, unwind_idx);
                D_Entity *process = d_process_from_entity(entity);
                D_Entity *module = d_module_from_process_vaddr(process, ip_vaddr);
                state.selected_ctx.reg_space_id     = d_registers_space_id(entity->id, unwind_idx);
                state.selected_ctx.addr             = ip_vaddr;
                state.selected_ctx.module_base_addr = module->vaddr_range.min;
                state.selected_ctx.tls_base_addr    = d_cached_tls_vaddr_from_thread_module(entity->handle, module->handle, d_eval_cache->async_endt_us, 0);
                state.selected_ctx.cfa_addr         = d_cached_cfa_from_thread_unwind(entity, unwind_idx);
                state.selected_ctx.flags            = (E2_CtxFlag_HasModuleBase|E2_CtxFlag_HasCFA|E2_CtxFlag_HasTLSBase);
                if(d_space_kind_from_id(interp.space_id) == D_EvalCtxIDKind_Registers)
                {
                  state.selected_ctx.space_id = state.selected_ctx.reg_space_id;
                }
                else
                {
                  state.selected_ctx.reg_space_id = d_memory_space_id(entity->id, unwind_idx);
                }
              }
            }break;
          }
        }break;
      }
      if(e2_interp_status_is_terminal(interp.status))
      {
        break;
      }
    }
    scratch_end(scratch);
  }
  return val;
}

internal E2_Val
d_eval_val_from_irtree(Arena *arena, E2_IRNode *irtree, E2_SpaceID *space_id_out, E2_MsgList *msgs_out)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8 bytecode = e2_bytecode_from_irnode(scratch.arena, irtree);
  E2_Val val = d_eval_val_from_bytecode(arena, bytecode, space_id_out, msgs_out);
  scratch_end(scratch);
  return val;
}

internal D_Entity *
d_entity_from_irtree(Arena *arena, E2_IRNode *irtree, E2_MsgList *msgs_out)
{
  E2_SpaceID space_id = 0;
  E2_Val val = d_eval_val_from_irtree(arena, irtree, &space_id, msgs_out);
  D_Entity *entity = d_entity_from_id(val.u64);
  return entity;
}

internal ARCH_RegCode
d_eval_reg_code_from_arch_string(Arch arch, String8 string)
{
  ARCH_Info *arch_info = arch_info_from_arch(arch);
  ARCH_RegCode reg_code = 0;
  {
    D_String2NumTable *string2regcode = &d_shared->arch_string2regcode_tables[arch];
    if(string2regcode->slots_count != 0)
    {
      reg_code = (ARCH_RegCode)d_num_from_string(string2regcode, string);
    }
  }
  return reg_code;
}

internal E2_IRNode *
d_eval_irtree_from_arch_regcode(Arena *arena, D_EntityID thread_id, U64 frame_idx, Arch arch, ARCH_RegCode reg_code)
{
  E2_IRNode *result = &e2_irnode_nil;
  ARCH_Info *arch_info = arch_info_from_arch(arch);
  
  // rjf: reg code -> range
  Rng1U16 range = {0};
  if(0 < reg_code && reg_code < arch_info->reg_code_count)
  {
    range = arch_info->reg_code_rng_table[reg_code];
  }
  U16 range_size = dim_1u16(range);
  
  // rjf: form final evaluation
  if(range_size != 0)
  {
    result = e2_irnode(arena);
    result->type_key = e2_uint_type_key_from_byte_size(range_size);
    result->mode     = E2_Mode_Address;
    result->op       = E2_EvalOp_SetSpaceID;
    result->val.u64  = d_registers_space_id(thread_id, frame_idx);
    E2_IRNode *reg_addr = e2_irnode_const_u64_or_smaller(arena, range.min);
    e2_irnode_push_child(arena, result, reg_addr);
  }
  
  return result;
}

internal E2_IRNode *
d_eval_irtree_from_location(Arena *arena, RDI_Parsed *rdi, RDI_Location location, U64 ctx_voff)
{
  E2_IRNode *irtree = &e2_irnode_nil;
  
  //- rjf: resolve location by contextual evaluation voff
  location = rdi_location_from_location_voff(rdi, location, ctx_voff);
  
  //- rjf: location -> irtree
  {
    B32 need_extra_memread = 0;
    switch((RDI_LocationKindEnum)rdi_kind_from_location(location))
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
        RDI_TopLevelInfo *tli = rdi_element_from_name_idx(rdi, TopLevelInfo, 0);
        Arch arch = arch_from_rdi_arch(tli->arch);
        U64 arch_addr_bytesize = byte_size_from_arch(arch);
        RDI_RegCode regcode = rdi_regcode_from_location(location);
        S64 reg_off = rdi_regoff_from_location(location);
        E2_IRNode *reg_read = e2_irnode(arena);
        reg_read->op = RDI_EvalOp_RegRead;
        reg_read->val.u64 = RDI_EncodeRegReadParam(regcode, arch_addr_bytesize, 0);
        E2_IRNode *reg_offset = e2_irnode_const_u64_or_smaller(arena, (U64)reg_off);
        E2_IRNode *reg_plus_offset = e2_irnode_binary_op(arena, e2_type_key_basic(E2_TypeKind_U64), RDI_EvalOp_Add, reg_read, reg_offset);
        irtree = reg_plus_offset;
        if(need_extra_memread)
        {
          E2_IRNode *mem_read = e2_irnode(arena);
          mem_read->op = RDI_EvalOp_MemRead;
          mem_read->val.u64 = arch_addr_bytesize;
          mem_read->type_key = e2_type_key_basic(E2_TypeKind_U64);
          e2_irnode_push_child(arena, mem_read, reg_plus_offset);
          irtree = mem_read;
        }
        irtree->mode = E2_Mode_Address;
      }break;
      
      //- rjf: reg
      case RDI_LocationKind_ValReg:
      {
        RDI_TopLevelInfo *tli = rdi_element_from_name_idx(rdi, TopLevelInfo, 0);
        Arch arch = arch_from_rdi_arch(tli->arch);
        ARCH_Info *arch_info = arch_info_from_arch(arch);
        RDI_RegCode rdi_regcode = rdi_regcode_from_location(location);
        ARCH_RegCode reg_code = arch_reg_code_from_rdi(arch, rdi_regcode);
        Rng1U16 reg_rng = arch_info->reg_code_rng_table[reg_code];
        U64 byte_size = (U64)dim_1u16(reg_rng);
        // e_oplist_push_op(arena, &result, RDI_EvalOp_RegRead, e_value_u64(RDI_EncodeRegReadParam(rdi_regcode, byte_size, 0)));
      }break;
      
      //- rjf: space offsets
      case RDI_LocationKind_ModuleOff:
      {
        U64 voff = rdi_voff_from_location(location);
        irtree = e2_irnode_const_u64_or_smaller(arena, voff);
        irtree->mode = E2_Mode_Address;
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
  
  return irtree;
}

internal E2_IRNode *
d_eval_irtree_from_thread_unwind_idx_local_string(Arena *arena, D_Entity *thread, U64 unwind_idx, String8 name)
{
  E2_IRNode *result = &e2_irnode_nil;
  if(thread->kind == D_EntityKind_Thread)
  {
    Access *access = access_open();
    U64 ip_vaddr = d_cached_ip_from_thread_unwind(thread, unwind_idx);
    D_Entity *process = d_process_from_entity(thread);
    D_Entity *module = d_module_from_process_vaddr(process, ip_vaddr);
    DI_Key dbgi_key = d_dbgi_key_from_module(module);
    U64 ip_voff = d_voff_from_vaddr(module, ip_vaddr);
    RDI_Parsed *rdi = di_rdi_from_key(access, dbgi_key, 1, d_eval_cache->async_endt_us);
    RDI_Scope *scope = rdi_scope_from_voff(rdi, ip_voff);
    U64 scope_voff = rdi_first_voff_from_scope(rdi, scope);
    D_String2NumTable *locals_map = d_locals_map_from_dbgi_key_voff(access, dbgi_key, scope_voff, d_eval_cache->async_endt_us);
    U64 local_num = d_num_from_string(locals_map, name);
    if(local_num != 0)
    {
      // rjf: dbgi key -> debug info ID
      //
      // TODO(rjf): @eval2 we need to support non-linear lookups, (dbgi key -> dbgi ID)
      //
      E2_DbgInfoID dbgi_id = 0;
      for EachIndex(idx, e2_assets->dbg_infos_count)
      {
        if(di_key_match(e2_assets->dbg_infos[idx].dbgi_key, dbgi_key))
        {
          dbgi_id = e2_assets->dbg_infos[idx].id;
          break;
        }
      }
      RDI_Symbol *lvar = rdi_element_from_name_idx(rdi, LocalVariables, local_num);
      RDI_TypeNode *type_node = rdi_element_from_name_idx(rdi, TypeNodes, lvar->type_idx);
      E2_IRNode *compute_loc = d_eval_irtree_from_location(arena, rdi, lvar->location, ip_voff);
      if(compute_loc != &e2_irnode_nil)
      {
        E2_SpaceID new_space_id = d_memory_space_id(thread->id, unwind_idx);
        E2_IRNode *set_space_id = e2_irnode_set_space_id(arena, new_space_id);
        result = e2_irnode(arena);
        e2_irnode_push_child(arena, result, set_space_id);
        e2_irnode_push_child(arena, result, compute_loc);
        result->type_key = e2_type_key_dbgi(e2_type_kind_from_rdi(type_node->kind), dbgi_id, lvar->type_idx);
        result->mode = compute_loc->mode;
      }
    }
    access_close(access);
  }
  return result;
}

//- rjf: eval cache operations

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
d_eval_cache_select(D_EvalCache *cache, D_EntityID selected_thread, U32 selected_unwind_idx, U64 async_endt_us)
{
  d_eval_cache = cache;
  cache->async_endt_us = async_endt_us;
  cache->selected_thread = selected_thread;
  cache->selected_unwind_idx = selected_unwind_idx;
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
      String8 bytecode = {0};
      E2_MsgList msgs = {0};
      E2_SpaceID space_id = 0;
      E2_Val val = {0};
      B32 need_bytecode = 1;
      B32 need_val = 1;
      
      //- rjf: resolve as parent expression marker ($)
      if(!resolved && str8_match(identifier, s("$"), 0) && d_eval_cache->top_parent->v != 0)
      {
        U64 parent_slot_idx = d_eval_cache->top_parent->v%d_eval_cache->slots_count;
        for(D_EvalNode *n = d_eval_cache->string_slots[string_slot_idx].first; n != 0; n = n->string_next)
        {
          if(n->eval.id == d_eval_cache->top_parent->v)
          {
            need_bytecode = 0;
            need_val = 0;
            irtree   = n->eval.irtree;
            bytecode = n->eval.bytecode;
            msgs     = n->eval.msgs;
            space_id = n->eval.space_id;
            val      = n->eval.val;
            break;
          }
        }
      }
      
      //- rjf: resolve as a local variable name (within the implicitly selected context)
      if(!resolved)
      {
        D_Entity *thread = d_entity_from_id(d_eval_cache->selected_thread);
        U64 unwind_idx = d_eval_cache->selected_unwind_idx;
        irtree = d_eval_irtree_from_thread_unwind_idx_local_string(d_eval_cache->arena, thread, unwind_idx, identifier);
        resolved = (irtree != &e2_irnode_nil);
      }
      
      //- rjf: resolve as a register name (within the implicitly selected context)
      if(!resolved)
      {
        D_Entity *thread = d_entity_from_id(d_eval_cache->selected_thread);
        Arch arch = thread->arch;
        ARCH_RegCode reg_code = d_eval_reg_code_from_arch_string(arch, identifier);
        irtree = d_eval_irtree_from_arch_regcode(d_eval_cache->arena, thread->id, d_eval_cache->selected_unwind_idx, arch, reg_code);
        resolved = (irtree != &e2_irnode_nil);
      }
      
      //- rjf: resolve as an implicit parent expression member name
      if(!resolved && d_eval_cache->top_parent->v != 0)
      {
        U64 parent_slot_idx = d_eval_cache->top_parent->v%d_eval_cache->slots_count;
        for(D_EvalNode *n = d_eval_cache->string_slots[string_slot_idx].first; n != 0; n = n->string_next)
        {
          if(n->eval.id == d_eval_cache->top_parent->v)
          {
            // TODO(rjf): we need to compile the equivalent of $.<identifier> here, *without*
            // recursing into another sub-compilation. probably we want to extract member access
            // compilation logic, and use that to build our ir tree?
            break;
          }
        }
      }
      
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
        B32 force_value_mode = 0;
        if(match.section_kind != RDI_SectionKind_NULL)
        {
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
              force_value_mode = 1;
            }break;
            case RDI_SectionKind_TypeNodes:
            {
              RDI_TypeNode *type_node = rdi_element_from_name_idx(resolved_rdi, TypeNodes, match.idx);
              type_key = e2_type_key_dbgi(e2_type_kind_from_rdi(type_node->kind), dbgi_id, match.idx);
            }break;
          }
        }
        
        // rjf: location -> IR tree
        irtree = d_eval_irtree_from_location(d_eval_cache->arena, resolved_rdi, resolved_location, 0);
        resolved = (irtree != &e2_irnode_nil || !e2_type_key_match(e2_type_key_zero(), type_key));
        if(irtree != &e2_irnode_nil)
        {
          irtree->type_key = type_key;
          if(force_value_mode)
          {
            irtree->mode = E2_Mode_Value;
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
        resolved = 1;
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
#define BasicCase(str, kind) else if(str8_match(identifier, s(str), 0)) {type_key = e2_type_key_basic(E2_TypeKind_##kind);}
        BuiltInType_XList
#undef BasicCase
        else
        {
          resolved = 0;
        }
      }
      
      //- rjf: resolve as a debug entity
      if(!resolved)
      {
        D_EntityID entity_id = 0;
        if(0){}
#define Case(string, id) else if(str8_match(identifier, s(string), 0)) do { entity_id = (id); } while(0)
        Case("current_thread",  d_eval_cache->selected_thread);
        Case("current_process", d_entity_ancestor_from_kind(d_entity_from_id(d_eval_cache->selected_thread), D_EntityKind_Process)->id);
        Case("current_server",  d_entity_ancestor_from_kind(d_entity_from_id(d_eval_cache->selected_thread), D_EntityKind_Server)->id);
        Case("current_module",  d_module_from_process_vaddr(d_entity_ancestor_from_kind(d_entity_from_id(d_eval_cache->selected_thread), D_EntityKind_Process), d_cached_ip_from_thread(d_entity_from_id(d_eval_cache->selected_thread)->handle))->id);
#undef Case
        if(entity_id != 0)
        {
          resolved = 1;
          D_Entity *entity = d_entity_from_id(entity_id);
          irtree = e2_irnode_set_u64(d_eval_cache->arena, d_entity_kind_code_name_table[entity->kind], entity_id);
        }
      }
      
      //- rjf: resolve as current-thread accesses
      if(!resolved)
      {
        if(str8_match(identifier, s("call_stack"), 0))
        {
          D_EntityID id = d_eval_cache->selected_thread;
          if(id != 0)
          {
            resolved = 1;
            irtree = e2_irnode_set_u64(d_eval_cache->arena, identifier, id);
          }
        }
      }
      
      //- rjf: resolve as a call stack frame
      if(!resolved)
      {
        if(str8_match(identifier, s("current_call_stack_frame"), 0))
        {
          D_EntityID id = d_eval_cache->selected_thread;
          U32 unwind_idx = d_eval_cache->selected_unwind_idx;
          if(id != 0)
          {
            resolved = 1;
            irtree = e2_irnode_set_u64(d_eval_cache->arena, s("call_stack_frame"), d_frame_id(id, unwind_idx));
          }
        }
      }
      
      //- rjf: resolve as current-call-stack-frame collections
      if(!resolved)
      {
        if(str8_match(identifier, s("locals"), 0) ||
           str8_match(identifier, s("registers"), 0))
        {
          D_EntityID id = d_eval_cache->selected_thread;
          U32 unwind_idx = d_eval_cache->selected_unwind_idx;
          if(id != 0)
          {
            resolved = 1;
            irtree = e2_irnode_set_u64(d_eval_cache->arena, identifier, d_frame_id(id, unwind_idx));
          }
        }
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
        if(need_bytecode)
        {
          bytecode = e2_bytecode_from_irnode(d_eval_cache->arena, irtree);
        }
        
        // rjf: bytecode -> value
        if(need_val)
        {
          val = d_eval_val_from_bytecode(d_eval_cache->arena, bytecode, &space_id, &msgs);
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
          node->eval.space_id          = space_id;
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
  D_Eval eval = {.expr = &e2_expr_nil, .irtree = &e2_irnode_nil};
  
  //////////////////////////////
  //- rjf: find existing evaluation of this expression
  //
  B32 found = 0;
  U64 string_hash = u64_hash_from_str8(string);
  U64 string_slot_idx = string_hash%d_eval_cache->slots_count;
  {
    for(D_EvalNode *n = d_eval_cache->string_slots[string_slot_idx].first; n != 0; n = n->string_next)
    {
      if(str8_match(n->eval.string, string, 0) && n->eval.parent_id == d_eval_cache->top_parent->v)
      {
        found = 1;
        eval = n->eval;
        break;
      }
    }
  }
  
  //////////////////////////////
  //- rjf: not found? -> evaluate & cache
  //
  if(!found)
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
          
          //////////////////////
          //- rjf: missed identifier -> look up resolved identifier & plug back in
          //
          case E2_CompileStatus_MissedIdentifierResolution:
          {
            D_Eval identifier_eval = d_eval_from_leaf_identifier(compile.identifier, compile.qualifiers);
            if(!e2_type_key_match(identifier_eval.irtree->type_key, e2_type_key_zero()))
            {
              E2_IRNode *identifier_eval_val = e2_irnode(d_eval_cache->arena);
              identifier_eval_val->val = identifier_eval.val;
              if(identifier_eval.val.string.size != 0)
              {
                identifier_eval_val->op = RDI_EvalOp_ConstString;
              }
              else if(identifier_eval.irtree->mode == E2_Mode_Address)
              {
                identifier_eval_val->op = RDI_EvalOp_ConstU64;
              }
              else if(identifier_eval.irtree->mode == E2_Mode_Value)
              {
                U64 byte_size = e2_byte_size_from_type_key(identifier_eval.irtree->type_key);
                switch(byte_size)
                {
                  default:{identifier_eval_val->op = RDI_EvalOp_ConstU512;}break;
                  case 1: {identifier_eval_val->op = RDI_EvalOp_ConstU8;}break;
                  case 2: {identifier_eval_val->op = RDI_EvalOp_ConstU16;}break;
                  case 4: {identifier_eval_val->op = RDI_EvalOp_ConstU32;}break;
                  case 8: {identifier_eval_val->op = RDI_EvalOp_ConstU64;}break;
                  case 16:{identifier_eval_val->op = RDI_EvalOp_ConstU128;}break;
                  case 32:{identifier_eval_val->op = RDI_EvalOp_ConstU256;}break;
                }
              }
              resolve_result = identifier_eval_val;
              if(identifier_eval.space_id != 0)
              {
                E2_IRNode *set_space_id = e2_irnode_set_space_id(d_eval_cache->arena, identifier_eval.space_id);
                e2_irnode_push_child(d_eval_cache->arena, set_space_id, identifier_eval_val);
                resolve_result = set_space_id;
              }
              resolve_result->type_key = identifier_eval.irtree->type_key;
              resolve_result->mode = identifier_eval.irtree->mode;
              msgs = identifier_eval.msgs;
            }
          }break;
          
          //////////////////////
          //- rjf: new macro definition
          //
          case E2_CompileStatus_NewIdentifierDefinition:
          {
            // TODO(rjf)
          }break;
          
          //////////////////////
          //- rjf: dot operator
          //
          case E2_CompileStatus_MemberAccess:
          {
            E2_TypeKey type_key = compile.irtree->type_key;
            E2_TypeKind type_kind = e2_type_kind_from_key(type_key);
            switch(type_kind)
            {
              default:{}break;
              
              //////////////////
              //- rjf: set . accesses
              //
              case E2_TypeKind_Set:
              {
                Temp scratch = scratch_begin(0, 0);
                String8 type_name = e2_name_from_type_key(scratch.arena, type_key);
                MD_Node *entity_schema = cfg_schema_from_name(&d_shared->entity_schema_table, type_name);
                MD_Node *cfg_schema = cfg_schema_from_name(&d_shared->cfg_schema_table, type_name);
                
                ////////////////
                //- rjf: debug entity . accesses
                //
                if(entity_schema != &md_nil_node)
                {
                  D_Entity *lhs_entity = d_entity_from_irtree(d_eval_cache->arena, compile.irtree, &msgs);
                  if(lhs_entity != &d_entity_nil)
                  {
                    // NOTE(rjf): @schema_builtin entity .id access
                    if(str8_match(compile.member_name, s("id"), 0))
                    {
                      resolve_result = e2_irnode_const_u64(d_eval_cache->arena, lhs_entity->os_id);
                    }
                    
                    // NOTE(rjf): @schema_builtin entity .name access
                    else if(str8_match(compile.member_name, s("name"), 0))
                    {
                      String8 name = lhs_entity->string;
                      resolve_result = e2_irnode_set_space_id(d_eval_cache->arena, d_entity_metadata_space_id(lhs_entity->id));
                      resolve_result->type_key = e2_type_key_cons_array(e2_type_key_basic(E2_TypeKind_U8), name.size);
                      resolve_result->mode     = E2_Mode_Address;
                      E2_IRNode *member_addr = e2_irnode_const_u64_or_smaller(d_eval_cache->arena, u64_hash_from_str8(compile.member_name));
                      e2_irnode_push_child(d_eval_cache->arena, resolve_result, member_addr);
                    }
                    
                    // NOTE(rjf): @schema_builtin entity .active access
                    else if(str8_match(compile.member_name, s("active"), 0))
                    {
                      resolve_result = e2_irnode_set_space_id(d_eval_cache->arena, d_entity_metadata_space_id(lhs_entity->id));
                      resolve_result->type_key = e2_type_key_basic(E2_TypeKind_Bool);
                      resolve_result->mode     = E2_Mode_Address;
                      E2_IRNode *member_addr = e2_irnode_const_u64_or_smaller(d_eval_cache->arena, u64_hash_from_str8(compile.member_name));
                      e2_irnode_push_child(d_eval_cache->arena, resolve_result, member_addr);
                    }
                    
                    // NOTE(rjf): schema set members
                    else
                    {
                      MD_Node *member_schema = md_child_from_string(entity_schema, compile.member_name, 0);
                      if(str8_match(member_schema->first->string, s("set"), 0))
                      {
                        resolve_result = e2_irnode_set_u64(d_eval_cache->arena, compile.member_name, lhs_entity->id);
                      }
                    }
                  }
                }
                
                ////////////////
                //- rjf: cfg . accesses
                //
                else if(cfg_schema != &md_nil_node)
                {
                  // TODO(rjf)
                }
                
                ////////////////
                //- rjf: call stack frame . accesses
                //
                else if(str8_match(type_name, s("call_stack_frame"), 0))
                {
                  if(str8_match(compile.member_name, s("registers"), 0) ||
                     str8_match(compile.member_name, s("locals"), 0))
                  {
                    E2_Val lhs_val = d_eval_val_from_irtree(d_eval_cache->arena, compile.irtree, 0, &msgs);
                    resolve_result = e2_irnode_set_u64(d_eval_cache->arena, compile.member_name, lhs_val.u64);
                  }
                }
                
                ////////////////
                //- rjf: registers . accesses
                //
                else if(str8_match(type_name, s("registers"), 0))
                {
                  E2_Val lhs_val = d_eval_val_from_irtree(d_eval_cache->arena, compile.irtree, 0, &msgs);
                  D_EntityID lhs_entity_id = d_entity_id_from_frame_id(lhs_val.u64);
                  U32 lhs_idx = d_unwind_idx_from_frame_id(lhs_val.u64);
                  D_Entity *lhs_entity = d_entity_from_id(lhs_entity_id);
                  if(lhs_entity->kind == D_EntityKind_Thread)
                  {
                    Arch arch = lhs_entity->arch;
                    ARCH_RegCode reg_code = d_eval_reg_code_from_arch_string(arch, compile.member_name);
                    resolve_result = d_eval_irtree_from_arch_regcode(d_eval_cache->arena, lhs_entity_id, lhs_idx, arch, reg_code);
                  }
                }
                
                ////////////////
                //- rjf: locals . accesses
                //
                else if(str8_match(type_name, s("locals"), 0))
                {
                  E2_Val lhs_val = d_eval_val_from_irtree(d_eval_cache->arena, compile.irtree, 0, &msgs);
                  D_EntityID lhs_entity_id = d_entity_id_from_frame_id(lhs_val.u64);
                  D_Entity *lhs_entity = d_entity_from_id(lhs_entity_id);
                  if(lhs_entity->kind == D_EntityKind_Thread)
                  {
                    U32 lhs_idx = d_unwind_idx_from_frame_id(lhs_val.u64);
                    resolve_result = d_eval_irtree_from_thread_unwind_idx_local_string(d_eval_cache->arena, lhs_entity, lhs_idx, compile.member_name);
                  }
                }
                
                scratch_end(scratch);
              }break;
            }
          }break;
          
          //- rjf: indexing operator
          case E2_CompileStatus_IndexAccess:
          {
            E2_TypeKey type_key = compile.irtree->type_key;
            E2_TypeKind type_kind = e2_type_kind_from_key(type_key);
            switch(type_kind)
            {
              default:{}break;
              
              //- rjf: set [] accesses
              case E2_TypeKind_Set:
              {
                Temp scratch = scratch_begin(0, 0);
                String8 type_name = e2_name_from_type_key(scratch.arena, type_key);
                
                //- rjf: call stack [] accesses
                if(str8_match(type_name, s("call_stack"), 0))
                {
                  // rjf: unpack lhs
                  E2_Val lhs_val = d_eval_val_from_irtree(d_eval_cache->arena, compile.irtree, 0, &msgs);
                  D_EntityID lhs_entity_id = lhs_val.u64;
                  D_Entity *lhs_entity = d_entity_from_id(lhs_entity_id);
                  
                  // rjf: lhs is thread -> resolve to frame
                  if(lhs_entity->kind == D_EntityKind_Thread)
                  {
                    E2_Val rhs_val = d_eval_val_from_irtree(d_eval_cache->arena, compile.params_irtree, 0, &msgs);
                    U64 rhs_idx = rhs_val.u64;
                    D_FrameID frame_id = d_frame_id(lhs_entity_id, rhs_idx);
                    resolve_result = e2_irnode_set_u64(d_eval_cache->arena, s("call_stack_frame"), frame_id);
                  }
                }
                scratch_end(scratch);
              }break;
            }
          }break;
          
          //- rjf: calls
          case E2_CompileStatus_Call:
          {
          }break;
          
          //- rjf: compile-time-evaluation
          case E2_CompileStatus_CompileTimeEval:
          {
            compile_time_eval_result = d_eval_val_from_irtree(d_eval_cache->arena, compile.irtree, 0, &msgs);
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
    E2_SpaceID space_id = 0;
    E2_Val val = d_eval_val_from_bytecode(d_eval_cache->arena, bytecode, &space_id, &msgs);
    
    ////////////////////////////
    //- rjf: generate an ID for this expression string
    //
    d_eval_cache->id_gen += 1;
    D_EvalID id = d_eval_cache->id_gen;
    U64 id_slot_idx = id%d_eval_cache->slots_count;
    
    ////////////////////////////
    //- rjf: store in cache
    //
    {
      D_EvalNode *node = push_array(d_eval_cache->arena, D_EvalNode, 1);
      node->eval.id                = id;
      node->eval.parent_id         = d_eval_cache->top_parent->v;
      node->eval.string            = str8_copy(d_eval_cache->arena, string);
      node->eval.expr              = expr;
      node->eval.irtree            = irtree;
      node->eval.bytecode          = bytecode;
      node->eval.space_id          = space_id;
      node->eval.val               = val;
      node->eval.msgs              = msgs;
      SLLQueuePush_N(d_eval_cache->string_slots[string_slot_idx].first, d_eval_cache->string_slots[string_slot_idx].last, node, string_next);
      SLLQueuePush_N(d_eval_cache->id_slots[id_slot_idx].first, d_eval_cache->id_slots[id_slot_idx].last, node, id_next);
      eval = node->eval;
    }
  }
  
  return eval;
}

////////////////////////////////
//~ rjf: Layer Initialization

internal void
d_init(void)
{
  //- rjf: set up shared state
  {
    Arena *arena = arena_alloc();
    d_shared = push_array(arena, D_Shared, 1);
    d_shared->arena = arena;
    
    // rjf: build per-architecture (name -> regcode) tables
    for EachNonZeroEnumVal(Arch, arch)
    {
      ARCH_Info *arch_info = arch_info_from_arch(arch);
      D_String2NumTable *table = &d_shared->arch_string2regcode_tables[arch];
      table->slots_count = arch_info->reg_code_count;
      table->slots = push_array(arena, D_String2NumSlot, table->slots_count);
      for EachIndex(code, arch_info->reg_code_count)
      {
        String8 name = arch_info->reg_code_name_table[code];
        d_string2num_table_insert(arena, table, name, code, 0);
      }
    }
    
    // rjf: build evaluation schema tables
    {
      for EachElement(idx, d_entity_schema_info_table)
      {
        D_SchemaInfo *info = &d_entity_schema_info_table[idx];
        MD_Node *schema = md_tree_from_string(arena, info->schema)->first;
        cfg_schema_table_insert(arena, &d_shared->entity_schema_table, info->name, schema);
      }
      for EachElement(idx, d_cfg_schema_info_table)
      {
        D_SchemaInfo *info = &d_cfg_schema_info_table[idx];
        MD_Node *schema = md_tree_from_string(arena, info->schema)->first;
        cfg_schema_table_insert(arena, &d_shared->cfg_schema_table, info->name, schema);
      }
    }
  }
  
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
