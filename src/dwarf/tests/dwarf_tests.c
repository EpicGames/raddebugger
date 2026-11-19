// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

internal U64
t_dw_test_uleb128(U64 v, U64 expected_length)
{
  Temp scratch = scratch_begin(0, 0);
  B32 is_ok = 0;
  
  U64 v0 = v;
  String8 e = dw_write_uleb128(scratch.arena, v0);
  if (!(expected_length == e.size)) { goto exit; }
  
  U64 v1;
  U64 bytes_read = str8_deserial_read_uleb128(e, 0, &v1);
  if (!(bytes_read == e.size)) { goto exit; }
  if (!(v0 == v1)) { goto exit; }
  
  is_ok = 1;
  exit:;
  scratch_end(scratch);
  return is_ok;
}

internal U64
t_dw_test_sleb128(S64 v, U64 expected_length)
{
  Temp scratch = scratch_begin(0,0);
  B32 is_ok = 0;
  
  S64 v0 = v;
  String8 e = dw_write_sleb128(scratch.arena, v0);
  if (!(expected_length == e.size)) { goto exit; }
  
  S64 v1;
  U64 bytes_read = str8_deserial_read_sleb128(e, 0, &v1);
  if (!(bytes_read == e.size)) { goto exit; }
  if (!(v0 == v1)) { goto exit; }
  
  is_ok = 1;
  exit:;
  scratch_end(scratch);
  return is_ok;
}

Test(test_leb128)
{
  TestCheck(t_dw_test_uleb128(0, 1));
  TestCheck(t_dw_test_sleb128(0, 1));
  TestCheck(t_dw_test_sleb128(-1, 1));
  
  TestCheck(t_dw_test_uleb128(max_U64, 10));
  TestCheck(t_dw_test_sleb128(min_S64, 10));
  TestCheck(t_dw_test_sleb128(max_S64, 10));
  
  TestCheck(t_dw_test_uleb128(0xDEADBEEFCAFEBABE, 10));
  
  for EachIndex(i, 64) {
    TestCheck(t_dw_test_uleb128((1ull << i), 1 + (i / 7)));
  }
  for EachIndex(i, 64) {
    TestCheck(t_dw_test_sleb128((1ull << i), 1 + (i + 1) / 7));
  }
}

internal B32
dw_test__form_match(DW_WriterEncodedForm a, DW2_FormVal b, String8 info)
{
  B32 is_match = 0;
  
  if (a.kind == b.kind) {
    switch (a.kind) {
      case DW_FormKind_Null: { is_match = 1; } break;
      case DW_FormKind_Addr: {
        U64 addr = 0;
        if (a.addr.size <= sizeof(addr)) {
          MemoryCopy(&addr, a.addr.str, a.addr.size);
          is_match = addr == b.addr && addr == b.u128.u64[0];
        }
      } break;
      case DW_FormKind_String: { is_match = str8_match(a.string, b.string, 0); } break;
      case DW_FormKind_ExprLoc: {
        U64 off = b.u128.u64[0], size = b.u128.u64[1];
        is_match = off <= info.size && size <= info.size - off &&
                   str8_match(a.exprloc, str8_substr(info, r1u64(off, off + size)), 0);
      } break;
      
      case DW_FormKind_Block: { is_match = str8_match(a.block, b.string, 0); } break;
      case DW_FormKind_Block1:
      case DW_FormKind_Block2:
      case DW_FormKind_Block4: {
        U64 prefix_size = a.kind == DW_FormKind_Block1 ? 1 : a.kind == DW_FormKind_Block2 ? 2 : 4;
        U64 off = b.u128.u64[1], size = b.u128.u64[0];
        if (off <= info.size && prefix_size <= info.size - off) {
          off += prefix_size;
          is_match = size <= info.size - off && str8_match(a.block, str8_substr(info, r1u64(off, off + size)), 0);
        }
      } break;
      
      case DW_FormKind_Data1:
      case DW_FormKind_Data2:
      case DW_FormKind_Data4:
      case DW_FormKind_Data8:
      case DW_FormKind_Data16: {
        U128 data = {0};
        U64 size = a.kind == DW_FormKind_Data1 ? 1 : a.kind == DW_FormKind_Data2 ? 2 :
                   a.kind == DW_FormKind_Data4 ? 4 : a.kind == DW_FormKind_Data8 ? 8 : 16;
        if (a.data.size == size) {
          MemoryCopy(&data, a.data.str, size);
          is_match = u128_match(data, b.u128);
        }
      } break;
      
      case DW_FormKind_Flag:
      case DW_FormKind_FlagPresent: {
        is_match = a.flag == b.u128.u64[0];
      } break;
      case DW_FormKind_SData: {
        is_match = a.sdata == (S64)b.u128.u64[0];
      } break;
      case DW_FormKind_UData: {
        is_match = a.udata == b.u128.u64[0];
      } break;
      
      case DW_FormKind_RefAddr:
      case DW_FormKind_Ref1:
      case DW_FormKind_Ref2:
      case DW_FormKind_Ref4:
      case DW_FormKind_Ref8:
      case DW_FormKind_RefUData:
      case DW_FormKind_RefSig8:
      case DW_FormKind_RefSup4:
      case DW_FormKind_RefSup8: {
        is_match = a.ref == b.u128.u64[0];
      } break;
      
      case DW_FormKind_SecOffset:
      case DW_FormKind_Strp:
      case DW_FormKind_LineStrp: {
        is_match = a.sec_offset == b.u128.u64[0];
      } break;
      
      case DW_FormKind_ImplicitConst: {
        is_match = a.implicit_const == (S64)b.u128.u64[0];
      } break;
      
      case DW_FormKind_Strx:
      case DW_FormKind_Strx1:
      case DW_FormKind_Strx2:
      case DW_FormKind_Strx3:
      case DW_FormKind_Strx4:
      case DW_FormKind_Addrx:
      case DW_FormKind_Addrx1:
      case DW_FormKind_Addrx2:
      case DW_FormKind_Addrx3:
      case DW_FormKind_Addrx4:
      case DW_FormKind_RngListx:
      case DW_FormKind_LocListx: {
        is_match = a.xval == b.u128.u64[0];
      } break;
      
      case DW_FormKind_StrpSup: {
        is_match = a.strp_sup == b.u128.u64[0];
      } break;
      default: break;
    }
  }
  
  return is_match;
}

// Walk the actual DIE stream, retaining offsets and consuming a null DIE at
// every child-list boundary. DW2_Tag deliberately has no tree or stream offset.
internal B32
dwt_tags_must_match(Arena *arena, DW_Raw *raw, DW2_ParseCtx *ctx, String8 info,
                    U64 *off, DW_WriterTag *writer_tag, DW_WriterTag *parent)
{
  U64 abbrev_id = 0;
  
  // match headers
  if (*off >= info.size || !str8_deserial_read_uleb128(info, *off, &abbrev_id)) { return 0; }
  if (abbrev_id == 0 || abbrev_id != writer_tag->abbrev_id)                     { return 0; }
  if (*off != writer_tag->info_off || writer_tag->parent != parent)             { return 0; }
  
  DW2_Tag tag = {0};
  U64 size = dw2_read_tag(arena, raw, ctx, info, *off, &tag);
  
  if (!size || size > info.size - *off) { return 0; }
  *off += size;

  if (tag.kind != writer_tag->kind || tag.has_children != (writer_tag->first_child != 0) || tag.attribs.count != writer_tag->attrib_count) { return 0; }

  // match attribs
  DW2_AttribNode *reader_attrib = tag.attribs.first;
  for EachNode(writer_attrib, DW_WriterAttrib, writer_tag->first_attrib) {
    if (!reader_attrib || writer_attrib->kind != reader_attrib->v.attrib_kind || !dw_test__form_match(writer_attrib->form.reader, reader_attrib->v.val, info)) { return 0; }
    if (writer_attrib->form.writer.kind == DW_WriterFormKind_Ref && reader_attrib->v.val.u128.u64[0] != writer_attrib->form.writer.ref->info_off) { return 0; }
    reader_attrib = reader_attrib->next;
  }

  if (reader_attrib) { return 0; }
  
  // visit children
  for EachNode(child, DW_WriterTag, writer_tag->first_child) {
    if (!dwt_tags_must_match(arena, raw, ctx, info, off, child, writer_tag)) { return 0; }
  }

  if (tag.has_children) {
    DW2_Tag end = {0};
    U64 end_id = max_U64;
    if (*off >= info.size || str8_deserial_read_uleb128(info, *off, &end_id) != 1 || end_id != 0) { return 0; }
    size = dw2_read_tag(arena, raw, ctx, info, *off, &end);
  
    if (size != 1 || end.kind != DW_TagKind_Null || end.has_children || end.attribs.count) { return 0; }
    *off += size;
  }
  
  return 1;
}

internal U64
dwt_parse_ctx(Arena *arena, DW_Raw *raw, DW2_ParseCtx *ctx, DW2_UnitHeader *header)
{
  U64 off = dw2_read_unit_header(raw->sec[DW_SectionKind_Info].data, 0, header);
  dw2_parse_ctx_equip_unit_header(ctx, header, 0);
  ctx->abbrev_map = push_array(arena, DW2_AbbrevMap, 1);
  *ctx->abbrev_map = dw2_abbrev_map_from_data(arena, raw->sec[DW_SectionKind_Abbrev].data, header->abbrev_off);
  DW2_Tag root = {0};
  dw2_read_tag(arena, raw, ctx, raw->sec[DW_SectionKind_Info].data, off, &root);
  DW2_OffsetTableSet tables = dw2_offset_table_set_from_raw(arena, raw);
  dw2_parse_ctx_equip_unit_root_tag(ctx, &root, &tables);
  return off;
}

internal B32
dwt_writer_matches(Arena *arena, DW_Raw *raw, DW_Writer *writer)
{
  String8 info = raw->sec[DW_SectionKind_Info].data;
  Rng1U64Array ranges = dw2_unit_ranges_from_data(arena, info);
  if (ranges.count != 1 || ranges.v[0].min != 0 || ranges.v[0].max > info.size) { return 0; }
  info = str8_substr(info, ranges.v[0]);
  DW2_ParseCtx ctx = {0};
  DW2_UnitHeader header = {0};
  U64 off = dwt_parse_ctx(arena, raw, &ctx, &header);
  if (header.version != writer->version || header.format != writer->format ||
      header.addr_size != writer->address_size || header.abbrev_off != 0 ||
      (header.version >= DW_Version_5 && header.kind != writer->cu_kind)) { return 0; }
  return dwt_tags_must_match(arena, raw, &ctx, info, &off, writer->root, 0) && off == info.size;
}

internal B32
dwt_line_header(Arena *arena, DW_Raw *raw, DW2_LineTableHeader *header, String8 *program)
{
  DW2_ParseCtx ctx = {0};
  DW2_UnitHeader unit = {0};
  dwt_parse_ctx(arena, raw, &ctx, &unit);
  String8 data = raw->sec[DW_SectionKind_Line].data;
  U64 bytes_read = dw2_read_line_table_header(arena, raw, &ctx, data, 0, header);
  if (!bytes_read || bytes_read > header->line_program_off ||
      header->line_program_off > header->total_unit_data_size || header->total_unit_data_size > data.size) { return 0; }
  *program = str8_substr(data, r1u64(header->line_program_off, header->total_unit_data_size));
  return 1;
}

internal DW_Raw
dw_input_from_writer_sections(Arena *arena, DW_Writer *writer)
{
  dw_writer_emit(writer);
  DW_Raw raw = {0};
  for EachElement(i, raw.sec) {
    raw.sec[i].name = dw_regular_name_from_section_kind(i);
    raw.sec[i].data = str8_list_join(arena, &writer->sections[i].srl, 0);
  }
  return raw;
}

