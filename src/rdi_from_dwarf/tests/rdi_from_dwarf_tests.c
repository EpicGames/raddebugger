// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

internal DW_Writer *
d2rt_line_writer_begin(void)
{
  DW_Writer *writer = dw_writer_begin(DW_Format_32Bit, DW_Version_5, DW_CompUnitKind_Compile, Arch_x64);
  dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
  dw_writer_push_attrib_string(writer, DW_AttribKind_CompDir, str8_lit("c:/d2r"));
  dw_writer_push_attrib_string(writer, DW_AttribKind_Name, str8_lit("first.c"));
  dw_writer_push_attrib_uint(writer, DW_AttribKind_Language, DW_Language_C);
  dw_writer_push_attrib_line_ptr(writer, DW_AttribKind_StmtList, 0);
  dw_writer_tag_end(writer);
  return writer;
}

internal RDIM_BakeParams
d2rt_convert_lines(Arena *arena, DW_Raw raw)
{
  D2R_ConvertParams params = {0};
  params.exe_name = str8_lit("line_test");
  params.arch = Arch_x64;
  params.base_vaddr = 0x1000;
  params.raw = raw;
  params.subset_flags = RDIM_SubsetFlag_All;
  params.deterministic = 1;
  RDIM_BinarySection *section = rdim_binary_section_list_push(arena, &params.binary_sections);
  section->name = str8_lit(".text");
  section->voff_opl = 0x1000;
  
  // The converter broadcasts even with one lane; restore the runner's context afterwards.
  U64 broadcast_memory = 0;
  LaneCtx convert_lane_ctx = {0, 1, {0}, &broadcast_memory};
  LaneCtx restore_lane_ctx = lane_ctx(convert_lane_ctx);
  RDIM_BakeParams result = d2r_convert(arena, &params);
  lane_ctx(restore_lane_ctx);
  return result;
}

internal B32
d2rt_line_sequence_matches(RDIM_LineSequence *seq, String8 path, U64 *voffs, U32 *lines, U64 line_count)
{
  if(seq->line_count != line_count || seq->src_file == 0 ||
     !str8_match(seq->src_file->path, path, StringMatchFlag_SlashInsensitive))
  {
    return 0;
  }
  for EachIndex(idx, line_count)
  {
    if(seq->voffs[idx] != voffs[idx] || seq->line_nums[idx] != lines[idx])
    {
      return 0;
    }
  }
  return seq->voffs[line_count] == voffs[line_count];
}

Test(d2r_line_eof_without_end_sequence)
{
  for(B32 advance_at_eof = 0; advance_at_eof <= 1; advance_at_eof += 1)
  {
    DW_Writer *writer = d2rt_line_writer_begin();
    DW_WriterFile *file = dw_writer_new_file(writer, str8_lit("first.c"));
    dw_writer_line_emit(writer, file, 10, 0, 0x1010);
    dw_writer_line_emit(writer, file, 11, 0, 0x1014);
    if(advance_at_eof)
    {
      dw_writer_line_set_address(writer, 0x1030);
    }
    DW_Raw raw = dw_input_from_writer_sections(arena, writer);
    dw_writer_end(&writer);
    
    // Remove the writer's automatic end_sequence, keeping the line unit's length valid.
    String8 *line_data = &raw.sec[DW_SectionKind_Line].data;
    U8 terminator[] = {DW_StdOpcode_ExtendedOpcode, 1, DW_ExtOpcode_EndSequence};
    TestCheck(line_data->size >= sizeof(U32) + sizeof(terminator));
    TestCheck(str8_match(str8_skip(*line_data, line_data->size - sizeof(terminator)), str8_array_fixed(terminator), 0));
    U32 unit_length = 0;
    MemoryCopy(&unit_length, line_data->str, sizeof(unit_length));
    TestCheck(unit_length == line_data->size - sizeof(unit_length));
    line_data->size -= sizeof(terminator);
    unit_length -= sizeof(terminator);
    MemoryCopy(line_data->str, &unit_length, sizeof(unit_length));
    
    // Conversion output must outlive the converter's nested scratch scopes.
    Arena *convert_arena = arena_alloc();
    RDIM_BakeParams result = d2rt_convert_lines(convert_arena, raw);
    TestCheck(result.line_tables.total_count == 1 && result.line_tables.first != 0);
    TestCheck(result.line_tables.first->count == 1);
    RDIM_LineTable *table = &result.line_tables.first->v[0];
    TestCheck(result.units.total_count == 1 && result.units.first != 0);
    TestCheck(result.units.first->v[0].line_table == table);
    TestCheck(table->seq_count == 1 && table->line_count == 2);
    TestCheck(result.line_tables.total_seq_count == 1 && result.line_tables.total_line_count == 2);
    TestCheck(table->first_seq != 0 && table->first_seq == table->last_seq && table->last_seq->next == 0);
    
    // EOF supplies only the terminal voff, whether the last opcode emits a row or advances the PC.
    U64 voffs[] = {0x10, 0x14, advance_at_eof ? 0x30 : 0x14};
    U32 lines[] = {10, 11};
    RDIM_LineSequence *seq = &table->first_seq->v;
    TestCheck(d2rt_line_sequence_matches(seq, str8_lit("c:/d2r/first.c"), voffs, lines, ArrayCount(lines)));
    TestCheck(seq->src_file->total_line_count == 2);
    TestCheck(seq->src_file->first_line_map_fragment != 0);
    TestCheck(seq->src_file->first_line_map_fragment->seq == seq && seq->src_file->first_line_map_fragment->next == 0);
    arena_release(convert_arena);
  }
}

