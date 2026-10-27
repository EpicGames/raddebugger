// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

internal D2R_ConvertParams
d2r_test_location_params(Arena *arena, String8 frame_base, String8 location)
{
  // One DWARF 5 unit containing a base type, a procedure with a frame base,
  // and a local variable. All abbreviation operands fit in one ULEB128 byte.
  U8 abbrev[] =
  {
    1, DW_TagKind_CompileUnit, 1,
    DW_AttribKind_Name, DW_FormKind_String,
    DW_AttribKind_Language, DW_FormKind_Data2,
    DW_AttribKind_LowPc, DW_FormKind_Addr,
    DW_AttribKind_HighPc, DW_FormKind_Data4,
    DW_AttribKind_StmtList, DW_FormKind_SecOffset, 0, 0,
    2, DW_TagKind_BaseType, 0,
    DW_AttribKind_Name, DW_FormKind_String,
    DW_AttribKind_Encoding, DW_FormKind_Data1,
    DW_AttribKind_ByteSize, DW_FormKind_Data1, 0, 0,
    3, DW_TagKind_SubProgram, 1,
    DW_AttribKind_Name, DW_FormKind_String,
    DW_AttribKind_LowPc, DW_FormKind_Addr,
    DW_AttribKind_HighPc, DW_FormKind_Data4,
    DW_AttribKind_FrameBase, DW_FormKind_ExprLoc, 0, 0,
    4, DW_TagKind_Variable, 0,
    DW_AttribKind_Name, DW_FormKind_String,
    DW_AttribKind_Type, DW_FormKind_Ref4,
    DW_AttribKind_Location, DW_FormKind_ExprLoc, 0, 0,
    0,
  };
  Assert(frame_base.size < 128 && location.size < 128);
  String8List info = {0};
  str8_serial_begin(arena, &info);
  U32 *info_size = str8_serial_push_u32(arena, &info, 0);
  str8_serial_push_u16(arena, &info, 5);
  str8_serial_push_u8(arena, &info, DW_CompUnitKind_Compile);
  str8_serial_push_u8(arena, &info, 8);
  str8_serial_push_u32(arena, &info, 0);
  str8_serial_push_u8(arena, &info, 1);
  str8_serial_push_cstr(arena, &info, s("locations.c"));
  str8_serial_push_u16(arena, &info, DW_Language_C);
  str8_serial_push_u64(arena, &info, 0x400000);
  str8_serial_push_u32(arena, &info, 0x1000);
  str8_serial_push_u32(arena, &info, 0);
  U32 type_off = (U32)info.total_size;
  str8_serial_push_u8(arena, &info, 2);
  str8_serial_push_cstr(arena, &info, s("unsigned long long"));
  str8_serial_push_u8(arena, &info, DW_ATE_Unsigned);
  str8_serial_push_u8(arena, &info, 8);
  str8_serial_push_u8(arena, &info, 3);
  str8_serial_push_cstr(arena, &info, s("procedure"));
  str8_serial_push_u64(arena, &info, 0x400100);
  str8_serial_push_u32(arena, &info, 0x40);
  str8_serial_push_u8(arena, &info, (U8)frame_base.size);
  str8_serial_push_string(arena, &info, frame_base);
  str8_serial_push_u8(arena, &info, 4);
  str8_serial_push_cstr(arena, &info, s("local"));
  str8_serial_push_u32(arena, &info, type_off);
  str8_serial_push_u8(arena, &info, (U8)location.size);
  str8_serial_push_string(arena, &info, location);
  str8_serial_push_u8(arena, &info, 0);
  str8_serial_push_u8(arena, &info, 0);
  *info_size = (U32)info.total_size - 4;

  // A minimal line table with one directory, one file, and an empty sequence.
  U8 line_header[] =
  {
    1, 1, 1, 0, 1, 1, // instruction lengths, is_stmt, line_base/range, opcode_base
    1, DW_LNCT_Path, DW_FormKind_String, 1, '.', 0,
    1, DW_LNCT_Path, DW_FormKind_String, 1, 'l', 'o', 'c', 'a', 't', 'i', 'o', 'n', 's', '.', 'c', 0,
  };
  U8 line_program[] = {0, 1, DW_ExtOpcode_EndSequence};
  String8List line = {0};
  str8_serial_begin(arena, &line);
  str8_serial_push_u32(arena, &line, 8 + sizeof(line_header) + sizeof(line_program));
  str8_serial_push_u16(arena, &line, 5);
  str8_serial_push_u8(arena, &line, 8);
  str8_serial_push_u8(arena, &line, 0);
  str8_serial_push_u32(arena, &line, sizeof(line_header));
  str8_serial_push_array(arena, &line, line_header, ArrayCount(line_header));
  str8_serial_push_array(arena, &line, line_program, ArrayCount(line_program));

  D2R_ConvertParams params = {0};
  params.arch = Arch_x64;
  params.base_vaddr = 0x400000;
  params.exe_name = s("locations");
  params.path_style = PathStyle_UnixAbsolute;
  params.subset_flags = RDIM_SubsetFlag_All;
  params.deterministic = 1;
  params.raw.sec[DW_SectionKind_Abbrev].data = str8_copy(arena, str8(abbrev, sizeof(abbrev)));
  params.raw.sec[DW_SectionKind_Info].data = str8_serial_end(arena, &info);
  params.raw.sec[DW_SectionKind_Line].data = str8_serial_end(arena, &line);
  RDIM_BinarySection *section = rdim_binary_section_list_push(arena, &params.binary_sections);
  section->name = s(".text");
  section->voff_opl = 0x1000;
  return params;
}