internal DW_Raw
dw_input_from_writer(Arena *arena, DW_Writer *writer)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  OBJ *obj = obj_alloc(0, Arch_x64);
  OBJ_Section *text_section = obj_push_section(obj, str8_lit(".text"), OBJ_SectionFlag_Read|OBJ_SectionFlag_Exec|OBJ_SectionFlag_Load);
  str8_serial_push_u8(obj->arena, &text_section->data, 0x90);
  str8_serial_push_u8(obj->arena, &text_section->data, 0x90);
  str8_serial_push_u8(obj->arena, &text_section->data, 0x90);
  str8_serial_push_u8(obj->arena, &text_section->data, 0xc3);
  obj_push_symbol(obj, str8_lit("entry"), OBJ_SymbolScope_Global, OBJ_RefKind_Section, text_section);
  
  dw_writer_emit_to_obj(writer, obj);
  String8 raw_coff = coff_from_obj(scratch.arena, obj);
  
  t_write_file(str8_lit("dwarf.obj"), raw_coff);
  t_invoke(t_linker_path(), str8_lit("/subsystem:console /entry:entry /out:a.exe /debug:full dwarf.obj"), max_U64);
  delete_file_at_path(t_make_file_path(scratch.arena, str8_lit("a.pdb")));
  
  String8             exe           = t_read_file(arena, str8_lit("a.exe"));
  PE_BinInfo          pe            = pe_bin_info_from_data(scratch.arena, exe);
  COFF_SectionHeader *section_table = (COFF_SectionHeader *)(exe.str + pe.section_table_range.min);
  String8             string_table  = str8_substr(exe, pe.string_table_range);
  DW_Raw            input         = dw_raw_from_coff_section_table(arena, exe, string_table, pe.section_count, section_table);
  
  obj_release(&obj);
  scratch_end(scratch);
  return input;
}

Test(dwarf_32bit)
{
  DW_Writer *writer = dw_writer_begin(DW_Format_32Bit, DW_Version_5, DW_CompUnitKind_Compile, Arch_x64);
  dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
  dw_writer_push_attrib_string(writer, DW_AttribKind_Producer, str8_lit("RAD DWARF WRITER"));
  dw_writer_tag_end(writer);
  
  DW_Raw input = dw_input_from_writer(arena, writer);
  TestCheck(dwt_writer_matches(arena, &input, writer));
  TestCheck(dw2_unit_ranges_from_data(arena, input.sec[DW_SectionKind_Info].data).count == 1);
  TestCheck(dw2_unit_ranges_from_data(arena, input.sec[DW_SectionKind_Line].data).count == 1);

  for EachElement(sec_idx, input.sec) {
    if (sec_idx == DW_SectionKind_Abbrev) continue;
    Rng1U64Array unit_ranges = dw2_unit_ranges_from_data(arena, input.sec[sec_idx].data);
    for EachIndex(range_idx, unit_ranges.count) {
      Rng1U64 range = unit_ranges.v[range_idx];
      
      U32 first_four_bytes = 0;
      TestCheck(str8_deserial_read_struct(input.sec[sec_idx].data, range.min, &first_four_bytes) == sizeof(first_four_bytes));
      TestCheck(first_four_bytes + 4 == dim_1u64(range));
      
      U32 unit_length = 0;
      TestCheck(str8_deserial_read_struct(input.sec[sec_idx].data, range.min, &unit_length) == sizeof(U32));
      TestCheck(unit_length + 4 == dim_1u64(range));
    }
  }
  
  dw_writer_end(&writer);
}

Test(dwarf_64bit)
{
  DW_Writer *writer = dw_writer_begin(DW_Format_64Bit, DW_Version_5, DW_CompUnitKind_Compile, Arch_x64);
  dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
  dw_writer_push_attrib_string(writer, DW_AttribKind_Producer, str8_lit("RAD DWARF WRITER"));
  dw_writer_tag_end(writer);
  
  DW_Raw input = dw_input_from_writer(arena, writer);
  TestCheck(dwt_writer_matches(arena, &input, writer));
  TestCheck(dw2_unit_ranges_from_data(arena, input.sec[DW_SectionKind_Info].data).count == 1);
  TestCheck(dw2_unit_ranges_from_data(arena, input.sec[DW_SectionKind_Line].data).count == 1);

  for EachElement(sec_idx, input.sec) {
    if (sec_idx == DW_SectionKind_Abbrev) continue;
    Rng1U64Array unit_ranges = dw2_unit_ranges_from_data(arena, input.sec[sec_idx].data);
    for EachIndex(range_idx, unit_ranges.count) {
      Rng1U64 range = unit_ranges.v[range_idx];
      
      U32 first_four_bytes = 0;
      TestCheck(str8_deserial_read_struct(input.sec[sec_idx].data, range.min, &first_four_bytes) == sizeof(first_four_bytes));
      TestCheck(first_four_bytes == max_U32);
      
      U64 unit_length = 0;
      TestCheck(str8_deserial_read_struct(input.sec[sec_idx].data, range.min + sizeof(U32), &unit_length) == sizeof(U64));
      TestCheck(unit_length + 12 == dim_1u64(range));
    }
  }
  
  dw_writer_end(&writer);
}

Test(dwarf_line_opcodes)
{
  DW_Writer *writer = dw_writer_begin(DW_Format_32Bit, DW_Version_5, DW_CompUnitKind_Compile, Arch_x64);
  String8 comp_dir  = str8_lit("c:/devel/");
  String8 comp_name = str8_lit("test.c");
  U64     address   = 0xDEADBEEFCAFEBABE;
  dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
  dw_writer_push_attrib_string(writer, DW_AttribKind_Producer, str8_lit("RAD DWARF WRITER"));
  dw_writer_push_attrib_string(writer, DW_AttribKind_CompDir, comp_dir);
  dw_writer_push_attrib_string(writer, DW_AttribKind_Name, comp_name);
  dw_writer_push_attrib_line_ptr(writer, DW_AttribKind_StmtList, 0);
  dw_writer_tag_end(writer);
  
  // test directory table and file table
  DW_WriterFile *file2 = dw_writer_new_file(writer, str8_lit("d:/foobar/qwe.c"));
  file2->time_stamp = 123;
  file2->size       = max_U64;
  file2->md5        = *(U128 *)&((U8[sizeof(U128)]) { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, });
  file2->source     = str8_lit("a quick brown fox jumps over the lazy dog\n");
  DW_WriterFile *file = dw_writer_new_file(writer, comp_name);
  file->time_stamp = max_U64;
  file->size       = 314;
  file->md5        = *(U128 *)&((U8[sizeof(U128)]) { 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, });
  file->source     = str8_lit("int main() { return 057; }\n");
  
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_copy());
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_advance_pc(7));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_advance_pc(-7));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_advance_line(4));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_advance_line(-4));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_file(file));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_column(max_U64));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_column(0));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_negate_stmt());
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_negate_stmt());
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_basic_block());
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_const_add_pc());
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_fixed_advance_pc(max_U16));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_prologue_end());
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_epilogue_begin());
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_isa(max_U64));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNE_set_address(address));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNE_set_discriminator(2));
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNE_end_sequence());
  
  DW_Raw input = dw_input_from_writer(arena, writer);
  DW2_LineTableHeader header = {0};
  String8 program = {0};
  TestCheck(dwt_line_header(arena, &input, &header, &program));
  TestCheck(dwt_writer_matches(arena, &input, writer));
  DW2_LineVM vm = dw2_line_vm_init(&header);

  TestCheck(header.version == DW_Version_5 && header.format == DW_Format_32Bit);
  TestCheck(header.addr_size == 8 && header.segment_selector_size == 0);
  TestCheck(header.min_inst_length == 1 && header.max_ops_per_inst == 1);
  TestCheck(header.default_is_stmt == 1 && header.line_base == -5 && header.line_range == 14);
  TestCheck(header.opcode_base == 13 && header.opcode_lengths_count == 12);
  U8 expected_lengths[] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1};
  TestCheck(str8_match(str8(header.opcode_lengths, header.opcode_lengths_count), str8_array_fixed(expected_lengths), 0));
  TestCheck(header.dirs.count == 2);
  TestCheck(header.files.count == 2);
  TestCheck(str8_match(header.dirs.v[0].file_name, comp_dir, 0));
  TestCheck(str8_match(header.dirs.v[1].file_name, str8_lit("d:/foobar"), 0));

  DW2_LineTableFile *file_reader = &header.files.v[0];
  TestCheck(str8_match(file_reader->file_name, comp_name, 0));
  TestCheck(file_reader->dir_idx == 0);
  TestCheck(file_reader->modify_time == file->time_stamp);
  TestCheck(file_reader->file_size == file->size);
  TestCheck(file_reader->flags == (DW2_LineTableFileFlag_HasMD5|DW2_LineTableFileFlag_HasModifyTime));
  TestCheck(u128_match(file_reader->md5.u128, file->md5));
  TestCheck(str8_match(file_reader->source, file->source, 0));

  DW2_LineTableFile *file2_reader = &header.files.v[1];
  TestCheck(str8_match(file2_reader->file_name, str8_lit("qwe.c"), 0));
  TestCheck(file2_reader->dir_idx == 1);
  TestCheck(file2_reader->modify_time == file2->time_stamp);
  TestCheck(file2_reader->file_size == file2->size);
  TestCheck(file2_reader->flags == (DW2_LineTableFileFlag_HasMD5|DW2_LineTableFileFlag_HasModifyTime));
  TestCheck(u128_match(file2_reader->md5.u128, file2->md5));
  TestCheck(str8_match(file2_reader->source, file2->source, 0));

  TestCheck(!vm.emit_row && !vm.error && vm.off == 0);
  TestCheck(vm.regs.address == 0 && vm.regs.line == 1 && vm.regs.column == 0);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_Copy);
  TestCheck(vm.emit_row == 1);
  TestCheck(vm.regs.discriminator == 0);
  TestCheck(vm.regs.basic_block == 0);
  TestCheck(vm.regs.prologue_end == 0);
  TestCheck(vm.regs.epilogue_begin == 0);
  TestCheck(vm.off == 1 && !vm.error);
  
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_AdvancePc);
  TestCheck(vm.regs.address == 7);
  TestCheck(vm.off == 3 && !vm.emit_row && !vm.error);
  
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_AdvancePc);
  TestCheck(vm.regs.address == 0);
  TestCheck(vm.off == 14 && !vm.emit_row && !vm.error);
  
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
  TestCheck(vm.regs.line == 5);
  TestCheck(vm.off == 16 && !vm.emit_row && !vm.error);
  
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
  TestCheck(vm.regs.line == 1);
  TestCheck(vm.off == 18 && !vm.emit_row && !vm.error);
  
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_SetFile);
  TestCheck(vm.regs.file_index == 0);
  TestCheck(vm.off == 20 && !vm.emit_row && !vm.error);
  
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_SetColumn);
  TestCheck(vm.regs.column == max_U64);
  TestCheck(vm.off == 31 && !vm.emit_row && !vm.error);
  
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_SetColumn);
  TestCheck(vm.regs.column == 0);
  TestCheck(vm.off == 33 && !vm.emit_row && !vm.error);
  
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_NegateStmt);
  TestCheck(vm.regs.is_stmt == 0);
  TestCheck(vm.off == 34 && !vm.emit_row && !vm.error);
  
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_NegateStmt);
  TestCheck(vm.regs.is_stmt == 1);
  TestCheck(vm.off == 35 && !vm.emit_row && !vm.error);
  
  TestCheck(vm.regs.basic_block == 0);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_SetBasicBlock);
  TestCheck(vm.regs.basic_block == 1);
  TestCheck(vm.off == 36 && !vm.emit_row && !vm.error);
  
  {
    U64 addr_before = vm.regs.address;
    U64 const_advance = (0xffu - header.opcode_base) / header.line_range;
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_ConstAddPc);
    TestCheck(!vm.emit_row);
    TestCheck(vm.regs.address == addr_before + const_advance);
    TestCheck(vm.off == 37 && !vm.error);
  }
  
  TestCheck(vm.regs.address == 0x11);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_FixedAdvancePc);
  TestCheck(vm.regs.address == max_U16 + 0x11);
  TestCheck(vm.off == 40 && !vm.emit_row && !vm.error);
  
  TestCheck(vm.regs.prologue_end == 0);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_SetPrologueEnd);
  TestCheck(vm.regs.prologue_end == 1);
  TestCheck(vm.off == 41 && !vm.emit_row && !vm.error);
  
  TestCheck(vm.regs.epilogue_begin == 0);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_SetEpilogueBegin);
  TestCheck(vm.regs.epilogue_begin == 1);
  TestCheck(vm.off == 42 && !vm.emit_row && !vm.error);
  
  TestCheck(vm.regs.isa == 0);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_SetIsa);
  TestCheck(vm.regs.isa == max_U64);
  TestCheck(vm.off == 53 && !vm.emit_row && !vm.error);
  
  TestCheck(vm.regs.address != address);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_ExtendedOpcode);
  TestCheck(vm.ext_opcode == DW_ExtOpcode_SetAddress);
  TestCheck(vm.regs.address == address);
  TestCheck(vm.off == 64 && !vm.emit_row && !vm.error);
  
  TestCheck(vm.regs.discriminator == 0);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_ExtendedOpcode);
  TestCheck(vm.ext_opcode == DW_ExtOpcode_SetDiscriminator);
  TestCheck(vm.regs.discriminator == 2);
  TestCheck(vm.off == 68 && !vm.emit_row && !vm.error);
  
  TestCheck(!vm.regs.end_sequence);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.opcode == DW_StdOpcode_ExtendedOpcode);
  TestCheck(vm.ext_opcode == DW_ExtOpcode_EndSequence);
  TestCheck(vm.regs.end_sequence);
  TestCheck(vm.off == 71 && vm.emit_row && !vm.error);
  TestCheck(vm.regs.address == address && vm.regs.line == 1);
  TestCheck(vm.regs.basic_block && vm.regs.prologue_end && vm.regs.epilogue_begin);
  TestCheck(vm.regs.discriminator == 2 && vm.regs.isa == max_U64);
  TestCheck(vm.off == program.size);
  TestCheck(!dw2_line_vm_step(&header, program, &vm) && !vm.error);
  
  dw_writer_end(&writer);
}

