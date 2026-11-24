// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

// TODO: DWARF converter regressions & lost functionality after the rewrite:
//
// 1. Compilation-unit ranges misinterpret high_pc; Address-form values are added to voff_base; size-form
//    values are treated as absolute ending offsets. The old converter handled both correctly.
//    PR#1039 fixes procedure/scope ranges, but leaves this path unchanged.
//
// 2. Location disappear when a parent has no frame base. Conversion executes zero iterations when a parent
//    exists with an empty frame-base list -- even for register only expressions. This also prevents namespace
//    contained functions from converting their own frame bases.
//
// 3. DW_FORM_loclistx uses the wrong offset base. Offset-table entries are interpreted relative to the
//    section instead of the table's entries base, causing unrelated bytes to be decoded as locations.
//
// 4. DW_OP_lit0 - DW_OP_lit31 no longer push values in the evaluator. Their metadata specifies push_count=0
//    This also desynchronizes the converter's metadata stack. The old evaluator explicitly pushed literals.
//
// 5. Address arithmetic loses address/value classification. plus_uconst and general arithmetic discard
//    address metadata, so an expression describing memory can become a value expression.
//
// 6. DW_OP_stack_value is ignored. Address-producing expressions remain address locations instead of
//    becoming values, introducing an unwanted dereference. The old converter handled the transition
//    explicitly.
//
// 7. Runtime stack operations are broken. dup copies an unpopulated temp, over never enters its
//    section handler, and sawp/not use incorrect ordering. The old evaluator implemented these correctly.
//
// 8. Fixed-with signed operands are zero-extended. Negative 8/16/32 bit constants become positive;
//    negative branch displacements are also broken.
//
// 9. Arithmetic lowering changed semantics. ne emits Neg instead of NtEq; not emits LogNot instead of BitNot;
//    arithmetic right shift uses an unsigned type group. These were handled correctly in the old convereter.
//
// 10. Brach relocation was left unfinished. Skip and Cond are emitted with displacement zero; both
//     resolution phases are TODOs. Old converter had byte-offset fixup pass that was lost in the rewrite.
//
// 11. Ordinary constant-valued locals lose their values. DW_AT_const_value is collected but never
//     converted into a location/value for ordinary locals. The old converter emitted constant-value bytecode.
//
// 12. DW_FORM_udata references resolve to zero. The resolver omits this previously supported CU-relative
//     reference encoding.
//
// 13. Enums without DW_AT_type become zero-sized. The default non-null void direct type overrides the enum's
//     explicit byte size. The old converter used the enum DIE's size.
//
// 14. Variadic function markers are dropped. Parameter collection ignores DW_TAG_unspecified_parameters; the
//     old converter appended RDI_TypeKind_Variadic.
//
// 15. .debug_aranges padding can skip a valid first tuple. The new calculation adds an entire tuple when already
//     aligned and aligns against the section offset rather than the contribution-relative offset. The old parser
//     conditionally aligned its unit-relative cursor. 
//

////////////////////////////////
//~ rjf: Helpers

internal int
d2r_tree_hash_is_less_than(D2R_TreeHash *a, D2R_TreeHash *b)
{
  int is_less_than = (a->hash < b->hash);
  return is_less_than;
}

internal int
d2r_unique_tag_node_is_less_than(D2R_UniqueTagNode **l, D2R_UniqueTagNode **r)
{
  int is_less_than = (l[0]->hash < r[0]->hash);
  return is_less_than;
}

internal void
d2r_collect_tag_attributes(Arena *arena,
                           D2R_CommonAttribs *attribs,
                           DW_Raw *raw,
                           Rng1U64Array unit_info_tag_ranges_array,
                           DW2_ParseCtx *unit_parse_ctxs,
                           DW2_Tag tag)
{
  Temp scratch = scratch_begin(&arena, 1);
  MemoryZeroArray(attribs->v);

  typedef struct Task {
    struct Task *next;
    DW2_Attrib *attrib;
  } Task;

  HashMap  visited_tags = {0};
  Task     first_task   = {0};
  Task    *tasks        = &first_task;
  while (tasks) {
    Task *t = tasks;
    SLLStackPop(tasks);

    DW2_Tag current_tag = tag;
    if (t->attrib) {
      // find unit where attribute is defined
      U64 unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, t->attrib->off);
      if (unit_num == 0) {
        log_user_errorf("[.debug_info@0x%I64x] abstract origin or specification has no owning unit\n", t->attrib->off);
        continue;
      }

      // resolve attribute reference
      DW2_ParseCtx *unit_parse_ctx = &unit_parse_ctxs[unit_num - 1];
      U64           ref_off        = dw2_reference_info_off_from_form_val(unit_parse_ctx, &t->attrib->val);
      B32           valid_ref      = (ref_off != 0 &&
                                      (t->attrib->val.kind == DW_FormKind_RefAddr ||
                                       (t->attrib->val.u128.u64[0] <= max_U64 - unit_parse_ctx->unit_base_info_off && contains_1u64(unit_info_tag_ranges_array.v[unit_num - 1], ref_off))));
      if ( ! valid_ref) {
        log_user_errorf("[.debug_info@0x%I64x] invalid or unsupported abstract origin or specification reference\n", t->attrib->off);
        continue;
      }

      // resolve reference unit
      U64 ref_unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, ref_off);
      if (ref_unit_num == 0 || ref_off >= raw->sec[DW_SectionKind_Info].data.size) {
        log_user_errorf("[.debug_info@0x%I64x] abstract origin or specification points to invalid debug info offset %I64x\n", t->attrib->off, ref_off);
        continue;
      }

      // skip repeated references
      if (hash_map_search_u64_u64(&visited_tags, ref_off)) {
        continue;
      }
      hash_map_push_u64_u64(scratch.arena, &visited_tags, ref_off, 1);

      // read tag
      current_tag = (DW2_Tag){0};
      U64 ref_tag_size = dw2_read_tag(scratch.arena, raw, &unit_parse_ctxs[ref_unit_num - 1], raw->sec[DW_SectionKind_Info].data, ref_off, &current_tag);
      if (ref_tag_size == 0 || current_tag.kind == DW_TagKind_Null ||
          ref_tag_size > unit_info_tag_ranges_array.v[ref_unit_num - 1].max - ref_off) {
        log_user_errorf("[.debug_info@0x%I64x] failed to read abstract origin or specification tag at %I64x\n", t->attrib->off, ref_off);
        continue;
      }
    }

    // collect current tag attributes
    U64         new_refs_count = 0;
    DW2_Attrib *new_refs[2]    = {0};
    for EachNode(n, DW2_AttribNode, current_tag.attribs.first) {
      if (n->v.attrib_kind == DW_AttribKind_AbstractOrigin || n->v.attrib_kind == DW_AttribKind_Specification) {
        if (new_refs_count < ArrayCount(new_refs)) {
          new_refs[new_refs_count++] = &n->v;
        }
      } else {
        // exceptions: declaration and sibling are non-sharable attributes
        if (t != &first_task && (n->v.attrib_kind == DW_AttribKind_Declaration || n->v.attrib_kind == DW_AttribKind_Sibling)) {
          continue;
        }

        // retain only the attributes consumed by symbol conversion
        U64 slot = D2R_CommonAttrib_COUNT;
        switch (n->v.attrib_kind) {
#define X(id, kind) case DW_AttribKind_##kind: slot = D2R_CommonAttrib_##kind; break;
          D2R_CommonAttrib_XList
#undef X
        }
        if (slot < D2R_CommonAttrib_COUNT && attribs->v[slot] == 0) {
          // original tag lives in the caller's arena; referenced tags do not
          DW2_Attrib *dst = &n->v;
          if (t != &first_task) {
            dst = &attribs->inherited[slot];
            *dst = n->v;
          }
          attribs->v[slot] = dst;
        }
      }
    }

    // append new tag references for the walk
    for EachIndex(ref_idx, new_refs_count) {
      if (new_refs[ref_idx] != 0) {
        Task *new_task = push_array(scratch.arena, Task, 1);
        new_task->attrib = new_refs[ref_idx];
        SLLStackPush(tasks, new_task);
      }
    }
  }

  scratch_end(scratch);
}

internal void
d2r_name_refs_from_tag(DW2_ParseCtx *unit_parse_ctx, Rng1U64 unit_info_tag_range, DW2_Tag *tag, U64 ref_offs[3])
{
  MemoryZero(ref_offs, sizeof(ref_offs[0])*3);
  U64 ref_count = 0;
  for EachNode(n, DW2_AttribNode, tag->attribs.first) {
    DW2_Attrib *attrib  = &n->v;
    U64         ref_idx = max_U64;
    if ((attrib->attrib_kind == DW_AttribKind_AbstractOrigin ||
         attrib->attrib_kind == DW_AttribKind_Specification) && ref_count < 2) {
      ref_idx = ref_count++;
    } else if (tag->kind == DW_TagKind_Namespace && attrib->attrib_kind == DW_AttribKind_Extension) {
      ref_idx = 2;
    }
    if (ref_idx != max_U64) {
      U64 ref_off = dw2_reference_info_off_from_form_val(unit_parse_ctx, &attrib->val);
      B32 valid_ref = (ref_off != 0 &&
                       (attrib->val.kind == DW_FormKind_RefAddr ||
                        (attrib->val.u128.u64[0] <= max_U64 - unit_parse_ctx->unit_base_info_off &&
                         contains_1u64(unit_info_tag_range, ref_off))));
      if (valid_ref) {
        ref_offs[ref_idx] = ref_off;
      }
    }
  }
}

internal D2R_NameNode *
d2r_name_parent(HashTable *name_ht, D2R_NameNode *node)
{
  // lexical contexts need no reference walk
  D2R_NameNode *result = node->parent;
  if (node->ref_offs[0] != 0 || node->ref_offs[1] != 0 || node->ref_offs[2] != 0) {
    Temp scratch = scratch_begin(0, 0);

    typedef struct Task {
      struct Task  *next;
      D2R_NameNode *node;
      B32           use_parent;
    } Task;

    HashMap  visited    = {0};
    Task     first_task = {0, node, 0};
    Task    *tasks      = &first_task;
    while (tasks != 0) {
      Task *t = tasks;
      SLLStackPop(tasks);
      if (t->use_parent) {
        if (t->node->parent != 0) {
          result = t->node->parent;
          break;
        }
        continue;
      }
      if (hash_map_search_u64_u64(&visited, t->node->info_off)) {
        continue;
      }
      hash_map_push_u64_u64(scratch.arena, &visited, t->node->info_off, 1);

      // prefer declaration/origin context over the lexical parent
      Task *fallback = push_array(scratch.arena, Task, 1);
      fallback->node = t->node;
      fallback->use_parent = 1;
      SLLStackPush(tasks, fallback);
      for EachElement(ref_idx, t->node->ref_offs) {
        U64           off = t->node->ref_offs[ref_idx];
        D2R_NameNode *ref = off != 0 ? hash_table_search_u64_raw(name_ht, off) : 0;
        if (ref != 0) {
          Task *next = push_array(scratch.arena, Task, 1);
          next->node = ref;
          SLLStackPush(tasks, next);
        }
      }
    }
    scratch_end(scratch);
  }
  return result;
}