Test(d2r_location_kinds)
{
  struct
  {
    String8 name;
    String8 expr;
    RDI_LocationKind kind;
    RDI_EvalOp required_op;
  }
  cases[] =
  {
    {s("constant address"),       s("\x10\x80\x20"),                     RDI_LocationKind_AddrBytecodeStream},
    {s("constant value"),         s("\x10\x80\x20\x9f"),                 RDI_LocationKind_ValBytecodeStream},
    {s("literal address"),        s("\x31"),                             RDI_LocationKind_AddrBytecodeStream},
    {s("literal value"),          s("\x31\x9f"),                         RDI_LocationKind_ValBytecodeStream},
    {s("breg address"),           s("\x76\x00"),                         RDI_LocationKind_AddrBytecodeStream},
    {s("breg value"),             s("\x76\x00\x9f"),                     RDI_LocationKind_ValReg},
    {s("breg offset address"),    s("\x76\x70"),                         RDI_LocationKind_AddrRegPlusOff},
    {s("breg offset value"),      s("\x76\x70\x9f"),                     RDI_LocationKind_ValBytecodeStream},
    {s("bregx address"),          s("\x92\x06\x00"),                     RDI_LocationKind_AddrBytecodeStream},
    {s("bregx value"),            s("\x92\x06\x00\x9f"),                 RDI_LocationKind_ValReg},
    {s("fbreg address"),          s("\x91\x78\x23\x01"),                 RDI_LocationKind_AddrBytecodeStream},
    {s("fbreg value"),            s("\x91\x78\x23\x01\x9f"),             RDI_LocationKind_ValBytecodeStream},
    {s("CFA address"),            s("\x9c"),                             RDI_LocationKind_AddrBytecodeStream},
    {s("CFA value"),              s("\x9c\x9f"),                         RDI_LocationKind_ValBytecodeStream},
    {s("arithmetic address"),     s("\x31\x32\x22"),                     RDI_LocationKind_AddrBytecodeStream},
    {s("arithmetic value"),       s("\x31\x32\x22\x9f"),                 RDI_LocationKind_ValBytecodeStream},
    {s("plus_uconst address"),    s("\x76\x00\x23\x01"),                 RDI_LocationKind_AddrBytecodeStream},
    {s("plus_uconst value"),      s("\x76\x00\x23\x01\x9f"),             RDI_LocationKind_ValBytecodeStream},
    {s("deref address"),          s("\x76\x00\x06"),                     RDI_LocationKind_AddrBytecodeStream, RDI_EvalOp_MemRead},
    {s("deref value"),            s("\x76\x00\x06\x9f"),                 RDI_LocationKind_ValBytecodeStream,  RDI_EvalOp_MemRead},
    {s("deref_size address"),     s("\x76\x00\x94\x04"),                 RDI_LocationKind_AddrBytecodeStream, RDI_EvalOp_MemRead},
    {s("deref_size value"),       s("\x76\x00\x94\x04\x9f"),             RDI_LocationKind_ValBytecodeStream,  RDI_EvalOp_MemRead},
    {s("module address"),        s("\x03\x00\x10\x40\x00\x00\x00\x00\x00"), RDI_LocationKind_ModuleOff},
    {s("module value"),          s("\x03\x00\x10\x40\x00\x00\x00\x00\x00\x9f"), RDI_LocationKind_ValBytecodeStream},
    {s("module add address"),    s("\x03\x00\x10\x40\x00\x00\x00\x00\x00\x31\x22"), RDI_LocationKind_ModuleOff},
    {s("module add value"),      s("\x03\x00\x10\x40\x00\x00\x00\x00\x00\x31\x22\x9f"), RDI_LocationKind_ValBytecodeStream},
    {s("reverse add address"),   s("\x31\x03\x00\x10\x40\x00\x00\x00\x00\x00\x22"), RDI_LocationKind_ModuleOff},
    {s("reverse add value"),     s("\x31\x03\x00\x10\x40\x00\x00\x00\x00\x00\x22\x9f"), RDI_LocationKind_ValBytecodeStream},
    {s("TLS address"),           s("\x10\x10\x9b"),                     RDI_LocationKind_TLSOff},
    {s("TLS value"),             s("\x10\x10\x9b\x9f"),                 RDI_LocationKind_ValBytecodeStream},
    {s("register location"),     s("\x56"),                             RDI_LocationKind_ValReg},
    {s("regx location"),         s("\x90\x06"),                         RDI_LocationKind_ValReg},
    {s("implicit value"),        s("\x9e\x04\x78\x56\x34\x12"),         RDI_LocationKind_ValBytecodeStream},
    // Keep composition operations following a terminated simple expression.
    // Their execution is not implemented yet; do not turn them into a value.
    {s("stack value piece"),     s("\x31\x9f\x93\x08"),                 RDI_LocationKind_NULL, RDI_EvalOp_PartialValue},
    {s("stack value bit piece"), s("\x31\x9f\x9d\x40\x00"),             RDI_LocationKind_NULL, RDI_EvalOp_PartialValueBit},
  };
  String8 frame_bases[] =
  {
    s("\x9c"),     // CFA
    s("\x56"),     // register location
    s("\x76\x70"), // register + signed offset (also exercises compact frame bases)
    s("\x10\x80\x40\x9f"), // constant stack value
    s("\x03\x00\x10\x40\x00\x00\x00\x00\x00\x9f"), // module-based stack value
  };
  for EachElement(frame_idx, frame_bases)
  {
    for EachElement(case_idx, cases)
    {
      test_outf("frame base %I64u, %S\n", frame_idx, cases[case_idx].name);
      Arena *convert_arena = arena_alloc();
      D2R_ConvertParams params = d2r_test_location_params(convert_arena, frame_bases[frame_idx], cases[case_idx].expr);
      U64 broadcast_memory = 0;
      Barrier barrier = barrier_alloc(1);
      LaneCtx restore_lane_ctx = tctx_set_lane_ctx((LaneCtx){.lane_count = 1, .barrier = barrier, .broadcast_memory = &broadcast_memory});
      RDIM_BakeParams converted = d2r_convert(convert_arena, &params);
      tctx_set_lane_ctx(restore_lane_ctx);
      barrier_release(barrier);
      TestCheck(converted.units.total_count == 1);
      RDIM_Unit *unit = &converted.units.first->v[0];
      TestCheck(unit->procedures.total_count == 1);
      RDIM_Symbol *procedure = &unit->procedures.first->v[0];
      TestCheck(procedure->location_cases.count == 1);
      TestCheck(procedure->root_scope != 0);
      RDIM_SymbolChunkList *locals = &procedure->root_scope->locals;
      RDIM_SymbolChunkList *variables = locals->total_count ? locals : &unit->thread_variables;
      TestCheck(variables->total_count == 1);
      RDIM_Symbol *variable = &variables->first->v[0];
      TestCheck(variable->location_cases.count == 1);
      RDIM_Location *location = &variable->location_cases.first->location;
      if(cases[case_idx].kind != RDI_LocationKind_NULL)
      {
        test_outf("location kind: got %u, expected %u\n", location->kind, cases[case_idx].kind);
        TestCheck(location->kind == cases[case_idx].kind);
      }
      if(cases[case_idx].required_op != 0)
      {
        U64 matches = 0;
        for EachNode(op, RDIM_EvalBytecodeOp, location->bytecode.first_op)
        {
          matches += (op->op == cases[case_idx].required_op);
        }
        TestCheck(matches == 1);
      }
      arena_release(convert_arena);
    }
  }
}