Test(dwarf_line_emit)
{
  DW_Writer *writer = dw_writer_begin(DW_Format_32Bit, DW_Version_5, DW_CompUnitKind_Compile, Arch_x64);
  String8 comp_dir  = str8_lit("c:/devel/");
  String8 comp_name = str8_lit("test.c");
  dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
  dw_writer_push_attrib_string(writer, DW_AttribKind_Producer, str8_lit("RAD DWARF WRITER"));
  dw_writer_push_attrib_string(writer, DW_AttribKind_CompDir, comp_dir);
  dw_writer_push_attrib_string(writer, DW_AttribKind_Name, comp_name);
  dw_writer_push_attrib_line_ptr(writer, DW_AttribKind_StmtList, 0);
  dw_writer_tag_end(writer);
  
  // test special opcode writer and reader
  DW_WriterFile *file = dw_writer_new_file(writer, comp_name);
  
  // init sequence
  dw_writer_line_emit(writer, file, 10000, 0, 1000);
  
  for EachIndex(i, abs_s64(writer->line.line_base) + 1) {
    dw_writer_line_emit(writer, file, writer->line.ln-i, 0, writer->line.addr+1);
    
    dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_advance_line(i));
    writer->line.ln += i;
  }
  
  // special opcode line window underflow, must emit three instructions to advance
  dw_writer_line_emit(writer, file, (writer->line.ln + writer->line.line_base) - 1, 0, writer->line.addr + 1);
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_advance_line(-(writer->line.line_base - 1)));
  writer->line.ln += -(writer->line.line_base - 1);
  
  for EachIndex(i, (writer->line.line_range + writer->line.line_base) + 1) {
    dw_writer_line_emit(writer, file, writer->line.ln+i, 0, writer->line.addr+1);
    
    dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_advance_line(-i));
    writer->line.ln -= i;
  }
  
  dw_writer_line_end_sequence(writer);
  dw_writer_line_emit(writer, file, 1000, 0, writer->line.addr+1);
  
  // test address window
  S64 c = writer->line.line_range + writer->line.line_base;
  U64 line_addr_count = 0;
  for (S64 i = 0; i < c; ++i) {
    U64 ln_delta       = i + writer->line.line_base;
    S64 max_ln_delta   = writer->line.line_range + writer->line.line_base;
    S64 max_addr_delta = (max_U8 - writer->line.opcode_base - (ln_delta - writer->line.line_base)) / writer->line.line_range;
    for EachIndex(k, max_addr_delta) {
      dw_writer_line_emit(writer, file, writer->line.ln + i, 0, writer->line.addr + k);
      line_addr_count += 1;
    }
  }
  
  DW_Raw input = dw_input_from_writer(arena, writer);
  DW2_LineTableHeader header = {0};
  String8 program = {0};
  TestCheck(dwt_line_header(arena, &input, &header, &program));
  DW2_LineVM vm = dw2_line_vm_init(&header);

  // check init sequence
  {
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_SetFile);
    TestCheck(vm.regs.file_index == 0);
    TestCheck(vm.regs.line == 1 && vm.regs.address == 0);
    TestCheck(vm.emit_row == 0);
    
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_AdvancePc);
    TestCheck(vm.regs.address == 1000);
    TestCheck(vm.regs.line == 1 && !vm.emit_row);
    
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
    TestCheck(vm.regs.line == 10000);
    TestCheck(vm.regs.address == 1000);
    TestCheck(vm.emit_row == 0);
    
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_Copy);
    TestCheck(vm.regs.line == 10000);
    TestCheck(vm.regs.address == 1000);
    TestCheck(vm.emit_row == 1);
  }
  
  {
    U64 pc = vm.regs.address;
    for EachIndex(i, abs_s64(writer->line.line_base) + 1) {
      pc += 1;
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == 0x20 - i);
      TestCheck(vm.regs.line == 10000 - i);
      TestCheck(vm.regs.address == pc);
      TestCheck(vm.emit_row == 1);
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
      TestCheck(vm.regs.line == 10000);
      TestCheck(vm.regs.address == pc);
      TestCheck(!vm.emit_row);
    }
    
    // line window underflow check
    {
      pc += 1;
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_AdvancePc);
      TestCheck(vm.regs.line == 10000);
      TestCheck(vm.regs.address == pc);
      TestCheck(!vm.emit_row);
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
      TestCheck(vm.regs.line == 9994);
      TestCheck(vm.regs.address == pc);
      TestCheck(!vm.emit_row);
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_Copy);
      TestCheck(vm.regs.line == 9994);
      TestCheck(vm.regs.address == pc);
      TestCheck(vm.emit_row);
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
      TestCheck(vm.regs.line == 10000);
      TestCheck(vm.regs.address == pc);
      TestCheck(!vm.emit_row);
    }
    
    for EachIndex(i, writer->line.line_range + writer->line.line_base) {
      pc += 1;
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == 0x20 + i);
      TestCheck(vm.regs.line == 10000 + i);
      TestCheck(vm.regs.address == pc);
      TestCheck(vm.emit_row);
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
      TestCheck(vm.regs.line == 10000);
      TestCheck(vm.regs.address == pc);
      TestCheck(!vm.emit_row);
    }
    
    // line window overflow check
    {
      pc += 1;
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_AdvancePc);
      TestCheck(vm.regs.line == 10000);
      TestCheck(vm.regs.address == pc);
      TestCheck(!vm.emit_row);
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
      TestCheck(vm.regs.line == 10009);
      TestCheck(vm.regs.address == pc);
      TestCheck(!vm.emit_row);
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_Copy);
      TestCheck(vm.regs.address == pc);
      TestCheck(vm.regs.line == 10009);
      TestCheck(vm.emit_row);
      
      TestCheck(dw2_line_vm_step(&header, program, &vm));
      TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
      TestCheck(vm.regs.line == 10000);
      TestCheck(vm.regs.address == pc);
      TestCheck(!vm.emit_row);
    }
    
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_ExtendedOpcode);
    TestCheck(vm.ext_opcode == DW_ExtOpcode_EndSequence);
    TestCheck(vm.regs.line == 10000);
    TestCheck(vm.regs.address == pc);
    TestCheck(vm.emit_row && vm.regs.end_sequence);
  }
  
  {
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_SetFile);
    TestCheck(vm.regs.line == 1 && vm.regs.address == 0 && !vm.emit_row);
    TestCheck(!vm.regs.end_sequence && vm.regs.file_index == 0);
    
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_AdvancePc);
    TestCheck(vm.regs.line == 1 && vm.regs.address == 1 && !vm.emit_row);
    
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_AdvanceLine);
    TestCheck(vm.regs.line == 1000 && vm.regs.address == 1 && !vm.emit_row);
    
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_Copy);
    TestCheck(vm.regs.line == 1000 && vm.regs.address == 1 && vm.emit_row);
    
    U64 expected_line = 1000, expected_pc = 1, rows = 0;
    for (S64 i = 0; i < c; ++i) {
      S64 max_addr_delta = (max_U8 - header.opcode_base - i) / header.line_range;
      for EachIndex(k, max_addr_delta) {
        expected_line += i;
        expected_pc += k;
        U8 opcode = header.opcode_base + (i - header.line_base) + k * header.line_range;
        U64 old_off = vm.off;
        TestCheck(dw2_line_vm_step(&header, program, &vm));
        TestCheck(vm.off == old_off + 1 && !vm.error);
        TestCheck(vm.opcode == opcode && header.opcode_base < vm.opcode);
        TestCheck(vm.regs.line == expected_line && vm.regs.address == expected_pc);
        TestCheck(vm.emit_row);
        rows += 1;
      }
    }
    // zero-delta emission produces a row in the current writer
    TestCheck(rows == line_addr_count);
    
    TestCheck(dw2_line_vm_step(&header, program, &vm));
    TestCheck(vm.opcode == DW_StdOpcode_ExtendedOpcode);
    TestCheck(vm.ext_opcode == DW_ExtOpcode_EndSequence);
    TestCheck(vm.regs.line == 1586);
    TestCheck(vm.regs.address == 1161);
    TestCheck(vm.emit_row && vm.regs.end_sequence);
  }
  
  TestCheck(vm.off == program.size);
  TestCheck(!dw2_line_vm_step(&header, program, &vm) && !vm.error);
  
  dw_writer_end(&writer);
}