Test(d2r_line_end_sequence_then_second_sequence)
{
  DW_Writer *writer = d2rt_line_writer_begin();
  DW_WriterFile *file = dw_writer_new_file(writer, str8_lit("first.c"));
  dw_writer_line_emit(writer, file, 10, 0, 0x1010);
  dw_writer_line_emit(writer, file, 11, 0, 0x1014);
  dw_writer_line_set_address(writer, 0x1020);
  dw_writer_line_end_sequence(writer);
  dw_writer_line_emit(writer, file, 20, 0, 0x1050);
  dw_writer_line_emit(writer, file, 21, 0, 0x1054);
  dw_writer_line_set_address(writer, 0x1060);
  dw_writer_line_end_sequence(writer);
  DW_Raw raw = dw_input_from_writer_sections(arena, writer);
  dw_writer_end(&writer);
  
  // Conversion output must outlive the converter's nested scratch scopes.
  Arena *convert_arena = arena_alloc();
  RDIM_BakeParams result = d2rt_convert_lines(convert_arena, raw);
  TestCheck(result.line_tables.total_count == 1 && result.line_tables.first != 0);
  TestCheck(result.line_tables.first->count == 1);
  RDIM_LineTable *table = &result.line_tables.first->v[0];
  TestCheck(table->seq_count == 2 && table->line_count == 4);
  TestCheck(result.line_tables.total_seq_count == 2 && result.line_tables.total_line_count == 4);
  TestCheck(table->first_seq != 0 && table->first_seq->next != 0);
  TestCheck(table->first_seq->next == table->last_seq && table->last_seq->next == 0);
  
  // end-sequence rows supply bounds, not source rows, and the second sequence starts from reset registers
  U64 first_voffs[] = {0x10, 0x14, 0x20};
  U32 first_lines[] = {10, 11};
  U64 second_voffs[] = {0x50, 0x54, 0x60};
  U32 second_lines[] = {20, 21};
  RDIM_LineSequence *first = &table->first_seq->v;
  RDIM_LineSequence *second = &table->last_seq->v;
  TestCheck(d2rt_line_sequence_matches(first, str8_lit("c:/d2r/first.c"), first_voffs, first_lines, ArrayCount(first_lines)));
  TestCheck(d2rt_line_sequence_matches(second, str8_lit("c:/d2r/first.c"), second_voffs, second_lines, ArrayCount(second_lines)));
  TestCheck(first->src_file == second->src_file && first->src_file->total_line_count == 4);
  RDIM_SrcFileLineMapFragment *fragment = first->src_file->first_line_map_fragment;
  TestCheck(fragment != 0 && fragment->seq == first);
  TestCheck(fragment->next != 0 && fragment->next->seq == second && fragment->next->next == 0);
  TestCheck(first->src_file->last_line_map_fragment == fragment->next);
  arena_release(convert_arena);
}