internal B32
d2r_name_nodes_share_identity(HashTable *name_ht, D2R_NameNode *a, D2R_NameNode *b)
{
  if (a == 0 || b == 0) {
    return 0;
  }
  if (a == b) {
    return 1;
  }

  Temp scratch = scratch_begin(0, 0);

  typedef struct Task {
    struct Task  *next;
    D2R_NameNode *node;
  } Task;

  HashMap       visited[2] = {0};
  D2R_NameNode *roots[]    = {a, b};
  B32           result     = 0;
  for EachElement(pass_idx, roots) {
    Task  first = {0, roots[pass_idx]};
    Task *tasks = &first;
    while (tasks != 0) {
      Task *t = tasks;
      SLLStackPop(tasks);

      if (pass_idx == 1 && hash_map_search_u64_u64(&visited[0], t->node->info_off)) {
        result = 1;
        goto done;
      }

      if (hash_map_search_u64_u64(&visited[pass_idx], t->node->info_off)) {
        continue;
      }
      hash_map_push_u64_u64(scratch.arena, &visited[pass_idx], t->node->info_off, 1);

      for EachElement(ref_idx, t->node->ref_offs) {
        U64           off = t->node->ref_offs[ref_idx];
        D2R_NameNode *ref = off != 0 ? hash_table_search_u64_raw(name_ht, off) : 0;
        if (ref != 0) {
          Task *next = push_array(scratch.arena, Task, 1);
          next->node = ref;
          SLLStackPush(tasks, next);
        }
      }
    }
  }
  done:;

  scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ rjf: Main Conversion Entry Point (New)

internal RDIM_BakeParams
d2r_convert(Arena *arena, D2R_ConvertParams *params)
{
  Temp scratch = scratch_begin(&arena, 1);

  DW_Raw                 *raw             = &params->raw;
  RDIM_BinarySectionList  binary_sections = params->binary_sections;
  Arch                    arch            = params->arch;
  ARCH_Info              *arch_info       = arch_info_from_arch(arch);
  U64                     base_vaddr      = params->base_vaddr;
  U64                     arch_addr_size  = byte_size_from_arch(arch);
  
  B32 make_procedures       = !!(params->subset_flags & RDIM_SubsetFlag_Procedures || params->subset_flags & RDIM_SubsetFlag_ProcedureNameMap);
  B32 make_types            = !!(params->subset_flags & RDIM_SubsetFlag_Types || params->subset_flags & RDIM_SubsetFlag_TypeNameMap);
  B32 make_namespaces       = !!(params->subset_flags & RDIM_SubsetFlag_Namespaces);
  B32 make_scopes           = make_procedures && !!(params->subset_flags & RDIM_SubsetFlag_Scopes);
  B32 make_locals           = make_scopes && !!(params->subset_flags & RDIM_SubsetFlag_Locals);
  B32 make_global_vars      = !!(params->subset_flags & RDIM_SubsetFlag_GlobalVariables || params->subset_flags & RDIM_SubsetFlag_GlobalVariableNameMap);
  B32 make_thread_vars      = !!(params->subset_flags & RDIM_SubsetFlag_ThreadVariables || params->subset_flags & RDIM_SubsetFlag_ThreadVariableNameMap);
  B32 make_source_line_maps = !!(params->subset_flags & RDIM_SubsetFlag_SourceLineMaps);
  B32 make_line_tables      = !!(params->subset_flags & RDIM_SubsetFlag_LineInfo) || make_source_line_maps;
  B32 make_inline_sites     = make_procedures && make_scopes && !!(params->subset_flags & RDIM_SubsetFlag_InlineLineInfo);
 
  // detect breakpad mode and skip procedures that don't have a valid address range (functions removed during linker's GC pass, etc.)
  B32 prune_dead_output = (make_procedures && make_scopes && make_namespaces &&
                           !make_types && !make_locals && !make_global_vars && !make_thread_vars &&
                           !(params->subset_flags & (RDIM_SubsetFlag_NameMaps|RDIM_SubsetFlag_Constants)));
  
  ////////////////////////////
  //- rjf: determine acceptable address range
  //
  // in many cases, linkers seem to trample over addresses in various DWARF sections,
  // potentially due to optimizations. we'd like to filter out those busted addresses
  // from our final debug info - a good enough heuristic is to disqualify them by
  // whether or not they actually fall into the ranges covered by the binary sections.
  //
  Rng1U64 acceptable_vaddr_range = {0};
  {
    acceptable_vaddr_range.min = max_U64;
    acceptable_vaddr_range.max = 0;
    for EachNode(n, RDIM_BinarySectionNode, binary_sections.first)
    {
      acceptable_vaddr_range.min = Min(base_vaddr + n->v.voff_first, acceptable_vaddr_range.min);
      acceptable_vaddr_range.max = Max(base_vaddr + n->v.voff_opl, acceptable_vaddr_range.max);
    }
  }
  
  ////////////////////////////
  //- rjf: compute exe hash
  //
  U64 exe_hash = 0;
  ProfScope("compute exe hash")
  if( ! params->skip_exe_hash)
  {
    if(lane_idx() == 0)
    {
      exe_hash = rdi_hash(params->exe_data.str, params->exe_data.size);
    }
    lane_sync_u64(&exe_hash, 0);
  }
  
  ////////////////////////////
  //- rjf: produce top-level-info
  //
  RDIM_TopLevelInfo top_level_info = {0};
  ProfScope("produce top-level-info")
  {
    top_level_info.arch     = rdi_arch_from_arch(arch);
    top_level_info.exe_name = params->exe_name;
    top_level_info.exe_hash = exe_hash;
    top_level_info.voff_max = acceptable_vaddr_range.max - base_vaddr;
    if(!params->deterministic)
    {
      // TODO(rjf): top_level_info.guid = ...;
      top_level_info.producer_name = str8_lit(BUILD_TITLE_STRING_LITERAL);
    }
  }
  
  ////////////////////////////
  //- rjf: gather unit ranges from .debug_info, .debug_aranges
  //
  Rng1U64Array *unit_info_ranges = 0;
  Rng1U64Array *unit_arange_ranges = 0;
  ProfScope("gather unit ranges from .debug_info, .debug_aranges") if(lane_idx() == 0)
  {
    unit_info_ranges = push_array(scratch.arena, Rng1U64Array, 1);
    *unit_info_ranges = dw2_unit_ranges_from_data(scratch.arena, raw->sec[DW_SectionKind_Info].data);
    unit_arange_ranges = push_array(scratch.arena, Rng1U64Array, 1);
    *unit_arange_ranges = dw2_unit_ranges_from_data(scratch.arena, raw->sec[DW_SectionKind_ARanges].data);
  }
  lane_sync_u64(&unit_info_ranges, 0);
  lane_sync_u64(&unit_arange_ranges, 0);
  U64 unit_count = unit_info_ranges->count;
  
  ////////////////////////////
  //- rjf: parse all .debug_info unit headers
  //
  DW2_UnitHeader *unit_headers = 0;
  Rng1U64 *unit_info_tag_ranges = 0;
  ProfScope("parse all unit headers")
  {
    // rjf: set up
    if(lane_idx() == 0)
    {
      unit_headers = push_array(scratch.arena, DW2_UnitHeader, unit_count);
      unit_info_tag_ranges = push_array(scratch.arena, Rng1U64, unit_count);
    }
    lane_sync_u64(&unit_headers, 0);
    lane_sync_u64(&unit_info_tag_ranges, 0);
    
    // rjf: parse all unit headers
    String8 data = raw->sec[DW_SectionKind_Info].data;
    Rng1U64 range = lane_range(unit_count);
    for EachInRange(idx, range)
    {
      Rng1U64 unit_info_range = unit_info_ranges->v[idx];
      U64 bytes_read = dw2_read_unit_header(str8_substr(data, unit_info_range), 0, &unit_headers[idx]);
      unit_info_tag_ranges[idx] = r1u64(unit_info_range.min + bytes_read, unit_info_range.max);
    }
  }
  lane_sync();
  Rng1U64Array unit_info_tag_ranges_array = {unit_info_tag_ranges, unit_count};
  
  ////////////////////////////
  //- rjf: parse all units from .debug_aranges
  //
  U64 *arange_info_offs = 0;
  RDIM_Rng1U64ChunkList *arange_voff_ranges = 0;
  {
    if(lane_idx() == 0)
    {
      arange_info_offs = push_array(scratch.arena, U64, unit_arange_ranges->count);
      arange_voff_ranges = push_array(scratch.arena, RDIM_Rng1U64ChunkList, unit_arange_ranges->count);
    }
    lane_sync_u64(&arange_info_offs, 0);
    lane_sync_u64(&arange_voff_ranges, 0);
    U64 unit_take_idx_ = 0;
    U64 *unit_take_idx_ptr = &unit_take_idx_;
    lane_sync_u64(&unit_take_idx_ptr, 0);
    String8 data = raw->sec[DW_SectionKind_ARanges].data;
    for(;;)
    {
      // rjf: take unit
      U64 unit_idx = ins_atomic_u64_inc_eval(unit_take_idx_ptr)-1;
      if(unit_idx >= unit_arange_ranges->count)
      {
        break;
      }
      
      // rjf: unpack
      Rng1U64 arange_range = unit_arange_ranges->v[unit_idx];
      U64 off = arange_range.min;
      
      // rjf: read unit data size / format
      U64 unit_size = 0;
      DW_Format fmt = 0;
      off += dw2_read_initial_length(data, off, &unit_size, &fmt);
      U64 unit_opl = off + unit_size;
      
      // rjf: read version
      DW_Version version = 0;
      U64 version_off = off;
      off += str8_deserial_read_struct(data, off, &version);
      
      // rjf: warn on non-version 2
      if(version != DW_Version_2)
      {
        log_infof("[.debug_aranges@0x%I64x] DWARF version for unit #%I64d was expected to be 2, but it was read as %i.\n", version_off, unit_idx, (S32)version);
      }
      
      // rjf: read .debug_info off for this unit
      U64 info_off = 0;
      off += dw2_read_fmt_u64(data, off, fmt, &info_off);
      
      // rjf: read address / segment selector size
      U8 addr_size = 0;
      U8 segment_selector_size = 0;
      off += str8_deserial_read_struct(data, off, &addr_size);
      off += str8_deserial_read_struct(data, off, &segment_selector_size);
      
      // rjf: round up past padding
      {
        U64 tuple_size = addr_size*2 + segment_selector_size;
        off += tuple_size - (off%tuple_size);
      }
      
      // rjf: parse ranges
      RDIM_Rng1U64ChunkList voff_ranges = {0};
      if(segment_selector_size != 0)
      {
        log_infof("[.debug_aranges@0x%I64x] Non-zero (%i) segment selector size parsed; this form of addressing is not currently supported in DWARF info.\n", off, (S32)segment_selector_size);
      }
      else for(;off < unit_opl;)
      {
        U64 start_off = off;
        U64 base_addr = 0;
        U64 range_size = 0;
        off += str8_deserial_read(data, off, &base_addr, addr_size, addr_size);
        off += str8_deserial_read(data, off, &range_size, addr_size, addr_size);
        if(base_addr == 0 && range_size == 0)
        {
          break;
        }
        if(base_addr < base_vaddr)
        {
          log_infof("[.debug_aranges@0x%I64x] Address (0x%I64x) parsed which was less than the image base address (0x%I64x). Skipping.\n", start_off, base_addr, base_vaddr);
        }
        else
        {
          U64 voff_first = (base_addr - base_vaddr);
          U64 voff_opl = voff_first + range_size;
          RDIM_Rng1U64 range = {voff_first, voff_opl};
          rdim_rng1u64_chunk_list_push(arena, &voff_ranges, 256, range);
        }
        if(off == start_off)
        {
          break;
        }
      }
      
      // rjf: store
      arange_info_offs[unit_idx] = info_off;
      arange_voff_ranges[unit_idx] = voff_ranges;
    }
    lane_sync();
  }
  
  ////////////////////////////
  //- rjf: produce info_off -> list(voff_range) map from aranges units;
  // we must do this because technically we can't guarantee that unit_idxs
  // inside of .debug_aranges are the same as unit_idxs inside of .debug_info,
  // nor can we guarantee that they'd be in the same order, so we need to
  // correllate via the encoded .debug_info offset from .debug_aranges.
  //
  // more excellence.
  //
  typedef struct D2R_ARangeUnitNode D2R_ARangeUnitNode;
  struct D2R_ARangeUnitNode
  {
    D2R_ARangeUnitNode *next;
    U64 info_off;
    RDIM_Rng1U64ChunkList *ranges;
  };
  U64 arange_unit_from_info_off_map_slots_count = unit_arange_ranges->count;
  D2R_ARangeUnitNode **arange_unit_from_info_off_map_slots = 0;
  {
    if(lane_idx() == 0)
    {
      arange_unit_from_info_off_map_slots = push_array(scratch.arena, D2R_ARangeUnitNode *, arange_unit_from_info_off_map_slots_count);
    }
    lane_sync_u64(&arange_unit_from_info_off_map_slots, 0);
    for EachIndex(arange_unit_idx, unit_arange_ranges->count)
    {
      U64 info_off = arange_info_offs[arange_unit_idx];
      RDIM_Rng1U64ChunkList *ranges = &arange_voff_ranges[arange_unit_idx];
      U64 hash = u64_hash_from_str8(str8_struct(&info_off));
      U64 slot_idx = hash%arange_unit_from_info_off_map_slots_count;
      D2R_ARangeUnitNode *n = push_array(scratch.arena, D2R_ARangeUnitNode, 1);
      SLLStackPush(arange_unit_from_info_off_map_slots[slot_idx], n);
      n->info_off = info_off;
      n->ranges = ranges;
    }
  }
  lane_sync();
  
  ////////////////////////////
  //- rjf: build all abbreviation maps, build (unit -> abbrev map)
  //
  DW2_UnitAbbrevMapMap unit_abbrev_map_map = dw2_unit_abbrev_map_map_from_data(scratch.arena, raw->sec[DW_SectionKind_Abbrev].data, unit_headers, unit_count);
  
  ////////////////////////////
  //- rjf: parse all unit offsets tables
  //
  // on .debug_str_offsets, as one example:
  //
  // in an incredible twist of fate, DWARF decided to decouple these from
  // compilation units. compilation units *do* contain a
  // DW_AttribKind_StrOffsetsBase attribute. but this base offset does *not*
  // point to the beginning of a table in .debug_str_offsets! it instead points
  // PAST THE INITIAL VARIABLE-WIDTH LENGTH AND FORMAT ENCODING! this means
  // you can't actually use the StrOffsetsBase attribute for ANYTHING other
  // than correllating a unit to its associated string offset table - but you
  // *necessarily needed to have parsed that table beforehand*, completely
  // independently from units.
  //
  // so, we have to parse all the string offset tables up-front, then
  // *binary search* their ranges to determine which unit has which table.
  //
  // of course, in practice, it's perhaps likely/expected that these match
  // one-to-one with units, and in the same order, because that is what is
  // most natural for generators. but, the format does not *guarantee this*,
  // and instead specced something far more arbitrary.
  //
  // thank you, again, DWARF.
  //
  DW2_OffsetTableSet *offset_tables = 0;
  ProfScope("parse all offset tables (.debug_rnglists, .debug_str_offsets, .debug_addr, .debug_loclists)") if(lane_idx() == 0)
  {
    offset_tables = push_array(scratch.arena, DW2_OffsetTableSet, 1);
    offset_tables[0] = dw2_offset_table_set_from_raw(scratch.arena, raw);
  }
  lane_sync_u64(&offset_tables, 0);
  
  ////////////////////////////
  //- rjf: build per-unit parsing contexts, parse compilation unit tags
  //
  DW2_ParseCtx *unit_parse_ctxs = 0;
  DW2_Tag *unit_root_tags = 0;
  {
    if(lane_idx() == 0)
    {
      unit_parse_ctxs = push_array(scratch.arena, DW2_ParseCtx, unit_count);
      unit_root_tags = push_array(scratch.arena, DW2_Tag, unit_count);
    }
    lane_sync_u64(&unit_parse_ctxs, 0);
    lane_sync_u64(&unit_root_tags, 0);
    Rng1U64 range = lane_range(unit_count);
    for EachInRange(unit_idx, range)
    {
      DW2_UnitHeader *hdr = &unit_headers[unit_idx];
      DW2_ParseCtx *ctx = &unit_parse_ctxs[unit_idx];
      
      // rjf: equip initial info - just enough to do a bootstrapping parse of the root tag
      dw2_parse_ctx_equip_unit_header(ctx, hdr, unit_info_ranges->v[unit_idx].min);
      dw2_parse_ctx_equip_unit_abbrev_map(ctx, &unit_abbrev_map_map, unit_idx);
      
      // rjf: do bootstrapping parse
      //
      // an incredible NOTE: the above context info is actually not sufficient for general
      // parsing, because this "bootstrapping" tag parse is what informs us which
      // .debug_str_offsets table each unit should be associated with (via the
      // StrOffsetsBase attribute), we actually don't have the right string offset table
      // *before* we parse this, which means we can't resolve some of the string attribute
      // values.
      //
      // so, in another incredible twist of fate, we actually must do this *twice*. first,
      // to find *just the StrOffsetsBase*, and then, to actually fully resolve everything.
      //
      // (all of the above is also true for RngListsBase)
      //
      DW2_Tag root_tag_bootstrap = {0};
      dw2_read_tag(scratch.arena, raw, ctx, raw->sec[DW_SectionKind_Info].data, unit_info_tag_ranges[unit_idx].min, &root_tag_bootstrap);
      
      // rjf: equip info extracted from bootstrapping parse
      dw2_parse_ctx_equip_unit_root_tag(ctx, &root_tag_bootstrap, offset_tables);
      
      // rjf: do non-bootstrapping parse
      dw2_read_tag(scratch.arena, raw, ctx, raw->sec[DW_SectionKind_Info].data, unit_info_tag_ranges[unit_idx].min, &unit_root_tags[unit_idx]);
      
      // rjf: equip info extracted from non-bootstrapping parse
      dw2_parse_ctx_equip_unit_root_tag(ctx, &unit_root_tags[unit_idx], offset_tables);
    }
  }
  lane_sync();
  
  ////////////////////////////
  //- rjf: parse each unit's line table header
  //
  DW2_LineTableHeader *unit_line_table_headers = 0;
  ProfScope("parse each unit's line table header")
  {
    if(lane_idx() == 0)
    {
      unit_line_table_headers = push_array(scratch.arena, DW2_LineTableHeader, unit_count);
    }
    lane_sync_u64(&unit_line_table_headers, 0);
    U64 unit_take_idx = 0;
    U64 *unit_take_idx_ptr = &unit_take_idx;
    lane_sync_u64(&unit_take_idx_ptr, 0);
    for(;;)
    {
      U64 unit_idx = ins_atomic_u64_inc_eval(unit_take_idx_ptr) - 1;
      if(unit_idx >= unit_count)
      {
        break;
      }
      DW2_ParseCtx *ctx = &unit_parse_ctxs[unit_idx];
      DW2_Tag *unit_root_tag = &unit_root_tags[unit_idx];
      DW2_Attrib *stmt_list = dw2_attrib_from_kind(unit_root_tag, DW_AttribKind_StmtList);
      U64 line_info_off = stmt_list->val.u128.u64[0];
      String8 line_info_data = raw->sec[DW_SectionKind_Line].data;
      dw2_read_line_table_header(scratch.arena, raw, ctx, line_info_data, line_info_off, &unit_line_table_headers[unit_idx]);
    }
    lane_sync();
  }
  
  RDIM_SrcFileChunkList    *all_src_files    = 0;
  RDIM_LineTableChunkList  *all_line_tables  = 0;
  RDIM_LineTable          **unit_line_tables = 0;
  if(make_line_tables)
  {
    ////////////////////////////
    //- rjf: deduplicate all source files
    //
    typedef struct UnitSrcFileMap UnitSrcFileMap;
    struct UnitSrcFileMap
    {
      RDIM_SrcFile **v;
    };
    UnitSrcFileMap *unit_src_file_maps = 0;
    ProfScope("deduplicate all source files") if(lane_idx() == 0)
    {
      Temp scratch2 = scratch_begin(&scratch.arena, 1);

      //- rjf: count all files in all units
      U64 total_file_count = 0;
      for EachIndex(unit_idx, unit_count)
      {
        total_file_count += unit_line_table_headers[unit_idx].files.count;
      }

      //- rjf: set up path -> src file map
      typedef struct SrcFileNode SrcFileNode;
      struct SrcFileNode
      {
        SrcFileNode *next;
        String8 full_path;
        RDIM_SrcFile *src_file;
      };
      all_src_files = push_array(scratch.arena, RDIM_SrcFileChunkList, 1);
      U64 slots_count = 1 + (total_file_count * 3) / 4;
      SrcFileNode **slots = push_array(scratch2.arena, SrcFileNode *, slots_count);

      //- rjf: set up unit -> src file maps
      unit_src_file_maps = push_array(scratch.arena, UnitSrcFileMap, unit_count);
      for EachIndex(unit_idx, unit_count)
      {
        unit_src_file_maps[unit_idx].v = push_array(scratch.arena, RDIM_SrcFile *, unit_line_table_headers[unit_idx].files.count);
      }

      //- rjf: build path -> src file map
      for EachIndex(unit_idx, unit_count)
      {
        DW2_ParseCtx *ctx = &unit_parse_ctxs[unit_idx];
        DW2_LineTableHeader *hdr = &unit_line_table_headers[unit_idx];
        for EachIndex(file_idx, hdr->files.count)
        {
          DW2_LineTableFile *f = &hdr->files.v[file_idx];
          if(f->file_name.size != 0)
          {
            DW2_LineTableFile *dir = &hdr->dirs.v[f->dir_idx];
            String8 root_dir = {0};
            if(path_style_from_str8(dir->file_name) == PathStyle_Relative)
            {
              root_dir = ctx->unit_dir;
            }
            String8 full_file_path = str8f(scratch2.arena, "%S%s%S%s%S",
                                           root_dir, root_dir.size != 0 ? "/" : "",
                                           dir->file_name, dir->file_name.size != 0 ? "/" : "",
                                           f->file_name);
            full_file_path = path_normalized_from_string(scratch2.arena, full_file_path);
            U64 hash = u64_hash_from_str8(full_file_path);
            U64 slot_idx = hash%slots_count;
            SrcFileNode *node = 0;
            for(SrcFileNode *n = slots[slot_idx]; n != 0; n = n->next)
            {
              if(str8_match(n->full_path, full_file_path, 0))
              {
                node = n;
                break;
              }
            }
            if(!node)
            {
              node = push_array(scratch2.arena, SrcFileNode, 1);
              SLLStackPush(slots[slot_idx], node);
              node->full_path = full_file_path;
              node->src_file = rdim_src_file_chunk_list_push(arena, all_src_files, slots_count);
              node->src_file->path = str8_copy(arena, full_file_path);
              if(f->flags & DW2_LineTableFileFlag_HasMD5)
              {
                node->src_file->checksum_kind = RDI_ChecksumKind_MD5;
                node->src_file->checksum = str8_copy(arena, str8_struct(&f->md5));
              }
              else if(f->flags & DW2_LineTableFileFlag_HasModifyTime)
              {
                node->src_file->checksum_kind = RDI_ChecksumKind_Timestamp;
                node->src_file->checksum = str8_copy(arena, str8_struct(&f->modify_time));
              }
            }
            unit_src_file_maps[unit_idx].v[file_idx] = node->src_file;
          }
        }
      }
      scratch_end(scratch2);
    }
    lane_sync_u64(&all_src_files, 0);
    lane_sync_u64(&unit_src_file_maps, 0);

    ////////////////////////////
    //- rjf: parse each unit's line info; produce line tables
    //
    RDIM_LineTableChunkList *unit_line_table_chunk_lists = 0;
    ProfScope("parse each unit's line info; produce line tables")
    {
      //- rjf: prep outputs
      typedef struct LineSeqChunk LineSeqChunk;
      struct LineSeqChunk
      {
        LineSeqChunk *next;
        U64 *voffs;
        U32 *line_nums;
        U16 *col_nums;
        U64 line_count;
        U64 line_cap;
      };
      typedef struct FileSeqNode FileSeqNode;
      struct FileSeqNode
      {
        FileSeqNode *next;
        RDIM_SrcFile *src_file;
        RDIM_SrcFileLineMapFragment *first_line_map_fragment;
        RDIM_SrcFileLineMapFragment *last_line_map_fragment;
      };
      typedef struct FileSeqMap FileSeqMap;
      struct FileSeqMap
      {
        U64 slots_count;
        FileSeqNode **slots;
      };
      FileSeqMap *unit_file_seq_maps = 0;
      if(lane_idx() == 0)
      {
        unit_line_table_chunk_lists = push_array(scratch.arena, RDIM_LineTableChunkList, unit_count);
        unit_line_tables = push_array(scratch.arena, RDIM_LineTable *, unit_count);
        if(make_source_line_maps)
        {
          unit_file_seq_maps = push_array(scratch.arena, FileSeqMap, unit_count);
        }
      }
      lane_sync_u64(&unit_line_table_chunk_lists, 0);
      lane_sync_u64(&unit_line_tables, 0);
      lane_sync_u64(&unit_file_seq_maps, 0);

      //- rjf: wide per-unit parse
      U64 unit_take_idx = 0;
      U64 *unit_take_idx_ptr = &unit_take_idx;
      lane_sync_u64(&unit_take_idx_ptr, 0);
      for(;;)
      {
        //- rjf: take unit
        U64 unit_idx = ins_atomic_u64_inc_eval(unit_take_idx_ptr) - 1;
        if(unit_idx >= unit_count)
        {
          break;
        }
        Temp scratch2 = scratch_begin(&scratch.arena, 1);

        //- rjf: unpack unit info
        DW2_LineTableHeader *line_table_header = &unit_line_table_headers[unit_idx];
        RDIM_LineTableChunkList *dst_line_tables = &unit_line_table_chunk_lists[unit_idx];
        DW2_Tag *unit_root_tag = &unit_root_tags[unit_idx];
        DW2_Attrib *stmt_list = dw2_attrib_from_kind(unit_root_tag, DW_AttribKind_StmtList);
        U64 line_info_off = stmt_list->val.u128.u64[0];
        String8 all_line_info_data = raw->sec[DW_SectionKind_Line].data;
        String8 unit_line_table_data = str8_substr(all_line_info_data, r1u64(line_table_header->line_program_off, line_info_off + line_table_header->total_unit_data_size));

        //- rjf: build unit's line table
        RDIM_LineTable *dst_line_table = rdim_line_table_chunk_list_push(arena, dst_line_tables, 1);
        unit_line_tables[unit_idx] = dst_line_table;

        //- rjf: set up per-unit file sequence map
        if(make_source_line_maps)
        {
          unit_file_seq_maps[unit_idx].slots_count = line_table_header->files.count + 1;
          unit_file_seq_maps[unit_idx].slots = push_array(scratch.arena, FileSeqNode *, unit_file_seq_maps[unit_idx].slots_count);
        }

        //- rjf: set up vm registers
        DW2_LineVM vm = dw2_line_vm_init(line_table_header);

        //- rjf: run the line opcode program
        RDIM_SrcFile *line_seq_src_file = 0;
        LineSeqChunk *first_line_seq_chunk = 0;
        LineSeqChunk *last_line_seq_chunk = 0;
        U64 total_line_seq_count = 0;
        for(;;)
        {
          B32 stepped = dw2_line_vm_step(line_table_header, unit_line_table_data, &vm);
          DW2_LineVMRegs vm_regs = vm.regs;

          //- rjf: map file index -> rdim src file
          RDIM_SrcFile *src_file = 0;
          if(vm_regs.file_index < line_table_header->files.count)
          {
            src_file = unit_src_file_maps[unit_idx].v[vm_regs.file_index];
          }

          // line sequence terminated -> emit
          B32 emitted_file_change = vm.emit_row && src_file != line_seq_src_file && first_line_seq_chunk != 0;
          if(line_seq_src_file != 0 && (vm_regs.end_sequence || emitted_file_change || !stepped))
          {
            // rjf: combine voffs/lines/cols
            U64 seq_line_count = total_line_seq_count;
            U64 *seq_voffs = push_array(arena, U64, seq_line_count+1);
            U32 *seq_lines = push_array(arena, U32, seq_line_count);
            U16 *seq_cols = push_array(arena, U16, 2*seq_line_count);
            {
              U64 voff_idx = 0;
              U64 line_idx = 0;
              U64 col_idx = 0;
              for(LineSeqChunk *c = first_line_seq_chunk; c != 0; c = c->next)
              {
                MemoryCopy(seq_voffs + voff_idx, c->voffs, sizeof(c->voffs[0]) * c->line_count);
                MemoryCopy(seq_lines + line_idx, c->line_nums, sizeof(c->line_nums[0]) * c->line_count);
                MemoryCopy(seq_cols + col_idx, c->col_nums, sizeof(c->col_nums[0]) * 2 * c->line_count);
                voff_idx += c->line_count;
                line_idx += c->line_count;
                col_idx += 2*c->line_count;
              }
              seq_voffs[seq_line_count] = vm_regs.address - base_vaddr;
            }

            // rjf: push sequence to line table
            RDIM_LineSequence *seq = rdim_line_table_push_sequence(arena, dst_line_tables, dst_line_table, line_seq_src_file, seq_voffs, seq_lines, seq_cols, seq_line_count);

            if(make_source_line_maps)
            {
              // rjf: map src file -> file seq node
              FileSeqNode *file_seq_n = 0;
              {
                U64 hash = u64_hash_from_str8(str8_struct(&line_seq_src_file));
                U64 slot_idx = hash%unit_file_seq_maps[unit_idx].slots_count;
                for(FileSeqNode *n = unit_file_seq_maps[unit_idx].slots[slot_idx]; n != 0; n = n->next)
                {
                  if(n->src_file == line_seq_src_file)
                  {
                    file_seq_n = n;
                    break;
                  }
                }
                if(file_seq_n == 0)
                {
                  file_seq_n = push_array(scratch.arena, FileSeqNode, 1);
                  SLLStackPush(unit_file_seq_maps[unit_idx].slots[slot_idx], file_seq_n);
                  file_seq_n->src_file = line_seq_src_file;
                }
              }

              // rjf: record sequence in file seq node
              {
                RDIM_SrcFileLineMapFragment *f = push_array(scratch.arena, RDIM_SrcFileLineMapFragment, 1);
                SLLQueuePush(file_seq_n->first_line_map_fragment, file_seq_n->last_line_map_fragment, f);
                f->seq = seq;
              }
            }

            // rjf: reset
            first_line_seq_chunk = last_line_seq_chunk = 0;
            total_line_seq_count = 0;
            line_seq_src_file = 0;
          }
          if(!stepped)
          {
            break;
          }

          //- rjf: emit lines
          if(vm.emit_row && !vm_regs.end_sequence && vm_regs.address != 0)
          {
            // rjf: grab the last emitted line info
            U32 last_line = 0;
            U16 last_col = 0;
            B32 has_last_line = last_line_seq_chunk != 0 && last_line_seq_chunk->line_count > 0;
            if(has_last_line)
            {
              last_line = last_line_seq_chunk->line_nums[last_line_seq_chunk->line_count-1];
              last_col = last_line_seq_chunk->col_nums[last_line_seq_chunk->line_count-1];
            }

            // rjf: determine if we need a new line
            //
            // TODO(rjf): in some cases this can trigger w/ DWARF due to columnar changes;
            // we need to adjust this when we want to start correctly supporting columnar
            // line info / stepping
            //
            B32 need_new_line = !has_last_line || last_line != (U32)vm_regs.line;

            // rjf: need new line -> grab chunk
            LineSeqChunk *chunk = last_line_seq_chunk;
            if(need_new_line && (chunk == 0 || chunk->line_count >= chunk->line_cap))
            {
              chunk = push_array(scratch2.arena, LineSeqChunk, 1);
              SLLQueuePush(first_line_seq_chunk, last_line_seq_chunk, chunk);
              chunk->line_cap  = 64;
              chunk->voffs     = push_array(scratch2.arena, U64, chunk->line_cap + 1);
              chunk->line_nums = push_array(scratch2.arena, U32, chunk->line_cap);
              chunk->col_nums  = push_array(scratch2.arena, U16, 2*chunk->line_cap);
            }

            // rjf: need new line -> push
            if(need_new_line)
            {
              U64 chunk_line_idx = chunk->line_count;
              chunk->voffs[chunk_line_idx] = vm_regs.address - base_vaddr;
              chunk->line_nums[chunk_line_idx] = (U32)vm_regs.line;
              chunk->col_nums[chunk_line_idx] = (U16)vm_regs.column;
              chunk->line_count += 1;
              total_line_seq_count += 1;
              line_seq_src_file = src_file;
              // NOTE(rjf): use for comparing against llvm-dwarfdump --debug-line
#if 0
              printf("0x%016I64x %6i %6i %6i %3i %13I64x %7i %s%s%s%s\n",
                     vm_regs.address,
                     (int)vm_regs.line,
                     (int)vm_regs.column,
                     (int)vm_regs.file_index,
                     (int)vm_regs.isa,
                     vm_regs.discriminator,
                     (int)vm_regs.vliw_op_index,
                     vm_regs.is_stmt ? " is_stmt" : "",
                     vm_regs.prologue_end ? " prologue_end" : "",
                     vm_regs.epilogue_begin ? " epilogue_begin" : "",
                     vm_regs.end_sequence ? " end_sequence" : "");
#endif
            }
          }
        }

        scratch_end(scratch2);
      }
      lane_sync();

      //- rjf: equip source files with their fragments
      //
      // TODO(rjf): this can *almost* be wide, but we are relying on a few top-level
      // summations inside the RDIM_SrcFileChunkList when we push a sequence to a src
      // file. if we just summed those later when baking (probably fine), then this
      // could go wide across all src files, which would be very nice. just lane-0ing
      // for now. when possible, we can probably do the same thing in PDB too.
      //
      if(make_source_line_maps && lane_idx() == 0)
      {
        for EachIndex(unit_idx, unit_count)
        {
          FileSeqMap *file_seq_map = &unit_file_seq_maps[unit_idx];
          for EachIndex(slot_idx, file_seq_map->slots_count)
          {
            for(FileSeqNode *n = file_seq_map->slots[slot_idx]; n != 0; n = n->next)
            {
              for(RDIM_SrcFileLineMapFragment *f = n->first_line_map_fragment; f != 0; f = f->next)
              {
                rdim_src_file_push_line_sequence(arena, all_src_files, n->src_file, f->seq);
              }
            }
          }
        }
      }
      lane_sync();
    }

    ////////////////////////////
    //- rjf: join all line tables
    //
    if(lane_idx() == 0)
    {
      all_line_tables = push_array(scratch.arena, RDIM_LineTableChunkList, 1);
      for EachIndex(unit_idx, unit_count)
      {
        rdim_line_table_chunk_list_concat_in_place(all_line_tables, &unit_line_table_chunk_lists[unit_idx]);
      }
    }
    lane_sync_u64(&all_line_tables, 0);
  }
  
  ////////////////////////////
  //- rjf: find all offsets of top-level tag trees across all units
  //
  HashTable *name_ht                     = 0;
  U64Array  *unit_name_ref_offs          = 0;
  Arena     *name_refs_arena             = (make_namespaces && !make_types) ? arena_alloc() : 0;
  U64Array  *unit_info_root_tag_offs     = 0;
  U64       *unit_tag_counts__first_pass = 0; // includes all tags when collecting name references; otherwise skips sibling-linked descendants
  U64        total_root_tag_count        = 0;
  U64       *total_root_tag_count_ptr    = &total_root_tag_count;
  lane_sync_u64(&total_root_tag_count_ptr, 0);
  ProfScope("find all offsets of top-level tag trees across all units")
  {
    if(lane_idx() == 0)
    {
      unit_info_root_tag_offs = push_array(scratch.arena, U64Array, unit_count);
      unit_tag_counts__first_pass = push_array(scratch.arena, U64, unit_count);
      if (name_refs_arena != 0) {
        unit_name_ref_offs = push_array(scratch.arena, U64Array, unit_count);
      }
    }
    lane_sync_u64(&unit_info_root_tag_offs, 0);
    lane_sync_u64(&unit_tag_counts__first_pass, 0);
    lane_sync_u64(&unit_name_ref_offs, 0);
    typedef struct D2R_UnitTagTreeOffChunkNode D2R_UnitTagTreeOffChunkNode;
    struct D2R_UnitTagTreeOffChunkNode
    {
      D2R_UnitTagTreeOffChunkNode *next;
      U64 *v;
      U64 count;
      U64 cap;
    };
    typedef struct D2R_UnitTagTreeOffChunkList D2R_UnitTagTreeOffChunkList;
    struct D2R_UnitTagTreeOffChunkList
    {
      D2R_UnitTagTreeOffChunkNode *first;
      D2R_UnitTagTreeOffChunkNode *last;
      U64 chunk_count;
      U64 total_count;
    };
    U64 unit_take_idx_ = 0;
    U64 *unit_take_idx_ptr = &unit_take_idx_;
    lane_sync_u64(&unit_take_idx_ptr, 0);
    for(;;)
    {
      //- rjf: take the next unit
      U64 unit_idx = ins_atomic_u64_inc_eval(unit_take_idx_ptr)-1;
      if(unit_idx >= unit_count)
      {
        break;
      }
      Temp scratch2 = scratch_begin(&scratch.arena, 1);
      
      //- rjf: unpack this unit
      Rng1U64 unit_info_tag_range = unit_info_tag_ranges[unit_idx];
      DW2_ParseCtx *unit_parse_ctx = &unit_parse_ctxs[unit_idx];
      
      //- rjf: gather all offsets of root-level trees
      D2R_UnitTagTreeOffChunkList tree_offs = {0};
      D2R_UnitTagTreeOffChunkList name_ref_offs = {0};
      S64 depth = 0;
      for(U64 off = unit_info_tag_range.min; contains_1u64(unit_info_tag_range, off);)
      {
        U64 start_off = off;
        Temp scratch3 = scratch_begin(&scratch2.arena, 1);
        
        //- rjf: if this next tag is root-level, gather
        if(depth == 1)
        {
          D2R_UnitTagTreeOffChunkNode *chunk = tree_offs.last;
          if(chunk == 0 || chunk->count >= chunk->cap)
          {
            chunk = push_array(scratch2.arena, D2R_UnitTagTreeOffChunkNode, 1);
            chunk->cap = 512;
            chunk->v = push_array(scratch2.arena, U64, chunk->cap);
            SLLQueuePush(tree_offs.first, tree_offs.last, chunk);
            tree_offs.chunk_count += 1;
          }
          chunk->v[chunk->count] = off;
          chunk->count += 1;
          tree_offs.total_count += 1;
        }
        
        //- rjf: reading the next tag
        DW2_Tag tag = {0};
        off += dw2_read_tag(scratch3.arena, raw, unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, off, &tag);
        unit_tag_counts__first_pass[unit_idx] += 1;

        // collect only references used by naming records
        if (name_refs_arena != 0 &&
            (dw_tag_is_name_container(tag.kind) ||
             ((make_global_vars || make_thread_vars) && (tag.kind == DW_TagKind_Member || tag.kind == DW_TagKind_Variable)) ||
             (make_locals && (tag.kind == DW_TagKind_Variable || tag.kind == DW_TagKind_FormalParameter)))) {
          U64 ref_offs[3];
          d2r_name_refs_from_tag(unit_parse_ctx, unit_info_tag_range, &tag, ref_offs);
          for EachElement(ref_idx, ref_offs) {
            if (ref_offs[ref_idx] != 0) {
              D2R_UnitTagTreeOffChunkNode *chunk = name_ref_offs.last;
              if (chunk == 0 || chunk->count >= chunk->cap) {
                chunk = push_array(scratch2.arena, D2R_UnitTagTreeOffChunkNode, 1);
                chunk->cap = 512;
                chunk->v = push_array(scratch2.arena, U64, chunk->cap);
                SLLQueuePush(name_ref_offs.first, name_ref_offs.last, chunk);
                name_ref_offs.chunk_count += 1;
              }
              chunk->v[chunk->count] = ref_offs[ref_idx];
              chunk->count += 1;
              name_ref_offs.total_count += 1;
            }
          }
        }
        
        //- rjf: look for sibling attribute fast path
        DW2_Attrib *sibling_attrib = dw2_attrib_from_kind(&tag, DW_AttribKind_Sibling);
        B32 sibling_encoded = (sibling_attrib != &dw2_attrib_nil);
        U64 sibling_off = dw2_reference_info_off_from_form_val(unit_parse_ctx, &sibling_attrib->val);
        
        //- rjf: if we have a sibling link, follow it -> depth stays the same
        if(name_refs_arena == 0 && sibling_encoded && sibling_off >= off)
        {
          off = sibling_off;
        }
        
        //- rjf: otherwise, do tree navigations...
        else
        {
          if(tag.has_children)
          {
            depth += 1;
          }
          if(tag.kind == DW_TagKind_Null)
          {
            depth -= 1;
          }
        }
        
        scratch_end(scratch3);
        if(off == start_off)
        {
          break;
        }
      }
      
      //- rjf: flatten & store
      {
        U64Array tree_offs_array = {0};
        tree_offs_array.count = tree_offs.total_count;
        tree_offs_array.v = push_array(scratch.arena, U64, tree_offs_array.count);
        {
          U64 idx = 0;
          for EachNode(n, D2R_UnitTagTreeOffChunkNode, tree_offs.first)
          {
            MemoryCopy(tree_offs_array.v + idx, n->v, sizeof(n->v[0])*n->count);
            idx += n->count;
          }
        }
        unit_info_root_tag_offs[unit_idx] = tree_offs_array;
        ins_atomic_u64_add_eval(total_root_tag_count_ptr, tree_offs_array.count);
      }

      if (name_refs_arena != 0) {
        U64Array ref_offs_array = {0};
        ref_offs_array.count = name_ref_offs.total_count;
        ref_offs_array.v = push_array(name_refs_arena, U64, ref_offs_array.count);
        U64 idx = 0;
        for EachNode(n, D2R_UnitTagTreeOffChunkNode, name_ref_offs.first) {
          MemoryCopy(ref_offs_array.v + idx, n->v, sizeof(n->v[0])*n->count);
          idx += n->count;
        }
        unit_name_ref_offs[unit_idx] = ref_offs_array;
      }
      
      scratch_end(scratch2);
    }
    lane_sync();
    total_root_tag_count = *total_root_tag_count_ptr;
    if (name_refs_arena != 0) ProfScope("build naming reference index") {
      if (lane_idx() == 0) {
        U64 total_ref_count = 0;
        for EachIndex(unit_idx, unit_count) {
          total_ref_count += unit_name_ref_offs[unit_idx].count;
        }
        name_ht = hash_table_init(scratch.arena, total_ref_count/2 + 1);
        for EachIndex(unit_idx, unit_count) {
          U64Array ref_offs = unit_name_ref_offs[unit_idx];
          for EachIndex(ref_idx, ref_offs.count) {
            U64 ref_off = ref_offs.v[ref_idx];
            if (hash_table_search_u64(name_ht, ref_off) == 0) {
              hash_table_push_u64_raw(scratch.arena, name_ht, ref_off, 0);
            }
          }
        }
      }
      lane_sync_u64(&name_ht, 0);
      arena_release(name_refs_arena);
      name_refs_arena = 0;
      unit_name_ref_offs = 0;
    }
  }
  
  ////////////////////////////
  //- rjf: produce list of (unit * range(root_tag_idx)), so that we can easily
  // subdivide work from all units across all lanes
  //
  typedef struct D2R_SubUnitWork D2R_SubUnitWork;
  struct D2R_SubUnitWork
  {
    U64 unit_idx;
    Rng1U64 root_tag_idx_range;
  };
  U64 sub_unit_works_count = 0;
  D2R_SubUnitWork *sub_unit_works = 0;
  ProfScope("produce sub-unit work division") if(lane_idx() == 0)
  {
    for(B32 build = 0; build <= 1; build += 1)
    {
      U64 sub_unit_work_idx = 0;
      for EachIndex(unit_idx, unit_count)
      {
        U64 root_tags_in_this_unit = unit_info_root_tag_offs[unit_idx].count;
        U64 root_tags_per_work = CeilIntegerDiv(root_tags_in_this_unit, lane_count());
        U64 works_per_this_unit = root_tags_per_work ? CeilIntegerDiv(root_tags_in_this_unit, root_tags_per_work) : 0;
        if(build)
        {
          D2R_SubUnitWork *works = sub_unit_works + sub_unit_work_idx;
          U64 works_count = works_per_this_unit;
          U64 root_tag_idx = 0;
          for EachIndex(work_idx, works_count)
          {
            U64 root_tag_idx_opl = root_tag_idx + root_tags_per_work;
            root_tag_idx_opl = ClampTop(root_tag_idx_opl, root_tags_in_this_unit);
            works[work_idx].unit_idx = unit_idx;
            works[work_idx].root_tag_idx_range = r1u64(root_tag_idx, root_tag_idx_opl);
            root_tag_idx = root_tag_idx_opl;
          }
        }
        sub_unit_work_idx += works_per_this_unit;
      }
      if(!build)
      {
        sub_unit_works_count = sub_unit_work_idx;
        sub_unit_works = push_array(scratch.arena, D2R_SubUnitWork, sub_unit_works_count);
      }
    }
  }
  lane_sync_u64(&sub_unit_works_count, 0);
  lane_sync_u64(&sub_unit_works, 0);
  
  ////////////////////////////
  //- rjf: predict the total number of tags in all units
  //
  U64 total_tag_count_estimate = 1;
  {
    U64 tag_size_estimate = 32;
    for EachIndex(unit_idx, unit_count)
    {
      total_tag_count_estimate += dim_1u64(unit_info_tag_ranges[unit_idx]) / tag_size_estimate;
    }
  }
  
  D2R_UniqueTagNode       **unique_tag_slots       = 0;
  U64                       unique_tag_slots_count = 0;
  D2R_UnitDedupedTagMap    *unit_deduped_tag_maps  = 0;
  RDIM_TypeChunkList       *all_types              = 0;
  RDIM_Type               **type_from_idx_map      = 0;
  RDIM_NamespaceChunkList  *all_namespaces         = 0;
  RDIM_Namespace          **namespace_from_idx_map = 0;
  U64                       type_count             = 0;
  D2R_UniqueTagNode       **type_tag_nodes         = 0;
  U64                       namespace_count        = 0;
  D2R_UniqueTagNode       **namespace_tag_nodes    = 0;
  RDIM_UDTChunkList        *all_udts               = 0;
  if(make_types)
  {
    ////////////////////////////
    //- rjf: hash all tags / attributes related to type info in all works,
    // for combining into type content hashes later. type content hashes
    // require combining a tree of several tag hashes - this just does the
    // tag hash portion.
    //
    // record into (off -> hash) map.
    //
    U64               tag_hash_slots_count = 0;
    D2R_TagHashNode **tag_hash_slots       = 0;
    tag_hash_slots_count = total_tag_count_estimate/2 + 1;
    if(lane_idx() == 0)
    {
      tag_hash_slots = push_array(scratch.arena, D2R_TagHashNode *, tag_hash_slots_count);
    }
    lane_sync_u64(&tag_hash_slots, 0);
    ProfScope("hash all tags / attributes related to type info in all works, for combining into type content hashes later")
    {
      U64 work_take_idx_ = 0;
      U64 *work_take_idx_ptr = &work_take_idx_;
      lane_sync_u64(&work_take_idx_ptr, 0);
      for(;;)
      {
        //- rjf: take next work
        U64 work_idx = ins_atomic_u64_inc_eval(work_take_idx_ptr) - 1;
        if(work_idx >= sub_unit_works_count)
        {
          break;
        }
        ProfBegin("hash work");

        //- rjf: unpack work
        U64 unit_idx = sub_unit_works[work_idx].unit_idx;
        Rng1U64 unit_root_tag_idx_range = sub_unit_works[work_idx].root_tag_idx_range;

        //- rjf: unpack unit info
        DW2_ParseCtx *unit_parse_ctx = &unit_parse_ctxs[unit_idx];
        Rng1U64 unit_info_tag_range = unit_info_tag_ranges[unit_idx];

        //- rjf: hash all relevant tags
        for(U64 root_tag_idx = unit_root_tag_idx_range.min;
            root_tag_idx < unit_root_tag_idx_range.max;
            root_tag_idx += 1)
        {
          typedef struct D2R_ParentTagNode D2R_ParentTagNode;
          struct D2R_ParentTagNode
          {
            D2R_ParentTagNode *next;
            U64 hash_seed;
          };
          D2R_ParentTagNode *top_parent = 0;
          D2R_ParentTagNode *free_parent = 0;
          U64 root_tag_start_off = unit_info_root_tag_offs[unit_idx].v[root_tag_idx];
          S64 depth = 0;
          for(U64 off = root_tag_start_off; contains_1u64(unit_info_tag_range, off) && (depth > 0 || off == root_tag_start_off);)
          {
            Temp tag_scratch = scratch_begin(&scratch.arena, 1);
            U64 start_off = off;

            // rjf: read tag
            DW2_Tag tag = {0};
            off += dw2_read_tag(tag_scratch.arena, raw, unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, off, &tag);

            // rjf: determine if we need this tag's hash
            B32 need_tag_hash = (tag.kind == DW_TagKind_ArrayType ||
                                 tag.kind == DW_TagKind_ClassType ||
                                 tag.kind == DW_TagKind_EnumerationType ||
                                 tag.kind == DW_TagKind_PointerType ||
                                 tag.kind == DW_TagKind_ReferenceType ||
                                 tag.kind == DW_TagKind_StringType ||
                                 tag.kind == DW_TagKind_StructureType ||
                                 tag.kind == DW_TagKind_SubroutineType ||
                                 tag.kind == DW_TagKind_SubProgram ||
                                 tag.kind == DW_TagKind_Typedef ||
                                 tag.kind == DW_TagKind_UnionType ||
                                 tag.kind == DW_TagKind_PtrToMemberType ||
                                 tag.kind == DW_TagKind_SetType ||
                                 tag.kind == DW_TagKind_BaseType ||
                                 tag.kind == DW_TagKind_ConstType ||
                                 tag.kind == DW_TagKind_FileType ||
                                 tag.kind == DW_TagKind_PackedType ||
                                 tag.kind == DW_TagKind_VolatileType ||
                                 tag.kind == DW_TagKind_RestrictType ||
                                 tag.kind == DW_TagKind_InterfaceType ||
                                 tag.kind == DW_TagKind_UnspecifiedType ||
                                 tag.kind == DW_TagKind_SharedType ||
                                 tag.kind == DW_TagKind_RValueReferenceType ||
                                 tag.kind == DW_TagKind_CoarrayType ||
                                 tag.kind == DW_TagKind_DynamicType ||
                                 tag.kind == DW_TagKind_AtomicType ||
                                 tag.kind == DW_TagKind_ImmutableType ||
                                 tag.kind == DW_TagKind_Member ||
                                 tag.kind == DW_TagKind_FormalParameter ||
                                 tag.kind == DW_TagKind_Namespace);

            // rjf: get top hash seed
            U64 hash_seed = top_parent ? top_parent->hash_seed : 0;

            // rjf: hash <:= tag kind
            U64 hash = hash_seed;
            if(need_tag_hash)
            {
              hash = u64_hash_from_seed_str8(hash, str8_struct(&tag.kind));
            }

            // rjf: hash <:= non-reference attributes,
            if(need_tag_hash) for(DW2_AttribNode *n = tag.attribs.first; n != 0; n = n->next)
            {
              B32 is_name = (n->v.attrib_kind == DW_AttribKind_Name);
              if(((tag.kind != DW_TagKind_SubProgram && tag.kind != DW_TagKind_FormalParameter) || !is_name) &&
                 n->v.val.kind != DW_FormKind_RefAddr &&
                 n->v.val.kind != DW_FormKind_Ref1 &&
                 n->v.val.kind != DW_FormKind_Ref2 &&
                 n->v.val.kind != DW_FormKind_Ref4 &&
                 n->v.val.kind != DW_FormKind_Ref8 &&
                 n->v.val.kind != DW_FormKind_RefUData &&
                 n->v.val.kind != DW_FormKind_RefSup4 &&
                 n->v.val.kind != DW_FormKind_RefSig8 &&
                 ((tag.kind != DW_TagKind_SubProgram &&
                   tag.kind != DW_TagKind_FormalParameter) ||
                  (n->v.attrib_kind != DW_AttribKind_Name &&
                   n->v.attrib_kind != DW_AttribKind_DeclFile &&
                   n->v.attrib_kind != DW_AttribKind_DeclLine &&
                   n->v.attrib_kind != DW_AttribKind_Prototyped &&
                   n->v.attrib_kind != DW_AttribKind_External &&
                   n->v.attrib_kind != DW_AttribKind_FrameBase &&
                   n->v.attrib_kind != DW_AttribKind_Location &&
                   n->v.attrib_kind != DW_AttribKind_LowPc &&
                   n->v.attrib_kind != DW_AttribKind_HighPc)))
              {
                hash = u64_hash_from_seed_str8(hash, str8_struct(&n->v.val.kind));
                if(n->v.val.string.size != 0)
                {
                  hash = u64_hash_from_seed_str8(hash, n->v.val.string);
                }
                else if(n->v.val.addr != 0)
                {
                  hash = u64_hash_from_seed_str8(hash, str8_struct(&n->v.val.addr));
                }
                else
                {
                  hash = u64_hash_from_seed_str8(hash, str8_struct(&n->v.val.u128));
                }
              }
            }

            // rjf: store
            if(need_tag_hash)
            {
              U64 off_hash = u64_hash_from_str8(str8_struct(&start_off));
              U64 slot_idx = off_hash%tag_hash_slots_count;
              D2R_TagHashNode *n = push_array(scratch.arena, D2R_TagHashNode, 1);
              n->info_off = start_off;
              n->hash = hash;
              for(B32 inserted = 0; !inserted;)
              {
                U64 slot_head_val = (U64)ins_atomic_u64_eval(&tag_hash_slots[slot_idx]);
                n->next = (D2R_TagHashNode *)slot_head_val;
                if(slot_head_val == ins_atomic_u64_eval_cond_assign(&tag_hash_slots[slot_idx], (U64)n, slot_head_val))
                {
                  inserted = 1;
                }
              }
            }

            // rjf: push parents
            if(tag.has_children)
            {
              // rjf: increase depth
              depth += 1;

              // rjf: determine hash seed - only add new hash seeds for namespaces
              U64 n_hash_seed = hash_seed;
              if(tag.kind == DW_TagKind_Namespace ||
                 tag.kind == DW_TagKind_StructureType ||
                 tag.kind == DW_TagKind_ClassType ||
                 tag.kind == DW_TagKind_UnionType)
              {
                n_hash_seed = hash;
              }

              // rjf: push node for this parent
              D2R_ParentTagNode *n = free_parent;
              if(n != 0)
              {
                SLLStackPop(free_parent);
              }
              else
              {
                n = push_array_no_zero(scratch.arena, D2R_ParentTagNode, 1);
              }
              MemoryZeroStruct(n);
              SLLStackPush(top_parent, n);
              n->hash_seed = n_hash_seed;
            }

            // rjf: pop parents
            if(tag.kind == DW_TagKind_Null)
            {
              // rjf: decrease depth
              depth -= 1;

              // rjf: pop
              if(top_parent != 0)
              {
                D2R_ParentTagNode *popped = top_parent;
                SLLStackPop(top_parent);
                SLLStackPush(free_parent, popped);
              }
            }

            scratch_end(tag_scratch);
            if(off == start_off)
            {
              break;
            }
          }
        }

        ProfEnd();
      }
      lane_sync();
    }

    ////////////////////////////
    //- rjf: gather all unique, to-be-deduplicated tags across all units (types, namespaces)
    //
    ProfScope("gather all unique, to-be-deduplicated tags across all units (types, namespaces)")
    {
      //- rjf: set up tables
      unique_tag_slots_count = total_tag_count_estimate/8 + 1;
      if(lane_idx() == 0)
      {
        unique_tag_slots = push_array(scratch.arena, D2R_UniqueTagNode *, unique_tag_slots_count);
        unit_deduped_tag_maps = push_array(scratch.arena, D2R_UnitDedupedTagMap, unit_count);
      }
      lane_sync_u64(&unique_tag_slots, 0);
      lane_sync_u64(&unit_deduped_tag_maps, 0);

      //- rjf: set up per-unit deduplicated tag maps
      {
        Rng1U64 range = lane_range(unit_count);
        for EachInRange(unit_idx, range)
        {
          Rng1U64 unit_info_tag_range = unit_info_tag_ranges[unit_idx];
          unit_deduped_tag_maps[unit_idx].slots_count = dim_1u64(unit_info_tag_range) / 32 + 1;
          unit_deduped_tag_maps[unit_idx].slots = push_array(scratch.arena, D2R_UnitDedupedTagNode *, unit_deduped_tag_maps[unit_idx].slots_count);
        }
        lane_sync();
      }

      //- rjf: gather unique tags
      {
        U64 work_take_idx_ = 0;
        U64 *work_take_idx_ptr = &work_take_idx_;
        lane_sync_u64(&work_take_idx_ptr, 0);
        for(;;)
        {
          //- rjf: take next work
          U64 work_idx = ins_atomic_u64_inc_eval(work_take_idx_ptr) - 1;
          if(work_idx >= sub_unit_works_count)
          {
            break;
          }
          ProfBegin("gather unique tags work");
          Temp work_scratch = scratch_begin(&scratch.arena, 1);

          //- rjf: unpack work
          U64 origin_unit_idx = sub_unit_works[work_idx].unit_idx;
          Rng1U64 origin_unit_root_tag_idx_range = sub_unit_works[work_idx].root_tag_idx_range;

          //- rjf: unpack unit info
          DW2_ParseCtx *origin_unit_parse_ctx = &unit_parse_ctxs[origin_unit_idx];
          Rng1U64 origin_unit_info_tag_range = unit_info_tag_ranges[origin_unit_idx];

          //- rjf: gather all tags which we want to deduplicate in this work
          typedef struct D2R_TagNode D2R_TagNode;
          struct D2R_TagNode
          {
            D2R_TagNode *next;
            U64 info_off;
          };
          U64 tags_to_dedup_count = 0;
          D2R_TagNode *first_tag_to_dedup = 0;
          D2R_TagNode *last_tag_to_dedup = 0;
          for(U64 root_tag_idx = origin_unit_root_tag_idx_range.min;
              root_tag_idx < origin_unit_root_tag_idx_range.max;
              root_tag_idx += 1)
          {
            U64 root_tag_start_off = unit_info_root_tag_offs[origin_unit_idx].v[root_tag_idx];
            typedef struct D2R_ParentTagNode D2R_ParentTagNode;
            struct D2R_ParentTagNode
            {
              D2R_ParentTagNode *next;
              B32 disable_dedup;
            };
            D2R_ParentTagNode *top_parent = 0;
            D2R_ParentTagNode *free_parent = 0;
            S64 depth = 0;
            for(U64 off = root_tag_start_off; contains_1u64(origin_unit_info_tag_range, off) && (depth > 0 || off == root_tag_start_off);)
            {
              Temp tag_scratch = scratch_begin(&work_scratch.arena, 1);
              U64 start_off = off;

              // rjf: read tag
              DW2_Tag tag = {0};
              off += dw2_read_tag(tag_scratch.arena, raw, origin_unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, off, &tag);

              // rjf: look for sibling attribute fast path
              DW2_Attrib *sibling_attrib = dw2_attrib_from_kind(&tag, DW_AttribKind_Sibling);
              B32 sibling_encoded = (sibling_attrib != &dw2_attrib_nil);
              U64 sibling_off = dw2_reference_info_off_from_form_val(origin_unit_parse_ctx, &sibling_attrib->val);

              // rjf: determine if we'll deduplicate children of this tag
              //
              // three possible cases:
              //
              //  (1) namespaced by a function - we do *not* want to deduplicate.
              //      functions are not deduplicated, and even if types defined
              //      within many functions match structurally, we consider them
              //      unique, given that they have different definition sites &
              //      names (fully qualified) still. (technically, if two UDTs
              //      match structurally, we could even dedup members - so two
              //      UDT records, w/ different decl sites/names/etc., but point
              //      at the same members)
              //
              //  (2) namespaced by a type - we *do* want to deduplicate. the
              //      root-level type will be deduplicated, but children are
              //      fragmentary - see (3).
              //
              //  (3) namespaced by a namespace - we *do* want to deduplicate. the
              //      children of a namespace are functionally top-level, and even
              //      if the same namespace definition is used across units, the
              //      debug info for each unit will contain fragmentary parts of it,
              //      since presumably unused pieces of the namespace will be stripped.
              //      thus, we need to visit namespace children as if they're top-level.
              //
              B32 children_should_be_deduped = 0;
              if(tag.kind == DW_TagKind_Namespace ||
                 tag.kind == DW_TagKind_ClassType ||
                 tag.kind == DW_TagKind_StructureType ||
                 tag.kind == DW_TagKind_UnionType ||
                 tag.kind == DW_TagKind_SubProgram)
              {
                children_should_be_deduped = 1;
              }

              // rjf: is this one of the tags we'd like to dedup? -> gather
              if((top_parent == 0 || !top_parent->disable_dedup) &&
                 (tag.kind == DW_TagKind_ArrayType ||
                  tag.kind == DW_TagKind_ClassType ||
                  tag.kind == DW_TagKind_EnumerationType ||
                  tag.kind == DW_TagKind_PointerType ||
                  tag.kind == DW_TagKind_ReferenceType ||
                  tag.kind == DW_TagKind_StringType ||
                  tag.kind == DW_TagKind_StructureType ||
                  tag.kind == DW_TagKind_SubroutineType ||
                  tag.kind == DW_TagKind_SubProgram ||
                  tag.kind == DW_TagKind_Typedef ||
                  tag.kind == DW_TagKind_UnionType ||
                  tag.kind == DW_TagKind_PtrToMemberType ||
                  tag.kind == DW_TagKind_SetType ||
                  tag.kind == DW_TagKind_BaseType ||
                  tag.kind == DW_TagKind_ConstType ||
                  tag.kind == DW_TagKind_FileType ||
                  tag.kind == DW_TagKind_PackedType ||
                  tag.kind == DW_TagKind_VolatileType ||
                  tag.kind == DW_TagKind_RestrictType ||
                  tag.kind == DW_TagKind_InterfaceType ||
                  tag.kind == DW_TagKind_UnspecifiedType ||
                  tag.kind == DW_TagKind_SharedType ||
                  tag.kind == DW_TagKind_RValueReferenceType ||
                  tag.kind == DW_TagKind_CoarrayType ||
                  tag.kind == DW_TagKind_DynamicType ||
                  tag.kind == DW_TagKind_AtomicType ||
                  tag.kind == DW_TagKind_ImmutableType ||
                  tag.kind == DW_TagKind_Namespace))
                  {
                    D2R_TagNode *n = push_array(work_scratch.arena, D2R_TagNode, 1);
                    n->info_off = start_off;
                    SLLQueuePush(first_tag_to_dedup, last_tag_to_dedup, n);
                    tags_to_dedup_count += 1;
                  }

              // rjf: if we have a sibling offset encoded, and if we do not need to dedup children,
              // then skip
              if(!children_should_be_deduped && sibling_encoded && sibling_off >= off)
              {
                off = sibling_off;
              }

              // rjf: otherwise, update depth / parent stack
              else
              {
                if(tag.has_children)
                {
                  depth += 1;
                  D2R_ParentTagNode *n = free_parent;
                  if(n != 0)
                  {
                    SLLStackPop(free_parent);
                  }
                  else
                  {
                    n = push_array(work_scratch.arena, D2R_ParentTagNode, 1);
                  }
                  SLLStackPush(top_parent, n);
                  n->disable_dedup = !children_should_be_deduped;
                }
                if(tag.kind == DW_TagKind_Null)
                {
                  depth -= 1;
                  depth = Max(0, depth);
                  if(top_parent != 0)
                  {
                    D2R_ParentTagNode *popped = top_parent;
                    SLLStackPop(top_parent);
                    SLLStackPush(free_parent, popped);
                  }
                }
              }

              scratch_end(tag_scratch);
              if(off == start_off)
              {
                break;
              }
            }
          }

          //- rjf: produce tree + reference hash for all tags we need to deduplicate & gather
          //
          // the tree hash is formed by combining the tag hashes under a single tag
          // the tree+ref hash is formed by combining the tree hashes across a graph of references
          //
          for(D2R_TagNode *tag_n = first_tag_to_dedup; tag_n != 0; tag_n = tag_n->next)
          {
            Temp dedup_root_scratch = scratch_begin(&scratch.arena, 1);

            //- rjf: unpack tag node
            U64 start_off = tag_n->info_off;

            //- rjf: compute tree hash
            U64 hash = 0;
            D2R_UniqueTagKind unique_tag_kind = D2R_UniqueTagKind_Type;
            U64 dependency_count = 0;
            U64 container_ancestor_info_off = 0;
            {
              typedef struct TagTask TagTask;
              struct TagTask
              {
                TagTask *next;
                DW_TagKind root_tag_kind;
                U64 container_ancestor_info_off;
                U64 dependency_count;
                U64 unit_idx;
                U64 start_off;
                U64 off;
                S64 depth;
                U64 hash;
                B32 is_member;
              };
              TagTask start_task = {0, DW_TagKind_Null, 0, 0, origin_unit_idx, start_off, start_off, 0};
              TagTask *top_task = &start_task;
              TagTask *free_task = 0;
              for(;top_task != 0;)
              {
                //- rjf: try to find an already-computed hash for this task
                U64 already_computed_hash = 0;
                U64 already_computed_dependency_count = 0;
                B32 found_already_computed_hash = 0;
#if 0
                {
                  U64 off_hash = u64_hash_from_str8(str8_struct(&top_task->start_off));
                  U64 off_slot_idx = off_hash%unit_deduped_tag_maps[top_task->unit_idx].slots_count;
                  D2R_UnitDedupedTagNode *slot_head = (D2R_UnitDedupedTagNode *)ins_atomic_u64_eval(&unit_deduped_tag_maps[top_task->unit_idx].slots[off_slot_idx]);
                  for(D2R_UnitDedupedTagNode *n = slot_head; n != 0; n = n->next)
                  {
                    if(n->src_info_off == top_task->start_off)
                    {
                      found_already_computed_hash = 1;
                      already_computed_hash = n->dst_hash;
                      already_computed_dependency_count = n->dependency_count;
                      break;
                    }
                  }
                }
#endif

                //- rjf: if we found an already-computed hash, just write it into this task as its result
                if(found_already_computed_hash)
                {
                  top_task->hash = already_computed_hash;
                  top_task->dependency_count = already_computed_dependency_count;
                }

                //- rjf: unpack task's unit
                Rng1U64 unit_info_tag_range = unit_info_tag_ranges[top_task->unit_idx];
                DW2_ParseCtx *unit_parse_ctx = &unit_parse_ctxs[top_task->unit_idx];

                //- rjf: determine if we're done with this hash
                B32 done = (found_already_computed_hash || (top_task->depth == 0 && top_task->start_off != top_task->off) || !contains_1u64(unit_info_tag_range, top_task->off));

                //- rjf: if we're not done -> continue contributing to the task's hash
                if(!done)
                {
                  Temp tag_scratch = scratch_begin(&dedup_root_scratch.arena, 1);

                  // rjf: read next tag
                  DW2_Tag tag = {0};
                  U64 start_off = top_task->off;
                  top_task->off += dw2_read_tag(tag_scratch.arena, raw, unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, top_task->off, &tag);

                  // rjf: determine whether or not to include this tag
                  B32 include_tag_in_hash = 1;
                  if(top_task->depth > 0)
                  {
                    if(top_task->root_tag_kind == DW_TagKind_SubProgram)
                    {
                      include_tag_in_hash = (tag.kind == DW_TagKind_FormalParameter);
                    }
                    else if(top_task->root_tag_kind == DW_TagKind_StructureType ||
                            top_task->root_tag_kind == DW_TagKind_ClassType ||
                            top_task->root_tag_kind == DW_TagKind_UnionType)
                    {
                      include_tag_in_hash = (tag.kind == DW_TagKind_Member);
                    }
                  }

                  // rjf: look up this tag's hash / ancestor
                  U64 tag_hash = 0;
                  U64 container_ancestor_info_off = 0;
                  if(include_tag_in_hash)
                  {
                    U64 hash = u64_hash_from_str8(str8_struct(&start_off));
                    U64 slot_idx = hash%tag_hash_slots_count;
                    for(D2R_TagHashNode *n = tag_hash_slots[slot_idx]; n != 0; n = n->next)
                    {
                      if(n->info_off == start_off)
                      {
                        tag_hash = n->hash;
                        container_ancestor_info_off = n->container_ancestor_info_off;
                        break;
                      }
                    }
                  }

                  // rjf: remember root tag info
                  if(start_off == top_task->start_off)
                  {
                    top_task->root_tag_kind = tag.kind;
                    top_task->container_ancestor_info_off = container_ancestor_info_off;
                  }

                  // rjf: combine tag's hash to tree hash
                  if(include_tag_in_hash)
                  {
                    top_task->hash = u64_hash_from_seed_str8(top_task->hash, str8_struct(&tag_hash));
                  }

                  // rjf: determine if we want to descend to children
                  B32 descend = (top_task->root_tag_kind != DW_TagKind_Namespace);

                  // rjf: tree navigations
                  if(descend)
                  {
                    if(tag.has_children)
                    {
                      top_task->depth += 1;
                    }
                    if(tag.kind == DW_TagKind_Null)
                    {
                      top_task->depth -= 1;
                    }
                  }

                  // rjf: unpack type reference in this tag
                  DW2_Attrib *type_attrib = &dw2_attrib_nil;
                  U64 type_info_off = 0;
                  if(include_tag_in_hash && (!top_task->is_member || (tag.kind != DW_TagKind_PointerType &&
                                                                      tag.kind != DW_TagKind_PtrToMemberType &&
                                                                      tag.kind != DW_TagKind_ReferenceType &&
                                                                      tag.kind != DW_TagKind_RValueReferenceType)))
                  {
                    type_attrib = dw2_attrib_from_kind(&tag, DW_AttribKind_Type);
                    type_info_off = dw2_reference_info_off_from_form_val(unit_parse_ctx, &type_attrib->val);
                  }

                  // rjf: if this type reference is recursive, just merge an ancestor identifier into the hash
                  B32 type_ref_is_recursive = 0;
                  if(type_info_off != 0)
                  {
                    U64 ancestor_idx = 0;
                    for(TagTask *t = top_task; t != 0; t = t->next, ancestor_idx += 1)
                    {
                      if(t->start_off == type_info_off)
                      {
                        type_ref_is_recursive = 1;
                        top_task->hash = u64_hash_from_seed_str8(top_task->hash, str8_struct(&ancestor_idx));
                        break;
                      }
                    }
                  }

                  // rjf: if this tag has a non-recursive type reference -> push a new task to hash the type.
                  if(!type_ref_is_recursive && type_info_off != 0)
                  {
                    if(tag.kind != DW_TagKind_Member)
                    {
                      top_task->dependency_count += 1;
                    }
                    U64 unit_idx = top_task->unit_idx;
                    if(!contains_1u64(unit_info_tag_range, type_info_off))
                    {
                      U64 new_unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, type_info_off);
                      if(0 < new_unit_num && new_unit_num <= unit_count)
                      {
                        unit_idx = new_unit_num-1;
                      }
                    }
                    TagTask *task = free_task;
                    if(task != 0)
                    {
                      SLLStackPop(free_task);
                    }
                    else
                    {
                      task = push_array_no_zero(dedup_root_scratch.arena, TagTask, 1);
                    }
                    MemoryZeroStruct(task);
                    task->unit_idx = unit_idx;
                    task->start_off = task->off = type_info_off;
                    task->is_member = (tag.kind == DW_TagKind_Member);
                    SLLStackPush(top_task, task);
                  }

                  scratch_end(tag_scratch);
                }

                //- rjf: if we're done, and if we computed the hash on this lane -> record in the per-unit (offset -> hash) table
                if(done && !found_already_computed_hash && top_task == &start_task)
                {
                  U64 info_off_hash = u64_hash_from_str8(str8_struct(&top_task->start_off));
                  U64 info_off_slot_idx = info_off_hash%unit_deduped_tag_maps[top_task->unit_idx].slots_count;
                  D2R_UnitDedupedTagNode *n = push_array(scratch.arena, D2R_UnitDedupedTagNode, 1);
                  n->src_info_off     = top_task->start_off;
                  n->dst_hash         = top_task->hash;
                  n->dependency_count = top_task->dependency_count;
                  for(B32 inserted = 0; !inserted;)
                  {
                    U64 slot_head_val = ins_atomic_u64_eval(&unit_deduped_tag_maps[origin_unit_idx].slots[info_off_slot_idx]);
                    n->next = (D2R_UnitDedupedTagNode *)slot_head_val;
                    if(slot_head_val == ins_atomic_u64_eval_cond_assign(&unit_deduped_tag_maps[origin_unit_idx].slots[info_off_slot_idx], (U64)n, slot_head_val))
                    {
                      inserted = 1;
                    }
                  }
                }

                //- rjf: if we're done -> pop this task off the stack, merge into parent task
                if(done && top_task != 0)
                {
                  TagTask *popped = top_task;
                  SLLStackPop(top_task);
                  SLLStackPush(free_task, popped);
                  if(top_task != 0)
                  {
                    top_task->hash = u64_hash_from_seed_str8(top_task->hash, str8_struct(&popped->hash));
                    if(!popped->is_member)
                    {
                      top_task->dependency_count += popped->dependency_count;
                    }
                  }
                }
              }
              hash = start_task.hash;
              unique_tag_kind = (start_task.root_tag_kind == DW_TagKind_Namespace ? D2R_UniqueTagKind_Namespace : D2R_UniqueTagKind_Type);
              container_ancestor_info_off = start_task.container_ancestor_info_off;
              dependency_count = start_task.dependency_count;
            }

            //- rjf: record this in the (hash -> offset) deduplicated table
            {
              B32 gathered = 0;
              U64 slot_idx = hash%unique_tag_slots_count;
              for(;!gathered;)
              {
                // rjf: read existing slot head pointer *before* we lookup / insert
                U64 slot_head_val = (U64)ins_atomic_u64_eval(&unique_tag_slots[slot_idx]);

                // rjf: determine if this hash has been gathered
                D2R_UniqueTagNode *already_gathered_node = 0;
                for(D2R_UniqueTagNode *n = (D2R_UniqueTagNode *)slot_head_val; n != 0; n = n->next)
                {
                  if(n->hash == hash)
                  {
                    gathered = 1;
                    already_gathered_node = n;
                    break;
                  }
                }

                // rjf: if this hash *has* been gathered, we want to prefer a
                // deterministic unit/info offset - so we'll just prefer the
                // lowest unit/offset.
                //
                // TODO(rjf): this seems to only be necessary because of a flaw
                // in the deduplicator logic (without this part). in theory, if we
                // trust the hash (all experiments suggested that we should - e.g.
                // no hash collisions w/ different data were observed), then the
                // same hash should imply an identical type graph. this may be false
                // for reasons I do not currently understand, or due to a mistake
                // somewhere. but for now, I've added this, to ensure that when there
                // are many type tag trees which map to an identical hash, we always
                // parse the one from the lowest .debug_info offset, just to make
                // sure it's deterministic. in a multi-threaded conversion, this
                // is not true in general without this step; one thread which finds
                // the same type with a higher .debug_info offset may win, and so
                // where exactly one type hash is parsed is not deterministic. but,
                // the theory is, that shouldn't matter - the type graphs should
                // match nonetheless. in any case, if that turns out to be a broken
                // theory, this should be a relatively cheap step to guarantee
                // deterministic (hash -> .debug_info offset), which is likely a
                // desirable property regardless. but I wanted to leave this here
                // so that I can more closely verify what is going on later.
                //
                if(gathered && already_gathered_node != 0)
                {
                  B32 minimized = 0;
                  for(;!minimized;)
                  {
                    U64 stored_info_off = ins_atomic_u64_eval(&already_gathered_node->info_off);
                    if(start_off < stored_info_off)
                    {
                      if(ins_atomic_u64_eval_cond_assign(&already_gathered_node->info_off, start_off, stored_info_off) == stored_info_off)
                      {
                        minimized = 1;
                      }
                    }
                    else
                    {
                      minimized = 1;
                    }
                  }
                }

                // rjf: if this hash has *not* been gathered, try an insert. we:
                //
                //  1. allocate/fill a node
                //  2. set it up to point to the old head
                //  3. compare/exchange the old head with the new head - IFF the head matches what we expect from above
                //  4. if we fail, another thread has touched this slot, we pop the allocated node & try again
                //     (we may find that another thread has filled this hash, so we'll just be done)
                //
                if(!gathered)
                {
                  Temp insert_temp = temp_begin(scratch.arena);
                  D2R_UniqueTagNode *n = push_array(scratch.arena, D2R_UniqueTagNode, 1);
                  n->next = (D2R_UniqueTagNode *)slot_head_val;
                  n->kind = unique_tag_kind;
                  n->hash = hash;
                  n->info_off = start_off;
                  n->container_ancestor_info_off = container_ancestor_info_off;
                  n->dependency_count = dependency_count;
                  U64 new_head_val = (U64)n;
                  if(slot_head_val == ins_atomic_u64_eval_cond_assign(&unique_tag_slots[slot_idx], new_head_val, slot_head_val))
                  {
                    gathered = 1;
                  }
                  else
                  {
                    temp_end(insert_temp);
                  }
                }
              }
            }

            scratch_end(dedup_root_scratch);
          }
          // ProfMsg("work_idx %I64u: %I64u tags encountered", work_idx, total_tag_count_this_work);
          ProfEnd();
          scratch_end(work_scratch);
        }
        lane_sync();
      }
    }

    ////////////////////////////
    //- rjf: sort all unique tag table slots
    //
    ProfScope("sort all unique tag table slots")
    {
      Rng1U64 range = lane_range(unique_tag_slots_count);
      for EachInRange(slot_idx, range)
      {
        Temp scratch2 = scratch_begin(&scratch.arena, 1);
        D2R_UniqueTagNode **slot = &unique_tag_slots[slot_idx];

        // rjf: count nodes in this slot
        U64 node_count = 0;
        for EachNode(n, D2R_UniqueTagNode, slot[0])
        {
          node_count += 1;
        }

        // rjf: flatten
        D2R_UniqueTagNode **slot_nodes_array = push_array(scratch2.arena, D2R_UniqueTagNode *, node_count);
        {
          U64 idx = 0;
          for EachNode(n, D2R_UniqueTagNode, slot[0])
          {
            slot_nodes_array[idx]  = n;
            idx += 1;
          }
        }

        // rjf: sort
        radsort(slot_nodes_array, node_count, (int (*)(void *, void *))d2r_unique_tag_node_is_less_than);

        // rjf: re-order in the slot
        {
          unique_tag_slots[slot_idx] = 0;
          for EachIndex(idx, node_count)
          {
            SLLStackPush(unique_tag_slots[slot_idx], slot_nodes_array[idx]);
          }
        }

        scratch_end(scratch2);
      }
      lane_sync();
    }

    ////////////////////////////
    //- rjf: produce [0...n) <-> unique-tag-node mapping for all categories of deduplicated tags
    //
    U64 *deduped_tag_counts = 0; // [D2R_UniqueTagKind_COUNT]
    D2R_UniqueTagNode ***deduped_tag_nodes = 0; // [D2R_UniqueTagKind_COUNT]
    ProfScope("produce [0...n) -> hash mapping for all categories of deduplicated tags") if(lane_idx() == 0)
    {
      deduped_tag_counts = push_array(scratch.arena, U64, D2R_UniqueTagKind_COUNT);
      deduped_tag_nodes = push_array(scratch.arena, D2R_UniqueTagNode **, D2R_UniqueTagKind_COUNT);
      for EachIndex(slot_idx, unique_tag_slots_count)
      {
        for EachNode(n, D2R_UniqueTagNode, unique_tag_slots[slot_idx])
        {
          deduped_tag_counts[n->kind] += 1;
        }
      }
      for EachEnumVal(D2R_UniqueTagKind, k)
      {
        deduped_tag_nodes[k] = push_array(scratch.arena, D2R_UniqueTagNode *, deduped_tag_counts[k]);
      }
      U64 idxs[D2R_UniqueTagKind_COUNT] = {0};
      for EachIndex(slot_idx, unique_tag_slots_count)
      {
        for EachNode(n, D2R_UniqueTagNode, unique_tag_slots[slot_idx])
        {
          deduped_tag_nodes[n->kind][idxs[n->kind]] = n;
          n->order_idx = idxs[n->kind];
          idxs[n->kind] += 1;
        }
      }
    }
    lane_sync_u64(&deduped_tag_counts, 0);
    lane_sync_u64(&deduped_tag_nodes, 0);

    type_count = deduped_tag_counts[D2R_UniqueTagKind_Type];
    type_tag_nodes = deduped_tag_nodes[D2R_UniqueTagKind_Type];
    namespace_count = deduped_tag_counts[D2R_UniqueTagKind_Namespace];
    namespace_tag_nodes = deduped_tag_nodes[D2R_UniqueTagKind_Namespace];

    ////////////////////////////
    //- rjf: build built-in types
    //
    RDIM_TypeChunkList *builtin_types = 0;
    RDIM_Type **builtin_type_from_kind_map = 0; // [RDI_TypeKind_LastBuiltIn - RDI_TypeKind_FirstBuiltIn + 1]
    U64 builtin_type_count = RDI_TypeKind_LastBuiltIn - RDI_TypeKind_FirstBuiltIn + 1;
    ProfScope("build built-in types") if(lane_idx() == 0)
    {
      builtin_types = push_array(scratch.arena, RDIM_TypeChunkList, 1);
      builtin_type_from_kind_map = push_array(scratch.arena, RDIM_Type *, builtin_type_count);
      for(RDI_TypeKind k = RDI_TypeKind_FirstBuiltIn; k <= RDI_TypeKind_LastBuiltIn; k += 1)
      {
        RDIM_Type *type = rdim_type_chunk_list_push(arena, builtin_types, builtin_type_count);
        type->kind = k;
        type->name.str = rdi_string_from_type_kind(k, &type->name.size);
        type->byte_size = rdi_size_from_basic_type_kind(k);
        if(type->byte_size == max_U32) { type->byte_size = byte_size_from_arch(arch); }
        builtin_type_from_kind_map[k - RDI_TypeKind_FirstBuiltIn] = type;
      }
    }
    lane_sync_u64(&builtin_types, 0);
    lane_sync_u64(&builtin_type_from_kind_map, 0);
#define d2r_type_from_builtin_kind(k) ((RDI_TypeKind_FirstBuiltIn <= (k) && (k) <= RDI_TypeKind_LastBuiltIn) ? builtin_type_from_kind_map[k - RDI_TypeKind_FirstBuiltIn] : builtin_type_from_kind_map[RDI_TypeKind_Void])

    ////////////////////////////
    //- rjf: build all types - build types w/ 1 dependency (leaves) first, then 2, 3, etc.,
    // to ensure dependencies always travel backwards
    //
    // NOTE: * DWARF vs RDI Array Type Graph *
    //
    // For example lets take following decl:
    //
    //    int (*foo[2])[3];
    //
    //  This compiles to in DWARF:
    //
    //  foo -> DW_TAG_ArrayType -> (A0) DW_TAG_Subrange [2]
    //                          |
    //                          +-> (B0) DW_TAG_PointerType -> (A1) DW_TAG_ArrayType -> DW_TAG_Subrange [3]
    //                                                      | 
    //                                                      +-> (B1) DW_TAG_BaseType (int)
    //
    // RDI expects:
    //
    //  foo -> Array[2] -> Pointer -> Array[3] -> int
    //
    // Note that DWARF forks the graph on DW_TAG_ArrayType to describe array ranges in branch A and
    // in branch B describes array type which might be a struct, pointer, base type, or any other type tag.
    // However, in RDI we have a simple list of type nodes and to convert we need to append type nodes from
    // B to A.
    //
    ProfScope("build all types")
    {
      if(lane_idx() == 0)
      {
        all_types = push_array(scratch.arena, RDIM_TypeChunkList, 1);
        type_from_idx_map = push_array(scratch.arena, RDIM_Type *, type_count);
        rdim_type_chunk_list_concat_in_place(all_types, builtin_types);
      }
      lane_sync_u64(&all_types, 0);
      lane_sync_u64(&type_from_idx_map, 0);
      Rng1U64 range = lane_range(type_count);
      U64 max_chain_count = 0;
      for(;max_chain_count < max_U64;)
      {
        Temp scratch2 = scratch_begin(&scratch.arena, 1);
        ProfBegin("types build (chain count <= %I64u)", max_chain_count);

        //- rjf: gather all types in this lane that fit the dependency restriction;
        // find this lane's next dependency restriction
        U64 next_max_chain_count = max_U64;
        RDIM_TypeChunkList lane_types = {0};
        U64 lane_types_chunk_count = 256;
        for EachInRange(type_idx, range)
        {
          // rjf: idx -> type tag node
          D2R_UniqueTagNode *type_tag_node = type_tag_nodes[type_idx];

          // rjf: if this type has a higher chain count than the current,
          // but it is lower than our next maximum chain count, collect it,
          // so we will hit
          //
          // if this type has a lower chain count than the current,
          // we should've already built it from a previous pass.
          //
          U64 dependency_count = type_tag_node->dependency_count;
          if(dependency_count > max_chain_count)
          {
            next_max_chain_count = Min(dependency_count, next_max_chain_count);
            continue;
          }
          else if(dependency_count < max_chain_count)
          {
            continue;
          }
          Temp temp = temp_begin(scratch2.arena);

          // rjf: unpack type tag node
          U64 info_off = type_tag_node->info_off;
          U64 unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, info_off);
          U64 unit_idx = unit_num > 0 ? unit_num-1 : 0;

          // rjf: unpack unit
          Rng1U64 unit_info_tag_range = unit_info_tag_ranges[unit_idx];
          DW2_ParseCtx *unit_parse_ctx = &unit_parse_ctxs[unit_idx];

          // rjf: parse root-level tag
          DW2_Tag tag = {0};
          U64 tag_info_size = dw2_read_tag(temp.arena, raw, unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, info_off, &tag);

          // rjf: extract common attributes
          DW2_Attrib *name_attrib = &dw2_attrib_nil;
          DW2_Attrib *direct_type_attrib = &dw2_attrib_nil;
          DW2_Attrib *bitsize_attrib = &dw2_attrib_nil;
          DW2_Attrib *bytesize_attrib = &dw2_attrib_nil;
          DW2_Attrib *encoding_attrib = &dw2_attrib_nil;
          DW2_Attrib *decl_attrib = &dw2_attrib_nil;
          DW2_Attrib *prototyped_attrib = &dw2_attrib_nil;
          //
          // TODO(rjf): if a DW_AttribKing_GNU_Vector is found on an DW_TagKind_ArrayType,
          // then: @native_vector_support extract byte size from the base type tag
          // and convert to U256, U512, S256 and S512
          //
          for EachNode(n, DW2_AttribNode, tag.attribs.first)
          {
            switch(n->v.attrib_kind)
            {
            default:{}break;
#define Case(dst_name, src_name) case DW_AttribKind_##src_name:{dst_name##_attrib = &n->v;}break
                    Case(name,        Name);
                    Case(direct_type, Type);
                    Case(bitsize,     BitSize);
                    Case(bytesize,    ByteSize);
                    Case(encoding,    Encoding);
                    Case(decl,        Declaration);
#undef Case
            }
          }

          // rjf: unpack common attributes
          String8 name = name_attrib->val.string;
          DW_ATE encoding = encoding_attrib->val.u128.u64[0];
          RDIM_Type *direct_type = d2r_type_from_builtin_kind(RDI_TypeKind_Void);
          U64 bitsize = 0;
          B32 is_decl = (decl_attrib != &dw2_attrib_nil);
          {
            // rjf: unpack direct type
            if(direct_type_attrib != &dw2_attrib_nil)
            {
              U64 direct_type_info_off = dw2_reference_info_off_from_form_val(unit_parse_ctx, &direct_type_attrib->val);
              U64 direct_type_unit_idx = unit_idx;
              if(!contains_1u64(unit_info_tag_range, direct_type_info_off))
              {
                U64 new_unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, direct_type_info_off);
                direct_type_unit_idx = (new_unit_num > 0 ? new_unit_num-1 : unit_idx);
              }
              D2R_UnitDedupedTagMap *direct_type_unit_type_map = &unit_deduped_tag_maps[direct_type_unit_idx];
              U64 info_off_hash = u64_hash_from_str8(str8_struct(&direct_type_info_off));
              U64 info_off_slot_idx = info_off_hash%direct_type_unit_type_map->slots_count;
              U64 direct_type_hash = 0;
              for(D2R_UnitDedupedTagNode *n = direct_type_unit_type_map->slots[info_off_slot_idx]; n != 0; n = n->next)
              {
                if(n->src_info_off == direct_type_info_off)
                {
                  direct_type_hash = n->dst_hash;
                  break;
                }
              }
              U64 unique_type_tag_slot_idx = direct_type_hash%unique_tag_slots_count;
              for(D2R_UniqueTagNode *n = unique_tag_slots[unique_type_tag_slot_idx]; n != 0; n = n->next)
              {
                if(n->hash == direct_type_hash)
                {
                  direct_type = type_from_idx_map[n->order_idx];
                  break;
                }
              }
            }

            // rjf: unpack bit/byte sizes
            if(bitsize_attrib != &dw2_attrib_nil)
            {
              bitsize = bitsize_attrib->val.u128.u64[0];
            }
            else if(bytesize_attrib != &dw2_attrib_nil)
            {
              bitsize = bytesize_attrib->val.u128.u64[0]*8;
            }
          }

          // rjf: convert type
          RDIM_Type *dst_type = 0;
          RDI_TypeKind rdi_type_kind = RDI_TypeKind_Null;
          RDI_TypeModifierFlags rdi_type_modifier_flags = 0;
          switch(tag.kind)
          {
          default:{}break;
          case DW_TagKind_ClassType:     rdi_type_kind = is_decl ? RDI_TypeKind_IncompleteClass  : RDI_TypeKind_Class; goto struct_type;
          case DW_TagKind_StructureType: rdi_type_kind = is_decl ? RDI_TypeKind_IncompleteStruct : RDI_TypeKind_Struct; goto struct_type;
          case DW_TagKind_UnionType:     rdi_type_kind = is_decl ? RDI_TypeKind_IncompleteUnion  : RDI_TypeKind_Union; goto struct_type;
          struct_type:;
          {
            dst_type = rdim_type_chunk_list_push(arena, &lane_types, lane_types_chunk_count);
            dst_type->kind      = rdi_type_kind;
            dst_type->name      = name;
            dst_type->byte_size = bitsize/8;
          }break;
          case DW_TagKind_EnumerationType:
          {
            dst_type = rdim_type_chunk_list_push(arena, &lane_types, lane_types_chunk_count);
            dst_type->kind        = is_decl ? RDI_TypeKind_IncompleteEnum : RDI_TypeKind_Enum;
            dst_type->name        = name;
            dst_type->direct_type = direct_type;
            dst_type->byte_size   = (direct_type != 0 ? direct_type->byte_size : bitsize/8);
          }break;
          case DW_TagKind_SubProgram:
          case DW_TagKind_SubroutineType:
          {
            // rjf: build type
            dst_type = rdim_type_chunk_list_push(arena, &lane_types, lane_types_chunk_count);
            dst_type->kind        = RDI_TypeKind_Function;
            dst_type->direct_type = direct_type;
            dst_type->byte_size   = arch_addr_size;

            // rjf: gather all parameter types
            typedef struct ParamNode ParamNode;
            struct ParamNode
            {
              ParamNode *next;
              U64 type_unit_idx;
              U64 type_info_off;
            };
            ParamNode *first_param = 0;
            ParamNode *last_param = 0;
            U64 param_count = 0;
            if(tag.has_children)
            {
              U64 tag_children_off = info_off + tag_info_size;
              S64 depth = 1;
              for(U64 off = tag_children_off; depth > 0 && contains_1u64(unit_info_tag_range, off);)
              {
                Temp scratch3 = scratch_begin(&scratch2.arena, 1);
                U64 start_off = off;

                // rjf: read child tag
                DW2_Tag child_tag = {0};
                off += dw2_read_tag(scratch3.arena, raw, unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, off, &child_tag);

                // rjf: gather parameters
                if(depth == 1 && child_tag.kind == DW_TagKind_FormalParameter)
                {
                  DW2_Attrib *type_attrib = dw2_attrib_from_kind(&child_tag, DW_AttribKind_Type);
                  ParamNode *n = push_array(scratch2.arena, ParamNode, 1);
                  SLLQueuePush(first_param, last_param, n);
                  n->type_info_off = dw2_reference_info_off_from_form_val(unit_parse_ctx, &type_attrib->val);
                  n->type_unit_idx = unit_idx;
                  if(!contains_1u64(unit_info_tag_range, n->type_info_off))
                  {
                    U64 new_unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, n->type_info_off);
                    n->type_unit_idx = (new_unit_num > 0 ? new_unit_num-1 : unit_idx);
                  }
                  param_count += 1;
                }

                // rjf: navigate tree
                if(child_tag.kind == DW_TagKind_Null)
                {
                  depth -= 1;
                }
                if(child_tag.has_children)
                {
                  depth += 1;
                }

                scratch_end(scratch3);
                if(off == start_off)
                {
                  break;
                }
              }
            }

            // rjf: tighten parameter types
            dst_type->count = (RDI_U32)param_count; // TODO(rjf): @u64_to_u32
            dst_type->param_types = push_array(arena, RDIM_Type *, dst_type->count);
            {
              U64 param_idx = 0;
              for(ParamNode *n = first_param; n != 0; n = n->next, param_idx += 1)
              {
                U64 param_type_info_off = n->type_info_off;
                U64 param_type_unit_idx = n->type_unit_idx;

                // rjf: map info_off/unit_idx -> hash
                U64 param_type_hash = 0;
                {
                  D2R_UnitDedupedTagMap *direct_type_unit_type_map = &unit_deduped_tag_maps[param_type_unit_idx];
                  U64 info_off_hash = u64_hash_from_str8(str8_struct(&param_type_info_off));
                  U64 info_off_slot_idx = info_off_hash%direct_type_unit_type_map->slots_count;
                  for(D2R_UnitDedupedTagNode *n = direct_type_unit_type_map->slots[info_off_slot_idx]; n != 0; n = n->next)
                  {
                    if(n->src_info_off == param_type_info_off)
                    {
                      param_type_hash = n->dst_hash;
                      break;
                    }
                  }
                }

                // rjf: map hash -> type
                RDIM_Type *param_type = 0;
                {
                  U64 unique_type_tag_slot_idx = param_type_hash%unique_tag_slots_count;
                  for(D2R_UniqueTagNode *n = unique_tag_slots[unique_type_tag_slot_idx]; n != 0; n = n->next)
                  {
                    if(n->hash == param_type_hash)
                    {
                      param_type = type_from_idx_map[n->order_idx];
                      break;
                    }
                  }
                }

                // rjf: store
                dst_type->param_types[param_idx] = param_type;
              }
            }
          }break;
          case DW_TagKind_Typedef:
          {
            dst_type = rdim_type_chunk_list_push(arena, &lane_types, lane_types_chunk_count);
            dst_type->kind        = RDI_TypeKind_Alias;
            dst_type->name        = name;
            dst_type->direct_type = direct_type;
            dst_type->byte_size   = (direct_type ? direct_type->byte_size : 0);
          }break;
          case DW_TagKind_BaseType:
          {
            if(0){}
#define CaseA(rdi_type_kind_, encoding_)                                  else if(encoding == (encoding_)) { rdi_type_kind = (rdi_type_kind_); }
#define CaseB(rdi_type_kind_, encoding_, bit_size_)                       else if(encoding == (encoding_) && bitsize == (bit_size_)) { rdi_type_kind = (rdi_type_kind_); }
#define CaseC(rdi_type_kind_, encoding_, name_) /* assumes x64/arm64 */   else if(encoding == (encoding_) && str8_match(name, str8_lit(name_), 0)) { rdi_type_kind = (rdi_type_kind_); }
#define CaseD(rdi_type_kind_, encoding_, name_, bit_size_)                else if(encoding == (encoding_) && bitsize == (bit_size_) && str8_match(name, str8_lit(name_), 0)) { rdi_type_kind = (rdi_type_kind_); }
        CaseA(RDI_TypeKind_Null,        DW_ATE_Null)
        CaseA(RDI_TypeKind_Void,        DW_ATE_Address)
        CaseA(RDI_TypeKind_Bool,        DW_ATE_Boolean)
        CaseB(RDI_TypeKind_ComplexF32,  DW_ATE_ComplexFloat, 64)
        CaseB(RDI_TypeKind_ComplexF64,  DW_ATE_ComplexFloat, 128)
        CaseB(RDI_TypeKind_ComplexF80,  DW_ATE_ComplexFloat, 160)
        CaseB(RDI_TypeKind_ComplexF128, DW_ATE_ComplexFloat, 256)
        CaseC(RDI_TypeKind_F80,         DW_ATE_Float,        "__float80")
        CaseC(RDI_TypeKind_F128,        DW_ATE_Float,        "__float128")
        CaseC(RDI_TypeKind_F16,         DW_ATE_Float,        "_Float16")
        CaseC(RDI_TypeKind_BF16,        DW_ATE_Float,        "__bf16")
        CaseB(RDI_TypeKind_F16,         DW_ATE_Float,        16)
        CaseB(RDI_TypeKind_F32,         DW_ATE_Float,        32)
        CaseB(RDI_TypeKind_F48,         DW_ATE_Float,        48)
        CaseB(RDI_TypeKind_F64,         DW_ATE_Float,        64)
        CaseB(RDI_TypeKind_F80,         DW_ATE_Float,        80)
        CaseB(RDI_TypeKind_F96,         DW_ATE_Float,        96)
        CaseB(RDI_TypeKind_F128,        DW_ATE_Float,        128)
        CaseB(RDI_TypeKind_Char8,       DW_ATE_SignedChar,   8)
        CaseB(RDI_TypeKind_Char16,      DW_ATE_SignedChar,   16)
        CaseB(RDI_TypeKind_Char32,      DW_ATE_SignedChar,   32)
        CaseD(RDI_TypeKind_Char8,       DW_ATE_Signed,       "wchar_t", 8)
        CaseD(RDI_TypeKind_Char16,      DW_ATE_Signed,       "wchar_t", 16)
        CaseD(RDI_TypeKind_Char32,      DW_ATE_Signed,       "wchar_t", 32)
        CaseB(RDI_TypeKind_S8,          DW_ATE_Signed,       8)
        CaseB(RDI_TypeKind_S16,         DW_ATE_Signed,       16)
        CaseB(RDI_TypeKind_S32,         DW_ATE_Signed,       32)
        CaseB(RDI_TypeKind_S64,         DW_ATE_Signed,       64)
        CaseB(RDI_TypeKind_S128,        DW_ATE_Signed,       128)
        CaseB(RDI_TypeKind_S256,        DW_ATE_Signed,       256)
        CaseB(RDI_TypeKind_S512,        DW_ATE_Signed,       512)
        CaseB(RDI_TypeKind_U8,          DW_ATE_Unsigned,     8)
        CaseB(RDI_TypeKind_U16,         DW_ATE_Unsigned,     16)
        CaseB(RDI_TypeKind_U32,         DW_ATE_Unsigned,     32)
        CaseB(RDI_TypeKind_U64,         DW_ATE_Unsigned,     64)
        CaseB(RDI_TypeKind_U128,        DW_ATE_Unsigned,     128)
        CaseB(RDI_TypeKind_U256,        DW_ATE_Unsigned,     256)
        CaseB(RDI_TypeKind_U512,        DW_ATE_Unsigned,     512)
        CaseB(RDI_TypeKind_UChar8,      DW_ATE_UnsignedChar, 8)
        CaseB(RDI_TypeKind_UChar8,      DW_ATE_Utf,          8)
        CaseB(RDI_TypeKind_UChar16,     DW_ATE_UnsignedChar, 16)
        CaseB(RDI_TypeKind_UChar16,     DW_ATE_Utf,          16)
        CaseB(RDI_TypeKind_UChar32,     DW_ATE_UnsignedChar, 32)
        CaseB(RDI_TypeKind_UChar32,     DW_ATE_Utf,          32)
        CaseB(RDI_TypeKind_Decimal32,   DW_ATE_DecimalFloat, 32)
        CaseB(RDI_TypeKind_Decimal64,   DW_ATE_DecimalFloat, 64)
        CaseB(RDI_TypeKind_Decimal128,  DW_ATE_DecimalFloat, 128)
#undef CaseA
#undef CaseB
#undef CaseC
#undef CaseD
            dst_type = rdim_type_chunk_list_push(arena, &lane_types, lane_types_chunk_count);
            dst_type->kind        = RDI_TypeKind_Alias;
            dst_type->name        = name;
            dst_type->direct_type = d2r_type_from_builtin_kind(rdi_type_kind);
            dst_type->byte_size   = bitsize/8; // TODO: byte size on __float80 alias mismatches byte size on direct type now
          }break;
          case DW_TagKind_PointerType:         rdi_type_kind = RDI_TypeKind_Ptr; goto ptr_or_ref_type;
          case DW_TagKind_ReferenceType:       rdi_type_kind = RDI_TypeKind_LRef; goto ptr_or_ref_type;
          case DW_TagKind_RValueReferenceType: rdi_type_kind = RDI_TypeKind_RRef; goto ptr_or_ref_type;
          ptr_or_ref_type:;
          {
            dst_type = rdim_type_chunk_list_push(arena, &lane_types, lane_types_chunk_count);
            dst_type->kind        = rdi_type_kind;
            dst_type->direct_type = direct_type;
            dst_type->byte_size   = arch_addr_size;
          }break;
          case DW_TagKind_RestrictType:
          {
            rdi_type_modifier_flags = RDI_TypeModifierFlag_Restrict;
          }goto basic_type_operators;
          case DW_TagKind_VolatileType:
          {
            rdi_type_modifier_flags = RDI_TypeModifierFlag_Volatile;
          }goto basic_type_operators;
          case DW_TagKind_ConstType:
          {
            rdi_type_modifier_flags = RDI_TypeModifierFlag_Const;
          }goto basic_type_operators;
          basic_type_operators:;
          {
            dst_type = rdim_type_chunk_list_push(arena, &lane_types, lane_types_chunk_count);
            dst_type->kind        = RDI_TypeKind_Modifier;
            dst_type->flags       = rdi_type_modifier_flags;
            dst_type->direct_type = direct_type;
            dst_type->byte_size   = (direct_type ? direct_type->byte_size : 0);
          }break;
          case DW_TagKind_ArrayType:
           if(tag.has_children)
           {
             // rjf: parse array type children; extract dimensions (each one gets a SubrangeType)
             typedef struct ArrayDimensionNode ArrayDimensionNode;
             struct ArrayDimensionNode
             {
               ArrayDimensionNode *next;
               U64 count;
             };
             ArrayDimensionNode *top_dimension = 0;
             {
               U64 tag_children_off = info_off + tag_info_size;
               S64 depth = 1;
               for(U64 off = tag_children_off; contains_1u64(unit_info_tag_range, off) && depth > 0;)
               {
                 U64 start_off = off;
                 DW2_Tag child_tag = {0};
                 off += dw2_read_tag(temp.arena, raw, unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, off, &child_tag);
                 if(child_tag.kind == DW_TagKind_SubrangeType)
                 {
                   // rjf: extract bounds attribs
                   DW2_Attrib *lower_bound_attrib = &dw2_attrib_nil;
                   DW2_Attrib *upper_bound_attrib = &dw2_attrib_nil;
                   DW2_Attrib *count_attrib = &dw2_attrib_nil;
                   for(DW2_AttribNode *n = child_tag.attribs.first;
                       n != 0 && (lower_bound_attrib == &dw2_attrib_nil || (upper_bound_attrib == &dw2_attrib_nil && count_attrib == &dw2_attrib_nil));
                       n = n->next)
                   {
                     if(n->v.attrib_kind == DW_AttribKind_LowerBound)
                     {
                       lower_bound_attrib = &n->v;
                     }
                     else if(n->v.attrib_kind == DW_AttribKind_UpperBound)
                     {
                       upper_bound_attrib = &n->v;
                     }
                     else if(n->v.attrib_kind == DW_AttribKind_Count)
                     {
                       count_attrib = &n->v;
                     }
                   }

                   // rjf: resolve lower bound
                   U64 lower_bound = 0;
                   if(lower_bound_attrib != &dw2_attrib_nil)
                   {
                     if(dw_attrib_class_from_form_kind(unit_parse_ctx->version, unit_parse_ctx->exts, lower_bound_attrib->val.kind) & DW_AttribClass_Reference)
                     {
                       log_infof("[.debug_info@0x%I64x] Array type lower bound is a variable; this is not currently supported.\n", start_off);
                     }
                     else
                     {
                       lower_bound = lower_bound_attrib->val.u128.u64[0];
                     }
                   }
                   else switch(unit_parse_ctx->language)
                   {
                   default:{}break;
#define X(name, code, default_array_lower_bound) case DW_Language_##name:{lower_bound = (default_array_lower_bound);}break;
                                                                                    DW_Language_XList
#undef X
                   }

                   // rjf: resolve upper bound
                   U64 upper_bound = 0;
                   {
                     if(count_attrib != &dw2_attrib_nil)
                     {
                       upper_bound = lower_bound + count_attrib->val.u128.u64[0];
                     }
                     else if(upper_bound_attrib != &dw2_attrib_nil)
                     {
                       if(dw_attrib_class_from_form_kind(unit_parse_ctx->version, unit_parse_ctx->exts, upper_bound_attrib->val.kind) & DW_AttribClass_Reference)
                       {
                         log_infof("[.debug_info@0x%I64x] Array type upper bound is a variable; this is not currently supported.\n", start_off);
                       }
                       else
                       {
                         upper_bound = upper_bound_attrib->val.u128.u64[0];
                         upper_bound += 1; // NOTE(rjf): turn to exclusive range
                       }
                     }
                   }

                   // rjf: push node
                   ArrayDimensionNode *dim_n = push_array(temp.arena, ArrayDimensionNode, 1);
                   SLLStackPush(top_dimension, dim_n);
                   dim_n->count = (upper_bound - lower_bound);
                 }
                 if(child_tag.has_children)
                 {
                   depth += 1;
                 }
                 else if(child_tag.kind == DW_TagKind_Null)
                 {
                   depth -= 1;
                 }
                 if(off == start_off)
                 {
                   break;
                 }
                }
              }

              // rjf: create array type operators for each dimension
              RDIM_Type *array_direct_type = direct_type;
              for EachNode(dim_n, ArrayDimensionNode, top_dimension)
              {
                RDIM_Type *array_type = rdim_type_chunk_list_push(arena, &lane_types, lane_types_chunk_count);
                array_type->kind = RDI_TypeKind_Array;
                array_type->byte_size = array_direct_type ? array_direct_type->byte_size*dim_n->count : 0;
                array_type->direct_type = array_direct_type;
                array_direct_type = array_type;
              }

              // rjf: final destination type -> the final array dimension type
              dst_type = array_direct_type;
            }break;
          }

          // rjf: store type in type-from-idx table
          type_from_idx_map[type_idx] = dst_type;

          temp_end(temp);
        }

        //- rjf: combine all types from all lanes
        RDIM_TypeChunkList *lanes_types = 0;
        if(lane_idx() == 0)
        {
          lanes_types = push_array(scratch2.arena, RDIM_TypeChunkList, lane_count());
        }
        lane_sync_u64(&lanes_types, 0);
        lanes_types[lane_idx()] = lane_types;
        lane_sync();
        if(lane_idx() == 0)
        {
          RDIM_TypeChunkList pass_lane_combined_types = {0};
          for EachIndex(l_idx, lane_count())
          {
            rdim_type_chunk_list_concat_in_place(&pass_lane_combined_types, &lanes_types[l_idx]);
          }
          rdim_type_chunk_list_concat_in_place(all_types, &pass_lane_combined_types);
        }
        lane_sync();

        //- rjf: find minimum next max chain count across all lanes, for next iteration
        U64 *lane_next_max_chain_counts = 0;
        if(lane_idx() == 0)
        {
          lane_next_max_chain_counts = push_array(scratch2.arena, U64, lane_count());
        }
        lane_sync_u64(&lane_next_max_chain_counts, 0);
        lane_next_max_chain_counts[lane_idx()] = next_max_chain_count;
        lane_sync();
        if(lane_idx() == 0)
        {
          for EachIndex(l_idx, lane_count())
          {
            next_max_chain_count = Min(next_max_chain_count, lane_next_max_chain_counts[l_idx]);
          }
        }
        lane_sync_u64(&next_max_chain_count, 0);

        //- rjf: update max chain count
        max_chain_count = next_max_chain_count;

        ProfEnd();
        scratch_end(scratch2);
      }
    }
    lane_sync();

    ////////////////////////////
    //- rjf: convert all namespaces
    //
    {
      // rjf: set up
      if(lane_idx() == 0)
      {
        all_namespaces = push_array(scratch.arena, RDIM_NamespaceChunkList, 1);
        namespace_from_idx_map = push_array(scratch.arena, RDIM_Namespace *, namespace_count);
      }
      lane_sync_u64(&all_namespaces, 0);
      lane_sync_u64(&namespace_from_idx_map, 0);

      // rjf: build all namespaces on this lane
      RDIM_NamespaceChunkList lane_namespaces = {0};
      Rng1U64 range = lane_range(namespace_count);
      for EachInRange(namespace_idx, range)
      {
        Temp scratch2 = scratch_begin(&scratch.arena, 1);

        // rjf: unpack tag node
        D2R_UniqueTagNode *tag_node = namespace_tag_nodes[namespace_idx];
        U64 info_off = tag_node->info_off;
        U64 unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, info_off);
        U64 unit_idx = unit_num > 0 ? unit_num-1 : 0;
        DW2_ParseCtx *unit_parse_ctx = &unit_parse_ctxs[unit_idx];

        // rjf: read namespace tag
        DW2_Tag tag = {0};
        dw2_read_tag(scratch2.arena, raw, unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, info_off, &tag);
        DW2_Attrib *name_attrib = dw2_attrib_from_kind(&tag, DW_AttribKind_Name);
        String8 name = name_attrib->val.string;

        // rjf: build namespace
        RDIM_Namespace *ns = rdim_namespace_chunk_list_push(arena, &lane_namespaces, 64);
        ns->name = name;

        // rjf: store in (deduped tag idx -> namespace) table
        namespace_from_idx_map[tag_node->order_idx] = ns;

        scratch_end(scratch2);
      }
      lane_sync();

      // rjf: join all namespaces
      {
        Temp scratch2 = scratch_begin(&scratch.arena, 1);
        RDIM_NamespaceChunkList *lanes_namespaces = 0;
        if(lane_idx() == 0)
        {
          lanes_namespaces = push_array(scratch2.arena, RDIM_NamespaceChunkList, lane_count());
        }
        lane_sync_u64(&lanes_namespaces, 0);
        lanes_namespaces[lane_idx()] = lane_namespaces;
        lane_sync();
        if(lane_idx() == 0)
        {
          for EachIndex(l_idx, lane_count())
          {
            rdim_namespace_chunk_list_concat_in_place(all_namespaces, &lanes_namespaces[l_idx]);
          }
        }
        lane_sync();
        scratch_end(scratch2);
      }
    }
    lane_sync();

    ////////////////////////////
    //- rjf: convert all UDTs
    //
    ProfScope("convert all UDTs")
    {
      //- rjf: produce UDTs across all lanes
      U64 chunk_count = 512;
      RDIM_UDTChunkList lane_udts = {0};
      Rng1U64 range = lane_range(type_count);
      for EachInRange(type_idx, range)
      {
        RDIM_Type *type = type_from_idx_map[type_idx];
        if(type != 0 &&
           (type->kind == RDI_TypeKind_Struct ||
            type->kind == RDI_TypeKind_Union ||
            type->kind == RDI_TypeKind_Class ||
            type->kind == RDI_TypeKind_Enum ||
            type->kind == RDI_TypeKind_Alias))
        {
          // rjf: produce UDT
          RDIM_UDT *udt = rdim_udt_chunk_list_push(arena, &lane_udts, chunk_count);
          type->udt = udt;
          udt->self_type = type;

          // rjf: idx -> type tag node
          D2R_UniqueTagNode *type_tag_node = type_tag_nodes[type_idx];
          U64 info_off = type_tag_node->info_off;
          U64 unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, info_off);
          U64 unit_idx = unit_num > 0 ? unit_num-1 : 0;

          // rjf: unpack unit
          Rng1U64 unit_info_tag_range = unit_info_tag_ranges[unit_idx];
          DW2_ParseCtx *unit_parse_ctx = &unit_parse_ctxs[unit_idx];

          // rjf: parse all tags
          S64 depth = 0;
          for(U64 off = info_off; contains_1u64(unit_info_tag_range, off) && (depth > 0 || off == info_off);)
          {
            Temp scratch2 = scratch_begin(&scratch.arena, 1);
            U64 start_off = off;

            // rjf: parse tag
            DW2_Tag tag = {0};
            U64 tag_parse_off = off;
            off += dw2_read_tag(scratch2.arena, raw, unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, off, &tag);

            // rjf: unpack tag attributes
            DW2_Attrib *name_attrib = &dw2_attrib_nil;
            DW2_Attrib *type_attrib = &dw2_attrib_nil;
            DW2_Attrib *off_attrib = &dw2_attrib_nil;
            DW2_Attrib *val_attrib = &dw2_attrib_nil;
            DW2_Attrib *declfile_attrib = &dw2_attrib_nil;
            DW2_Attrib *declline_attrib = &dw2_attrib_nil;
            DW2_Attrib *declcol_attrib = &dw2_attrib_nil;
            for EachNode(n, DW2_AttribNode, tag.attribs.first)
            {
              switch(n->v.attrib_kind)
              {
              default:{}break;
#define Case(dst, src) case DW_AttribKind_##src:{dst##_attrib = &n->v;}break
                      Case(name,     Name);
                      Case(type,     Type);
                      Case(off,      DataMemberLocation);
                      Case(val,      ConstValue);
                      Case(declfile, DeclFile);
                      Case(declline, DeclLine);
                      Case(declcol,  DeclColumn);
#undef Case
              }
            }

            // rjf: unpack basic attributes
            String8 name = name_attrib->val.string;
            U64 off = off_attrib->val.u128.u64[0];
            U64 val = val_attrib->val.u128.u64[0];

            // rjf: unpack type
            RDIM_Type *type = 0;
            if(type_attrib != &dw2_attrib_nil)
            {
              // rjf: attrib -> info off / unit idx
              U64 type_info_off = dw2_reference_info_off_from_form_val(unit_parse_ctx, &type_attrib->val);
              U64 type_unit_idx = unit_idx;
              if(!contains_1u64(unit_info_tag_range, type_info_off))
              {
                U64 new_unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, type_info_off);
                type_unit_idx = (new_unit_num > 0 ? new_unit_num-1 : unit_idx);
              }

              // rjf: (info_off, unit_idx) -> hash
              U64 type_hash = 0;
              {
                D2R_UnitDedupedTagMap *unit_deduped_tag_map = &unit_deduped_tag_maps[type_unit_idx];
                U64 info_off_hash = u64_hash_from_str8(str8_struct(&type_info_off));
                U64 info_off_slot_idx = info_off_hash%unit_deduped_tag_map->slots_count;
                for(D2R_UnitDedupedTagNode *n = unit_deduped_tag_map->slots[info_off_slot_idx]; n != 0; n = n->next)
                {
                  if(n->src_info_off == type_info_off)
                  {
                    type_hash = n->dst_hash;
                    break;
                  }
                }
              }

              // rjf: hash -> type
              {
                U64 unique_type_tag_slot_idx = type_hash%unique_tag_slots_count;
                for(D2R_UniqueTagNode *n = unique_tag_slots[unique_type_tag_slot_idx]; n != 0; n = n->next)
                {
                  if(n->hash == type_hash)
                  {
                    type = type_from_idx_map[n->order_idx];
                    break;
                  }
                }
              }
            }

            // rjf: root-level tag -> collect decl file/line info
            if(tag_parse_off == start_off)
            {
              // TODO(rjf): we need to have gathered the source files before this!!!!!!
              // currently, they only come from line info - we need to gather/dedup the
              // ones that also come from tags.
            }

            // rjf: gather children
            switch(tag.kind)
            {
            default:{}break;
            case DW_TagKind_Member:
                    {
                      RDIM_UDTMember *member = rdim_udt_push_member(arena, &lane_udts, udt);
                      member->kind = RDI_MemberKind_DataField;
                      member->name = name;
                      member->type = type;
                      member->off = (RDI_U32)off; // TODO(rjf): @u64_to_u32
                    }break;
            case DW_TagKind_Enumerator:
                    {
                      RDIM_UDTEnumVal *enum_val = rdim_udt_push_enum_val(arena, &lane_udts, udt);
                      enum_val->name = name;
                      enum_val->val = val;
                    }break;
            }

            // rjf: tree nav
            if(tag.kind == DW_TagKind_Null)
            {
              depth -= 1;
            }
            if(tag.has_children)
            {
              depth += 1;
            }

            scratch_end(scratch2);
            if(off == start_off)
            {
              break;
            }
          }
        }
      }
      lane_sync();

      //- rjf: combine all lanes
      RDIM_UDTChunkList *lanes_udts = 0;
      if(lane_idx() == 0)
      {
        lanes_udts = push_array(scratch.arena, RDIM_UDTChunkList, lane_count());
      }
      lane_sync_u64(&lanes_udts, 0);
      lanes_udts[lane_idx()] = lane_udts;
      lane_sync();
      if(lane_idx() == 0)
      {
        all_udts = push_array(scratch.arena, RDIM_UDTChunkList, 1);
        for EachIndex(l_idx, lane_count())
        {
          rdim_udt_chunk_list_concat_in_place(all_udts, &lanes_udts[l_idx]);
        }
      }
      lane_sync_u64(&all_udts, 0);
    }
  }
  
  ////////////////////////////
  //- rjf: convert all units
  //
  RDIM_UnitChunkList *all_units = 0;
  RDIM_Unit **unit_from_idx_map = 0;
  ProfScope("convert all units")
  {
    if(lane_idx() == 0)
    {
      all_units = push_array(scratch.arena, RDIM_UnitChunkList, 1);
      unit_from_idx_map = push_array(scratch.arena, RDIM_Unit *, unit_count);
      for EachIndex(unit_idx, unit_count)
      {
        unit_from_idx_map[unit_idx] = rdim_unit_chunk_list_push(arena, all_units, unit_count);
      }
    }
    lane_sync_u64(&all_units, 0);
    lane_sync_u64(&unit_from_idx_map, 0);
    U64 unit_take_idx_ = 0;
    U64 *unit_take_idx_ptr = &unit_take_idx_;
    lane_sync_u64(&unit_take_idx_ptr, 0);
    for(;;)
    {
      U64 unit_idx = ins_atomic_u64_inc_eval(unit_take_idx_ptr) - 1;
      if(unit_idx >= unit_count)
      {
        break;
      }
      
      //- rjf: unpack unit info
      DW2_ParseCtx *unit_parse_ctx = &unit_parse_ctxs[unit_idx];
      Rng1U64 unit_info_tag_range = unit_info_tag_ranges[unit_idx];
      RDIM_Unit *dst_unit = unit_from_idx_map[unit_idx];
      
      //- rjf: unpack unit's root tag
      DW2_Tag *unit_root_tag = &unit_root_tags[unit_idx];
      DW2_Attrib *name_attrib = &dw2_attrib_nil;
      DW2_Attrib *comp_dir_attrib = &dw2_attrib_nil;
      DW2_Attrib *producer_attrib = &dw2_attrib_nil;
      DW2_Attrib *lang_attrib = &dw2_attrib_nil;
      DW2_Attrib *ranges_attrib = &dw2_attrib_nil;
      DW2_Attrib *lopc_attrib = &dw2_attrib_nil;
      DW2_Attrib *hipc_attrib = &dw2_attrib_nil;
      for EachNode(n, DW2_AttribNode, unit_root_tag->attribs.first)
      {
        switch(n->v.attrib_kind)
        {
          default:{}break;
#define Case(dst, src) case DW_AttribKind_##src:{dst##_attrib = &n->v;}break
          Case(name,     Name);
          Case(comp_dir, CompDir);
          Case(producer, Producer);
          Case(lang,     Language);
          Case(ranges,   Ranges);
          Case(lopc,     LowPc);
          Case(hipc,     HighPc);
#undef Case
        }
      }
      
      //- rjf: unpack attributes
      String8 unit_name = name_attrib->val.string;
      String8 unit_comp_dir = comp_dir_attrib->val.string;
      String8 unit_producer = producer_attrib->val.string;
      RDI_Language unit_lang = RDI_Language_NULL;
      {
        DW_Language dw_lang = lang_attrib->val.u128.u64[0];
        switch(dw_lang)
        {
          default:{}break;
          case DW_Language_C89:
          case DW_Language_C99:
          case DW_Language_C11:
          case DW_Language_C:
          {
            unit_lang = RDI_Language_C;
          }break;
          case DW_Language_CPlusPlus03:
          case DW_Language_CPlusPlus11:
          case DW_Language_CPlusPlus14:
          case DW_Language_CPlusPlus:
          {
            unit_lang = RDI_Language_CPlusPlus;
          }break;
        }
      }
      
      //- rjf: get unit's ranges from .debug_aranges parse artifacts, if we have them
      RDIM_Rng1U64ChunkList unit_voff_ranges = {0};
      if(arange_unit_from_info_off_map_slots_count != 0)
      {
        U64 unit_info_off = unit_info_ranges->v[unit_idx].min;
        U64 hash = u64_hash_from_str8(str8_struct(&unit_info_off));
        U64 slot_idx = hash%arange_unit_from_info_off_map_slots_count;
        for(D2R_ARangeUnitNode *n = arange_unit_from_info_off_map_slots[slot_idx]; n != 0; n = n->next)
        {
          if(n->info_off == unit_info_off)
          {
            unit_voff_ranges = n->ranges[0];
            break;
          }
        }
      }
      
      //- rjf: if we have no voff ranges from .debug_aranges, then we need to extract
      // this info from the unit root rag instead (via ranges & low-pc/high-pc tags)
      if(unit_voff_ranges.total_count == 0)
      {
        // rjf: gather ranges from a ranges attribute
        if(ranges_attrib != &dw2_attrib_nil)
        {
          Rng1U64List ranges = dw2_rnglist_from_form_val(scratch.arena, unit_parse_ctx, raw, ranges_attrib->val);
          for EachNode(n, Rng1U64Node, ranges.first)
          {
            rdim_rng1u64_chunk_list_push(arena, &unit_voff_ranges, 256, (RDIM_Rng1U64){n->v.min - base_vaddr, n->v.max - base_vaddr});
          }
        }
        
        // rjf: gather contiguous range from low-pc / high-pc attribute
        if(lopc_attrib != &dw2_attrib_nil && hipc_attrib != &dw2_attrib_nil)
        {
          U64 voff_base = lopc_attrib->val.addr - base_vaddr;
          U64 voff_opl = 0;
          if(dw_attrib_class_from_form_kind(unit_parse_ctx->version, unit_parse_ctx->exts, hipc_attrib->val.kind) & DW_AttribClass_Address)
          {
            voff_opl = voff_base + hipc_attrib->val.u128.u64[0];
          }
          else
          {
            voff_opl = hipc_attrib->val.addr;
          }
          rdim_rng1u64_chunk_list_push(arena, &unit_voff_ranges, 256, (RDIM_Rng1U64){voff_base, voff_opl});
        }
      }
      
      //- rjf: fill top-level unit info
      {
        dst_unit->unit_name     = unit_name;
        dst_unit->compiler_name = unit_producer;
        // TODO(rjf): dst_unit->source_file   = ???;
        // TODO(rjf): dst_unit->object_file   = ???;
        // TODO(rjf): dst_unit->archive_file  = ???;
        dst_unit->build_path    = unit_comp_dir;
        dst_unit->language      = unit_lang;
        dst_unit->line_table    = make_line_tables ? unit_line_tables[unit_idx] : 0;
        dst_unit->voff_ranges   = unit_voff_ranges;
      }
    }
    lane_sync();
  }
  
  ////////////////////////////
  //- rjf: convert all symbols
  //
  typedef struct D2R_ScopeContainerNode D2R_ScopeContainerNode;
  struct D2R_ScopeContainerNode
  {
    D2R_ScopeContainerNode *next;
    U64 info_off;
    RDIM_Scope *scope;
  };
  typedef struct D2R_ScopeContainerMap D2R_ScopeContainerMap;
  struct D2R_ScopeContainerMap
  {
    D2R_ScopeContainerNode **slots;
    U64 slots_count;
  };
  typedef struct D2R_SubUnitWorkArtifacts D2R_SubUnitWorkArtifacts;
  struct D2R_SubUnitWorkArtifacts
  {
    RDIM_SymbolChunkList global_variables;
    RDIM_SymbolChunkList thread_variables;
    RDIM_SymbolChunkList constants;
    RDIM_SymbolChunkList procedures;
    RDIM_ScopeChunkList scopes;
    RDIM_InlineSiteChunkList inline_sites;
    RDIM_NamespaceChunkList namespaces;
    D2R_NameNode *first_name_node;
    D2R_NameNode *last_name_node;
    U64 name_node_count;
  };
  D2R_ScopeContainerMap *scope_container_map = 0;
  D2R_SubUnitWorkArtifacts *sub_unit_work_artifacts = 0;
  ProfScope("convert all symbols")
  {
    typedef struct D2R_ParentNode D2R_ParentNode;
    struct D2R_ParentNode
    {
      D2R_ParentNode        *next;
      DW_TagKind             tag_kind;
      U64                    info_off;
      RDIM_Scope            *scope;
      RDIM_Type             *container_type;
      RDIM_Namespace        *container_namespace;
      D2R_NameNode          *name_context;
      D2R_ParentNode        *name_pending_next;
      String8                name;
      U64                    name_ref_offs[3];
      RDIM_LocationCaseList  framebase_location_cases;
    };
    D2R_ParentNode *free_parent = 0;

    if(lane_idx() == 0)
    {
      if(make_types && make_scopes)
      {
        scope_container_map = push_array(scratch.arena, D2R_ScopeContainerMap, 1);
        scope_container_map->slots_count = total_tag_count_estimate/8 + 1;
        scope_container_map->slots = push_array(scratch.arena, D2R_ScopeContainerNode *, scope_container_map->slots_count);
      }
      sub_unit_work_artifacts = push_array(scratch.arena, D2R_SubUnitWorkArtifacts, sub_unit_works_count);
    }
    lane_sync_u64(&scope_container_map, 0);
    lane_sync_u64(&sub_unit_work_artifacts, 0);
    U64 work_take_idx_ = 0;
    U64 *work_take_idx_ptr = &work_take_idx_;
    lane_sync_u64(&work_take_idx_ptr, 0);
    for(;;)
    {
      U64 work_idx = ins_atomic_u64_inc_eval(work_take_idx_ptr) - 1;
      if(work_idx >= sub_unit_works_count)
      {
        break;
      }
      
      //- rjf: unpack work
      U64 unit_idx = sub_unit_works[work_idx].unit_idx;
      Rng1U64 root_tag_idx_range = sub_unit_works[work_idx].root_tag_idx_range;
      D2R_SubUnitWorkArtifacts *dst_artifacts = &sub_unit_work_artifacts[work_idx];
      
      //- rjf: unpack unit info
      DW2_ParseCtx *unit_parse_ctx = &unit_parse_ctxs[unit_idx];
      Rng1U64 unit_info_tag_range = unit_info_tag_ranges[unit_idx];
      
      //- rjf: produce all unit symbols in this work
      U64 chunk_count = 256;
      for EachInRange(root_tag_idx, root_tag_idx_range)
      {
        D2R_ParentNode *top_parent         = 0;
        U64             root_tag_start_off = unit_info_root_tag_offs[unit_idx].v[root_tag_idx];
        S64             depth              = 1;
        Temp            scratch2           = scratch_begin(&scratch.arena, 1);
        Temp            temp               = temp_begin(scratch2.arena);
        for(U64 off = root_tag_start_off;
            off < unit_info_tag_range.max && (depth > 1 || off == root_tag_start_off); )
        {
          temp_end(temp);
          temp = temp_begin(scratch2.arena);

          U64 start_off = off;
          
          ////////////////////////
          //- rjf: parse tag
          //
          DW2_Tag tag = {0};
          off += dw2_read_tag(temp.arena, raw, unit_parse_ctx, raw->sec[DW_SectionKind_Info].data, off, &tag);

          D2R_CommonAttribs attribs;
          d2r_collect_tag_attributes(temp.arena, &attribs, raw, unit_info_tag_ranges_array, unit_parse_ctxs, tag);

          // declare attributes
#define X(id, kind, ...) \
          DW2_Attrib *id##_attrib = attribs.v[D2R_CommonAttrib_##kind]; \
          id##_attrib = id##_attrib ? id##_attrib : &dw2_attrib_nil;
          D2R_CommonAttrib_XList
#undef X
          
          ////////////////////////
          //- rjf: unpack basic attributes
          //
          String8    name        = name_attrib->val.string;
          String8    link_name   = linkname_attrib->val.string;
          DW_InlKind inl_kind    = (DW_InlKind)inline_attrib->val.u128.u64[0];
          B32        is_external = external_attrib->val.u128.u64[0] != 0;
          B32        is_decl     = decl_attrib->val.u128.u64[0] != 0;
          
          ////////////////////////
          //- rjf: unpack ranges
          //
          RDIM_Rng1U64List ranges = {0};
          if(!prune_dead_output ||
             (tag.kind == DW_TagKind_SubProgram && !is_decl) || (tag.kind == DW_TagKind_LexicalBlock && top_parent != 0 && top_parent->scope != 0))
          {
            Temp range_temp = temp_begin(temp.arena);

            Rng1U64List tag_ranges = {0};

            if (ranges_attrib != &dw2_attrib_nil) {
              tag_ranges = dw2_rnglist_from_form_val(range_temp.arena, unit_parse_ctx, raw, ranges_attrib->val);
            }

            if (lopc_attrib != &dw2_attrib_nil && hipc_attrib != &dw2_attrib_nil) {
              U64 vaddr_min = lopc_attrib->val.addr;
              U64 vaddr_opl = 0;
              if (dw_attrib_class_from_form_kind(unit_parse_ctx->version, unit_parse_ctx->exts, hipc_attrib->val.kind) & DW_AttribClass_Address) {
                vaddr_opl = hipc_attrib->val.addr;
              } else {
                U64 size = hipc_attrib->val.u128.u64[0];
                if (size <= max_U64 - vaddr_min) {
                  vaddr_opl = vaddr_min + size;
                }
              }
              rng1u64_list_push(range_temp.arena, &tag_ranges, (Rng1U64){vaddr_min, vaddr_opl});
            }

            for EachNode(n, Rng1U64Node, tag_ranges.first) {
              // discard linker tombstones and invalid absolute ranges before rebasing
              if (n->v.min == 0 || n->v.min < base_vaddr || n->v.min >= n->v.max ||
                  (binary_sections.count != 0 &&
                   (n->v.min < acceptable_vaddr_range.min || n->v.max > acceptable_vaddr_range.max))) {
                continue;
              }
              rdim_rng1u64_list_push(arena, &ranges, (RDIM_Rng1U64){n->v.min - base_vaddr, n->v.max - base_vaddr});
            }

            temp_end(range_temp);
          }
          
          ////////////////////////
          //- rjf: unpack type
          //
          RDIM_Type *type = 0;
          if(make_types)
          {
            if(type_attrib != &dw2_attrib_nil || tag.kind == DW_TagKind_SubProgram)
            {
              // rjf: functions need to get their type info from themselves; the type attrib
              // only references the return type. so, in that case, we'll use the procedure's
              // own info offset / unit index, rather than the type attribute's.
              U64 type_info_off = start_off;
              U64 type_unit_idx = unit_idx;

              // rjf: attrib -> (info_off, unit_idx)
              if(tag.kind != DW_TagKind_SubProgram)
              {
                type_info_off = dw2_reference_info_off_from_form_val(unit_parse_ctx, &type_attrib->val);
                type_unit_idx = unit_idx;
                if(!contains_1u64(unit_info_tag_range, type_info_off))
                {
                  U64 new_unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, type_info_off);
                  type_unit_idx = (new_unit_num > 0 ? new_unit_num-1 : unit_idx);
                }
              }

              // rjf: (info_off, unit_idx) -> hash
              U64 type_hash = 0;
              {
                D2R_UnitDedupedTagMap *unit_deduped_tag_map = &unit_deduped_tag_maps[type_unit_idx];
                U64 info_off_hash = u64_hash_from_str8(str8_struct(&type_info_off));
                U64 info_off_slot_idx = info_off_hash%unit_deduped_tag_map->slots_count;
                for(D2R_UnitDedupedTagNode *n = unit_deduped_tag_map->slots[info_off_slot_idx]; n != 0; n = n->next)
                {
                  if(n->src_info_off == type_info_off)
                  {
                    type_hash = n->dst_hash;
                    break;
                  }
                }
              }

              // rjf: hash -> type
              {
                U64 unique_type_tag_slot_idx = type_hash%unique_tag_slots_count;
                for(D2R_UniqueTagNode *n = unique_tag_slots[unique_type_tag_slot_idx]; n != 0; n = n->next)
                {
                  if(n->hash == type_hash)
                  {
                    type = type_from_idx_map[n->order_idx];
                    break;
                  }
                }
              }
            }
          }
          
          ////////////////////////
          //- rjf: unpack location info
          //
          RDIM_LocationCaseList location_cases = {0};
          RDIM_LocationCaseList framebase_location_cases = {0};
          B32 location_is_tls_dependent = 0;
          B32 framebase_location_is_tls_dependent = 0;
          if(make_locals || make_global_vars || make_thread_vars)
          {
            struct
            {
              DW2_Attrib *attrib;
              RDIM_LocationCaseList *dst_locations;
              B32 *dst_is_tls_dependent;
            }
            tasks[] =
            {
              {location_attrib, &location_cases, &location_is_tls_dependent},
              {framebase_attrib, &framebase_location_cases, &framebase_location_is_tls_dependent},
            };
            for EachElement(task_idx, tasks)
            {
              if(tasks[task_idx].attrib == &dw2_attrib_nil)
              {
                continue;
              }
              DW2_Attrib *attrib = tasks[task_idx].attrib;
              RDIM_LocationCaseList *dst_locations = tasks[task_idx].dst_locations;
              B32 *dst_is_tls_dependent = tasks[task_idx].dst_is_tls_dependent;
              
              //////////////////////
              //- rjf: gather DWARF location exprs
              //
              DW2_LocList locs = {0};
              switch(attrib->val.kind)
              {
                default:{}break;
                case DW_FormKind_ExprLoc:
                {
                  U64 expr_info_off  = attrib->val.u128.u64[0];
                  U64 expr_info_size = attrib->val.u128.u64[1];
                  String8 expr = str8_substr(raw->sec[DW_SectionKind_Info].data, r1u64(expr_info_off, expr_info_off+expr_info_size));
                  DW2_LocNode *n = push_array(temp.arena, DW2_LocNode, 1);
                  n->v.expr = expr;
                  n->v.range = r1u64(0, max_U64);
                  SLLQueuePush(locs.first, locs.last, n);
                  locs.count += 1;
                }break;
                case DW_FormKind_LocListx:   // NOTE(rjf): dwarf5+, loclistx -> location list offset table
                case DW_FormKind_SecOffset:  // NOTE(rjf): pre-dwarf5, location section offset -> a location list
                {
                  locs = dw2_loclist_from_form_val(temp.arena, unit_parse_ctx, raw, attrib->val);
                }break;
              }
              
              //////////////////////
              //- rjf: convert DWARF location exprs -> RDIM locations
              //
              // we do this for each possible frame base location case - locations in DWARF
              // refer to the frame base via a special op. in our case, we want to bake that
              // into the location directly to reduce extra context for evaluation.
              //
              // if we find that a location doesn't rely on the frame base at all, we just
              // skip the rest.
              //
              B32 is_tls_dependent = 0;
              for EachNode(n, DW2_LocNode, locs.first)
              {
                //- rjf: unpack location
                Rng1U64 range = n->v.range;
                String8 expr = n->v.expr;
                
                //- rjf: iterate each frame base location case, or once if there are none,
                // and convert the location in that context
                RDIM_LocationCase nil_framebase_loc_case = {0, {0}, {0, max_U64}};
                for(RDIM_LocationCase *framebase_loc_n = top_parent ? top_parent->framebase_location_cases.first : &nil_framebase_loc_case;
                    framebase_loc_n != 0;
                    framebase_loc_n = framebase_loc_n->next)
                {
                  //- rjf: unpack framebase location
                  RDIM_Location framebase_loc = framebase_loc_n->location;
                  RDIM_Rng1U64 framebase_voff_range = framebase_loc_n->voff_range;
                  
                  //- rjf: set up type stack for type-evaluating bytecode
                  typedef struct D2R_ExprVal D2R_ExprVal;
                  struct D2R_ExprVal
                  {
                    RDI_TypeKind type_kind;
                    B32 is_addr;
                  };
                  typedef struct D2R_ExprValNode D2R_ExprValNode;
                  struct D2R_ExprValNode
                  {
                    D2R_ExprValNode *next;
                    D2R_ExprVal v;
                  };
                  D2R_ExprValNode *val_stack_top = 0;
                  D2R_ExprValNode *free_val = 0;
                  
                  //- rjf: process DWARF bytecode; produce RDI bytecode + mini-type-info for the expression's result
                  typedef struct JumpOpNode JumpOpNode;
                  struct JumpOpNode
                  {
                    JumpOpNode *next;
                    RDIM_EvalBytecodeOp *op;
                    S64 inst_delta;
                  };
                  JumpOpNode *first_jump_op = 0;
                  JumpOpNode *last_jump_op = 0;
                  RDIM_EvalBytecode dst_bytecode = {0};
                  B32 dst_bytecode_is_good = 1;
                  B32 bytecode_is_framebase_dependent = 0;
                  for(U64 expr_off = 0; expr_off < expr.size;)
                  {
                    U64 start_expr_off = expr_off;
                    
                    //- rjf: read next opcode
                    DW_ExprOp opcode = 0;
                    expr_off += str8_deserial_read_struct(expr, expr_off, &opcode);
                    
                    //- rjf: unpack opcode
                    DW_ExprOpInfo *opcode_info = dw_info_from_expr_op(unit_parse_ctx->version, unit_parse_ctx->exts, opcode);
                    U64 operands_count = opcode_info->operand_count;
                    DW_ExprOperandKind operands_kinds[2] = {opcode_info->operand_kinds[0], opcode_info->operand_kinds[1]};
                    U64 push_count = opcode_info->push_count;
                    U64 pop_count = opcode_info->pop_count;
                    
                    //- rjf: read op operands
                    U64 operand_u64s[2] = {0};
                    S64 operand_s64s[2] = {0};
                    String8 operand_str8s[2] = {0};
                    for EachIndex(operand_idx, operands_count)
                    {
                      switch(operands_kinds[operand_idx])
                      {
                        case DW_ExprOperandKind_Null:
                        default:{}break;
                        case DW_ExprOperandKind_U8:        {expr_off += str8_deserial_read(expr, expr_off, &operand_u64s[operand_idx], 1, 1);}break;
                        case DW_ExprOperandKind_U16:       {expr_off += str8_deserial_read(expr, expr_off, &operand_u64s[operand_idx], 2, 2);}break;
                        case DW_ExprOperandKind_U32:       {expr_off += str8_deserial_read(expr, expr_off, &operand_u64s[operand_idx], 4, 4);}break;
                        case DW_ExprOperandKind_U64:       {expr_off += str8_deserial_read(expr, expr_off, &operand_u64s[operand_idx], 8, 8);}break;
                        case DW_ExprOperandKind_S8:        {expr_off += str8_deserial_read(expr, expr_off, &operand_s64s[operand_idx], 1, 1);}break;
                        case DW_ExprOperandKind_S16:       {expr_off += str8_deserial_read(expr, expr_off, &operand_s64s[operand_idx], 2, 2);}break;
                        case DW_ExprOperandKind_S32:       {expr_off += str8_deserial_read(expr, expr_off, &operand_s64s[operand_idx], 4, 4);}break;
                        case DW_ExprOperandKind_S64:       {expr_off += str8_deserial_read(expr, expr_off, &operand_s64s[operand_idx], 8, 8);}break;
                        case DW_ExprOperandKind_ULEB128:   {expr_off += str8_deserial_read_uleb128(expr, expr_off, &operand_u64s[operand_idx]);} break;
                        case DW_ExprOperandKind_SLEB128:   {expr_off += str8_deserial_read_sleb128(expr, expr_off, &operand_s64s[operand_idx]);} break;
                        case DW_ExprOperandKind_Addr:      {expr_off += str8_deserial_read(expr, expr_off, &operand_u64s[operand_idx], unit_parse_ctx->addr_size, unit_parse_ctx->addr_size);}break;
                        case DW_ExprOperandKind_DwarfUInt: {expr_off += dw2_read_fmt_u64(expr, expr_off, unit_parse_ctx->format, &operand_u64s[operand_idx]);}break;
                        case DW_ExprOperandKind_Block:
                        {
                          U8 block_size = 0;
                          expr_off += str8_deserial_read_struct(expr, expr_off, &block_size);
                          operand_str8s[operand_idx] = str8_substr(expr, r1u64(expr_off, expr_off+block_size));
                          expr_off += block_size;
                        }break;
                      }
                    }
                    
                    //- rjf: pop stack values
                    D2R_ExprVal popped_vals[2] = {0};
                    {
                      pop_count = Min(pop_count, ArrayCount(popped_vals));
                      for EachIndex(pop_idx, pop_count)
                      {
                        if(val_stack_top != 0)
                        {
                          D2R_ExprValNode *popped = val_stack_top;
                          popped_vals[pop_idx] = popped->v;
                          SLLStackPop(val_stack_top);
                          SLLStackPush(free_val, popped);
                        }
                      }
                    }
                    
                    //- rjf: exec op (produce RDI bytecode, + manipulate value stack)
                    D2R_ExprVal push_vals[2] = {0};
                    U64 regcode_dw = 0;
                    S64 regval_off = 0;
                    B32 regread_is_addr = 0;
                    U64 target_pick_val_idx = 0;
                    RDI_EvalOp rdi_eval_op = 0;
                    U64 memread_size = 0;
                    switch(opcode)
                    {
                      default:{}break;
                      
                      //- rjf: small opcode-embedded unsigned literals
                      case DW_ExprOp_Lit0:  case DW_ExprOp_Lit1:  case DW_ExprOp_Lit2:
                      case DW_ExprOp_Lit3:  case DW_ExprOp_Lit4:  case DW_ExprOp_Lit5:
                      case DW_ExprOp_Lit6:  case DW_ExprOp_Lit7:  case DW_ExprOp_Lit8:
                      case DW_ExprOp_Lit9:  case DW_ExprOp_Lit10: case DW_ExprOp_Lit11:
                      case DW_ExprOp_Lit12: case DW_ExprOp_Lit13: case DW_ExprOp_Lit14:
                      case DW_ExprOp_Lit15: case DW_ExprOp_Lit16: case DW_ExprOp_Lit17:
                      case DW_ExprOp_Lit18: case DW_ExprOp_Lit19: case DW_ExprOp_Lit20:
                      case DW_ExprOp_Lit21: case DW_ExprOp_Lit22: case DW_ExprOp_Lit23:
                      case DW_ExprOp_Lit24: case DW_ExprOp_Lit25: case DW_ExprOp_Lit26:
                      case DW_ExprOp_Lit27: case DW_ExprOp_Lit28: case DW_ExprOp_Lit29:
                      case DW_ExprOp_Lit30: case DW_ExprOp_Lit31:
                      {
                        U64 lit_val = (U64)(opcode - DW_ExprOp_Lit0);
                        rdim_bytecode_push_uconst(arena, &dst_bytecode, lit_val);
                      }break;
                      
                      //- rjf: small unsigned literals
                      case DW_ExprOp_Const1U: push_vals[0].type_kind = RDI_TypeKind_U8;  goto const_u;
                      case DW_ExprOp_Const2U: push_vals[0].type_kind = RDI_TypeKind_U16; goto const_u;
                      case DW_ExprOp_Const4U: push_vals[0].type_kind = RDI_TypeKind_U32; goto const_u;
                      case DW_ExprOp_Const8U: push_vals[0].type_kind = RDI_TypeKind_U64; goto const_u;
                      case DW_ExprOp_ConstU:  push_vals[0].type_kind = RDI_TypeKind_U64; goto const_u;
                      const_u:;
                      {
                        rdim_bytecode_push_uconst(arena, &dst_bytecode, operand_u64s[0]);
                      }break;
                      
                      //- rjf: small signed literals
                      case DW_ExprOp_Const1S: push_vals[0].type_kind = RDI_TypeKind_S8;  goto const_s;
                      case DW_ExprOp_Const2S: push_vals[0].type_kind = RDI_TypeKind_S16; goto const_s;
                      case DW_ExprOp_Const4S: push_vals[0].type_kind = RDI_TypeKind_S32; goto const_s;
                      case DW_ExprOp_Const8S: push_vals[0].type_kind = RDI_TypeKind_S64; goto const_s;
                      case DW_ExprOp_ConstS : push_vals[0].type_kind = RDI_TypeKind_S64; goto const_s;
                      const_s:;
                      {
                        rdim_bytecode_push_sconst(arena, &dst_bytecode, operand_s64s[0]);
                      }break;
                      
                      //- rjf: address (module offsets)
                      case DW_ExprOp_Addr:
                      {
                        U64 voff = operand_u64s[0] - base_vaddr;
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_ModuleOff, voff);
                        push_vals[0].type_kind = RDI_TypeKind_U64;
                        push_vals[0].is_addr = 1;
                      }break;
                      case DW_ExprOp_Addrx:
                      if(unit_parse_ctx->addr_table != 0)
                      {
                        U64 addr_idx = operand_u64s[0];
                        U64 addr = 0;
                        if(dw2_try_offset_from_table_idx(unit_parse_ctx->addr_table, addr_idx, &addr))
                        {
                          U64 voff = (addr > base_vaddr) ? (addr - base_vaddr) : 0;
                          rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_ModuleOff, voff);
                          push_vals[0].type_kind = RDI_TypeKind_U64;
                          push_vals[0].is_addr = 1;
                        }
                      }break;
                      
                      //- rjf: register reads
                      case DW_ExprOp_Reg0:  case DW_ExprOp_Reg1:  case DW_ExprOp_Reg2:
                      case DW_ExprOp_Reg3:  case DW_ExprOp_Reg4:  case DW_ExprOp_Reg5:
                      case DW_ExprOp_Reg6:  case DW_ExprOp_Reg7:  case DW_ExprOp_Reg8:
                      case DW_ExprOp_Reg9:  case DW_ExprOp_Reg10: case DW_ExprOp_Reg11:
                      case DW_ExprOp_Reg12: case DW_ExprOp_Reg13: case DW_ExprOp_Reg14:
                      case DW_ExprOp_Reg15: case DW_ExprOp_Reg16: case DW_ExprOp_Reg17:
                      case DW_ExprOp_Reg18: case DW_ExprOp_Reg19: case DW_ExprOp_Reg20:
                      case DW_ExprOp_Reg21: case DW_ExprOp_Reg22: case DW_ExprOp_Reg23:
                      case DW_ExprOp_Reg24: case DW_ExprOp_Reg25: case DW_ExprOp_Reg26:
                      case DW_ExprOp_Reg27: case DW_ExprOp_Reg28: case DW_ExprOp_Reg29:
                      case DW_ExprOp_Reg30: case DW_ExprOp_Reg31:
                      {
                        regcode_dw = (U64)(opcode - DW_ExprOp_Reg0);
                      }goto reg_read;
                      case DW_ExprOp_RegX:
                      {
                        regcode_dw = operand_u64s[0];
                      }goto reg_read;
                      case DW_ExprOp_BReg0:  case DW_ExprOp_BReg1:  case DW_ExprOp_BReg2:
                      case DW_ExprOp_BReg3:  case DW_ExprOp_BReg4:  case DW_ExprOp_BReg5:
                      case DW_ExprOp_BReg6:  case DW_ExprOp_BReg7:  case DW_ExprOp_BReg8:
                      case DW_ExprOp_BReg9:  case DW_ExprOp_BReg10: case DW_ExprOp_BReg11:
                      case DW_ExprOp_BReg12: case DW_ExprOp_BReg13: case DW_ExprOp_BReg14:
                      case DW_ExprOp_BReg15: case DW_ExprOp_BReg16: case DW_ExprOp_BReg17:
                      case DW_ExprOp_BReg18: case DW_ExprOp_BReg19: case DW_ExprOp_BReg20:
                      case DW_ExprOp_BReg21: case DW_ExprOp_BReg22: case DW_ExprOp_BReg23:
                      case DW_ExprOp_BReg24: case DW_ExprOp_BReg25: case DW_ExprOp_BReg26:
                      case DW_ExprOp_BReg27: case DW_ExprOp_BReg28: case DW_ExprOp_BReg29:
                      case DW_ExprOp_BReg30: case DW_ExprOp_BReg31:
                      {
                        regcode_dw = (U64)(opcode - DW_ExprOp_BReg0);
                        regval_off = operand_s64s[0];
                        regread_is_addr = 1;
                      }goto reg_read;
                      case DW_ExprOp_BRegX:
                      {
                        regcode_dw = operand_u64s[0];
                        regval_off = operand_s64s[1];
                        regread_is_addr = 1;
                      }goto reg_read;
                      reg_read:;
                      {
                        // rjf: DWARF regcode -> internal regcode
                        ARCH_RegCode regcode = arch_reg_code_from_dw(arch, regcode_dw);
                        
                        // rjf: internal regcode -> RDI, offset, size
                        RDI_RegCode regcode_rdi = 0;
                        U64 reg_off = 0;
                        U64 reg_size = 0;
                        {
                          Rng1U16 reg_rng = arch_info->reg_code_rng_table[regcode];
                          reg_size = dim_1u16(reg_rng);
                          regcode_rdi = arch_rdi_from_reg_code_table_from_arch(arch)[regcode];
                        }
                        
                        // rjf: push op
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_RegRead, RDI_EncodeRegReadParam(regcode_rdi, reg_size, reg_off));
                        
                        // rjf: push add to value offset, if needed
                        if(regval_off != 0)
                        {
                          rdim_bytecode_push_sconst(arena, &dst_bytecode, regval_off);
                          rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_Add, RDI_EvalTypeGroup_S);
                        }
                        
                        // rjf: choose if we're doing a float op
                        B32 is_float_op = 0;
                        if(arch == Arch_x64 &&
                           (regcode_rdi == RDI_RegCodeX64_st0 ||
                            regcode_rdi == RDI_RegCodeX64_st1 ||
                            regcode_rdi == RDI_RegCodeX64_st2 ||
                            regcode_rdi == RDI_RegCodeX64_st3 ||
                            regcode_rdi == RDI_RegCodeX64_st4 ||
                            regcode_rdi == RDI_RegCodeX64_st5 ||
                            regcode_rdi == RDI_RegCodeX64_st6 ||
                            regcode_rdi == RDI_RegCodeX64_st7))
                        {
                          is_float_op = 1;
                        }
                        
                        // rjf: set up push value
                        push_vals[0].is_addr = regread_is_addr;
                        if(is_float_op)
                        {
                          switch(reg_size)
                          {
                            default:{}break;
                            case 2: {push_vals[0].type_kind = RDI_TypeKind_F16;}break;
                            case 4: {push_vals[0].type_kind = RDI_TypeKind_F32;}break;
                            case 6: {push_vals[0].type_kind = RDI_TypeKind_F48;}break;
                            case 8: {push_vals[0].type_kind = RDI_TypeKind_F64;}break;
                            case 10:{push_vals[0].type_kind = RDI_TypeKind_F80;}break;
                            case 12:{push_vals[0].type_kind = RDI_TypeKind_F96;}break;
                            case 16:{push_vals[0].type_kind = RDI_TypeKind_F128;}break;
                          }
                        }
                        else
                        {
                          switch(reg_size)
                          {
                            default:{}break;
                            case 1:{push_vals[0].type_kind = RDI_TypeKind_U8;}break;
                            case 2:{push_vals[0].type_kind = RDI_TypeKind_U16;}break;
                            case 4:{push_vals[0].type_kind = RDI_TypeKind_U32;}break;
                            case 8:{push_vals[0].type_kind = RDI_TypeKind_U64;}break;
                          }
                        }
                      }break;
                      
                      //- rjf: implicit values
                      case DW_ExprOp_ImplicitValue:
                      {
                        if(operand_str8s[0].size <= sizeof(U64))
                        {
                          U64 implicit_value = 0;
                          MemoryCopy(&implicit_value, operand_str8s[0].str, Min(sizeof(U64), operand_str8s[0].size));
                          rdim_bytecode_push_uconst(arena, &dst_bytecode, implicit_value);
                          switch(operand_str8s[0].size)
                          {
                            default:{}break;
                            case 1:{push_vals[0].type_kind = RDI_TypeKind_U8;}break;
                            case 2:{push_vals[0].type_kind = RDI_TypeKind_U16;}break;
                            case 4:{push_vals[0].type_kind = RDI_TypeKind_U32;}break;
                            case 8:{push_vals[0].type_kind = RDI_TypeKind_U64;}break;
                          }
                        }
                        else
                        {
                          log_infof("[.debug_info@0x%I64x] Implicit value DWARF expression operation (DW_ExprOp_ImplicitValue) with block size >8 (%I64u) found. This is not currently supported.\n", start_off, operand_str8s[0].size);
                        }
                      }break;
                      
                      //- rjf: pieces
                      case DW_ExprOp_Piece:
                      {
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_PartialValue, operand_u64s[0]);
                      }break;
                      case DW_ExprOp_BitPiece:
                      {
                        U64 size = operand_u64s[0];
                        U64 off = operand_u64s[1];
                        U64 partial_value = ((size<<32)|(off));
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_PartialValueBit, partial_value);
                      }break;
                      
                      //- rjf: stack ops
                      case DW_ExprOp_Over: {target_pick_val_idx = 1;}goto pick;
                      case DW_ExprOp_Pick: {target_pick_val_idx = operand_u64s[0];}goto pick;
                      case DW_ExprOp_Dup:  {target_pick_val_idx = 0;}goto pick;
                      pick:;
                      {
                        // rjf: pick value from stack
                        D2R_ExprVal picked_val = {0};
                        {
                          U64 idx = 0;
                          for(D2R_ExprValNode *n = val_stack_top; n != 0; n = n->next)
                          {
                            if(idx == target_pick_val_idx)
                            {
                              picked_val = n->v;
                              break;
                            }
                          }
                        }
                        
                        // rjf: push opcode
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_Pick, target_pick_val_idx);
                        
                        // rjf: push val
                        push_vals[0] = picked_val;
                      }break;
                      case DW_ExprOp_Swap:
                      {
                        push_vals[0] = popped_vals[1];
                        push_vals[1] = popped_vals[0];
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_Swap, 0);
                      }break;
                      case DW_ExprOp_Drop:
                      {
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_Pop, 0);
                      }break;
                      
                      //- rjf: jumps
                      case DW_ExprOp_Skip: rdi_eval_op = RDI_EvalOp_Skip; goto jumps;
                      case DW_ExprOp_Bra:  rdi_eval_op = RDI_EvalOp_Cond; goto jumps;
                      jumps:;
                      {
                        // rjf: jump delta in bytes (how the instruction is encoded) -> jump delta in instructions
                        S64 jump_delta_bytes = operand_s64s[0];
                        S64 jump_delta_insts = 0;
                        {
                          // TODO(rjf): expr op jumps bytes -> inst counts
                        }
                        
                        // rjf: push incomplete op (we need to resolve inst delta -> byte delta later, once we have the full bytecode stream)
                        RDIM_EvalBytecodeOp *op = rdim_bytecode_push_op(arena, &dst_bytecode, rdi_eval_op, 0);
                        
                        // rjf: gather op
                        JumpOpNode *n = push_array(temp.arena, JumpOpNode, 1);
                        SLLQueuePush(first_jump_op, last_jump_op, n);
                        n->op = op;
                        n->inst_delta = jump_delta_insts;
                      }break;
                      
                      //- rjf: containing procedure frame offsets
                      case DW_ExprOp_FBReg:
                      {
                        bytecode_is_framebase_dependent = 1;
                        for EachNode(n, RDIM_EvalBytecodeOp, framebase_loc.bytecode.first_op)
                        {
                          rdim_bytecode_push_op(arena, &dst_bytecode, n->op, n->p);
                        }
                        rdim_bytecode_push_sconst(arena, &dst_bytecode, operand_s64s[0]);
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_Add, RDI_EvalTypeGroup_S);
                        push_vals[0].type_kind = RDI_TypeKind_U64;
                        push_vals[0].is_addr = 1;
                      }break;
                      
                      //- rjf: call frame CFA
                      case DW_ExprOp_CallFrameCfa:
                      {
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_PushCfa, 0);
                      }break;
                      
                      //- rjf: memory reads
                      case DW_ExprOp_Deref: memread_size = unit_parse_ctx->addr_size; goto deref;
                      case DW_ExprOp_DerefSize: memread_size = operand_u64s[0]; goto deref;
                      deref:;
                      {
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_MemRead, memread_size);
                        push_vals[0].is_addr = 1;
                        push_vals[0].type_kind = RDI_TypeKind_U64;
                      }break;
                      
                      //- rjf: TLS offsets
                      case DW_ExprOp_FormTlsAddress:
                      case DW_GNU_ExprOp_PushTlsAddress:
                      {
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_TLSOff, 0);
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_Add, RDI_EvalTypeGroup_U);
                        push_vals[0].is_addr = 1;
                        push_vals[0].type_kind = RDI_TypeKind_U64;
                        is_tls_dependent = 1;
                      }break;
                      
                      //- rjf: call site value
                      case DW_ExprOp_EntryValue:
                      case DW_GNU_ExprOp_EntryValue:
                      {
                        // TODO(rjf): expr op entry value ops
                      }break;
                      
                      //- rjf: fixed adds
                      case DW_ExprOp_PlusUConst:
                      {
                        rdim_bytecode_push_uconst(arena, &dst_bytecode, operand_u64s[0]);
                        rdim_bytecode_push_op(arena, &dst_bytecode, RDI_EvalOp_Add, RDI_EvalTypeGroup_U);
                        push_vals[0].type_kind = RDI_TypeKind_U64; // TODO(rjf): do we need to adjust this based on popped value at all, even?
                      }break;
                      
                      //- rjf: arithmetic ops
                      case DW_ExprOp_Eq:     {rdi_eval_op = RDI_EvalOp_EqEq;}goto arithmetic_op;
                      case DW_ExprOp_Ge:     {rdi_eval_op = RDI_EvalOp_GrEq;}goto arithmetic_op;
                      case DW_ExprOp_Gt:     {rdi_eval_op = RDI_EvalOp_Grtr;}goto arithmetic_op;
                      case DW_ExprOp_Le:     {rdi_eval_op = RDI_EvalOp_LsEq;}goto arithmetic_op;
                      case DW_ExprOp_Lt:     {rdi_eval_op = RDI_EvalOp_Less;}goto arithmetic_op;
                      case DW_ExprOp_Ne:     {rdi_eval_op = RDI_EvalOp_Neg;}goto arithmetic_op;
                      case DW_ExprOp_Div:    {rdi_eval_op = RDI_EvalOp_Div;}goto arithmetic_op;
                      case DW_ExprOp_Minus:  {rdi_eval_op = RDI_EvalOp_Sub;}goto arithmetic_op;
                      case DW_ExprOp_Mul:    {rdi_eval_op = RDI_EvalOp_Mul;}goto arithmetic_op;
                      case DW_ExprOp_Plus:   {rdi_eval_op = RDI_EvalOp_Add;}goto arithmetic_op;
                      case DW_ExprOp_Xor:    {rdi_eval_op = RDI_EvalOp_BitXor;}goto arithmetic_op;
                      case DW_ExprOp_And:    {rdi_eval_op = RDI_EvalOp_BitAnd;}goto arithmetic_op;
                      case DW_ExprOp_Or:     {rdi_eval_op = RDI_EvalOp_BitOr;}goto arithmetic_op;
                      case DW_ExprOp_Shl:    {rdi_eval_op = RDI_EvalOp_LShift;}goto arithmetic_op;
                      case DW_ExprOp_Shr:    {rdi_eval_op = RDI_EvalOp_RShift;}goto arithmetic_op;
                      case DW_ExprOp_Shra:   {rdi_eval_op = RDI_EvalOp_RShift;}goto arithmetic_op;
                      case DW_ExprOp_Mod:    {rdi_eval_op = RDI_EvalOp_Mod;}goto arithmetic_op;
                      case DW_ExprOp_Abs:    {rdi_eval_op = RDI_EvalOp_Abs;}goto arithmetic_op;
                      case DW_ExprOp_Neg:    {rdi_eval_op = RDI_EvalOp_Neg;}goto arithmetic_op;
                      case DW_ExprOp_Not:    {rdi_eval_op = RDI_EvalOp_LogNot;}goto arithmetic_op;
                      arithmetic_op:;
                      {
                        // TODO(rjf): eval arithmetic conversions etc.
                        rdim_bytecode_push_op(arena, &dst_bytecode, rdi_eval_op, RDI_EvalTypeGroup_U);
                      }break;
                      
                      //- rjf: currently unsupported
                      case DW_ExprOp_XDeref:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression XDeref operation encountered, implying multiple address spaces; this is not supported for location info.\n", start_off);
                      }break;
                      case DW_ExprOp_XDerefSize:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression XDerefSize operation encountered, implying multiple address spaces; this is not supported for location info.\n", start_off);
                      }break;
                      case DW_ExprOp_Call2:
                      case DW_ExprOp_Call4:
                      case DW_ExprOp_CallRef:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression call operation encountered. This is not supported for location info.\n", start_off);
                      }break;
                      case DW_ExprOp_ImplicitPointer:
                      case DW_GNU_ExprOp_ImplicitPointer:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression implicit pointer operation encountered. This is not supported for location info.\n", start_off);
                      }break;
                      case DW_GNU_ExprOp_ParameterRef:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression GNU_ParameterRef operation encountered. This is not supported for location info.\n", start_off);
                      }break;
                      case DW_ExprOp_DerefType:
                      case DW_GNU_ExprOp_DerefType:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression DerefType operation encountered. This is not supported for location info.\n", start_off);
                      }break;
                      case DW_ExprOp_ConstType:
                      case DW_GNU_ExprOp_ConstType:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression ConstType operation encountered. This is not supported for location info.\n", start_off);
                      }break;
                      case DW_ExprOp_RegvalType:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression RegvalType operation encountered. This is not supported for location info.\n", start_off);
                      }break;
                      case DW_ExprOp_PushObjectAddress:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression PushObjectAddress operation encountered. This is not supported for location info.\n", start_off);
                      }break;
                      case DW_ExprOp_Rot:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression Rot (rotate) operation encountered. This is not supported for location info.\n", start_off);
                      }break;
                      case DW_GNU_ExprOp_UnInit:
                      {
                        dst_bytecode_is_good = 0;
                        log_infof("[.debug_info@0x%I64x] DWARF expression GNU_UnInit operation encountered. This is not supported for location info.\n", start_off);
                        // TODO: flag value as unitialized; this must be last opcode; possible to use with DW_ExprOp_Piece;
                      } break;
                    }
                    
                    //- rjf: push values to stack
                    {
                      push_count = Min(push_count, ArrayCount(push_vals));
                      for EachIndex(push_idx, push_count)
                      {
                        D2R_ExprValNode *n = free_val;
                        if(n != 0)
                        {
                          SLLStackPop(free_val);
                        }
                        else
                        {
                          n = push_array(temp.arena, D2R_ExprValNode, 1);
                        }
                        n->v = push_vals[push_idx];
                        SLLStackPush(val_stack_top, n);
                      }
                    }
                    
                    if(expr_off == start_expr_off)
                    {
                      break;
                    }
                  }
                  
                  //- TODO(rjf): Try to statically evaluate the expression here if possible
                  // (or maybe intermixed above? investigate...), to reduce to simpler bytecode
                  // forms - reduce pressure on pattern matching
                  {
                    // TODO(rjf)
                  }
                  
                  //- rjf: apply RDI byte offsets to jump ops
                  {
                    // TODO(rjf): apply RDI byte offsets to jump ops
                  }
                  
                  //- rjf: map bytecode -> RDIM_Location - we may want to simplify
                  RDIM_Location loc = {0};
                  {
                    loc.kind = val_stack_top && val_stack_top->v.is_addr ? RDI_LocationKind_AddrBytecodeStream : RDI_LocationKind_ValBytecodeStream;
                    loc.bytecode = dst_bytecode;
                    
                    // rjf: gather first few bytecodes for pattern matching
                    RDI_EvalOp beginning_ops[5] = {0};
                    RDIM_EvalBytecodeOp *beginning_nodes[5] = {0};
                    {
                      U64 idx = 0;
                      for EachNode(n, RDIM_EvalBytecodeOp, dst_bytecode.first_op)
                      {
                        if(idx >= ArrayCount(beginning_ops))
                        {
                          break;
                        }
                        beginning_ops[idx] = n->op;
                        beginning_nodes[idx] = n;
                        idx += 1;
                      }
                    }
                    
                    // rjf: match val registers
                    if(loc.kind == RDI_LocationKind_ValBytecodeStream &&
                       beginning_ops[0] == RDI_EvalOp_RegRead &&
                       beginning_ops[1] == RDI_EvalOp_Stop)
                    {
                      U8 rdi_reg_code = (beginning_nodes[0]->p&0x0000FF)>>0;
                      U8 byte_size    = (beginning_nodes[0]->p&0x00FF00)>>8;
                      U8 byte_off     = (beginning_nodes[0]->p&0xFF0000)>>16;
                      if(byte_size == unit_parse_ctx->addr_size &&
                         byte_off == 0)
                      {
                        loc.kind = RDI_LocationKind_ValReg;
                        loc.reg_code = rdi_reg_code;
                      }
                    }
                    
                    // rjf: match simple register offsets
                    if(loc.kind == RDI_LocationKind_AddrBytecodeStream &&
                       beginning_ops[0] == RDI_EvalOp_RegRead &&
                       (beginning_ops[1] == RDI_EvalOp_ConstU8 ||
                        beginning_ops[1] == RDI_EvalOp_ConstU16) &&
                       beginning_ops[2] == RDI_EvalOp_TruncSigned &&
                       beginning_ops[3] == RDI_EvalOp_Add &&
                       beginning_ops[4] == RDI_EvalOp_Stop)
                    {
                      U8 rdi_reg_code = (beginning_nodes[0]->p&0x0000FF)>>0;
                      U8 byte_size    = (beginning_nodes[0]->p&0x00FF00)>>8;
                      U8 byte_off     = (beginning_nodes[0]->p&0xFF0000)>>16;
                      if(byte_off == 0 && byte_size == unit_parse_ctx->addr_size)
                      {
                        loc.kind = RDI_LocationKind_AddrRegPlusOff;
                        loc.reg_code = rdi_reg_code;
                        loc.offset   = beginning_nodes[1]->p;
                      }
                    }
                    
                    // rjf: match simple module offsets
                    if(beginning_ops[0] == RDI_EvalOp_ModuleOff && beginning_ops[1] == RDI_EvalOp_Stop)
                    {
                      U64 voff = beginning_nodes[0]->p;
                      loc.kind = RDI_LocationKind_ModuleOff;
                      loc.offset = voff;
                    }
                    else if(((beginning_ops[0] == RDI_EvalOp_ModuleOff &&
                              (beginning_ops[1] == RDI_EvalOp_ConstU8 ||
                               beginning_ops[1] == RDI_EvalOp_ConstU16 ||
                               beginning_ops[1] == RDI_EvalOp_ConstU32 ||
                               beginning_ops[1] == RDI_EvalOp_ConstU64)) ||
                             (beginning_ops[1] == RDI_EvalOp_ModuleOff &&
                              (beginning_ops[0] == RDI_EvalOp_ConstU8 ||
                               beginning_ops[0] == RDI_EvalOp_ConstU16 ||
                               beginning_ops[0] == RDI_EvalOp_ConstU32 ||
                               beginning_ops[0] == RDI_EvalOp_ConstU64))) &&
                            beginning_ops[2] == RDI_EvalOp_Add &&
                            beginning_ops[3] == RDI_EvalOp_Stop)
                    {
                      U64 voff = beginning_nodes[0]->p + beginning_nodes[1]->p;
                      loc.kind = RDI_LocationKind_ModuleOff;
                      loc.offset = voff;
                    }
                    
                    // rjf: match simple TLS offsets
                    if(beginning_ops[0] == RDI_EvalOp_TLSOff && beginning_ops[1] == RDI_EvalOp_Stop)
                    {
                      U64 toff = beginning_nodes[0]->p;
                      loc.kind = RDI_LocationKind_TLSOff;
                      loc.offset = toff;
                    }
                    else if(((beginning_ops[0] == RDI_EvalOp_TLSOff &&
                              (beginning_ops[1] == RDI_EvalOp_ConstU8 ||
                               beginning_ops[1] == RDI_EvalOp_ConstU16 ||
                               beginning_ops[1] == RDI_EvalOp_ConstU32 ||
                               beginning_ops[1] == RDI_EvalOp_ConstU64)) ||
                             (beginning_ops[1] == RDI_EvalOp_TLSOff &&
                              (beginning_ops[0] == RDI_EvalOp_ConstU8 ||
                               beginning_ops[0] == RDI_EvalOp_ConstU16 ||
                               beginning_ops[0] == RDI_EvalOp_ConstU32 ||
                               beginning_ops[0] == RDI_EvalOp_ConstU64))) &&
                            beginning_ops[2] == RDI_EvalOp_Add &&
                            beginning_ops[3] == RDI_EvalOp_Stop)
                    {
                      U64 toff = beginning_nodes[0]->p + beginning_nodes[1]->p;
                      loc.kind = RDI_LocationKind_TLSOff;
                      loc.offset = toff;
                    }
                  }
                  
                  //- rjf: collect
                  {
                    RDIM_Rng1U64 voff_range = {range.min - base_vaddr, range.max - base_vaddr};
                    if(bytecode_is_framebase_dependent)
                    {
                      voff_range.min = Max(voff_range.min, framebase_voff_range.min);
                      voff_range.max = Min(voff_range.max, framebase_voff_range.max);
                    }
                    rdim_location_case_list_push(arena, dst_locations, loc, voff_range);
                    dst_is_tls_dependent[0] = is_tls_dependent;
                  }
                  
                  //- rjf: not frame-base dependent? -> break - don't do this per-framebase case
                  if(!bytecode_is_framebase_dependent)
                  {
                    break;
                  }
                }
              }
            }
          }
          
          ////////////////////////
          //- rjf: produce symbols from tag
          //
          RDIM_Symbol *new_symbol = 0;
          RDIM_Scope *new_scope_open = 0;
          B32 parent_tag_is_container_to_deduped_tags = 0;
          switch(tag.kind)
          {
            default:{}break;
            
            //- rjf: determine if parent tag is a container to deduplicated tags
            case DW_TagKind_Namespace:
            case DW_TagKind_StructureType:
            case DW_TagKind_UnionType:
            case DW_TagKind_ClassType:
            case DW_TagKind_EnumerationType:
            case DW_TagKind_Typedef:
            {
              parent_tag_is_container_to_deduped_tags = 1;
            }break;
            
            //- rjf: subprograms (procedures)
            case DW_TagKind_SubProgram:
            {
              if(make_procedures)
              {
                if(!is_decl && (!prune_dead_output || ranges.count != 0))
                {
                  U64          scope_chunk_count = Min(chunk_count, Max(16, dst_artifacts->scopes.total_count));
                  U64          proc_chunk_count  = Min(chunk_count, Max(16, dst_artifacts->procedures.total_count));
                  RDIM_Scope  *root_scope        = rdim_scope_chunk_list_push(arena, &dst_artifacts->scopes, scope_chunk_count);
                  RDIM_Symbol *procedure         = rdim_symbol_chunk_list_push(arena, &dst_artifacts->procedures, proc_chunk_count);
                  procedure->name           = name;
                  procedure->link_name      = link_name;
                  procedure->type           = type;
                  procedure->root_scope     = root_scope;
                  procedure->location_cases = framebase_location_cases;
                  root_scope->symbol = procedure;
                  root_scope->voff_ranges = ranges;
                  dst_artifacts->scopes.scope_voff_count += 2*ranges.count;
                  new_symbol = procedure;
                  new_scope_open = root_scope;
                }
              }
            }break;
            
            //- rjf: inline site
            case DW_TagKind_InlinedSubroutine:
            {
              if(make_inline_sites)
              {
                // TODO(rjf)
              }
            }break;
            
            //- rjf: variables
            case DW_TagKind_Variable:
            case DW_TagKind_FormalParameter:
            {
              if(make_locals)
              {
                if(name.size != 0)
                {
                  U64 var_chunk_count = chunk_count;
                  RDIM_SymbolChunkList *dst_symbols = 0;
                  if(location_is_tls_dependent)
                  {
                    if (make_thread_vars)
                    {
                      dst_symbols = &dst_artifacts->thread_variables;
                    }
                  }
                  else if(top_parent != 0 && top_parent->scope != 0)
                  {
                    if (make_locals)
                    {
                      dst_symbols = &top_parent->scope->locals;
                      var_chunk_count = 8;
                    }
                  }
                  else
                  {
                    if(make_global_vars)
                    {
                      dst_symbols = &dst_artifacts->global_variables;
                    }
                  }

                  if(dst_symbols)
                  {
                    var_chunk_count = Min(var_chunk_count, Max(16, dst_symbols->total_count));
                    RDIM_Symbol *var = rdim_symbol_chunk_list_push(arena, dst_symbols, var_chunk_count);
                    var->is_extern = is_external;
                    var->is_param  = (tag.kind == DW_TagKind_FormalParameter);
                    var->name      = name;
                    var->link_name = link_name;
                    var->type      = type;
                    var->location_cases = location_cases;
                    new_symbol = var;
                  }
                }
              }
            } break;
            
            //- rjf: lexical blocks (scopes)
            case DW_TagKind_LexicalBlock:
            {
              if(make_scopes && (!prune_dead_output ||
                                (ranges.count != 0 && top_parent != 0 && top_parent->scope != 0)))
              {
                U64 scope_chunk_count = Min(chunk_count, Max(16, dst_artifacts->scopes.total_count));
                RDIM_Scope *scope = rdim_scope_chunk_list_push(arena, &dst_artifacts->scopes, scope_chunk_count);
                scope->voff_ranges = ranges;
                dst_artifacts->scopes.scope_voff_count += 2*ranges.count;
                new_scope_open = scope;
              }
            }break;
          }
          
          if(scope_container_map != 0)
          {
            ////////////////////////
            //- rjf: if we've determined that the parent tag is a container to
            // deduplicated tags (types, namespaces), then we need to record
            // the .debug_info offset -> scope mapping for this scope, so it
            // can be equipped as a container later.
            //
            if(parent_tag_is_container_to_deduped_tags)
            {
              U64 info_off = 0;
              RDIM_Scope *scope = 0;
              for(D2R_ParentNode *n = top_parent; n != 0; n = n->next)
              {
                if(n->scope != 0)
                {
                  info_off = n->info_off;
                  scope = n->scope;
                  break;
                }
              }
              if(scope != 0)
              {
                D2R_ScopeContainerNode *n = push_array(scratch.arena, D2R_ScopeContainerNode, 1);
                n->info_off = info_off;
                n->scope = scope;
                U64 hash = u64_hash_from_str8(str8_struct(&info_off));
                U64 slot_idx = hash%scope_container_map->slots_count;
                for(B32 gathered = 0; !gathered;)
                {
                  U64 expected_head_value = ins_atomic_u64_eval(&scope_container_map->slots[slot_idx]);
                  n->next = (D2R_ScopeContainerNode *)expected_head_value;
                  if(expected_head_value == ins_atomic_u64_eval_cond_assign(&scope_container_map->slots[slot_idx], (U64)n, expected_head_value))
                  {
                    gathered = 1;
                  }
                }
              }
            }
          }
          
          // Keep unused ancestors in the recyclable stack until an output or target
          // needs their naming context. Only persistent nodes may escape that stack.
          D2R_NameNode *name_context = 0;
          U64 name_ref_offs[3] = {0};
          if (make_namespaces && !make_types) {
            B32 is_name_container  = dw_tag_is_name_container(tag.kind);
            B32 is_member_identity = ((make_global_vars || make_thread_vars) &&
                                      (tag.kind == DW_TagKind_Member || tag.kind == DW_TagKind_Variable));
            if (is_name_container || is_member_identity || new_symbol != 0) {
              d2r_name_refs_from_tag(unit_parse_ctx, unit_info_tag_range, &tag, name_ref_offs);

              BucketNode *target = hash_table_search_u64(name_ht, start_off);
              if (tag.kind == DW_TagKind_Namespace || new_symbol != 0 || target != 0) {
                D2R_NameNode   *parent_context = 0;
                D2R_ParentNode *pending        = 0;
                for EachNode(parent, D2R_ParentNode, top_parent) {
                  if (parent->name_context != 0) {
                    parent_context = parent->name_context;
                    break;
                  }
                  if (dw_tag_is_name_container(parent->tag_kind)) {
                    parent->name_pending_next = pending;
                    pending = parent;
                  }
                }

                for (D2R_ParentNode *parent = pending; parent != 0; parent = parent->name_pending_next) {
                  D2R_NameNode *n = push_array(scratch.arena, D2R_NameNode, 1);
                  SLLQueuePush(dst_artifacts->first_name_node, dst_artifacts->last_name_node, n);
                  dst_artifacts->name_node_count += 1;
                  n->info_off = parent->info_off;
                  n->parent   = parent_context;
                  n->name     = parent->name;
                  MemoryCopy(n->ref_offs, parent->name_ref_offs, sizeof(n->ref_offs));
                  parent->name_context = n;
                  parent_context = n;
                }

                D2R_NameNode *n = push_array(scratch.arena, D2R_NameNode, 1);
                SLLQueuePush(dst_artifacts->first_name_node, dst_artifacts->last_name_node, n);
                dst_artifacts->name_node_count += 1;
                n->info_off = start_off;
                n->parent   = parent_context;
                n->name     = name;
                n->symbol   = new_symbol;
                MemoryCopy(n->ref_offs, name_ref_offs, sizeof(n->ref_offs));

                // emit actual namespaces; other ancestors remain name-only records
                if (tag.kind == DW_TagKind_Namespace) {
                  U64 namespace_chunk_count = Min(64, Max(16, dst_artifacts->namespaces.total_count));
                  n->ns = rdim_namespace_chunk_list_push(arena, &dst_artifacts->namespaces, namespace_chunk_count);
                  n->ns->name         = name.size != 0 ? name : str8_lit("(anonymous namespace)");
                  n->ns->parent_scope = top_parent ? top_parent->scope : 0;
                  if (n->ns->parent_scope == 0) {
                    for (D2R_NameNode *parent = parent_context; parent != 0; parent = parent->parent) {
                      if (parent->ns != 0) {
                        n->ns->parent_namespace = parent->ns;
                        break;
                      }
                    }
                  }
                }
                if (target != 0) {
                  target->v.value_raw = n;
                }
                if (is_name_container) {
                  name_context = n;
                }
              }
            }
          }

          ////////////////////////
          //- rjf: equip new symbols with container info
          //
          if(new_symbol != 0 && top_parent != 0)
          {
            if(top_parent->scope != 0)
            {
              new_symbol->container_scope = top_parent->scope;
            }
            else if(top_parent->container_type != 0)
            {
              new_symbol->container_type = top_parent->container_type;
            }
            else if(top_parent->container_namespace != 0)
            {
              new_symbol->container_namespace = top_parent->container_namespace;
            }
          }
          
          ////////////////////////
          //- rjf: insert new scopes to their parent
          //
          if(new_scope_open && top_parent != 0 && top_parent->scope != 0)
          {
            RDIM_Scope *parent = top_parent->scope;
            SLLQueuePush_N(parent->first_child, parent->last_child, new_scope_open, next_sibling);
            new_scope_open->parent_scope = parent;
            if(new_scope_open->symbol == 0)
            {
              new_scope_open->symbol = parent->symbol;
            }
          }
          
          ////////////////////////
          //- rjf: gather container info for new tag parents
          //
          RDIM_Type *container_type = 0;
          RDIM_Namespace *container_namespace = 0;
          if(make_types)
          {
            if(tag.has_children)
            {
              // rjf: info offset -> hash
              U64 hash = 0;
              {
                D2R_UnitDedupedTagMap *unit_deduped_tag_map = &unit_deduped_tag_maps[unit_idx];
                U64 info_off_hash = u64_hash_from_str8(str8_struct(&start_off));
                U64 info_off_slot_idx = info_off_hash%unit_deduped_tag_map->slots_count;
                for EachNode(n, D2R_UnitDedupedTagNode, unit_deduped_tag_map->slots[info_off_slot_idx])
                {
                  if(n->src_info_off == start_off)
                  {
                    hash = n->dst_hash;
                    break;
                  }
                }
              }

              // rjf: map hash -> unique tag node
              D2R_UniqueTagNode *tag_node = 0;
              {
                U64 slot_idx = hash%unique_tag_slots_count;
                for(D2R_UniqueTagNode *n = unique_tag_slots[slot_idx]; n != 0; n = n->next)
                {
                  if(n->hash == hash)
                  {
                    tag_node = n;
                    break;
                  }
                }
              }

              // rjf: map tag node -> type/namespace
              if(tag_node != 0) switch(tag_node->kind)
              {
              default:{}break;
              case D2R_UniqueTagKind_Type:     {container_type = type_from_idx_map[tag_node->order_idx];}break;
              case D2R_UniqueTagKind_Namespace:{container_namespace = namespace_from_idx_map[tag_node->order_idx];}break;
              }
            }
          }
          
          ////////////////////////
          //- rjf: push tag parents
          //
          if(tag.has_children)
          {
            // rjf: push new parent to stack
            D2R_ParentNode *n = free_parent;
            if(n != 0)
            {
              SLLStackPop(free_parent);
              MemoryZeroStruct(n);
            }
            else
            {
              n = push_array(scratch.arena, D2R_ParentNode, 1);
            }
            n->tag_kind            = tag.kind;
            n->info_off            = start_off;
            n->scope               = new_scope_open ? new_scope_open : top_parent ? top_parent->scope : 0;
            n->container_type      = new_scope_open ? 0 : container_type;
            n->container_namespace = new_scope_open ? 0 : container_namespace;
            n->name_context        = name_context;
            n->name                = name;
            MemoryCopy(n->name_ref_offs, name_ref_offs, sizeof(n->name_ref_offs));
            if(framebase_attrib != &dw2_attrib_nil)
            {
              n->framebase_location_cases = framebase_location_cases;
            }
            else if(top_parent != 0)
            {
              n->framebase_location_cases = top_parent->framebase_location_cases;
            }
            SLLStackPush(top_parent, n);
          }
          
          ////////////////////////
          //- rjf: pop scopes
          //
          if(tag.kind == DW_TagKind_Null && top_parent != 0)
          {
            D2R_ParentNode *n = top_parent;
            SLLStackPop(top_parent);
            SLLStackPush(free_parent, n);
          }
          
          ////////////////////////
          //- rjf: tree nav
          //
          if(tag.has_children)
          {
            depth += 1;
          }
          if(tag.kind == DW_TagKind_Null)
          {
            depth -= 1;
          }
          
          if(off == start_off)
          {
            break;
          }
        }

        // recycle active parents if a malformed/truncated tree stopped early
        while(top_parent != 0)
        {
          D2R_ParentNode *n = top_parent;
          SLLStackPop(top_parent);
          SLLStackPush(free_parent, n);
        }

        scratch_end(scratch2);
      }
    }
    lane_sync();
  }
  
  ////////////////////////////
  //- rjf: join all symbols from all sub-unit works into their units
  //
  ProfScope("join all symbols from all sub-unit works into their units")
  {
    U64 unit_take_idx_ = 0;
    U64 *unit_take_idx_ptr = &unit_take_idx_;
    lane_sync_u64(&unit_take_idx_ptr, 0);
    for(;;)
    {
      U64 unit_idx = ins_atomic_u64_inc_eval(unit_take_idx_ptr)-1;
      if(unit_idx >= unit_count)
      {
        break;
      }
      RDIM_Unit *dst_unit = unit_from_idx_map[unit_idx];
      for EachIndex(work_idx, sub_unit_works_count)
      {
        if(sub_unit_works[work_idx].unit_idx == unit_idx)
        {
          rdim_symbol_chunk_list_concat_in_place(&dst_unit->global_variables, &sub_unit_work_artifacts[work_idx].global_variables);
          rdim_symbol_chunk_list_concat_in_place(&dst_unit->thread_variables, &sub_unit_work_artifacts[work_idx].thread_variables);
          rdim_symbol_chunk_list_concat_in_place(&dst_unit->constants, &sub_unit_work_artifacts[work_idx].constants);
          rdim_symbol_chunk_list_concat_in_place(&dst_unit->procedures, &sub_unit_work_artifacts[work_idx].procedures);
          rdim_scope_chunk_list_concat_in_place(&dst_unit->scopes, &sub_unit_work_artifacts[work_idx].scopes);
          rdim_inline_site_chunk_list_concat_in_place(&dst_unit->inline_sites, &sub_unit_work_artifacts[work_idx].inline_sites);
        }
      }
    }
    lane_sync();
  }
  
  if (make_namespaces && !make_types) ProfScope("resolve lightweight naming contexts") {
    if (lane_idx() == 0) {
      all_namespaces = push_array(scratch.arena, RDIM_NamespaceChunkList, 1);
      for EachIndex(work_idx, sub_unit_works_count) {
        D2R_SubUnitWorkArtifacts *work = &sub_unit_work_artifacts[work_idx];
        rdim_namespace_chunk_list_concat_in_place(all_namespaces, &work->namespaces);
      }
    }
    lane_sync_u64(&all_namespaces, 0);

    // resolve namespace extensions before qualifying symbols; a nameless extension
    // inherits its name rather than becoming an anonymous namespace
    Rng1U64 range = lane_range(sub_unit_works_count);
    for EachInRange(work_idx, range) {
      for EachNode(n, D2R_NameNode, sub_unit_work_artifacts[work_idx].first_name_node) {
        if (n->ns == 0 || (!prune_dead_output && n->ref_offs[2] == 0)) {
          continue;
        }
        Temp    temp    = scratch_begin(&arena, 1);
        HashMap visited = {0};
        String8 name    = {0};
        for (D2R_NameNode *origin = n; origin != 0 && origin->ns != 0;
             origin = origin->ref_offs[2] != 0 ? hash_table_search_u64_raw(name_ht, origin->ref_offs[2]) : 0) {
          if (hash_map_search_u64_u64(&visited, origin->info_off)) {
            break;
          }
          hash_map_push_u64_u64(temp.arena, &visited, origin->info_off, 1);
          if (origin->name.size != 0) {
            name = origin->name;
            break;
          }
        }
        n->ns->name = name.size != 0 ? name : str8_lit("(anonymous namespace)");

        String8List parts = {0};
        str8_list_push(temp.arena, &parts, n->ns->name);
        n->ns->parent_scope = 0;
        n->ns->parent_namespace = 0;
        HashMap visited_parents = {0};
        for (D2R_NameNode *parent = d2r_name_parent(name_ht, n); parent != 0;
             parent = d2r_name_parent(name_ht, parent)) {
          if (parent == n || hash_map_search_u64_u64(&visited_parents, parent->info_off)) {
            break;
          }
          hash_map_push_u64_u64(temp.arena, &visited_parents, parent->info_off, 1);
          if (parent->ns != 0) {
            n->ns->parent_namespace = parent->ns;
            n->ns->parent_scope     = 0;
            break;
          }
          if (parent->symbol != 0 && parent->symbol->root_scope != 0) {
            n->ns->parent_scope     = parent->symbol->root_scope;
            n->ns->parent_namespace = 0;
            break;
          }
          if (parent->name.size != 0) {
            str8_list_push_front(temp.arena, &parts, parent->name);
          }
        }
        if (parts.node_count > 1) {
          StringJoin join = {.sep = str8_lit("::")};
          n->ns->name = str8_list_join(arena, &parts, &join);
        }
        scratch_end(temp);
      }
    }
    lane_sync();

    // qualify symbols with namespace and name-only class containers
    for EachInRange(work_idx, range) {
      for EachNode(n, D2R_NameNode, sub_unit_work_artifacts[work_idx].first_name_node) {
        RDIM_Symbol *symbol = n->symbol;
        if (symbol == 0 || symbol->name.size == 0) {
          continue;
        }

        Temp temp  = scratch_begin(&arena, 1);

        String8List parts = {0};
        str8_list_push(temp.arena, &parts, symbol->name);

        HashMap         visited     = {0};
        RDIM_Namespace *ns          = 0;
        RDIM_Symbol    *scope_owner = symbol->container_scope ? symbol->container_scope->symbol : 0;
        D2R_NameNode   *scope_node  = 0;
        if (scope_owner != 0) {
          for (D2R_NameNode *parent = n->parent; parent != 0; parent = parent->parent) {
            if (parent->symbol == scope_owner) {
              scope_node = parent;
              break;
            }
          }
        }

        B32 found_scope = (symbol->container_scope == 0);
        for (D2R_NameNode *parent = d2r_name_parent(name_ht, n); parent != 0; parent = d2r_name_parent(name_ht, parent)) {
          if (hash_map_search_u64_u64(&visited, parent->info_off)) {
            break;
          }
          hash_map_push_u64_u64(temp.arena, &visited, parent->info_off, 1);

          if (scope_owner != 0 &&
              (parent->symbol == scope_owner ||
               d2r_name_nodes_share_identity(name_ht, scope_node, parent))) {
            found_scope = 1;
            break;
          }

          if (parent->ns != 0) {
            ns = parent->ns;
            break;
          }

          if (parent->name.size != 0) {
            str8_list_push_front(temp.arena, &parts, parent->name);
          }
        }

        if (found_scope) {
          // retain real namespace/scope containers; class names need no synthetic types
          if (parts.node_count > 1) {
            StringJoin join = {.sep = str8_lit("::")};
            symbol->name = str8_list_join(arena, &parts, &join);
          }
          if (symbol->container_scope == 0) {
            symbol->container_namespace = ns;
          }
        }
        scratch_end(temp);
      }
    }
    lane_sync();
  }

  ////////////////////////////
  //- rjf: upgrade all types with namespacing info
  //
  ProfScope("upgrade all types with namespacing info")
  {
    Rng1U64 range = lane_range(type_count);
    for EachInRange(type_idx, range)
    {
      D2R_UniqueTagNode *type_tag_node = type_tag_nodes[type_idx];
      U64 container_ancestor_info_off = type_tag_node->container_ancestor_info_off;
      RDIM_Type *dst_type = type_from_idx_map[type_idx];
      if(dst_type == 0)
      {
        continue;
      }
      RDIM_UDT *dst_udt = dst_type->udt;
      if(dst_udt == 0)
      {
        continue;
      }
      
      // rjf: find container scopes
      RDIM_Scope *container_scope = 0;
      {
        U64 info_off = container_ancestor_info_off;
        U64 hash = u64_hash_from_str8(str8_struct(&info_off));
        U64 slot_idx = hash%scope_container_map->slots_count;
        for(D2R_ScopeContainerNode *n = scope_container_map->slots[slot_idx]; n != 0; n = n->next)
        {
          if(n->info_off == info_off)
          {
            container_scope = n->scope;
            break;
          }
        }
      }
      
      // rjf: find container types/namespaces
      RDIM_Type *container_type = 0;
      RDIM_Namespace *container_namespace = 0;
      if(!container_scope)
      {
        U64 info_off = container_ancestor_info_off;
        U64 unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, info_off);
        U64 unit_idx = unit_num > 0 ? unit_num-1 : 0;
        
        // rjf: info off -> hash
        U64 hash = 0;
        {
          D2R_UnitDedupedTagMap *unit_deduped_tag_map = &unit_deduped_tag_maps[unit_idx];
          U64 info_off_hash = u64_hash_from_str8(str8_struct(&info_off));
          U64 info_off_slot_idx = info_off_hash%unit_deduped_tag_map->slots_count;
          for(D2R_UnitDedupedTagNode *n = unit_deduped_tag_map->slots[info_off_slot_idx]; n != 0; n = n->next)
          {
            if(n->src_info_off == info_off)
            {
              hash = n->dst_hash;
              break;
            }
          }
        }
        
        // rjf: hash -> type
        {
          U64 unique_type_tag_slot_idx = hash%unique_tag_slots_count;
          for(D2R_UniqueTagNode *n = unique_tag_slots[unique_type_tag_slot_idx]; n != 0; n = n->next)
          {
            if(n->hash == hash)
            {
              if(n->kind == D2R_UniqueTagKind_Type)
              {
                container_type = type_from_idx_map[n->order_idx];
              }
              else if(n->kind == D2R_UniqueTagKind_Namespace)
              {
                container_namespace = namespace_from_idx_map[n->order_idx];
              }
              break;
            }
          }
        }
      }
      
      // rjf: fill
      dst_udt->container_scope = container_scope;
      dst_udt->container_type = container_type;
      dst_udt->container_namespace = container_namespace;
    }
  }
  lane_sync();
  
  ////////////////////////////
  //- rjf: upgrade all namespaces with namespacing info
  //
  ProfScope("upgrade all namespaces with namespacing info")
  {
    Rng1U64 range = lane_range(namespace_count);
    for EachInRange(namespace_idx, range)
    {
      D2R_UniqueTagNode *unique_tag_node = namespace_tag_nodes[namespace_idx];
      U64 container_ancestor_info_off = unique_tag_node->container_ancestor_info_off;
      RDIM_Namespace *dst_namespace = namespace_from_idx_map[namespace_idx];
      if(dst_namespace == 0)
      {
        continue;
      }
      
      // rjf: find container scopes
      RDIM_Scope *container_scope = 0;
      {
        U64 info_off = container_ancestor_info_off;
        U64 hash = u64_hash_from_str8(str8_struct(&info_off));
        U64 slot_idx = hash%scope_container_map->slots_count;
        for(D2R_ScopeContainerNode *n = scope_container_map->slots[slot_idx]; n != 0; n = n->next)
        {
          if(n->info_off == info_off)
          {
            container_scope = n->scope;
            break;
          }
        }
      }
      
      // rjf: find container types/namespaces
      RDIM_Type *container_type = 0;
      RDIM_Namespace *container_namespace = 0;
      {
        U64 info_off = container_ancestor_info_off;
        U64 unit_num = rng1u64_array_num_from_value__binary_search(&unit_info_tag_ranges_array, info_off);
        U64 unit_idx = unit_num > 0 ? unit_num-1 : 0;
        
        // rjf: info off -> hash
        U64 hash = 0;
        {
          D2R_UnitDedupedTagMap *unit_deduped_tag_map = &unit_deduped_tag_maps[unit_idx];
          U64 info_off_hash = u64_hash_from_str8(str8_struct(&info_off));
          U64 info_off_slot_idx = info_off_hash%unit_deduped_tag_map->slots_count;
          for(D2R_UnitDedupedTagNode *n = unit_deduped_tag_map->slots[info_off_slot_idx]; n != 0; n = n->next)
          {
            if(n->src_info_off == info_off)
            {
              hash = n->dst_hash;
              break;
            }
          }
        }
        
        // rjf: hash -> type
        {
          U64 unique_type_tag_slot_idx = hash%unique_tag_slots_count;
          for(D2R_UniqueTagNode *n = unique_tag_slots[unique_type_tag_slot_idx]; n != 0; n = n->next)
          {
            if(n->hash == hash)
            {
              if(n->kind == D2R_UniqueTagKind_Type)
              {
                container_type = type_from_idx_map[n->order_idx];
              }
              else if(n->kind == D2R_UniqueTagKind_Namespace)
              {
                container_namespace = namespace_from_idx_map[n->order_idx];
              }
              break;
            }
          }
        }
      }
      
      // rjf: fill
      dst_namespace->parent_scope = container_scope;
      dst_namespace->parent_type = container_type;
      dst_namespace->parent_namespace = container_namespace;
    }
  }
  lane_sync();
  
  ////////////////////////////
  //- rjf: fill result
  //
  RDIM_BakeParams result = {0};
  {
    result.subset_flags    = params->subset_flags;
    result.top_level_info  = top_level_info;
    result.binary_sections = binary_sections;
    result.units           = all_units       ? *all_units       : (RDIM_UnitChunkList){0};
    result.namespaces      = all_namespaces  ? *all_namespaces  : (RDIM_NamespaceChunkList){0};
    result.types           = all_types       ? *all_types       : (RDIM_TypeChunkList){0};
    result.udts            = all_udts        ? *all_udts        : (RDIM_UDTChunkList){0};
    result.src_files       = all_src_files   ? *all_src_files   : (RDIM_SrcFileChunkList){0};
    result.line_tables     = all_line_tables ? *all_line_tables : (RDIM_LineTableChunkList){0};
  }
  
#undef d2r_type_from_builtin_kind
  lane_sync();
  scratch_end(scratch);
  return result;
}