Test(dwarf_writer)
{
  DW_Writer *writer = dw_writer_begin(DW_Format_32Bit, DW_Version_5, DW_CompUnitKind_Compile, Arch_x64);
  // info
  {
    dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
    dw_writer_push_attrib_string(writer, DW_AttribKind_Producer, str8_lit("RAD DWARF WRITER"));
    dw_writer_push_attrib_enum(writer, DW_AttribKind_Language, DW_Language_C99);
    dw_writer_push_attrib_flag(writer, DW_AttribKind_UseUtf8, 1);
    dw_writer_push_attrib_address(writer, DW_AttribKind_LowPc, 0x140001000);
    dw_writer_push_attrib_address(writer, DW_AttribKind_HighPc, 0x140001004);
    {
      DW_WriterTag *char_type = dw_writer_tag_begin(writer, DW_TagKind_BaseType);
      dw_writer_push_attrib_sint(writer, DW_AttribKind_ByteSize, 1);
      dw_writer_push_attrib_enum(writer, DW_AttribKind_Encoding, DW_ATE_SignedChar);
      dw_writer_push_attrib_string(writer, DW_AttribKind_Name, str8_lit("char"));
      dw_writer_tag_end(writer);
      
      dw_writer_tag_begin(writer, DW_TagKind_ConstType);
      dw_writer_push_attrib_ref(writer, DW_AttribKind_Type, char_type);
      dw_writer_tag_end(writer);
      
      // test abbrev dedup
      DW_WriterTag *dup_char_type = dw_writer_tag_begin(writer, DW_TagKind_BaseType);
      dw_writer_push_attrib_sint(writer, DW_AttribKind_ByteSize, 1);
      dw_writer_push_attrib_enum(writer, DW_AttribKind_Encoding, DW_ATE_SignedChar);
      dw_writer_push_attrib_string(writer, DW_AttribKind_Name, str8_lit("char"));
      dw_writer_tag_end(writer);
      
      // simple struct test
      DW_WriterTag *simple_struct_tag = dw_writer_tag_begin(writer, DW_TagKind_StructureType);
      dw_writer_push_attrib_string(writer, DW_AttribKind_Name, str8_lit("FooBar"));
      dw_writer_push_attrib_uint(writer, DW_AttribKind_ByteSize, 90);
      {
        dw_writer_tag_begin(writer, DW_TagKind_Member);
        dw_writer_push_attrib_string(writer, DW_AttribKind_Name, str8_lit("m0"));
        dw_writer_push_attrib_ref(writer, DW_AttribKind_Type, char_type);
        dw_writer_push_attrib_sint(writer, DW_AttribKind_DataMemberLocation, 0);
        dw_writer_tag_end(writer);
        
        dw_writer_tag_begin(writer, DW_TagKind_Member);
        dw_writer_push_attrib_string(writer, DW_AttribKind_Name, str8_lit("m1"));
        dw_writer_push_attrib_ref(writer, DW_AttribKind_Type, char_type);
        dw_writer_push_attrib_sint(writer, DW_AttribKind_DataMemberLocation, 10);
        dw_writer_push_attrib_implicit(writer, DW_AttribKind_ByteSize, 123);
        dw_writer_tag_end(writer);
      }
      dw_writer_tag_end(writer);
      
      dw_writer_tag_begin(writer, DW_TagKind_SubProgram);
      dw_writer_push_attrib_flag(writer, DW_AttribKind_External, 1);
      dw_writer_push_attrib_flag(writer, DW_AttribKind_Prototyped, 1);
      dw_writer_push_attrib_address(writer, DW_AttribKind_LowPc, 0x140001000);
      dw_writer_push_attrib_address(writer, DW_AttribKind_HighPc, 0x140001004);
      dw_writer_push_attrib_string(writer, DW_AttribKind_Name, str8_lit("main"));
      dw_writer_push_attrib_ref(writer, DW_AttribKind_Type, simple_struct_tag);
      dw_writer_push_attrib_expr(writer, DW_AttribKind_FrameBase, &(DW_ExprEnc)DW_ExprEnc_Op(Reg7), 1);
      dw_writer_tag_end(writer);
    }
    dw_writer_tag_end(writer);
  }
  
  DW_Raw input = dw_input_from_writer(arena, writer);
  TestCheck(dwt_writer_matches(arena, &input, writer));
  
  // validate the writer
  
  TestCheck(writer->current == 0);
  TestCheck(writer->format == DW_Format_32Bit);
  TestCheck(writer->cu_kind == DW_CompUnitKind_Compile);
  TestCheck(writer->version == DW_Version_5);
  TestCheck(writer->address_size == 8);
  TestCheck(writer->abbrev_base_info_off == 8);
  TestCheck(writer->fixups.count == 0);
  TestCheck(writer->abbrev_id_map->count == 7);
  
  DW_WriterTag *comp_unit_tag  = writer->root;
  {
    TestCheck(comp_unit_tag->kind == DW_TagKind_CompileUnit);
    TestCheck(comp_unit_tag->next == 0);
    TestCheck(comp_unit_tag->parent == 0);
    TestCheck(comp_unit_tag->attrib_count == 5);
    TestCheck(comp_unit_tag->abbrev_id == 1);
    TestCheck(comp_unit_tag->info_off == 0xc);
    
    DW_WriterAttrib *producer_attrib = comp_unit_tag->first_attrib;
    TestCheck(producer_attrib->kind == DW_AttribKind_Producer);
    TestCheck(producer_attrib->form.reader.kind == DW_FormKind_String);
    TestCheck(str8_match(producer_attrib->form.writer.string, str8_lit("RAD DWARF WRITER"), 0));
    
    DW_WriterAttrib *language_attrib = producer_attrib->next;
    TestCheck(language_attrib->kind == DW_AttribKind_Language);
    TestCheck(language_attrib->form.reader.kind == DW_FormKind_Data1);
    TestCheck(language_attrib->form.reader.data.size == 1);
    TestCheck(*(U8 *)language_attrib->form.reader.data.str == DW_Language_C99);
    
    DW_WriterAttrib *use_utf8_attrib = language_attrib->next;
    TestCheck(use_utf8_attrib->kind == DW_AttribKind_UseUtf8);
    TestCheck(use_utf8_attrib->form.reader.kind == DW_FormKind_Flag);
    TestCheck(use_utf8_attrib->form.reader.flag == 1);
  }
  
  DW_WriterTag *char_type_tag = comp_unit_tag->first_child;
  {
    TestCheck(char_type_tag->kind == DW_TagKind_BaseType);
    TestCheck(char_type_tag->next != 0);
    TestCheck(char_type_tag->parent == comp_unit_tag);
    TestCheck(char_type_tag->attrib_count == 3);
    TestCheck(char_type_tag->abbrev_id == 2);
    TestCheck(char_type_tag->info_off == 0x30);
    TestCheck(char_type_tag->first_attrib != char_type_tag->last_attrib);
    
    DW_WriterAttrib *byte_size_attrib = char_type_tag->first_attrib;
    TestCheck(byte_size_attrib->kind == DW_AttribKind_ByteSize);
    TestCheck(byte_size_attrib->form.reader.kind == DW_FormKind_Data1);
    TestCheck(byte_size_attrib->form.reader.data.size == 1);
    TestCheck(*(U8 *)byte_size_attrib->form.reader.data.str == 1);
    
    DW_WriterAttrib *encoding_attrib  = byte_size_attrib->next;
    TestCheck(encoding_attrib->kind == DW_AttribKind_Encoding);
    TestCheck(encoding_attrib->form.reader.kind == DW_FormKind_Data1);
    TestCheck(encoding_attrib->form.reader.data.size == 1);
    TestCheck(*(U8 *)encoding_attrib->form.reader.data.str == DW_ATE_SignedChar);
    
    DW_WriterAttrib *name_attrib = encoding_attrib->next;
    TestCheck(name_attrib->kind == DW_AttribKind_Name);
    TestCheck(name_attrib->form.reader.kind == DW_FormKind_String);
    TestCheck(str8_match(name_attrib->form.reader.string, str8_lit("char"), 0));
  }
  
  DW_WriterTag *const_type_tag = char_type_tag->next;
  {
    TestCheck(const_type_tag->kind == DW_TagKind_ConstType);
    TestCheck(const_type_tag->next != 0);
    TestCheck(const_type_tag->parent == comp_unit_tag);
    TestCheck(const_type_tag->attrib_count == 1);
    TestCheck(const_type_tag->abbrev_id == 3);
    TestCheck(const_type_tag->info_off == 0x38);
    TestCheck(const_type_tag->first_attrib && const_type_tag->first_attrib == const_type_tag->last_attrib);
    
    DW_WriterAttrib *type_attrib = const_type_tag->first_attrib;
    TestCheck(type_attrib->kind == DW_AttribKind_Type);
    TestCheck(type_attrib->form.writer.kind == DW_WriterFormKind_Ref);
    TestCheck(type_attrib->form.writer.ref == char_type_tag);
  }
  
  DW_WriterTag *dup_char_type_tag = const_type_tag->next;
  {
    TestCheck(dup_char_type_tag->kind == DW_TagKind_BaseType);
    TestCheck(dup_char_type_tag->next != 0);
    TestCheck(dup_char_type_tag->parent == comp_unit_tag);
    TestCheck(dup_char_type_tag->attrib_count == 3);
    TestCheck(dup_char_type_tag->abbrev_id == 2);
    TestCheck(dup_char_type_tag->info_off == 0x3a);
    TestCheck(dup_char_type_tag->first_attrib != dup_char_type_tag->last_attrib);
    
    DW_WriterAttrib *byte_size_attrib = dup_char_type_tag->first_attrib;
    TestCheck(byte_size_attrib->kind == DW_AttribKind_ByteSize);
    TestCheck(byte_size_attrib->form.reader.kind == DW_FormKind_Data1);
    TestCheck(byte_size_attrib->form.reader.data.size == 1);
    TestCheck(*(U8 *)byte_size_attrib->form.reader.data.str == 1);
    
    DW_WriterAttrib *encoding_attrib  = byte_size_attrib->next;
    TestCheck(encoding_attrib->kind == DW_AttribKind_Encoding);
    TestCheck(encoding_attrib->form.reader.kind == DW_FormKind_Data1);
    TestCheck(encoding_attrib->form.reader.data.size == 1);
    TestCheck(*(U8 *)encoding_attrib->form.reader.data.str == DW_ATE_SignedChar);
    
    DW_WriterAttrib *name_attrib = encoding_attrib->next;
    TestCheck(name_attrib->kind == DW_AttribKind_Name);
    TestCheck(name_attrib->form.reader.kind == DW_FormKind_String);
    TestCheck(str8_match(name_attrib->form.reader.string, str8_lit("char"), 0));
  }
  
  DW_WriterTag *simple_struct_tag = dup_char_type_tag->next;
  {
    TestCheck(simple_struct_tag->kind == DW_TagKind_StructureType);
    TestCheck(simple_struct_tag->next != 0);
    TestCheck(simple_struct_tag->parent == comp_unit_tag);
    TestCheck(simple_struct_tag->attrib_count == 2);
    TestCheck(simple_struct_tag->abbrev_id == 4);
    TestCheck(simple_struct_tag->info_off == 0x42);
    
    DW_WriterAttrib *simple_struct_name = simple_struct_tag->first_attrib;
    TestCheck(simple_struct_name->kind == DW_AttribKind_Name);
    TestCheck(simple_struct_name->form.reader.kind == DW_FormKind_String);
    TestCheck(str8_match(simple_struct_name->form.reader.string, str8_lit("FooBar"), 0));
    
    DW_WriterTag *m0_tag = simple_struct_tag->first_child;
    {
      TestCheck(m0_tag->kind == DW_TagKind_Member);
      TestCheck(m0_tag->parent == simple_struct_tag);
      TestCheck(m0_tag->attrib_count == 3);
      TestCheck(m0_tag->info_off == 0x4b);
      TestCheck(m0_tag->abbrev_id == 5);
      
      DW_WriterAttrib *name = m0_tag->first_attrib;
      TestCheck(name->kind == DW_AttribKind_Name);
      TestCheck(name->form.reader.kind == DW_FormKind_String);
      TestCheck(str8_match(name->form.reader.string, str8_lit("m0"), 0));
      
      DW_WriterAttrib *type = name->next;
      TestCheck(type->kind == DW_AttribKind_Type);
      TestCheck(type->form.reader.kind == DW_FormKind_Ref1);
      TestCheck(type->form.reader.ref == 0x30);
      
      DW_WriterAttrib *data_loc = type->next;
      TestCheck(data_loc->kind == DW_AttribKind_DataMemberLocation);
      TestCheck(data_loc->form.reader.kind == DW_FormKind_Data1);
      TestCheck(data_loc->form.reader.data.size == 1);
      TestCheck(*(U8 *)data_loc->form.reader.data.str == 0);
    }
    
    DW_WriterTag *m1_tag = m0_tag->next;
    {
      TestCheck(m1_tag->kind == DW_TagKind_Member);
      TestCheck(m1_tag->parent == simple_struct_tag);
      TestCheck(m1_tag->attrib_count == 4);
      TestCheck(m1_tag->info_off == 0x51);
      TestCheck(m1_tag->abbrev_id == 6);
      
      DW_WriterAttrib *name = m1_tag->first_attrib;
      TestCheck(name->kind == DW_AttribKind_Name);
      TestCheck(name->form.reader.kind == DW_FormKind_String);
      TestCheck(str8_match(name->form.reader.string, str8_lit("m1"), 0));
      
      DW_WriterAttrib *type = name->next;
      TestCheck(type->kind == DW_AttribKind_Type);
      TestCheck(type->form.reader.kind == DW_FormKind_Ref1);
      TestCheck(type->form.reader.ref == 0x30);
      
      DW_WriterAttrib *data_loc = type->next;
      TestCheck(data_loc->kind == DW_AttribKind_DataMemberLocation);
      TestCheck(data_loc->form.reader.kind == DW_FormKind_Data1);
      TestCheck(data_loc->form.reader.data.size == 1);
      TestCheck(*(U8 *)data_loc->form.reader.data.str == 10);
      
      DW_WriterAttrib *byte_size = data_loc->next;
      TestCheck(byte_size->kind == DW_AttribKind_ByteSize);
      TestCheck(byte_size->form.reader.kind == DW_FormKind_ImplicitConst);
      TestCheck(byte_size->form.reader.implicit_const == 123);
    }
  }
  
  DW_WriterTag *main_tag = simple_struct_tag->next;
  TestCheck(main_tag->next == 0);
  TestCheck(main_tag->kind == DW_TagKind_SubProgram);
  TestCheck(main_tag->parent == comp_unit_tag);
  TestCheck(main_tag->abbrev_id == 7);
  {
    DW_WriterAttrib *external = main_tag->first_attrib;
    TestCheck(external->kind == DW_AttribKind_External);
    TestCheck(external->form.reader.kind == DW_FormKind_Flag);
    TestCheck(external->form.reader.flag);
    
    DW_WriterAttrib *prototyped = external->next;
    TestCheck(prototyped->kind == DW_AttribKind_Prototyped);
    TestCheck(prototyped->form.reader.kind == DW_FormKind_Flag);
    TestCheck(prototyped->form.reader.flag);
    
    DW_WriterAttrib *low_pc = prototyped->next;
    TestCheck(low_pc->kind == DW_AttribKind_LowPc);
    TestCheck(low_pc->form.reader.kind == DW_FormKind_Addr);
    TestCheck(low_pc->form.reader.addr.size == sizeof(U64));
    TestCheck(*(U64 *)low_pc->form.reader.addr.str == 0x140001000);
    
    DW_WriterAttrib *high_pc = low_pc->next;
    TestCheck(high_pc->kind == DW_AttribKind_HighPc);
    TestCheck(high_pc->form.reader.kind == DW_FormKind_Addr);
    TestCheck(high_pc->form.reader.addr.size == sizeof(U64));
    TestCheck(*(U64 *)high_pc->form.reader.addr.str == 0x140001004);
    
    DW_WriterAttrib *name = high_pc->next;
    TestCheck(name->kind == DW_AttribKind_Name);
    TestCheck(name->form.reader.kind == DW_FormKind_String);
    TestCheck(str8_match(name->form.reader.string, str8_lit("main"), 0));
    
    DW_WriterAttrib *type = name->next;
    TestCheck(type->kind == DW_AttribKind_Type);
    TestCheck(type->form.reader.kind == DW_FormKind_Ref1);
    TestCheck(type->form.reader.ref == simple_struct_tag->info_off);
    
    DW_WriterAttrib *frame_base = type->next;
    TestCheck(frame_base->kind == DW_AttribKind_FrameBase);
    TestCheck(frame_base->form.reader.kind == DW_FormKind_ExprLoc);
    TestCheck(frame_base->form.reader.exprloc.size == 1);
    TestCheck(str8_match(frame_base->form.reader.exprloc, str8_lit("\x57"), 0)); // DW_OP_reg7, no operands
    TestCheck(frame_base->next == 0);
  }
  
  dw_writer_end(&writer);
}