Test(d2r_line_file_changes)
{
  DW_Writer *writer = d2rt_line_writer_begin();
  DW_WriterFile *first_file = dw_writer_new_file(writer, str8_lit("first.c"));
  DW_WriterFile *second_file = dw_writer_new_file(writer, str8_lit("second.c"));
  dw_writer_line_emit(writer, first_file, 10, 0, 0x1010);
  dw_writer_line_emit(writer, first_file, 11, 0, 0x1014);
  dw_writer_line_set_address(writer, 0x1020);
  dw_writer_line_emit(writer, second_file, 11, 0, 0x1030);
  dw_writer_line_emit(writer, second_file, 12, 0, 0x1034);
  dw_writer_line_set_address(writer, 0x1040);
  dw_writer_line_emit(writer, first_file, 11, 0, 0x1050);
  dw_writer_line_set_address(writer, 0x1060);
  dw_writer_line_end_sequence(writer);
  DW_Raw raw = dw_input_from_writer_sections(arena, writer);
  dw_writer_end(&writer);
  
  // conversion output must outlive the converter's nested scratch scopes
  Arena *convert_arena = arena_alloc();
  RDIM_BakeParams result = d2rt_convert_lines(convert_arena, raw);
  TestCheck(result.line_tables.total_count == 1 && result.line_tables.first != 0);
  TestCheck(result.line_tables.first->count == 1);
  RDIM_LineTable *table = &result.line_tables.first->v[0];
  TestCheck(table->seq_count == 3 && table->line_count == 5);
  TestCheck(result.line_tables.total_seq_count == 3 && result.line_tables.total_line_count == 5);
  TestCheck(table->first_seq != 0 && table->first_seq->next != 0);
  TestCheck(table->first_seq->next->next != 0 && table->first_seq->next->next == table->last_seq);
  TestCheck(table->last_seq->next == 0);
  
  // file changes terminate at the PC of set_file, before the next emitted row advances it
  U64 first_voffs[] = {0x10, 0x14, 0x20};
  U32 first_lines[] = {10, 11};
  U64 second_voffs[] = {0x30, 0x34, 0x40};
  U32 second_lines[] = {11, 12};
  U64 third_voffs[] = {0x50, 0x60};
  U32 third_lines[] = {11};
  RDIM_LineSequence *first = &table->first_seq->v;
  RDIM_LineSequence *second = &table->first_seq->next->v;
  RDIM_LineSequence *third = &table->last_seq->v;
  TestCheck(d2rt_line_sequence_matches(first, str8_lit("c:/d2r/first.c"), first_voffs, first_lines, ArrayCount(first_lines)));
  TestCheck(d2rt_line_sequence_matches(second, str8_lit("c:/d2r/second.c"), second_voffs, second_lines, ArrayCount(second_lines)));
  TestCheck(d2rt_line_sequence_matches(third, str8_lit("c:/d2r/first.c"), third_voffs, third_lines, ArrayCount(third_lines)));
  TestCheck(result.src_files.total_count == 2);
  TestCheck(first->src_file == third->src_file && first->src_file != second->src_file);
  TestCheck(first->src_file->total_line_count == 3 && second->src_file->total_line_count == 2);
  RDIM_SrcFileLineMapFragment *first_fragment = first->src_file->first_line_map_fragment;
  RDIM_SrcFileLineMapFragment *second_fragment = second->src_file->first_line_map_fragment;
  TestCheck(first_fragment != 0 && first_fragment->seq == first);
  TestCheck(first_fragment->next != 0 && first_fragment->next->seq == third && first_fragment->next->next == 0);
  TestCheck(first->src_file->last_line_map_fragment == first_fragment->next);
  TestCheck(second_fragment != 0 && second_fragment->seq == second && second_fragment->next == 0);
  TestCheck(second->src_file->last_line_map_fragment == second_fragment);
  arena_release(convert_arena);
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