SkippedTest(d2r_regressions)
{
  String8 radbin_path = test_build_exe_path(arena, s("radbin"));
  String8 bin_paths[] =
  {
    test_input_path(arena, ctx, s("raddbg_linux_dac63d6b/raddbg")),
  };
  
  // rjf: generate RDIs
  {
    ProcessList processes = {0};
    for EachElement(bin_idx, bin_paths)
    {
      String8 bin_path = bin_paths[bin_idx];
      String8 rdi_path = str8f(arena, "%S/%I64u.rdi", ctx->artifacts_path, bin_idx);
      Process process = launch_cmd_line(str8f(arena, "%S --rdi --deterministic %S --out:%S", radbin_path, bin_path, rdi_path));
      process_list_push(arena, &processes, process);
    }
    for EachNode(n, ProcessNode, processes.first)
    {
      process_join(n->v, max_U64, 0);
    }
  }
  
  // rjf: generate dumps
  {
    ProcessList processes = {0};
    for EachElement(bin_idx, bin_paths)
    {
      String8 rdi_path = str8f(arena, "%S/%I64u.rdi", ctx->artifacts_path, bin_idx);
      String8 dump_path = str8f(arena, "%S/current_%I64u", ctx->artifacts_path, bin_idx);
      Process process = launch_cmd_line(str8f(arena, "%S --dump --deterministic %S --out:%S", radbin_path, rdi_path, dump_path));
      process_list_push(arena, &processes, process);
    }
    for EachNode(n, ProcessNode, processes.first)
    {
      process_join(n->v, max_U64, 0);
    }
  }
  
  // rjf: check against exemplars
  {
    for EachElement(bin_idx, bin_paths)
    {
      Temp scratch = scratch_begin(&arena, 1);
      String8 dump_path_current = str8f(scratch.arena, "%S/current_%I64u", ctx->artifacts_path, bin_idx);
      String8 dump_data_current = data_from_file_path(scratch.arena, dump_path_current);
      String8 dump_path_exemplar = test_exemplar_path(scratch.arena, ctx, str8f(scratch.arena, "exemplar_%I64u", bin_idx));
      String8 dump_data_exemplar = data_from_file_path(scratch.arena, dump_path_exemplar);
      B32 dump_matches = str8_match(dump_data_current, dump_data_exemplar, 0);
      if(!dump_matches)
      {
        String8 diff_cmd = str8f(scratch.arena, "diff %S %S",
                                 path_normalized_from_string(scratch.arena, dump_path_current),
                                 path_normalized_from_string(scratch.arena, dump_path_exemplar));
        test_outf("Current log does not match exemplar; run `%S`\n", diff_cmd);
      }
      TestCheck(dump_matches);
      scratch_end(scratch);
    }
  }
}