Test(dwarf_writer_forms)
{
  DW_Format formats[] = {DW_Format_32Bit, DW_Format_64Bit};
  S64 signed_values[] = {min_S64, -129, -128, -65, -64, -1, 0, 127, 128, 32767, 32768, 0x7fffffff, 0x80000000ll, max_S64};
  U64 block_sizes[] = {0, 1, 255, 256, 65535, 65536};
  U64 data_sizes[] = {1, 2, 4, 8, 16};
  DW_FormKind data_forms[] = {DW_FormKind_Data1, DW_FormKind_Data2, DW_FormKind_Data4, DW_FormKind_Data8, DW_FormKind_Data16};
  for EachElement(format_idx, formats) {
    DW_Writer *writer = dw_writer_begin(formats[format_idx], DW_Version_5, DW_CompUnitKind_Compile, Arch_x64);
    dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
    DW_WriterAttrib *signed_attribs[ArrayCount(signed_values)] = {0};
    for EachElement(i, signed_values) {
      dw_writer_tag_begin(writer, DW_TagKind_Constant);
      signed_attribs[i] = dw_writer_push_attrib_sint(writer, DW_AttribKind_ConstValue, signed_values[i]);
      dw_writer_tag_end(writer);
    }
    DW_WriterAttrib *blocks[ArrayCount(block_sizes)] = {0};
    for EachElement(i, block_sizes) {
      String8 block = str8(push_array(arena, U8, block_sizes[i]), block_sizes[i]);
      for EachIndex(j, block.size) { block.str[j] = (U8)(j * 37 + 0xa5); }
      dw_writer_tag_begin(writer, DW_TagKind_Constant);
      blocks[i] = dw_writer_push_attrib_block(writer, DW_AttribKind_ConstValue, block);
      dw_writer_tag_end(writer);
    }
    U8 bytes[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    DW_WriterAttrib *data[ArrayCount(data_sizes)] = {0};
    for EachElement(i, data_sizes) {
      dw_writer_tag_begin(writer, DW_TagKind_Constant);
      data[i] = dw_writer_push_attrib_data(writer, DW_AttribKind_ConstValue, str8(bytes, data_sizes[i]));
      dw_writer_tag_end(writer);
    }
    dw_writer_tag_begin(writer, DW_TagKind_Constant);
    DW_WriterAttrib *uint = dw_writer_push_attrib_uint(writer, DW_AttribKind_ConstValue, max_U64);
    dw_writer_tag_end(writer);
    dw_writer_tag_end(writer);

    DW_Raw raw = dw_input_from_writer_sections(arena, writer);
    for EachElement(i, signed_values) {
      S64 value = signed_values[i];
      DW_FormKind kind = value < 0 || value > max_S32 ? DW_FormKind_SData :
                         value <= max_S8 ? DW_FormKind_Data1 : value <= max_S16 ? DW_FormKind_Data2 : DW_FormKind_Data4;
      TestCheck(signed_attribs[i]->form.reader.kind == kind);
      if (kind == DW_FormKind_SData) { TestCheck(signed_attribs[i]->form.reader.sdata == value); }
      else {
        U64 encoded_value = 0;
        U64 size = kind == DW_FormKind_Data1 ? 1 : kind == DW_FormKind_Data2 ? 2 : 4;
        TestCheck(signed_attribs[i]->form.reader.data.size == size);
        MemoryCopy(&encoded_value, signed_attribs[i]->form.reader.data.str, size);
        TestCheck(encoded_value == value);
      }
    }
    for EachElement(i, block_sizes) {
      TestCheck(blocks[i]->form.reader.kind == DW_FormKind_Block);
      TestCheck(blocks[i]->form.reader.block.size == block_sizes[i]);
    }
    for EachElement(i, data_sizes) {
      TestCheck(data[i]->form.reader.kind == data_forms[i]);
      TestCheck(str8_match(data[i]->form.reader.data, str8(bytes, data_sizes[i]), 0));
    }
    TestCheck(uint->form.reader.kind == DW_FormKind_UData && uint->form.reader.udata == max_U64);
    TestCheck(dwt_writer_matches(arena, &raw, writer));
    dw_writer_end(&writer);
  }
}

Test(dwarf_form_bytes)
{
  DW_FormKind kinds[] = {DW_FormKind_Block1, DW_FormKind_Block2, DW_FormKind_Block4, DW_FormKind_Block, DW_FormKind_ExprLoc};
  String8 expected[] = {
    str8_lit("\x03\xa5\x00\x5a"),
    str8_lit("\x03\x00\xa5\x00\x5a"),
    str8_lit("\x03\x00\x00\x00\xa5\x00\x5a"),
    str8_lit("\x03\xa5\x00\x5a"),
    str8_lit("\x03\xa5\x00\x5a"),
  };
  DW_Format formats[] = {DW_Format_32Bit, DW_Format_64Bit};
  for EachElement(format_idx, formats) {
    DW_Raw raw = {0};
    DW2_ParseCtx parse_ctx = {.format = formats[format_idx], .addr_size = 8};
    for EachElement(i, kinds) {
      DW_WriterXForm form = {0};
      form.reader.kind = kinds[i];
      form.reader.block = str8_lit("\xa5\x00\x5a");
      String8List srl = {0};
      str8_serial_begin(arena, &srl);
      str8_serial_push_u8(arena, &srl, 0xcc);
      TestCheck(dw_serial_push_form(arena, &srl, DW_Version_5, formats[format_idx], 8, 0, form) == expected[i].size);
      str8_serial_push_u8(arena, &srl, 0xdd);
      String8 data = str8_list_join(arena, &srl, 0);
      TestCheck(data.size == expected[i].size + 2);
      TestCheck(data.str[0] == 0xcc && data.str[data.size-1] == 0xdd);
      TestCheck(str8_match(str8_substr(data, r1u64(1, data.size-1)), expected[i], 0));
      
      // Parse at a nonzero offset; lengths and offsets must exclude the sentinels.
      DW2_FormVal parsed = {0};
      TestCheck(dw2_read_form_val(&raw, &parse_ctx, data, 1, kinds[i], 0, &parsed) == expected[i].size);
      TestCheck(dw_test__form_match(form.reader, parsed, data));
      if (kinds[i] == DW_FormKind_ExprLoc) {
        TestCheck(parsed.u128.u64[0] == 2 && parsed.u128.u64[1] == 3);
        parsed.u128.u64[0] -= 1;
        TestCheck(!dw_test__form_match(form.reader, parsed, data));
      } else if (kinds[i] != DW_FormKind_Block) {
        TestCheck(parsed.u128.u64[0] == 3 && parsed.u128.u64[1] == 1);
        parsed.u128.u64[1] -= 1;
        TestCheck(!dw_test__form_match(form.reader, parsed, data));
      }
    }
    
    DW_WriterXForm form = {0};
    form.reader.kind = DW_FormKind_SecOffset;
    form.reader.sec_offset = formats[format_idx] == DW_Format_32Bit ? 0x67452301 : 0xefcdab8967452301;
    U64 size = formats[format_idx] == DW_Format_32Bit ? 4 : 8;
    String8List srl = {0};
    str8_serial_begin(arena, &srl);
    TestCheck(dw_serial_push_form(arena, &srl, DW_Version_5, formats[format_idx], 8, 0, form) == size);
    String8 data = str8_list_join(arena, &srl, 0);
    TestCheck(str8_match(data, str8_prefix(str8_lit("\x01\x23\x45\x67\x89\xab\xcd\xef"), size), 0));
    DW2_FormVal parsed = {0};
    TestCheck(dw2_read_form_val(&raw, &parse_ctx, data, 0, form.reader.kind, 0, &parsed) == size);
    TestCheck(dw_test__form_match(form.reader, parsed, data));
  }
}

Test(dwarf_writer_references)
{
  DW_Format formats[] = {DW_Format_32Bit, DW_Format_64Bit};
  for EachElement(i, formats) {
    DW_Writer *writer = dw_writer_begin(formats[i], DW_Version_5, DW_CompUnitKind_Compile, Arch_x64);
    dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
    DW_WriterTag *target = dw_writer_tag_reserve(writer, DW_TagKind_PointerType);
    dw_writer_tag_begin(writer, DW_TagKind_Variable);
    DW_WriterAttrib *forward = dw_writer_push_attrib_ref(writer, DW_AttribKind_Type, target);
    // Put the target past both the Ref1 and Ref2 offset ranges.
    dw_writer_push_attrib_block(writer, DW_AttribKind_ConstValue, str8(push_array(arena, U8, 65536), 65536));
    dw_writer_tag_end(writer);
    dw_writer_tag_begin_reserved(writer, target);
    DW_WriterAttrib *self = dw_writer_push_attrib_ref(writer, DW_AttribKind_Type, target);
    dw_writer_tag_end(writer);
    dw_writer_tag_begin(writer, DW_TagKind_ConstType);
    DW_WriterAttrib *backward = dw_writer_push_attrib_ref(writer, DW_AttribKind_Type, target);
    dw_writer_tag_end(writer);
    dw_writer_tag_end(writer);
    TestCheck(target->info_off == 0);

    DW_Raw raw = dw_input_from_writer_sections(arena, writer);
    TestCheck(target->info_off > max_U16);
    TestCheck(forward->form.reader.kind == DW_FormKind_Ref8);
    TestCheck(self->form.reader.kind == DW_FormKind_Ref8);
    TestCheck(backward->form.reader.kind == DW_FormKind_Ref4);
    TestCheck(forward->form.reader.ref == target->info_off);
    TestCheck(self->form.reader.ref == target->info_off);
    TestCheck(backward->form.reader.ref == target->info_off);
    TestCheck(writer->fixups.count == 1);
    TestCheck(dwt_writer_matches(arena, &raw, writer));
    dw_writer_end(&writer);
  }
}

Test(dwarf_writer_implicit_constants)
{
  // Regression for the old comma expression: zero must match, unequal nonzero
  // values must not match, and the comparison must preserve the sign.
  DW_WriterEncodedForm encoded = {.kind = DW_FormKind_ImplicitConst};
  DW2_FormVal parsed = {.kind = DW_FormKind_ImplicitConst};
  TestCheck(dw_test__form_match(encoded, parsed, str8_zero()));
  encoded.implicit_const = 1;
  parsed.u128.u64[0] = 2;
  TestCheck(!dw_test__form_match(encoded, parsed, str8_zero()));
  encoded.implicit_const = -1;
  parsed.u128.u64[0] = max_U64;
  TestCheck(dw_test__form_match(encoded, parsed, str8_zero()));

  S64 values[] = {0, 1, 123, -1, -65, min_S64};
  DW_Writer *writer = dw_writer_begin(DW_Format_32Bit, DW_Version_5, DW_CompUnitKind_Compile, Arch_x64);
  dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
  DW_WriterTag *tags[ArrayCount(values)] = {0};
  for EachElement(i, values) {
    tags[i] = dw_writer_tag_begin(writer, DW_TagKind_Constant);
    dw_writer_push_attrib_implicit(writer, DW_AttribKind_ConstValue, values[i]);
    dw_writer_tag_end(writer);
  }
  dw_writer_tag_end(writer);
  DW_Raw raw = dw_input_from_writer_sections(arena, writer);
  for EachElement(i, values) {
    TestCheck(tags[i]->first_attrib->form.reader.kind == DW_FormKind_ImplicitConst);
    TestCheck(tags[i]->first_attrib->form.reader.implicit_const == values[i]);
    for (U64 j = 0; j < i; ++j) { TestCheck(tags[i]->abbrev_id != tags[j]->abbrev_id); }
    if (i) { TestCheck(tags[i]->info_off == tags[i-1]->info_off + 1); }
  }
  TestCheck(dwt_writer_matches(arena, &raw, writer));
  dw_writer_end(&writer);
}

Test(dwarf_unit_header_formats)
{
  // DWARF 2 offsets follow the initial-length format, not the address size.
  // Sentinels exercise a nonzero input offset and an exact header boundary.
  String8 data[] = {
    str8_lit("\xcc" "\x08\x00\x00\x00" "\x02\x00" "\x78\x56\x34\x12" "\x08" "\x00" "\xdd"),
    str8_lit("\xcc" "\xff\xff\xff\xff" "\x0c\x00\x00\x00\x00\x00\x00\x00" "\x02\x00"
             "\xef\xcd\xab\x89\x67\x45\x23\x01" "\x04" "\x00" "\xdd"),
  };
  DW_Format formats[] = {DW_Format_32Bit, DW_Format_64Bit};
  U64 header_sizes[] = {11, 23};
  U64 abbrev_offsets[] = {0x12345678, 0x0123456789abcdef};
  U64 address_sizes[] = {8, 4};
  for EachElement(i, data) {
    DW2_UnitHeader header = {0};
    U64 size = dw2_read_unit_header(data[i], 1, &header);
    TestCheck(size == header_sizes[i]);
    TestCheck(header.version == DW_Version_2 && header.format == formats[i]);
    TestCheck(header.abbrev_off == abbrev_offsets[i]);
    TestCheck(header.addr_size == address_sizes[i]);
    TestCheck(data[i].str[1 + size] == 0 && data[i].str[2 + size] == 0xdd);
  }
}

Test(dwarf_writer_expression_attribs)
{
  DW_Format formats[] = {DW_Format_32Bit, DW_Format_64Bit};
  struct { DW_AttribKind attrib_kind; DW_TagKind tag_kind; } cases[] = {
    {DW_AttribKind_UseLocation,        DW_TagKind_PtrToMemberType},
    {DW_AttribKind_VTableElemLocation, DW_TagKind_SubProgram},
    {DW_AttribKind_FrameBase,          DW_TagKind_SubProgram},
    {DW_AttribKind_ReturnAddr,         DW_TagKind_SubProgram},
    {DW_AttribKind_StaticLink,         DW_TagKind_SubProgram},
    {DW_AttribKind_Segment,            DW_TagKind_SubProgram},
    {DW_AttribKind_StringLength,       DW_TagKind_StringType},
    {DW_AttribKind_Location,           DW_TagKind_Variable},
    {DW_AttribKind_DataMemberLocation, DW_TagKind_Member},
  };
  for EachElement(format_idx, formats) {
    for (DW_Version version = DW_Version_2; version <= DW_Version_5; ++version) {
      DW_Writer *writer = dw_writer_begin(formats[format_idx], version, DW_CompUnitKind_Compile, Arch_x64);
      dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
      DW_WriterAttrib *exprs[ArrayCount(cases)] = {0};
      DW_WriterAttrib *blocks[ArrayCount(cases)] = {0};
      for EachElement(i, cases) {
        dw_writer_tag_begin(writer, cases[i].tag_kind);
        exprs[i] = dw_writer_push_attrib_exprv(writer, cases[i].attrib_kind, DW_ExprEnc_Op(ConstU), DW_ExprEnc_ULEB128(128 + i));
        dw_writer_tag_end(writer);
        // The legacy raw-block API is legal only before DW_FORM_exprloc.
        U8 bytes[] = {DW_ExprOp_ConstU, (U8)(0x80 + i), 0x01};
        DW_WriterForm block = {.kind = DW_WriterFormKind_Block, .block = str8_array_fixed(bytes)};
        TestCheck(dw_writer_attrib_form_is_legal(writer, cases[i].attrib_kind, block) == (version < DW_Version_4));
        if (version < DW_Version_4) {
          dw_writer_tag_begin(writer, cases[i].tag_kind);
          blocks[i] = dw_writer_push_attrib_block(writer, cases[i].attrib_kind, block.block);
          dw_writer_tag_end(writer);
        }
      }

      // Check the historical alternatives independently of expression lowering.
      U64 offset = 0x12345678;
      U64 offset_size = formats[format_idx] == DW_Format_32Bit ? 4 : 8;
      DW_WriterForm data = {.kind = DW_WriterFormKind_Data, .data = str8((U8 *)&offset, offset_size)};
      DW_WriterForm wrong_size = {.kind = DW_WriterFormKind_Data, .data = str8((U8 *)&offset, offset_size == 4 ? 8 : 4)};
      DW_WriterForm uint = {.kind = DW_WriterFormKind_UInt, .uint = 16};
      DW_WriterForm expr = {.kind = DW_WriterFormKind_ExprLoc, .exprloc = str8_lit("\x30")};
      DW_WriterForm ref = {.kind = DW_WriterFormKind_Ref, .ref = writer->root};
      TestCheck(!dw_writer_attrib_form_is_legal(writer, DW_AttribKind_UseLocation, uint));
      TestCheck(!dw_writer_attrib_form_is_legal(writer, DW_AttribKind_UseLocation, ref));
      TestCheck(!dw_writer_attrib_form_is_legal(writer, DW_AttribKind_VTableElemLocation, uint));
      TestCheck(!dw_writer_attrib_form_is_legal(writer, DW_AttribKind_FrameBase, uint));
      TestCheck(!dw_writer_attrib_form_is_legal(writer, DW_AttribKind_FrameBase, ref));
      TestCheck(!dw_writer_attrib_form_is_legal(writer, DW_AttribKind_FrameBase, wrong_size));
      TestCheck(!dw_writer_attrib_form_is_legal(writer, DW_AttribKind_StmtList, wrong_size));
      TestCheck(!dw_writer_attrib_form_is_legal(writer, DW_AttribKind_StmtList, uint));
      TestCheck(!dw_writer_attrib_form_is_legal(writer, DW_AttribKind_Name, expr));
      TestCheck(dw_writer_attrib_form_is_legal(writer, DW_AttribKind_FrameBase, data) == (version < DW_Version_4));
      TestCheck(dw_writer_attrib_form_is_legal(writer, DW_AttribKind_StmtList, data) == (version < DW_Version_4));
      TestCheck(dw_writer_attrib_form_is_legal(writer, DW_AttribKind_UseLocation, data) == (version == DW_Version_2));
      TestCheck(dw_writer_attrib_form_is_legal(writer, DW_AttribKind_VTableElemLocation, data) == (version == DW_Version_3));
      TestCheck(dw_writer_attrib_form_is_legal(writer, DW_AttribKind_VTableElemLocation, ref) == (version == DW_Version_2));
      TestCheck(dw_writer_attrib_form_is_legal(writer, DW_AttribKind_DataMemberLocation, ref) == (version == DW_Version_2));
      TestCheck(dw_writer_attrib_form_is_legal(writer, DW_AttribKind_DataMemberLocation, uint) == (version >= DW_Version_3));
      TestCheck(dw_writer_attrib_form_is_legal(writer, DW_AttribKind_HighPc, uint) == (version >= DW_Version_4));

      DW_AttribKind list_attribs[] = {DW_AttribKind_Location, DW_AttribKind_FrameBase, DW_AttribKind_ReturnAddr,
        DW_AttribKind_StaticLink, DW_AttribKind_Segment, DW_AttribKind_StringLength};
      if (version < DW_Version_4) {
        for EachElement(i, list_attribs) {
          DW_TagKind tag_kind = list_attribs[i] == DW_AttribKind_StringLength ? DW_TagKind_StringType :
                                list_attribs[i] == DW_AttribKind_Location ? DW_TagKind_Variable : DW_TagKind_SubProgram;
          dw_writer_tag_begin(writer, tag_kind);
          dw_writer_push_attrib_data(writer, list_attribs[i], data.data);
          dw_writer_tag_end(writer);
        }
        dw_writer_tag_begin(writer, version == DW_Version_2 ? DW_TagKind_PtrToMemberType : DW_TagKind_SubProgram);
        dw_writer_push_attrib_data(writer, version == DW_Version_2 ? DW_AttribKind_UseLocation : DW_AttribKind_VTableElemLocation, data.data);
        dw_writer_tag_end(writer);
      }
      if (version == DW_Version_2) {
        dw_writer_tag_begin(writer, DW_TagKind_SubProgram);
        dw_writer_push_attrib_ref(writer, DW_AttribKind_VTableElemLocation, writer->root);
        dw_writer_tag_end(writer);
      }
      dw_writer_tag_end(writer);

      DW_Raw raw = dw_input_from_writer_sections(arena, writer);
      for EachElement(i, cases) {
        U8 bytes[] = {DW_ExprOp_ConstU, (U8)(0x80 + i), 0x01};
        TestCheck(exprs[i]->form.reader.kind == (version < DW_Version_4 ? DW_FormKind_Block : DW_FormKind_ExprLoc));
        TestCheck(str8_match(exprs[i]->form.reader.exprloc, str8_array_fixed(bytes), 0));
        if (version < DW_Version_4) {
          TestCheck(blocks[i]->form.reader.kind == DW_FormKind_Block);
          TestCheck(str8_match(blocks[i]->form.reader.block, str8_array_fixed(bytes), 0));
        }
      }
      // Check the header separately so a format/version mismatch cannot hide
      // behind the recursive attribute comparison.
      DW2_UnitHeader header = {0};
      U64 header_size = dw2_read_unit_header(raw.sec[DW_SectionKind_Info].data, 0, &header);
      TestCheck(header_size == writer->root->info_off);
      TestCheck(header.version == version && header.format == formats[format_idx]);
      TestCheck(header.addr_size == writer->address_size && header.abbrev_off == 0);
      TestCheck(dwt_writer_matches(arena, &raw, writer));
      dw_writer_end(&writer);
    }
  }
}

Test(dwarf_line_writer_versions)
{
  DW_Format formats[] = {DW_Format_32Bit, DW_Format_64Bit};
  for EachElement(format_idx, formats) {
    for (DW_Version version = DW_Version_3; version <= DW_Version_5; ++version) {
      DW_Writer *writer = dw_writer_begin(formats[format_idx], version, DW_CompUnitKind_Compile, Arch_arm64);
      dw_writer_tag_begin(writer, DW_TagKind_CompileUnit);
      dw_writer_push_attrib_string(writer, DW_AttribKind_CompDir, str8_lit("c:/devel"));
      dw_writer_push_attrib_string(writer, DW_AttribKind_Name, str8_lit("test.c"));
      DW_WriterAttrib *stmt = dw_writer_push_attrib_line_ptr(writer, DW_AttribKind_StmtList, 0);
      DW_WriterAttrib *expr = dw_writer_push_attrib_exprv(writer, DW_AttribKind_FrameBase, DW_ExprEnc_Op(Reg7));
      dw_writer_tag_end(writer);
      DW_WriterFile *file = dw_writer_new_file(writer, str8_lit("test.c"));
      file->time_stamp = 123;
      file->size = 456;
      // Duplicate row, scaled PC advances, a large advance, backward address,
      // and an address that is not aligned to the minimum instruction length.
      U64 addresses[] = {0, 0, 4, 8, 4096, 4, 7};
      U64 lines[] = {1, 1, 2, 1, 1000, 7, 8};
      for EachElement(i, addresses) { dw_writer_line_emit(writer, file, lines[i], i, addresses[i]); }
      DW_Raw raw = dw_input_from_writer_sections(arena, writer);
      TestCheck(stmt->form.reader.kind == (version >= DW_Version_4 ? DW_FormKind_SecOffset :
                                          formats[format_idx] == DW_Format_64Bit ? DW_FormKind_Data8 : DW_FormKind_Data4));
      TestCheck(expr->form.reader.kind == (version >= DW_Version_4 ? DW_FormKind_ExprLoc : DW_FormKind_Block));
      TestCheck(str8_match(expr->form.reader.exprloc, str8_lit("\x57"), 0));
      TestCheck(dwt_writer_matches(arena, &raw, writer));
      DW2_LineTableHeader header = {0};
      String8 program = {0};
      TestCheck(dwt_line_header(arena, &raw, &header, &program));
      TestCheck(header.version == version && header.format == formats[format_idx]);
      TestCheck(header.min_inst_length == 4 && header.max_ops_per_inst == 1 && header.addr_size == 8);
      U64 file_index = version >= DW_Version_5 ? 0 : 1;
      TestCheck(file->file_idx == file_index);
      TestCheck(header.dirs.count == 1 && header.files.count == file_index + 1);
      TestCheck(str8_match(header.dirs.v[0].file_name, str8_lit("c:/devel"), 0));
      TestCheck(str8_match(header.files.v[file_index].file_name, str8_lit("test.c"), 0));
      TestCheck(header.files.v[file_index].modify_time == 123 && header.files.v[file_index].file_size == 456);
      DW2_LineVM vm = dw2_line_vm_init(&header);
      U64 row = 0, end_rows = 0, old_off = 0;
      while (dw2_line_vm_step(&header, program, &vm)) {
        TestCheck(vm.off > old_off && vm.off <= program.size && !vm.error);
        old_off = vm.off;
        if (vm.emit_row) {
          if (vm.regs.end_sequence) {
            TestCheck(row == ArrayCount(addresses));
            TestCheck(vm.regs.address == 7 && vm.regs.line == 8 && vm.regs.column == 6);
            end_rows += 1;
          } else {
            TestCheck(row < ArrayCount(addresses));
            TestCheck(vm.regs.address == addresses[row] && vm.regs.line == lines[row]);
            TestCheck(vm.regs.column == row && vm.regs.file_index == file_index);
            row += 1;
          }
        }
      }
      TestCheck(!vm.error && vm.off == program.size && row == ArrayCount(addresses) && end_rows == 1);
      dw_writer_end(&writer);
    }
  }
}

Test(dwarf_line_row_resets)
{
  DW2_LineTableHeader header = {.version = DW_Version_5, .addr_size = 8, .min_inst_length = 1,
    .max_ops_per_inst = 1, .default_is_stmt = 1, .line_base = -5, .line_range = 14, .opcode_base = 13};
  U8 bytes[] = {DW_StdOpcode_SetBasicBlock, DW_StdOpcode_SetPrologueEnd, DW_StdOpcode_SetEpilogueBegin,
    0, 2, DW_ExtOpcode_SetDiscriminator, 7, DW_StdOpcode_Copy,
    DW_StdOpcode_AdvancePc, 4, DW_StdOpcode_SetColumn, 3, DW_StdOpcode_SetIsa, 9,
    DW_StdOpcode_NegateStmt, 0, 1, DW_ExtOpcode_EndSequence, DW_StdOpcode_Copy};
  String8 program = str8_array_fixed(bytes);
  DW2_LineVM vm = dw2_line_vm_init(&header);
  TestCheck(vm.regs.file_index == 1 && !vm.reset_regs);
  for (U64 i = 0; i < 4; ++i) { TestCheck(dw2_line_vm_step(&header, program, &vm) && !vm.emit_row); }
  TestCheck(dw2_line_vm_step(&header, program, &vm) && vm.emit_row);
  TestCheck(vm.regs.basic_block && vm.regs.prologue_end && vm.regs.epilogue_begin && vm.regs.discriminator == 7);
  TestCheck(dw2_line_vm_step(&header, program, &vm) && !vm.emit_row);
  TestCheck(vm.regs.address == 4 && !vm.regs.basic_block && !vm.regs.prologue_end && !vm.regs.epilogue_begin && !vm.regs.discriminator);
  for (U64 i = 0; i < 3; ++i) { TestCheck(dw2_line_vm_step(&header, program, &vm) && !vm.emit_row); }
  TestCheck(dw2_line_vm_step(&header, program, &vm) && vm.emit_row && vm.regs.end_sequence);
  TestCheck(vm.reset_regs);
  TestCheck(vm.regs.address == 4 && vm.regs.column == 3 && vm.regs.isa == 9 && !vm.regs.is_stmt);
  TestCheck(dw2_line_vm_step(&header, program, &vm) && vm.emit_row && !vm.regs.end_sequence);
  TestCheck(vm.regs.file_index == 1 && !vm.reset_regs);
  TestCheck(vm.regs.address == 0 && vm.regs.line == 1 && vm.regs.column == 0 && vm.regs.isa == 0 && vm.regs.is_stmt);
  TestCheck(!vm.regs.basic_block && !vm.regs.prologue_end && !vm.regs.epilogue_begin && !vm.regs.discriminator);
  TestCheck(vm.off == program.size && !dw2_line_vm_step(&header, program, &vm) && !vm.error);
}

Test(dwarf_line_vliw)
{
  DW2_LineTableHeader header = {.version = DW_Version_5, .addr_size = 8, .min_inst_length = 4,
    .max_ops_per_inst = 3, .default_is_stmt = 1, .line_base = -5, .line_range = 14, .opcode_base = 13};
  U8 bytes[] = {DW_StdOpcode_AdvancePc, 2, DW_StdOpcode_AdvancePc, 2,
    DW_StdOpcode_FixedAdvancePc, 5, 0, 0x20, DW_StdOpcode_ConstAddPc};
  String8 program = str8_array_fixed(bytes);
  DW2_LineVM vm = dw2_line_vm_init(&header);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.regs.address == 0 && vm.regs.vliw_op_index == 2 && !vm.emit_row);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.regs.address == 4 && vm.regs.vliw_op_index == 1 && !vm.emit_row);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.regs.address == 9 && vm.regs.vliw_op_index == 0 && !vm.emit_row);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.regs.address == 9 && vm.regs.vliw_op_index == 1 && vm.regs.line == 1 && vm.emit_row);
  TestCheck(dw2_line_vm_step(&header, program, &vm));
  TestCheck(vm.regs.address == 33 && vm.regs.vliw_op_index == 0 && !vm.emit_row);
  TestCheck(vm.off == program.size && !dw2_line_vm_step(&header, program, &vm) && !vm.error);
}

Test(dwarf_line_bytecode_bounds)
{
  DW2_LineTableHeader header = {.version = DW_Version_5, .addr_size = 8, .min_inst_length = 1,
    .max_ops_per_inst = 1, .default_is_stmt = 1, .line_base = -5, .line_range = 14, .opcode_base = 13};
  String8 malformed[] = {
    str8_lit("\x02"),             // advance_pc without an operand
    str8_lit("\x03\x80"),         // truncated signed LEB128
    str8_lit("\x09\x01"),         // truncated fixed_advance_pc
    str8_lit("\x00"),             // missing extended length
    str8_lit("\x00\x00"),         // empty extended instruction
    str8_lit("\x00\x01"),         // missing extended opcode
    str8_lit("\x00\x09\x02\x01"), // set_address runs past program end
    str8_lit("\x00\x02\x02\x01"), // set_address's declared payload is too short
    str8_lit("\x00\x02\x04\x80"), // truncated discriminator
  };
  for EachElement(i, malformed) {
    DW2_LineVM vm = dw2_line_vm_init(&header);
    TestCheck(!dw2_line_vm_step(&header, malformed[i], &vm));
    TestCheck(vm.error && !vm.emit_row && vm.off <= malformed[i].size);
  }
  DW2_LineVM vm = dw2_line_vm_init(&header);
  TestCheck(!dw2_line_vm_step(&header, str8_zero(), &vm) && !vm.error && !vm.emit_row);
}