Test(d2r_determinism)
{
  U64 num_repeats_per_bin = 16;
  String8 radbin_path = test_build_exe_path(arena, s("radbin"));
  String8 bin_paths[] =
  {
    test_input_path(arena, ctx, s("raddbg_linux_dac63d6b/raddbg")),
  };
  for EachElement(bin_idx, bin_paths)
  {
    // rjf: unpack paths, make output directory
    String8 bin_path = bin_paths[bin_idx];
    
    // rjf: generate all RDIs
    String8List rdi_paths = {0};
    String8List dump_paths = {0};
    {
      ProcessList processes = {0};
      for EachIndex(repeat_idx, num_repeats_per_bin)
      {
        String8 rdi_name = str8f(arena, "repeat_%I64u.rdi", repeat_idx);
        String8 rdi_path = str8f(arena, "%S/%S", ctx->artifacts_path, rdi_name);
        str8_list_push(arena, &rdi_paths, rdi_path);
        String8 cmdl = str8f(arena, "%S --rdi --deterministic %S --out:%S", radbin_path, bin_path, rdi_path);
        Process process = launch_cmd_line(cmdl);
        TestCheck(!process_match(process_zero(), process));
        process_list_push(arena, &processes, process);
      }
      for EachNode(n, ProcessNode, processes.first)
      {
        process_join(n->v, max_U64, 0);
      }
    }
    
    // rjf: generate all dumps
    {
      ProcessList processes = {0};
      for EachNode(n, String8Node, rdi_paths.first)
      {
        String8 rdi_path = n->string;
        String8 dump_path = str8f(arena, "%S.dump", rdi_path);
        str8_list_push(arena, &dump_paths, dump_path);
        Process process_handle = launch_cmd_linef("%S --dump --deterministic %S --out:%S", radbin_path, rdi_path, dump_path);
        TestCheck(!process_match(process_zero(), process_handle));
        process_list_push(arena, &processes, process_handle);
      }
      for EachNode(n, ProcessNode, processes.first)
      {
        process_join(n->v, max_U64, 0);
      }
    }
    
    // rjf: gather all hashes/paths
    U64 rdi_hashes_count = rdi_paths.node_count;
    U128 *rdi_hashes = push_array(arena, U128, rdi_hashes_count);
    String8 *rdi_paths_array = push_array(arena, String8, rdi_hashes_count);
    U64 dump_hashes_count = dump_paths.node_count;
    U128 *dump_hashes = push_array(arena, U128, dump_hashes_count);
    String8 *dump_paths_array = push_array(arena, String8, dump_hashes_count);
    {
      U64 idx = 0;
      for EachNode(n, String8Node, rdi_paths.first)
      {
        Temp scratch = scratch_begin(0, 0);
        String8 rdi_path = n->string;
        String8 path = rdi_path;
        String8 data = data_from_file_path(scratch.arena, path);
        TestCheck(data.size != 0);
        rdi_hashes[idx] = u128_hash_from_str8(data);
        rdi_paths_array[idx] = path;
        scratch_end(scratch);
        idx += 1;
      }
    }
    {
      U64 idx = 0;
      for EachNode(n, String8Node, dump_paths.first)
      {
        Temp scratch = scratch_begin(0, 0);
        String8 path = n->string;
        String8 data = data_from_file_path(scratch.arena, path);
        TestCheck(data.size != 0);
        dump_hashes[idx] = u128_hash_from_str8(data);
        dump_paths_array[idx] = path;
        scratch_end(scratch);
        idx += 1;
      }
    }
    
    // rjf: determine if all hashes match
    U64 mismatch_num = 0;
    for EachIndex(idx, rdi_hashes_count)
    {
      if(!u128_match(rdi_hashes[idx], rdi_hashes[0]))
      {
        mismatch_num = idx+1;
        break;
      }
    }
    for EachIndex(idx, dump_hashes_count)
    {
      if(!u128_match(dump_hashes[idx], dump_hashes[0]))
      {
        mismatch_num = idx+1;
        break;
      }
    }
    
    // rjf: output bad case info
    if(mismatch_num != 0)
    {
      U64 idx = mismatch_num-1;
      test_outf("  bin[%I64u] \"%S\"\n", idx, bin_path);
      test_outf("    rdi[%I64u] 0x%I64x:%I64x \"%S\"\n", idx, rdi_hashes[idx].u64[0], rdi_hashes[idx].u64[1], rdi_paths_array[idx]);
      test_outf("    dump[%I64u] 0x%I64x:%I64x \"%S\"\n", idx, dump_hashes[idx].u64[0], dump_hashes[idx].u64[1], dump_paths_array[idx]);
    }
    TestCheck(mismatch_num == 0);
  }
}