Test(value_in_register)
{
  // setup register context
  X64_RegBlock regs = {0};
  ARCH_Info *arch_info = arch_info_from_arch(Arch_x64);
  ARCH_RegCode reg_code = arch_reg_code_from_dw(Arch_x64, DW_ExprOp_Reg3 - DW_ExprOp_Reg0);
  Rng1U16 reg_range = arch_info->reg_code_rng_table[reg_code];
  U64 value = 0xc0ffee;
  MemoryCopy((U8 *)&regs + reg_range.min, &value, sizeof(value));
  
  // compile a simple program which reads the value from register 3
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(Reg3) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  // evaluate the program
  MemoryMap memory = {0};
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, &regs, 0, 0, 0, &state, expr_data, 16);
  
  // validate eval result
  TestCheck(eval.status == DW_EvalStatus_Good);
  TestCheck(state.off == expr_data.size && state.op_idx == 1);
  TestCheck(eval.val.data.size == 0 && eval.val.u512.u64[0] == value);
  for (U64 i = 1; i < ArrayCount(eval.val.u512.u64); ++i) { TestCheck(eval.val.u512.u64[i] == 0); }
}

Test(value_in_x_register)
{
  // setup register context
  X64_RegBlock regs = {0};
  ARCH_Info *arch_info = arch_info_from_arch(Arch_x64);
  DW_RegCode fsbase = 58;
  ARCH_RegCode reg_code = arch_reg_code_from_dw(Arch_x64, fsbase);
  Rng1U16 reg_range = arch_info->reg_code_rng_table[reg_code];
  U64 value = 0xc0ffee;
  MemoryCopy((U8 *)&regs + reg_range.min, &value, sizeof(value));
  
  // compile a simple program which reads the value from an extended register
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(RegX), DW_ExprEnc_ULEB128(fsbase) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  // evaluate the program
  MemoryMap memory = {0};
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, &regs, 0, 0, 0, &state, expr_data, 16);
  
  // validate eval result
  TestCheck(eval.status == DW_EvalStatus_Good);
  TestCheck(state.off == expr_data.size && state.op_idx == 1);
  TestCheck(eval.val.data.size == 0 && eval.val.u512.u64[0] == value);
  for (U64 i = 1; i < ArrayCount(eval.val.u512.u64); ++i) { TestCheck(eval.val.u512.u64[i] == 0); }
}

Test(address_of_value)
{
  // compile a simple program which reads address
  U64 addr = 0xdeadbeef;
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(Addr), DW_ExprEnc_U64(addr) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  // evaluate the program
  MemoryMap memory = {0};
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, 0, 0, 0, 0, &state, expr_data, 16);
  
  // validate eval result
  TestCheck(eval.status == DW_EvalStatus_Good);
  TestCheck(state.off == expr_data.size && state.op_idx == 1);
  TestCheck(eval.val.data.size == 0 && eval.val.u512.u64[0] == addr);
}

Test(register_relative_variable)
{
  ARCH_Info *arch_info = arch_info_from_arch(Arch_x64);
  
  // setup register context
  X64_RegBlock regs = {0};
  DW_RegCode reg_dw_id = (DW_ExprOp_BReg11 - DW_ExprOp_BReg0);
  ARCH_RegCode reg_code = arch_reg_code_from_dw(Arch_x64, reg_dw_id);
  Rng1U16 reg_rng = arch_info->reg_code_rng_table[reg_code];
  U64 value = 1;
  MemoryCopy((U8 *)&regs + reg_rng.min, &value, sizeof(value));
  
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(BReg11), DW_ExprEnc_SLEB128(44) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  MemoryMap memory = {0};
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, &regs, 0, 0, 0, &state, expr_data, 16);
  
  // validate eval result
  TestCheck(eval.status == DW_EvalStatus_Good);
  TestCheck(state.off == expr_data.size && state.op_idx == 1);
  TestCheck(eval.val.data.size == 0 && eval.val.u512.u64[0] == 1 + 44);
}

Test(frame_relative_variable)
{
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(FBReg), DW_ExprEnc_SLEB128(-50) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  U64 frame_base = 123;
  MemoryMap memory = {0};
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, 0, frame_base, 0, 0, &state, expr_data, 16);
  
  TestCheck(eval.status == DW_EvalStatus_Good);
  TestCheck(state.off == expr_data.size && state.op_idx == 1);
  TestCheck(eval.val.data.size == 0 && eval.val.u512.u64[0] == frame_base - 50);
}

Test(call_by_reference)
{
  U8 *memory = push_array(arena, U8, 128);
  U64 value = 0xc0ffee;
  MemoryCopy(memory + 32, &value, sizeof(value));
  
  ARCH_Info *arch_info = arch_info_from_arch(Arch_x64);
  X64_RegBlock regs = {0};
  ARCH_RegCode reg_code = arch_reg_code_from_dw(Arch_x64, 58); // fsbase
  Rng1U16 reg_rng = arch_info->reg_code_rng_table[reg_code];
  U64 memory_ptr = 0x1000;
  MemoryCopy((U8 *)&regs + reg_rng.min, &memory_ptr, sizeof(memory_ptr));
  
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(BRegX), DW_ExprEnc_ULEB128(58), DW_ExprEnc_SLEB128(32), DW_ExprEnc_Op(Deref) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  MemoryMap map = {0};
  memory_map_push(arena, &map, r1u64(memory_ptr, memory_ptr + 128), memory);
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &map, &regs, 0, 0, 0, &state, expr_data, 16);
  
  TestCheck(eval.status == DW_EvalStatus_Good);
  TestCheck(state.off == expr_data.size && state.op_idx == 2);
  TestCheck(eval.val.data.size == 0 && eval.val.u512.u64[0] == value);
  for (U64 i = 1; i < ArrayCount(eval.val.u512.u64); ++i) { TestCheck(eval.val.u512.u64[i] == 0); }
}

Test(plus_uconst)
{
  U64 struct_addr = 0x123;
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(Addr), DW_ExprEnc_Addr(struct_addr), DW_ExprEnc_Op(PlusUConst), DW_ExprEnc_ULEB128(4) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  MemoryMap memory = {0};
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, 0, 0, 0, 0, &state, expr_data, 16);
  
  TestCheck(eval.status == DW_EvalStatus_Good);
  TestCheck(state.off == expr_data.size && state.op_idx == 2);
  TestCheck(eval.val.data.size == 0 && eval.val.u512.u64[0] == struct_addr + 4);
}

Test(reg_split_spill)
{
  // setup register context
  X64_RegBlock regs      = {0};
  {
    ARCH_RegCode reg_code  = arch_reg_code_from_dw(Arch_x64, DW_ExprOp_BReg3 - DW_ExprOp_BReg0);
    Rng1U16      reg_range = arch_info_from_arch(Arch_x64)->reg_code_rng_table[reg_code];
    U64          value     = 0xaaaa;
    MemoryCopy((U8 *)&regs + reg_range.min, &value, sizeof(value));
  }
  {
    ARCH_RegCode reg_code  = arch_reg_code_from_dw(Arch_x64, DW_ExprOp_BReg10 - DW_ExprOp_BReg0);
    Rng1U16      reg_range = arch_info_from_arch(Arch_x64)->reg_code_rng_table[reg_code];
    U64          value     = 0xbbbb;
    MemoryCopy((U8 *)&regs + reg_range.min, &value, sizeof(value));
  }
  
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(Reg3), DW_ExprEnc_Op(Piece), DW_ExprEnc_ULEB128(4), DW_ExprEnc_Op(Reg10), DW_ExprEnc_Op(Piece), DW_ExprEnc_ULEB128(2) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  MemoryMap memory = {0};
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, &regs, 0, 0, 0, &state, expr_data, 16);
  TestCheck(eval.status == DW_EvalStatus_UnsupportedOp);
  TestCheck(state.off == 3 && state.op_idx == 2); // stopped at the first DW_OP_piece
}

Test(terminal_piece_unsupported)
{
  X64_RegBlock regs = {0};
  String8 exprs[] = {
    str8_lit("\x53\x93\x04"),     // DW_OP_reg3, DW_OP_piece(4)
    str8_lit("\x53\x9d\x20\x00"), // DW_OP_reg3, DW_OP_bit_piece(32, 0)
  };
  for EachElement(i, exprs) {
    MemoryMap memory = {0};
    DW_EvalState state = {0};
    DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, &regs, 0, 0, 0, &state, exprs[i], 16);
    
    // reaching the end of the expression must preserve the unsupported status
    TestCheck(eval.status == DW_EvalStatus_UnsupportedOp);
    TestCheck(state.off == exprs[i].size && state.op_idx == 2);
  }
}

Test(terminal_deref_failed_memory_read)
{
  U64 addr = 0x1000;
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(Addr), DW_ExprEnc_Addr(addr), DW_ExprEnc_Op(Deref) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  MemoryMap memory = {0};
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, 0, 0, 0, 0, &state, expr_data, 16);
  
  TestCheck(eval.status == DW_EvalStatus_FailedMemoryRead);
  TestCheck(eval.missed_read_vaddr_range.min == addr);
  TestCheck(eval.missed_read_vaddr_range.max == addr + sizeof(U64));
  TestCheck(state.off == expr_data.size - 1 && state.op_idx == 1);
}

Test(extended_register_negative_offset)
{
  // setup register context
  X64_RegBlock regs = {0};
  ARCH_Info *arch_info = arch_info_from_arch(Arch_x64);
  DW_RegCode fsbase = 58;
  ARCH_RegCode reg_code = arch_reg_code_from_dw(Arch_x64, fsbase);
  Rng1U16 reg_range = arch_info->reg_code_rng_table[reg_code];
  U64 value = 0x1000;
  MemoryCopy((U8 *)&regs + reg_range.min, &value, sizeof(value));
  
  DW_ExprEnc expr_encs[] = { DW_ExprEnc_Op(BRegX), DW_ExprEnc_ULEB128(fsbase), DW_ExprEnc_SLEB128(-129) };
  String8    expr_data   = dw_encode_expr(arena, Arch_x64, DW_Format_64Bit, expr_encs, ArrayCount(expr_encs));
  
  MemoryMap memory = {0};
  DW_EvalState state = {0};
  DW_Eval eval = dw_eval(arena, DW_Format_64Bit, Arch_x64, &memory, &regs, 0, 0, 0, &state, expr_data, 16);
  
  // validate eval result
  TestCheck(eval.status == DW_EvalStatus_Good);
  TestCheck(state.off == expr_data.size && state.op_idx == 1);
  TestCheck(eval.val.data.size == 0 && eval.val.u512.u64[0] == value - 129);
}
