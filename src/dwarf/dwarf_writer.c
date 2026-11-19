// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

internal DW_IntEnc
dw_int_enc_from_sint(S64 v)
{
  if ((S8)v == v)  return DW_IntEnc_1Byte;
  if ((S16)v == v) return DW_IntEnc_2Byte;
  if ((S32)v == v) return DW_IntEnc_4Byte;
  return DW_IntEnc_LEB128;
}

internal DW_IntEnc
dw_int_enc_from_uint(U64 v)
{
  if ((U8)v == v)  return DW_IntEnc_1Byte;
  if ((U16)v == v) return DW_IntEnc_2Byte;
  if ((U32)v == v) return DW_IntEnc_4Byte;
  return DW_IntEnc_LEB128;
}

internal U64
dw_size_from_sint(S64 v)
{
  DW_IntEnc enc = dw_int_enc_from_sint(v);
  switch (enc) {
    case DW_IntEnc_Null: break;
    case DW_IntEnc_1Byte:  return 1;
    case DW_IntEnc_2Byte:  return 2;
    case DW_IntEnc_4Byte:  return 4;
    case DW_IntEnc_LEB128: return dw_size_from_sleb128(v);
    default: InvalidPath; break;
  }
  return 0;
}

internal U64
dw_size_from_uint(U64 v)
{
  DW_IntEnc enc = dw_int_enc_from_uint(v);
  switch (enc) {
    case DW_IntEnc_Null: break;
    case DW_IntEnc_1Byte:  return 1;
    case DW_IntEnc_2Byte:  return 2;
    case DW_IntEnc_4Byte:  return 4;
    case DW_IntEnc_LEB128: return dw_size_from_uleb128(v);
    default: InvalidPath; break;
  }
  return 0;
}

internal DW_WriterFixupNode *
dw_writer_fixup_list_push(Arena *arena, DW_WriterFixupList *list, DW_WriterFixup v)
{
  DW_WriterFixupNode *n = push_array(arena, DW_WriterFixupNode, 1);
  n->v = v;
  SLLQueuePush(list->first, list->last, n);
  list->count += 1;
  return n;
}

internal DW_WriterAttrib *
dw_writer_attrib_chunk_list_push(Arena *arena, DW_WriterAttribChunkList *list, U64 cap)
{
  if (list->last == 0 || list->last->count >= list->last->max) {
    DW_WriterAttribChunk *n = push_array(arena, DW_WriterAttribChunk, 1);
    n->v = push_array_no_zero(arena, DW_WriterAttrib, cap);
    n->max = cap;
    SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  
  DW_WriterAttrib *result = &list->last->v[list->last->count++];
  MemoryZeroStruct(result);
  
  list->total_count += 1;
  
  return result;
}

internal DW_WriterTag *
dw_writer_tag_chunk_list_push(Arena *arena, DW_WriterTagChunkList *list, U64 cap)
{
  if (list->last == 0 || list->last->count >= list->last->max) {
    DW_WriterTagChunk *n = push_array(arena, DW_WriterTagChunk, 1);
    n->v = push_array_no_zero(arena, DW_WriterTag, cap);
    n->max = cap;
    SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  
  DW_WriterTag *result = &list->last->v[list->last->count++];
  MemoryZeroStruct(result);
  
  list->total_count += 1;
  
  return result;
}

internal U64
dw_write_to_buffer_uleb128(U8 buffer[10], U64 v)
{
  U64 buffer_size = 0;
  U64 value = v;
  do {
    U8 byte = value & 0x7f;
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    Assert(buffer_size < 10);
    buffer[buffer_size++] = byte;
  } while (value > 0);
  return buffer_size;
}

internal U64
dw_write_to_buffer_sleb128(U8 buffer[10], S64 v)
{
  U64 buffer_size = 0;
  for (S64 value = v, more = 1; more != 0; ) {
    U8 byte = value & 0x7f;
    value >>= 7;
    U8 sign_bit = byte & 0x40;
    if ((value == 0 && sign_bit == 0) || (value == -1 && sign_bit != 0)) {
      more = 0;
    } else {
      byte |= 0x80;
    }
    Assert(buffer_size < 10);
    buffer[buffer_size++] = byte;
  }
  return buffer_size;
}

internal String8
dw_write_uleb128(Arena *arena, U64 v)
{
  U8 buffer[10];
  U64 buffer_size = dw_write_to_buffer_uleb128(buffer, v);
  return str8_copy(arena, str8(buffer, buffer_size));
}

internal String8
dw_write_sleb128(Arena *arena, S64 v)
{
  U8 buffer[10];
  U64 buffer_size = dw_write_to_buffer_sleb128(buffer, v);
  return str8_copy(arena, str8(buffer, buffer_size));
}

internal U64
dw_size_from_uleb128(U64 v)
{
  U8 buffer[10];
  return dw_write_to_buffer_uleb128(buffer, v);
}

internal U64
dw_size_from_sleb128(S64 v)
{
  U8 buffer[10];
  return dw_write_to_buffer_sleb128(buffer, v);
}

internal void *
dw_serial_push_length(Arena *arena, String8List *srl, DW_Format format, U64 length)
{
  void *result;
  if (format == DW_Format_64Bit) {
    U8 buffer[sizeof(U32) + sizeof(U64)];
    MemorySet(&buffer[0], 0xff, sizeof(U32));
    MemoryCopy(buffer + sizeof(U32), &length, sizeof(length));
    result = str8_serial_push_string(arena, srl, str8_array_fixed(buffer));
  } else {
    Assert(format == DW_Format_32Bit && length < 0xfffffff0);
    result = str8_serial_push_u32(arena, srl, safe_cast_u32(length));
  }
  return result;
}

internal void *
dw_serial_push_uint(Arena *arena, String8List *srl, DW_Format format, U64 v)
{
  String8 uint;
  if (format == DW_Format_64Bit) {
    U64 *ptr = push_array(arena, U64, 1);
    *ptr = v;
    uint = str8((U8 *)ptr, sizeof(*ptr));
  } else {
    Assert(format == DW_Format_32Bit);
    U32 *ptr = push_array(arena, U32, 1);
    *ptr = safe_cast_u32(v);
    uint = str8((U8 *)ptr, sizeof(*ptr));
  }
  return str8_serial_push_string(arena, srl, uint);
}

internal void *
dw_serial_push_uleb128(Arena *arena, String8List *srl, U64 v)
{
  U8 buffer[10];
  U64 buffer_size = dw_write_to_buffer_uleb128(buffer, v);
  return str8_serial_push_string(arena, srl, str8(buffer, buffer_size));
}

internal void *
dw_serial_push_sleb128(Arena *arena, String8List *srl, S64 v)
{
  U8 buffer[10];
  U64 buffer_size = dw_write_to_buffer_sleb128(buffer, v);
  return str8_serial_push_string(arena, srl, str8(buffer, buffer_size));
}

internal DW_Writer *
dw_writer_begin(DW_Format format, DW_Version version, DW_CompUnitKind cu_kind, Arch arch)
{
  Assert(format == DW_Format_32Bit || format == DW_Format_64Bit);
  Assert(DW_Version_2 <= version && version <= DW_Version_5);
  Assert(cu_kind == DW_CompUnitKind_Compile || cu_kind == DW_CompUnitKind_Partial);
  Arena *arena = arena_alloc();
  DW_Writer *writer = push_array(arena, DW_Writer, 1);
  writer->arena                 = arena;
  writer->arch                  = arch;
  writer->format                = format;
  writer->version               = version;
  writer->cu_kind               = cu_kind;
  writer->address_size          = byte_size_from_arch(arch);
  writer->current               = 0;
  writer->abbrev_id_map         = hash_table_init(arena, 0x2000);
  writer->line.max_ops_per_inst = 1;
  switch (arch) {
    case Arch_x86:
    case Arch_x64:   writer->line.min_inst_len = 1; break;
    case Arch_arm32: writer->line.min_inst_len = 2; break;
    case Arch_arm64: writer->line.min_inst_len = 4; break;
    default: InvalidPath; break;
  }
  writer->line.default_is_stmt  = 1;
  writer->line.line_base        = -5;
  writer->line.line_range       = 14;
  writer->line.opcode_base      = DW_StdOpcode_COUNT;
  writer->line.ln               = 1;
  Assert(writer->line.line_base < writer->line.line_range);
  for EachElement(i, writer->sections) {
    str8_serial_begin(arena, &writer->sections[i].srl);
  }
  
  // write info header
  {
    String8List *srl = &writer->sections[DW_SectionKind_Info].srl;
    
    // length
    writer->sections[DW_SectionKind_Info].length = dw_serial_push_length(writer->arena, srl, writer->format, 0);
    
    // version
    str8_serial_push_struct(writer->arena, srl, &writer->version);
    
    if (version >= DW_Version_5) {
      str8_serial_push_u8(writer->arena, srl, (U8)writer->cu_kind);
      str8_serial_push_u8(writer->arena, srl, writer->address_size);
    }
    
    // reserve 'abbrev_base'
    writer->abbrev_base_info_off = srl->total_size;
    dw_serial_push_uint(writer->arena, srl, writer->format, 0);
    
    if (version < DW_Version_5) {
      str8_serial_push_u8(writer->arena, srl, writer->address_size);
    }
  }
  
  return writer;
}

internal void
dw_writer_end(DW_Writer **writer_ptr)
{
  arena_release((*writer_ptr)->arena);
  *writer_ptr = 0;
}

internal U64
dw_serial_push_form(Arena *arena, String8List *srl, DW_Version version, DW_Format format, U8 address_size, DW_WriterFixupList *fixups, DW_WriterXForm form)
{
  U64 start_off = srl->total_size;
  switch (form.reader.kind) {
    case DW_FormKind_Addr: {
      Assert(form.reader.addr.size == address_size);
      str8_serial_push_string(arena, srl, form.reader.addr);
    } break;
    case DW_FormKind_Block1:
    case DW_FormKind_Block2:
    case DW_FormKind_Block4: {
      switch (form.reader.kind) {
        case DW_FormKind_Block1: {
          Assert(form.reader.block.size <= max_U8);
          str8_serial_push_u8(arena, srl, (U8)form.reader.block.size);
        } break;
        case DW_FormKind_Block2: {
          Assert(form.reader.block.size <= max_U16);
          str8_serial_push_u16(arena, srl, (U16)form.reader.block.size);
        } break;
        case DW_FormKind_Block4: {
          str8_serial_push_u32(arena, srl, safe_cast_u32(form.reader.block.size));
        } break;
      }
      str8_serial_push_string(arena, srl, form.reader.block);
    } break;
    case DW_FormKind_Data1:
    case DW_FormKind_Data2:
    case DW_FormKind_Data4:
    case DW_FormKind_Data8:
    case DW_FormKind_Data16: {
      U64 size = (form.reader.kind == DW_FormKind_Data1 ? 1 :
                  form.reader.kind == DW_FormKind_Data2 ? 2 :
                  form.reader.kind == DW_FormKind_Data4 ? 4 :
                  form.reader.kind == DW_FormKind_Data8 ? 8 : 16);
      Assert(form.reader.data.size == size);
      str8_serial_push_string(arena, srl, form.reader.data);
    } break;
    case DW_FormKind_String: {
      str8_serial_push_cstr(arena, srl, form.reader.string);
    } break;
    case DW_FormKind_Block: {
      dw_serial_push_uleb128(arena, srl, form.reader.block.size);
      str8_serial_push_string(arena, srl, form.reader.block);
    } break;
    case DW_FormKind_Flag: {
      str8_serial_push_u8(arena, srl, form.reader.flag);
    } break;
    case DW_FormKind_SData: {
      dw_serial_push_sleb128(arena, srl, form.reader.sdata);
    } break;
    case DW_FormKind_Strp: {
      dw_serial_push_uint(arena, srl, format, form.reader.sec_offset);
    } break;
    case DW_FormKind_UData: {
      dw_serial_push_uleb128(arena, srl, form.reader.udata);
    } break;
    case DW_FormKind_RefAddr: {
      if (version < DW_Version_3) {
        Assert(address_size <= sizeof(form.reader.ref));
        str8_serial_push_string(arena, srl, str8((U8 *)&form.reader.ref, address_size));
      } else {
        dw_serial_push_uint(arena, srl, format, form.reader.ref);
      }
    } break;
    case DW_FormKind_Ref1: {
      str8_serial_push_u8(arena, srl, (U8)form.reader.ref);
    } break;
    case DW_FormKind_Ref2: {
      str8_serial_push_u16(arena, srl, (U16)form.reader.ref);
    } break;
    case DW_FormKind_Ref4:
    case DW_FormKind_RefSup4: {
      str8_serial_push_u32(arena, srl, (U32)form.reader.ref);
    } break;
    case DW_FormKind_Ref8: {
      DW_WriterTag *tag = form.writer.kind == DW_WriterFormKind_Ref ? form.writer.ref : 0;
      void *value = str8_serial_push_u64(arena, srl, tag ? tag->info_off : form.reader.ref);
      if (tag && tag->info_off == 0) {
        Assert(fixups != 0);
        dw_writer_fixup_list_push(arena, fixups, (DW_WriterFixup){ .ptr = value, .tag = tag });
      }
    } break;
    case DW_FormKind_RefUData: {
      dw_serial_push_uleb128(arena, srl, form.reader.ref);
    } break;
    case DW_FormKind_Indirect: {
      NotImplemented;
    } break;
    case DW_FormKind_SecOffset: {
      dw_serial_push_uint(arena, srl, format, form.reader.sec_offset);
    } break;
    case DW_FormKind_ExprLoc: {
      dw_serial_push_uleb128(arena, srl, form.reader.exprloc.size);
      str8_serial_push_string(arena, srl, form.reader.exprloc);
    } break;
    case DW_FormKind_FlagPresent: {
    } break;
    case DW_FormKind_RefSig8:
    case DW_FormKind_RefSup8: {
      str8_serial_push_u64(arena, srl, form.reader.ref);
    } break;
    case DW_FormKind_Strx:
    case DW_FormKind_Addrx:
    case DW_FormKind_RngListx:
    case DW_FormKind_LocListx:
    case DW_GNU_FormKind_AddrIndex:
    case DW_GNU_FormKind_StrIndex: {
      dw_serial_push_uleb128(arena, srl, form.reader.xval);
    } break;
    case DW_FormKind_StrpSup: {
      dw_serial_push_uint(arena, srl, format, form.reader.strp_sup);
    } break;
    case DW_FormKind_LineStrp: {
      dw_serial_push_uint(arena, srl, format, form.reader.sec_offset);
    } break;
    case DW_FormKind_ImplicitConst: {
      // value is stored in the abbrev entry
    } break;
    case DW_FormKind_Strx1:
    case DW_FormKind_Addrx1: {
      str8_serial_push_u8(arena, srl, (U8)form.reader.xval);
    } break;
    case DW_FormKind_Strx2:
    case DW_FormKind_Addrx2: {
      str8_serial_push_u16(arena, srl, (U16)form.reader.xval);
    } break;
    case DW_FormKind_Strx3:
    case DW_FormKind_Addrx3: {
      str8_serial_push_string(arena, srl, str8((U8 *)&form.reader.xval, 3));
    } break;
    case DW_FormKind_Strx4:
    case DW_FormKind_Addrx4: {
      str8_serial_push_u32(arena, srl, (U32)form.reader.xval);
    } break;
    
    case DW_GNU_FormKind_StrpAlt: {
      dw_serial_push_uint(arena, srl, format, form.reader.sec_offset);
    } break;
    case DW_GNU_FormKind_RefAlt: {
      dw_serial_push_uint(arena, srl, format, form.reader.ref);
    } break;
    
    default: InvalidPath; break;
  }
  return srl->total_size - start_off;
}

internal String8
dw_make_abbrev_entry(Arena *arena, DW_WriterTag *tag)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  String8List srl = {0};
  str8_serial_begin(scratch.arena, &srl);
  
  // tag abbrev
  B8 has_children = tag->first_child != 0;
  dw_serial_push_uleb128(scratch.arena, &srl, tag->kind);
  str8_serial_push_string(scratch.arena, &srl, str8_struct(&has_children)); 
  
  // attrib abbrev
  for EachNode(attrib, DW_WriterAttrib, tag->first_attrib) {
    dw_serial_push_uleb128(scratch.arena, &srl, attrib->kind);
    dw_serial_push_uleb128(scratch.arena, &srl, attrib->form.reader.kind);
    if (attrib->form.reader.kind == DW_FormKind_ImplicitConst) {
      dw_serial_push_sleb128(scratch.arena, &srl, attrib->form.reader.implicit_const);
    }
  }
  
  // closing entry
  dw_serial_push_uleb128(scratch.arena, &srl, 0);
  dw_serial_push_uleb128(scratch.arena, &srl, 0);
  
  String8 tag_abbrev = str8_serial_end(arena, &srl);
  
  scratch_end(scratch);
  return tag_abbrev;
}

internal DW_WriterTag *
dw_writer_tag_begin(DW_Writer *writer, DW_TagKind kind)
{
  DW_WriterTag *tag = dw_writer_tag_reserve(writer, kind);
  dw_writer_tag_begin_reserved(writer, tag);
  return tag;
}

internal void
dw_writer_tag_end(DW_Writer *writer)
{
  // pop to parent
  writer->current = writer->current->parent;
}

internal DW_WriterTag *
dw_writer_tag_reserve(DW_Writer *writer, DW_TagKind kind)
{
  DW_WriterTag *tag = dw_writer_tag_chunk_list_push(writer->arena, &writer->tag_chunk_list, 512);
  tag->kind = kind;
  return tag;
}

internal void
dw_writer_tag_begin_reserved(DW_Writer *writer, DW_WriterTag *tag)
{
  tag->parent = writer->current;
  
  // add tag to the tree
  if (writer->current) {
    SLLQueuePush(writer->current->first_child, writer->current->last_child, tag);
  } else {
    writer->root = tag;
  }
  writer->current = tag;
}

internal B32
dw_writer_attrib_form_is_legal(DW_Writer *writer, DW_AttribKind kind, DW_WriterForm form)
{
  // validate attribute-form encoding
  B32            is_enc_legal    = 0;
  DW_AttribClass attrib_class = dw_attrib_class_from_kind(writer->version, 0, kind);
  // These metadata entries retain their DWARF 2 classes. Validate writer
  // semantics here: lowering uses data4/data8 for pre-v4 line pointers and
  // block for pre-v4 expressions, sec_offset/exprloc starting with v4.
  switch (kind) {
    case DW_AttribKind_StmtList: {
      attrib_class = DW_AttribClass_LinePtr;
    } break;
    case DW_AttribKind_Location:
    case DW_AttribKind_FrameBase:
    case DW_AttribKind_ReturnAddr:
    case DW_AttribKind_StaticLink:
    case DW_AttribKind_Segment:
    case DW_AttribKind_StringLength: {
      // The old constant class denotes a location-list offset, not an
      // arbitrary integer constant. Raw legacy offsets must be format-sized.
      attrib_class = DW_AttribClass_ExprLoc|DW_AttribClass_LocListPtr;
      if (writer->version < DW_Version_4) { attrib_class |= DW_AttribClass_Block; }
    } break;
    case DW_AttribKind_UseLocation: {
      // The pointer-to-member computation is expression-only starting in
      // DWARF 3; DWARF 2 also allowed a location-list offset.
      attrib_class = DW_AttribClass_ExprLoc;
      if (writer->version < DW_Version_3) { attrib_class |= DW_AttribClass_LocListPtr; }
      if (writer->version < DW_Version_4) { attrib_class |= DW_AttribClass_Block; }
    } break;
    case DW_AttribKind_VTableElemLocation: {
      // Like member locations, DWARF 3 replaced the reference alternative
      // with a location list, but did not add an integer-constant alternative.
      attrib_class = DW_AttribClass_ExprLoc;
      if (writer->version < DW_Version_3) { attrib_class |= DW_AttribClass_Reference; }
      else { attrib_class |= DW_AttribClass_LocListPtr; }
      if (writer->version < DW_Version_4) { attrib_class |= DW_AttribClass_Block; }
    } break;
    case DW_AttribKind_DataMemberLocation: {
      // DWARF 3 added constant member offsets and location lists, replacing
      // the DWARF 2 reference alternative. Expressions exist in all versions.
      attrib_class = DW_AttribClass_ExprLoc;
      if (writer->version < DW_Version_3) { attrib_class |= DW_AttribClass_Reference; }
      else { attrib_class |= DW_AttribClass_Const|DW_AttribClass_LocListPtr; }
      if (writer->version < DW_Version_4) { attrib_class |= DW_AttribClass_Block; }
    } break;
    case DW_AttribKind_HighPc: {
      // A constant is an offset from low_pc only in DWARF 4 and later.
      attrib_class = DW_AttribClass_Address;
      if (writer->version >= DW_Version_4) { attrib_class |= DW_AttribClass_Const; }
    } break;
  }
  for (DW_AttribClass f = attrib_class; f != 0; f &= f - 1) {
    DW_AttribClass v = f & -f;
    switch (v) {
      case DW_AttribClass_Null:          { is_enc_legal = (form.kind == DW_WriterFormKind_Null);                                        } break;
      case DW_AttribClass_Undefined:     { is_enc_legal = 0;                                                                            } break;
      case DW_AttribClass_Address:       { is_enc_legal = (form.kind == DW_WriterFormKind_Address);                                     } break;
      case DW_AttribClass_Block:         { is_enc_legal = (form.kind == DW_WriterFormKind_Block);                                       } break;
      case DW_AttribClass_Const:         { is_enc_legal = (form.kind == DW_WriterFormKind_SInt ||
                                                           form.kind == DW_WriterFormKind_UInt ||
                                                            form.kind == DW_WriterFormKind_Data ||
                                                            form.kind == DW_WriterFormKind_Implicit);                                    } break;
      case DW_AttribClass_ExprLoc:       { is_enc_legal = (form.kind == DW_WriterFormKind_ExprLoc);                                     } break;
      case DW_AttribClass_Flag:          { is_enc_legal = (form.kind == DW_WriterFormKind_Flag);                                        } break;
      case DW_AttribClass_LinePtr:       { is_enc_legal = (form.kind == DW_WriterFormKind_LinePtr ||
                                                         (writer->version < DW_Version_4 && form.kind == DW_WriterFormKind_Data &&
                                                          form.data.size == dw_addr_size_from_format(writer->format)));                } break;
      case DW_AttribClass_LocListPtr:    { is_enc_legal = (writer->version < DW_Version_4 && form.kind == DW_WriterFormKind_Data &&
                                                         form.data.size == dw_addr_size_from_format(writer->format)); /* TODO: sec_offset */ } break;
      case DW_AttribClass_MacPtr:        { is_enc_legal = (form.kind == DW_WriterFormKind_MacPtr);                                      } break;
      case DW_AttribClass_RngListPtr:    { is_enc_legal = (form.kind == DW_WriterFormKind_RngListPtr);                                  } break;
      case DW_AttribClass_Reference:     { is_enc_legal = (form.kind == DW_WriterFormKind_Ref);                                         } break;
      case DW_AttribClass_String:        { is_enc_legal = (form.kind == DW_WriterFormKind_String);                                      } break;
      case DW_AttribClass_LocList:       { is_enc_legal = 0; /* TODO */                                                                 } break;
      case DW_AttribClass_RngList:       { is_enc_legal = 0; /* TODO */                                                                 } break;
      case DW_AttribClass_StrOffsetsPtr: { is_enc_legal = 0; /* TODO */                                                                 } break;
      case DW_AttribClass_AddrPtr:       { is_enc_legal = 0; /* TODO */                                                                 } break;
      default:                           { InvalidPath;                                                                                 } break;
    }
    if (is_enc_legal) { break; }
  }
  return is_enc_legal;
}

internal DW_WriterAttrib *
dw_writer_push_attrib(DW_Writer *writer, DW_AttribKind kind, DW_WriterForm form)
{
  Assert(dw_writer_attrib_form_is_legal(writer, kind, form));
  
  DW_WriterAttrib *attrib = dw_writer_attrib_chunk_list_push(writer->arena, &writer->attrib_chunk_list, 512);
  attrib->kind        = kind;
  attrib->form.writer = form;
  SLLQueuePush(writer->current->first_attrib, writer->current->last_attrib, attrib);
  writer->current->attrib_count += 1;
  return attrib;
}

internal DW_WriterAttrib *
dw_writer_push_attrib_address(DW_Writer *writer, DW_AttribKind kind, U64 address)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_Address, .address = address });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_block(DW_Writer *writer, DW_AttribKind kind, String8 block)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_Block, .block = str8_copy(writer->arena, block) });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_data(DW_Writer *writer, DW_AttribKind kind, String8 data)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_Data, .data = str8_copy(writer->arena, data) });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_string(DW_Writer *writer, DW_AttribKind kind, String8 string)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_String, .string = str8_copy(writer->arena, string) });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_stringf(DW_Writer *writer, DW_AttribKind kind, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  String8 string = push_str8fv(writer->arena, fmt, args);
  va_end(args);
  return dw_writer_push_attrib_string(writer, kind, string);
}

internal DW_WriterAttrib *
dw_writer_push_attrib_flag(DW_Writer *writer, DW_AttribKind kind, B8 flag)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_Flag, .flag = flag });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_sint(DW_Writer *writer, DW_AttribKind kind, S64 sint)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_SInt, .sint = sint});
}

internal DW_WriterAttrib *
dw_writer_push_attrib_uint(DW_Writer *writer, DW_AttribKind kind, U64 uint)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_UInt, .uint = uint });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_enum(DW_Writer *writer, DW_AttribKind kind, S64 e)
{
  return dw_writer_push_attrib_sint(writer, kind, e);
}

internal DW_WriterAttrib *
dw_writer_push_attrib_ref(DW_Writer *writer, DW_AttribKind kind, DW_WriterTag *ref)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_Ref, .ref = ref });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_exprloc(DW_Writer *writer, DW_AttribKind kind, String8 exprloc)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_ExprLoc, .exprloc = str8_copy(writer->arena, exprloc) });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_expr(DW_Writer *writer, DW_AttribKind kind, DW_ExprEnc *encs, U64 encs_count)
{
  String8 exprloc = dw_encode_expr(writer->arena, writer->arch, writer->format, encs, encs_count);
  return dw_writer_push_attrib_exprloc(writer, kind, exprloc);
}

internal DW_WriterAttrib *
dw_writer_push_attrib_line_ptr(DW_Writer *writer, DW_AttribKind kind, U64 line_ptr)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_LinePtr, .line_ptr = line_ptr });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_mac_ptr(DW_Writer *writer, DW_AttribKind kind, void *mac_ptr)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_MacPtr, .mac_ptr = mac_ptr });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_rng_list_ptr(DW_Writer *writer, DW_AttribKind kind, void *rng_list_ptr)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_RngListPtr, .rng_list_ptr = rng_list_ptr });
}

internal DW_WriterAttrib *
dw_writer_push_attrib_implicit(DW_Writer *writer, DW_AttribKind kind, S64 implicit)
{
  return dw_writer_push_attrib(writer, kind, (DW_WriterForm){ .kind = DW_WriterFormKind_Implicit, .implicit = implicit });
}

internal void
dw_line_inst_list_push_node(DW_LineInstList *list, DW_LineInstNode *node)
{
  SLLQueuePush(list->first, list->last, node);
  list->count += 1;
}

internal DW_LineInstNode *
dw_line_inst_list_push(Arena *arena, DW_LineInstList *list, DW_LineInst v)
{
  DW_LineInstNode *node = push_array(arena, DW_LineInstNode, 1);
  node->v = v;
  dw_line_inst_list_push_node(list, node);
  return node;
}

internal String8
dw_data_from_line_insts(Arena *arena, U8 address_size, DW_LineInstList insts)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List srl = {0};
  str8_serial_begin(scratch.arena, &srl);
  
  for EachNode(inst_n, DW_LineInstNode, insts.first) {
    DW_LineInst *inst = &inst_n->v;
    
    str8_serial_push_struct(scratch.arena, &srl, &inst->opcode);
    switch (inst->opcode) {
      case DW_StdOpcode_Copy: {} break;
      case DW_StdOpcode_AdvancePc: {
        dw_serial_push_uleb128(arena, &srl, inst->advance_pc);
      } break;
      case DW_StdOpcode_AdvanceLine: {
        dw_serial_push_sleb128(arena, &srl, inst->advance_line);
      } break;
      case DW_StdOpcode_SetFile: {
        dw_serial_push_uleb128(arena, &srl, inst->set_file->file_idx);
      } break;
      case DW_StdOpcode_SetColumn: {
        dw_serial_push_uleb128(arena, &srl, inst->set_column);
      } break;
      case DW_StdOpcode_NegateStmt: {} break;
      case DW_StdOpcode_SetBasicBlock: {} break;
      case DW_StdOpcode_ConstAddPc: {} break;
      case DW_StdOpcode_FixedAdvancePc: {
        str8_serial_push_u16(arena, &srl, inst->fixed_advance_pc);
      } break;
      case DW_StdOpcode_SetPrologueEnd: {} break;
      case DW_StdOpcode_SetEpilogueBegin: {} break;
      case DW_StdOpcode_SetIsa: {
        dw_serial_push_uleb128(arena, &srl, inst->set_isa);
      } break;
      case DW_StdOpcode_ExtendedOpcode: {
        // get ext size
        U64 ext_size = 0;
        switch (inst->ext) {
          case DW_ExtOpcode_EndSequence: { ext_size = 0; } break;
          case DW_ExtOpcode_SetAddress:  { ext_size = address_size; } break;
          case DW_ExtOpcode_DefineFile: { 
            NotImplemented;
          } break;
          case DW_ExtOpcode_SetDiscriminator: {
            ext_size = dw_size_from_uleb128(inst->set_discriminator);
          } break;
          default: { InvalidPath; } break;
        }
        ext_size += sizeof(DW_ExtOpcode);
        
        // write ext header
        dw_serial_push_uleb128(arena, &srl, ext_size);
        str8_serial_push_struct(arena, &srl, &inst->ext);
        
        // write ext operands
        switch (inst->ext) {
          case DW_ExtOpcode_EndSequence: {} break;
          case DW_ExtOpcode_SetAddress: {
            str8_serial_push_string(arena, &srl, str8_copy(arena, str8((U8 *)&inst->set_address, address_size)));
          } break;
          case DW_ExtOpcode_DefineFile: {
            NotImplemented;
          } break;
          case DW_ExtOpcode_SetDiscriminator: {
            dw_serial_push_uleb128(arena, &srl, inst->set_discriminator);
          } break;
          default: { InvalidPath; } break;
        }
      } break;
      // special opcode
      default: { } break;
    }
  }
  
  String8 data = str8_serial_end(arena, &srl);
  scratch_end(scratch);
  return data;
}

internal void
dw_writer_line_emit(DW_Writer *writer, DW_WriterFile *file, U64 ln, U64 col, U64 addr)
{
  Assert(file != 0);
  Assert(writer->line.min_inst_len != 0 && writer->line.max_ops_per_inst == 1);
  if (writer->line.file != file) {
    dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_file(file));
  }
  
  if (col != writer->line.col) { dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_column(col)); }
  
  // advance_pc and special opcodes count instructions, not bytes.
  if (addr < writer->line.addr || (addr - writer->line.addr) % writer->line.min_inst_len != 0) {
    dw_writer_line_set_address(writer, addr);
  }
  U64 op_delta = (addr - writer->line.addr) / writer->line.min_inst_len;
  Assert(ln <= max_S64 && writer->line.ln <= max_S64);
  S64 ln_delta   = (S64)ln   - (S64)writer->line.ln;
  
  S64 max_ln_delta   = writer->line.line_range + writer->line.line_base;
  B32 use_special = 0;
  if (writer->line.line_base <= ln_delta && ln_delta < max_ln_delta) {
    U64 max_op_delta = (max_U8 - writer->line.opcode_base - (ln_delta - writer->line.line_base)) / writer->line.line_range;
    use_special = op_delta <= max_op_delta;
  }
  if (use_special) {
    U8 opcode = (ln_delta - writer->line.line_base) + (op_delta * writer->line.line_range) + writer->line.opcode_base;
    dw_line_inst_list_push(writer->arena, &writer->line.line_insts, (DW_LineInst){ .opcode = opcode });
  } else {
    if (op_delta != 0) { dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_advance_pc(op_delta)); }
    if (ln_delta   != 0) { dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_advance_line(ln_delta)); }
    dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_copy());
  }
  
  // update registers
  writer->line.file = file;
  writer->line.ln   = ln;
  writer->line.col  = col;
  writer->line.addr = addr;
}

internal void
dw_writer_line_set_address(DW_Writer *writer, U64 address)
{
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNE_set_address(address));
  writer->line.addr = address;
}

internal void
dw_writer_line_set_discriminator(DW_Writer *writer, U64 d)
{
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNE_set_discriminator(d));
}

internal void
dw_writer_line_end_sequence(DW_Writer *writer)
{
  writer->line.file = 0;
  writer->line.ln   = 1;
  writer->line.addr = 0;
  writer->line.col  = 0;
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNE_end_sequence());
}

internal void
dw_writer_line_set_prologue_end(DW_Writer *writer)
{
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_prologue_end());
}

internal void
dw_writer_line_epilogue_begin(DW_Writer *writer)
{
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_epilogue_begin());
}

internal void
dw_writer_line_set_isa(DW_Writer *writer, U64 isa)
{
  dw_line_inst_list_push(writer->arena, &writer->line.line_insts, DW_LNS_set_isa(isa));
}

internal DW_WriterFile *
dw_writer_new_file(DW_Writer *writer, String8 path)
{
  DW_WriterFile *file = push_array(writer->arena, DW_WriterFile, 1);
  file->path = str8_copy(writer->arena, path);
  
  SLLQueuePush(writer->line.first_file, writer->line.last_file, file);
  writer->line.file_count += 1;
  return file;
}

internal void
dw_lower_attrib_forms(DW_Writer *writer, DW_WriterTag *tag)
{
  for EachNode(attrib, DW_WriterAttrib, tag->first_attrib) {
    switch (attrib->form.writer.kind) {
      case DW_WriterFormKind_Null: {} break;
      case DW_WriterFormKind_Flag: {
        attrib->form.reader.kind = DW_FormKind_Flag;
        attrib->form.reader.flag = attrib->form.writer.flag;
      } break;
      case DW_WriterFormKind_SInt: {
        // Negative values need an explicitly signed form. Keep compact fixed
        // forms for nonnegative values that fit the original signed widths.
        attrib->form.reader.kind  = DW_FormKind_SData;
        attrib->form.reader.sdata = attrib->form.writer.sint;
        if (attrib->form.writer.sint >= 0) {
          switch (dw_int_enc_from_sint(attrib->form.writer.sint)) {
            case DW_IntEnc_1Byte: {
              attrib->form.reader.kind = DW_FormKind_Data1;
              attrib->form.reader.data = str8((U8 *)&attrib->form.writer.sint, 1);
            } break;
            case DW_IntEnc_2Byte: {
              attrib->form.reader.kind = DW_FormKind_Data2;
              attrib->form.reader.data = str8((U8 *)&attrib->form.writer.sint, 2);
            } break;
            case DW_IntEnc_4Byte: {
              attrib->form.reader.kind = DW_FormKind_Data4;
              attrib->form.reader.data = str8((U8 *)&attrib->form.writer.sint, 4);
            } break;
            default: break;
          }
        }
      } break;
      case DW_WriterFormKind_UInt: {
        attrib->form.reader.kind  = DW_FormKind_UData;
        attrib->form.reader.udata = attrib->form.writer.uint;
      } break;
      case DW_WriterFormKind_Address: {
        Assert(writer->address_size <= sizeof(attrib->form.writer.address));
        attrib->form.reader.kind = DW_FormKind_Addr;
        attrib->form.reader.addr = str8((U8 *)&attrib->form.writer.address, writer->address_size);
      } break;
      case DW_WriterFormKind_Ref: {
        if (attrib->form.writer.ref->info_off == 0) {
          attrib->form.reader.kind = DW_FormKind_Ref8;
        } else {
          switch (dw_int_enc_from_uint(attrib->form.writer.ref->info_off)) {
            case DW_IntEnc_Null: break;
            case DW_IntEnc_1Byte: {
              attrib->form.reader.kind = DW_FormKind_Ref1;
              attrib->form.reader.ref  = (U8)attrib->form.writer.ref->info_off;
            } break;
            case DW_IntEnc_2Byte: {
              attrib->form.reader.kind = DW_FormKind_Ref2;
              attrib->form.reader.ref  = (U16)attrib->form.writer.ref->info_off;
            } break;
            case DW_IntEnc_4Byte: {
              attrib->form.reader.kind = DW_FormKind_Ref4;
              attrib->form.reader.ref  = (U32)attrib->form.writer.ref->info_off;
            } break;
            case DW_IntEnc_LEB128: {
              attrib->form.reader.kind = DW_FormKind_RefUData;
              attrib->form.reader.ref  = attrib->form.writer.ref->info_off;
            } break;
            default: InvalidPath; break;
          }
        }
      } break;
      case DW_WriterFormKind_LinePtr: {
        attrib->form.reader.kind       = writer->version >= DW_Version_4 ? DW_FormKind_SecOffset :
                                        writer->format == DW_Format_64Bit ? DW_FormKind_Data8 : DW_FormKind_Data4;
        attrib->form.reader.sec_offset = attrib->form.writer.line_ptr;
        if (writer->version < DW_Version_4) {
          attrib->form.reader.data = str8((U8 *)&attrib->form.writer.line_ptr, dw_addr_size_from_format(writer->format));
        }
      } break;
      case DW_WriterFormKind_MacPtr: {
        NotImplemented;
      } break;
      case DW_WriterFormKind_RngListPtr: {
        NotImplemented;
      } break;
      case DW_WriterFormKind_Data: {
        attrib->form.reader.data = attrib->form.writer.data;
        switch (attrib->form.writer.data.size) {
          case 1:  attrib->form.reader.kind = DW_FormKind_Data1;  break;
          case 2:  attrib->form.reader.kind = DW_FormKind_Data2;  break;
          case 4:  attrib->form.reader.kind = DW_FormKind_Data4;  break;
          case 8:  attrib->form.reader.kind = DW_FormKind_Data8;  break;
          case 16: {
            Assert(writer->version >= DW_Version_5);
            attrib->form.reader.kind = DW_FormKind_Data16;
          } break;
          default: InvalidPath; break;
        }
      } break;
      case DW_WriterFormKind_Block: {
        attrib->form.reader.block = attrib->form.writer.block;
        attrib->form.reader.kind = DW_FormKind_Block;
      } break;
      case DW_WriterFormKind_String: {
        attrib->form.reader.kind   = DW_FormKind_String;
        attrib->form.reader.string = attrib->form.writer.string;
      } break;
      case DW_WriterFormKind_ExprLoc: {
        attrib->form.reader.kind    = writer->version >= DW_Version_4 ? DW_FormKind_ExprLoc : DW_FormKind_Block;
        attrib->form.reader.exprloc = attrib->form.writer.exprloc;
      } break;
      case DW_WriterFormKind_Implicit: {
        Assert(writer->version >= DW_Version_5);
        attrib->form.reader.kind           = DW_FormKind_ImplicitConst;
        attrib->form.reader.implicit_const = attrib->form.writer.implicit;
      } break;
      default: { InvalidPath; } break;
    }
  }
}

internal void
dw_writer_emit_tag(DW_Writer *writer, DW_WriterTag *tag)
{
  Assert(tag->info_off == 0);
  
  dw_lower_attrib_forms(writer, tag);
  
  // make abbrev id for the tag
  U64 abbrev_id;
  {
    Temp temp = temp_begin(writer->arena);
    String8 abbrev_entry = dw_make_abbrev_entry(temp.arena, tag);
    
    if ( ! hash_table_search_string_u64(writer->abbrev_id_map, abbrev_entry, &abbrev_id)) {
      abbrev_id = writer->abbrev_id_map->count + 1;
      
      hash_table_push_string_u64(writer->arena, writer->abbrev_id_map, abbrev_entry, abbrev_id);
      
      // push abbrev entry
      dw_serial_push_uleb128(writer->arena, &writer->sections[DW_SectionKind_Abbrev].srl, abbrev_id);
      str8_serial_push_string(writer->arena, &writer->sections[DW_SectionKind_Abbrev].srl, abbrev_entry);
    } else {
      temp_end(temp);
    }
  }
  
  tag->abbrev_id = abbrev_id;
  tag->info_off  = writer->sections[DW_SectionKind_Info].srl.total_size;
  
  // emit tag abbrev id
  dw_serial_push_uleb128(writer->arena, &writer->sections[DW_SectionKind_Info].srl, tag->abbrev_id);
  
  // emit attribs
  {
    String8List *debug_info = &writer->sections[DW_SectionKind_Info].srl;
    for EachNode(attrib, DW_WriterAttrib, tag->first_attrib) {
      dw_serial_push_form(writer->arena, debug_info, writer->version, writer->format, writer->address_size, &writer->fixups, attrib->form);
    }
  }
  
  // write children
  for EachNode(child, DW_WriterTag, tag->first_child) {
    dw_writer_emit_tag(writer, child);
  }
  
  // close tag
  if (tag->first_child) {
    dw_serial_push_uleb128(writer->arena, &writer->sections[DW_SectionKind_Info].srl, 0);
  }
}

internal void
dw_writer_emit(DW_Writer *writer)
{
  // line
  {
    // push line table terminator
    DW_LineInstNode *last_inst = writer->line.line_insts.last;
    if (last_inst == 0 || last_inst->v.opcode != DW_StdOpcode_ExtendedOpcode || last_inst->v.ext != DW_ExtOpcode_EndSequence) {
      dw_writer_line_end_sequence(writer);
    }
    
    // find comp dir in the compile unit tag
    String8 comp_dir  = {0};
    String8 comp_name = {0};
    Assert(writer->root->kind == DW_TagKind_CompileUnit || writer->root->kind == DW_TagKind_PartialUnit);
    for EachNode(attrib, DW_WriterAttrib, writer->root->first_attrib) {
      if (attrib->kind == DW_AttribKind_CompDir) {
        AssertAlways(attrib->form.writer.kind == DW_WriterFormKind_String);
        comp_dir = attrib->form.writer.string;
      } else if (attrib->kind == DW_AttribKind_Name) {
        AssertAlways(attrib->form.writer.kind == DW_WriterFormKind_String);
        comp_name = attrib->form.writer.string;
      }
    }
    
    String8List dir_table_srl = {0};
    str8_serial_begin(writer->arena, &dir_table_srl);
    {
      Temp scratch = scratch_begin(0, 0);
      String8List *srl = &dir_table_srl;
      
      if (writer->version >= DW_Version_5) {
        // (13) directory_entry_format_count
        str8_serial_push_u8(writer->arena, srl, 1);
        // (14) directory_entry_format
        dw_serial_push_uleb128(writer->arena, srl, DW_LNCT_Path);
        dw_serial_push_uleb128(writer->arena, srl, DW_FormKind_String);
      }
      
      // dedup directories
      String8List  dirs   = {0};
      HashTable   *dir_ht = hash_table_init(scratch.arena, writer->line.file_count + 1);
      // first entry must be compile unit directory
      hash_table_push_string_u64(scratch.arena, dir_ht, comp_dir, dir_ht->count);
      if (comp_dir.size == 2 && char_is_alpha(comp_dir.str[0]) && comp_dir.str[1] == ':') {
        str8_list_pushf(scratch.arena, &dirs, "%S/", comp_dir);
      } else {
        str8_list_push(scratch.arena, &dirs, comp_dir);
      }
      for EachNode(file, DW_WriterFile, writer->line.first_file) {
        String8 path = str8_chop_last_slash(file->path);
        if (path.size == 0) {
          continue;
        }
        if ( ! hash_table_search_string_u64(dir_ht, path, &file->dir_idx)) {
          // @dir_idx
          file->dir_idx = hash_table_push_string_u64(scratch.arena, dir_ht, path, dir_ht->count)->v.value_u64;
          if (path.size == 2 && char_is_alpha(path.str[0]) && path.str[1] == ':') {
            str8_list_pushf(scratch.arena, &dirs, "%S/", path);
          } else {
            str8_list_push(scratch.arena, &dirs, path);
          }
        }
      }
      
      // (15) directories_count
      if (writer->version >= DW_Version_5) {
        dw_serial_push_uleb128(writer->arena, srl, dirs.node_count);
      }
      
      // (16) directories
      for EachNode(dir_n, String8Node, dirs.first) {
        // Before v5 the compilation directory is implicit at index zero.
        if (writer->version < DW_Version_5 && dir_n == dirs.first) { continue; }
        dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, (DW_WriterXForm) { .reader = { .kind = DW_FormKind_String, .string = dir_n->string } });
      }
      if (writer->version < DW_Version_5) { str8_serial_push_u8(writer->arena, srl, 0); }
      
      scratch_end(scratch);
    }
    
    String8List file_table_srl = {0};
    str8_serial_begin(writer->arena, &file_table_srl);
    {
      Temp scratch = scratch_begin(0, 0);
      String8List *srl = &file_table_srl;
      
      struct { DW_LNCT lnct; DW_FormKind form_kind; } encs[] = {
        { DW_LNCT_Path,           DW_FormKind_String },
        { DW_LNCT_DirectoryIndex, DW_FormKind_UData  },
        { DW_LNCT_TimeStamp,      DW_FormKind_Null   }, // DW_FormKind_UData
        { DW_LNCT_Size,           DW_FormKind_Null   }, // DW_FormKind_UData
        { DW_LNCT_MD5,            DW_FormKind_Null   }, // DW_FormKind_Data16
        { DW_LNCT_LLVM_Source,    DW_FormKind_Null   }  // DW_FormKind_String
      };
      
      HashTable *encs_ht = hash_table_init(scratch.arena, ArrayCount(encs) * 2);
      for EachElement(i, encs) { hash_table_push_u64_raw(scratch.arena, encs_ht, encs[i].lnct, &encs[i].form_kind); }
      
      // enable encodings in use
      for EachNode(file, DW_WriterFile, writer->line.first_file) {
        if (file->time_stamp  != 0)                { *(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_TimeStamp  ) = DW_FormKind_UData;  }
        if (file->size        != 0)                { *(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_Size       ) = DW_FormKind_UData;  }
        if ( ! u128_match(file->md5, u128_zero())) { *(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_MD5        ) = DW_FormKind_Data16; }
        if (file->source.size != 0)                { *(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_LLVM_Source) = DW_FormKind_String; }
      }
      
      // count needed encodings
      U8 enc_count = 0;
      for EachElement(i, encs) { if (encs[i].form_kind != DW_FormKind_Null) { enc_count += 1; } }
      
      if (writer->version >= DW_Version_5) {
        // (17) file_name_entry_format_count
        str8_serial_push_u8(writer->arena, srl, enc_count);
        // (18) file_name_entry_format
        for EachElement(i, encs) {
          if (encs[i].form_kind == DW_FormKind_Null) { continue; }
          dw_serial_push_uleb128(writer->arena, srl, encs[i].lnct     );
          dw_serial_push_uleb128(writer->arena, srl, encs[i].form_kind);
        }
        // (19) file_names_count
        dw_serial_push_uleb128(writer->arena, srl, writer->line.file_count);
      }
      
      // (20) file_names
      if (comp_name.size) {
        for (DW_WriterFile *n = writer->line.first_file, *p = 0; n != 0; p = n, n = n->next) {
          String8 file_name = str8_skip_last_slash(n->path);
          if (str8_match(file_name, comp_name, 0)) {
            if (p == 0) {
              // file is already first
            } else {
              // move compile unit file to be first entry in the table
              p->next = n->next;
              n->next = writer->line.first_file;
              writer->line.first_file = n;
              if (writer->line.last_file == n) {
                writer->line.last_file = p;
              }
            }
            break;
          }
        }
      }
      U64 file_idx = writer->version >= DW_Version_5 ? 0 : 1;
      for EachNode(file, DW_WriterFile, writer->line.first_file) {
        static DW_WriterXForm null_form_udata  = { .reader = { .kind = DW_FormKind_UData  } };
        static DW_WriterXForm null_form_string = { .reader = { .kind = DW_FormKind_String } };
        
        // @file_idx
        file->file_idx = file_idx++;
        if (writer->version < DW_Version_5) {
          str8_serial_push_cstr(writer->arena, srl, str8_skip_last_slash(file->path));
          dw_serial_push_uleb128(writer->arena, srl, file->dir_idx);
          dw_serial_push_uleb128(writer->arena, srl, file->time_stamp);
          dw_serial_push_uleb128(writer->arena, srl, file->size);
          continue;
        }
        
        // path
        Assert(*(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_Path) == DW_FormKind_String);
        String8 file_name = str8_skip_last_slash(file->path);
        DW_WriterXForm path_form = { .reader = { .kind = DW_FormKind_String, .string = file_name } };
        dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, path_form);
        
        // directory index
        Assert(*(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_DirectoryIndex) == DW_FormKind_UData);
        DW_WriterXForm dir_idx_form = { .reader = { .kind = DW_FormKind_UData, .udata = file->dir_idx } };
        dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, dir_idx_form);
        
        // time stamp
        if (*(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_TimeStamp) == DW_FormKind_UData) {
          if (file->time_stamp) {
            DW_WriterXForm time_stamp_form = { .reader = { .kind = DW_FormKind_UData, .udata = file->time_stamp } };
            dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, time_stamp_form);
          } else {
            dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, null_form_udata);
          }
        }
        
        // size
        if (*(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_Size) == DW_FormKind_UData) {
          if (file->size) {
            DW_WriterXForm size_form = { .reader = { .kind = DW_FormKind_UData, .udata = file->size } };
            dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, size_form);
          } else {
            dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, null_form_udata);
          }
        }
        
        // MD5
        if (*(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_MD5) == DW_FormKind_Data16) {
          DW_WriterXForm md5_form = { .reader = { .kind = DW_FormKind_Data16, .data = str8_struct(&file->md5) } };
          dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, md5_form);
        }
        
        // LLVM Source
        if (*(DW_FormKind *)hash_table_search_u64_raw(encs_ht, DW_LNCT_LLVM_Source) == DW_FormKind_String) {
          if (file->source.size) {
            DW_WriterXForm source_form = { .reader = { .kind = DW_FormKind_String, .string = file->source } };
            dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, source_form);
          } else {
            dw_serial_push_form(writer->arena, srl, writer->version, writer->format, writer->address_size, 0, null_form_string);
          }
        }
      }
      
      if (writer->version < DW_Version_5) { str8_serial_push_u8(writer->arena, srl, 0); }
      scratch_end(scratch);
    }
    
    {
      String8List *srl = &writer->sections[DW_SectionKind_Line].srl;
      // (1) unit_length
      writer->sections[DW_SectionKind_Line].length = dw_serial_push_length(writer->arena, srl, writer->format, 0);
      // (2) version
      str8_serial_push_struct(writer->arena, srl, &writer->version);
      if (writer->version >= DW_Version_5) {
        // (3) address_size
        str8_serial_push_struct(writer->arena, srl, &writer->address_size);
        // (4) segment_selector_size
        str8_serial_push_struct(writer->arena, srl, &writer->segsel_size);
      }
      // (5) header_length
      void *header_length_ptr    = dw_serial_push_uint(writer->arena, srl, writer->format, 0);
      U64   header_length_offset = srl->total_size;
      // (6) minimum_instruction_length
      str8_serial_push_struct(writer->arena, srl, &writer->line.min_inst_len);
      // (7) maximum_operations_per_instruction
      if (writer->version >= DW_Version_4) {
        str8_serial_push_struct(writer->arena, srl, &writer->line.max_ops_per_inst);
      }
      // (8) default_is_stmt
      str8_serial_push_struct(writer->arena, srl, &writer->line.default_is_stmt);
      // (9) line_base
      str8_serial_push_struct(writer->arena, srl, &writer->line.line_base);
      // (10) line_range
      str8_serial_push_struct(writer->arena, srl, &writer->line.line_range);
      // (11) opcode_base
      str8_serial_push_struct(writer->arena, srl, &writer->line.opcode_base);
      // (12) standard_opcode_lengths
      U8 *std_op_lens = push_array(writer->arena, U8, writer->line.opcode_base - 1);
      for (U64 i = 1; i < writer->line.opcode_base; i += 1)
      {
        U64 len = 0;
        switch(i)
        {
          default:{}break;
#define X(name, code, len_) case DW_StdOpcode_##name:{len = (len_);}break;
          DW_StdOpcode_XList
#undef X
        }
        std_op_lens[i-1] = len;
      }
      str8_serial_push_string(writer->arena, srl, str8(std_op_lens, writer->line.opcode_base - 1));
      // directory table
      str8_list_concat_in_place(srl, &dir_table_srl);
      // file table
      str8_list_concat_in_place(srl, &file_table_srl);
      
      // fixup header length
      U64 bytes_to_line_program = srl->total_size - header_length_offset;
      if (writer->format == DW_Format_32Bit) { memory_write32(header_length_ptr, safe_cast_u32(bytes_to_line_program)); }
      else                                   { memory_write64(header_length_ptr, bytes_to_line_program);                }
      
      // line program
      String8 line_program_data = dw_data_from_line_insts(writer->arena, writer->address_size, writer->line.line_insts);
      str8_serial_push_string(writer->arena, srl, line_program_data);
    }
  }
  
  // info
  {
    dw_writer_emit_tag(writer, writer->root);
    
    // null terminate .debug_abbrev
    dw_serial_push_uleb128(writer->arena, &writer->sections[DW_SectionKind_Abbrev].srl, 0);
    
    // fixup units lengths
    for EachElement(i, writer->sections) {
      if (writer->sections[i].length) {
        if (writer->format == DW_Format_64Bit) {
          U64 *length      = (U64 *)((U8 *)writer->sections[i].length + sizeof(U32));
          U64  length_size = sizeof(U64) + sizeof(U32);
          Assert(writer->sections[i].srl.total_size >= length_size);
          memory_write64(length, writer->sections[i].srl.total_size - length_size);
        } else {
          U32 *length      = writer->sections[i].length;
          U32  length_size = sizeof(U32);
          Assert(writer->sections[i].srl.total_size >= length_size);
          Assert(writer->sections[i].srl.total_size - length_size < 0xfffffff0);
          memory_write32(length, safe_cast_u32(writer->sections[i].srl.total_size - length_size));
        }
      }
    }
    
    // fixup forward references in .debug_info
    for EachNode(fixup_n, DW_WriterFixupNode, writer->fixups.first) {
      DW_WriterFixup *fixup = &fixup_n->v;
      Assert(fixup->tag->info_off != 0);
      MemoryCopy(fixup->ptr, &fixup->tag->info_off, sizeof(U64));
    }

    // fixup reader references
    for EachNode(chunk, DW_WriterAttribChunk, writer->attrib_chunk_list.first) {
      for EachIndex(i, chunk->count) {
        DW_WriterAttrib *attrib = &chunk->v[i];
        if (attrib->form.writer.kind == DW_WriterFormKind_Ref) {
          attrib->form.reader.ref = attrib->form.writer.ref->info_off;
        }
      }
    }
  }
}

#ifdef OBJ_H
internal void
dw_writer_emit_to_obj(DW_Writer *writer, OBJ *obj)
{
  dw_writer_emit(writer);
  
  // push obj sections
  OBJ_Section *sections[DW_SectionKind_COUNT] = {0};
  for EachElement(i, writer->sections) {
    if (writer->sections[i].srl.total_size) {
      sections[i] = obj_push_section(obj, dw_regular_name_from_section_kind(i), OBJ_SectionFlag_Read|OBJ_SectionFlag_Write);
    }
  }
  
  // push OBJ relocation for 'abbrev base'
  OBJ_Section *debug_info = sections[DW_SectionKind_Info];
  obj_push_reloc(obj, debug_info, OBJ_RelocKind_SecRel, writer->abbrev_base_info_off, sections[DW_SectionKind_Abbrev]->symbol);
  
  // copy section data from writer to obj
  for EachElement(i, writer->sections) {
    if (writer->sections[i].srl.total_size > 0) {
      String8 data = str8_serial_end(obj->arena, &writer->sections[i].srl);
      str8_list_push(obj->arena, &sections[i]->data, data);
    }
  }
  
  // zero out writer sections
  MemoryZeroArray(writer->sections);
}
#endif

internal String8
dw_encode_expr(Arena *arena, Arch arch, DW_Format format, DW_ExprEnc *encs, U64 encs_count)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  String8List *srl = push_array(scratch.arena, String8List, 1);
  str8_serial_begin(scratch.arena, srl);
  
  HashTable *label_map = hash_table_init(scratch.arena, 128);
  
  struct Fixup { U64 offset; String8 label; struct Fixup *next; };
  struct Fixup *first_fixup = 0, *last_fixup = 0;
  
  for EachIndex(i, encs_count) {
    DW_ExprEnc *e = &encs[i];
    
    switch (e->type) {
      case DW_ExprEncType_Null: {} break;
      case DW_ExprEncType_Op: {
        str8_serial_push_struct(scratch.arena, srl, &e->op);
      } break;
      case DW_ExprEncType_U8: {
        str8_serial_push_struct(scratch.arena, srl, &e->u8);
      } break;
      case DW_ExprEncType_U16: {
        str8_serial_push_struct(scratch.arena, srl, &e->u16);
      } break;
      case DW_ExprEncType_U32: {
        str8_serial_push_struct(scratch.arena, srl, &e->u32);
      } break;
      case DW_ExprEncType_U64: {
        str8_serial_push_struct(scratch.arena, srl, &e->u64);
      } break;
      case DW_ExprEncType_S8: {
        str8_serial_push_struct(scratch.arena, srl, &e->s8);
      } break;
      case DW_ExprEncType_S16: {
        str8_serial_push_struct(scratch.arena, srl, &e->s16);
      } break;
      case DW_ExprEncType_S32: {
        str8_serial_push_struct(scratch.arena, srl, &e->s32);
      } break;
      case DW_ExprEncType_S64: {
        str8_serial_push_struct(scratch.arena, srl, &e->s64);
      } break;
      case DW_ExprEncType_ULEB128: {
        str8_serial_push_string(scratch.arena, srl, dw_write_uleb128(scratch.arena, e->u64));
      } break;
      case DW_ExprEncType_SLEB128: {
        str8_serial_push_string(scratch.arena, srl, dw_write_sleb128(scratch.arena, e->s64));
      } break;
      case DW_ExprEncType_Addr: {
        U64 addr_size = byte_size_from_arch(arch);
        Assert(addr_size <= sizeof(e->addr));
        str8_serial_push_string(scratch.arena, srl, str8((U8 *)&e->addr, addr_size));
      } break;
      case DW_ExprEncType_Block: {
        Assert(e->block.size <= max_U8);
        str8_serial_push_u8(scratch.arena, srl, (U8)e->block.size);
        str8_serial_push_string(scratch.arena, srl, e->block);
      } break;
      case DW_ExprEncType_DwarfUInt: {
        dw_serial_push_uint(scratch.arena, srl, format, e->u64);
      } break;
      case DW_ExprEncType_Label: {
        struct Fixup *fixup = push_array(scratch.arena, struct Fixup, 1);
        fixup->offset = srl->total_size;
        fixup->label  = e->label;
        SLLQueuePush(first_fixup, last_fixup, fixup);
        
        S16 delta_placeholder = 0;
        str8_serial_push_struct(scratch.arena, srl, &delta_placeholder);
      } break;
      case DW_ExprEncType_DeclLabel: {
        hash_table_push_string_u64(scratch.arena, label_map, e->label, srl->total_size);
      } break;
      default: { InvalidPath; } break;
    }
  }
  
  // finalize expression
  String8 expr = str8_serial_end(arena, srl);
  
  // apply fixups
  for EachNode(fixup, struct Fixup, first_fixup) {
    U64 label_offset;
    if (!hash_table_search_string_u64(label_map, fixup->label, &label_offset)) {
      Assert(0 && "undefined label");
      continue;
    }
    
    // Branch displacements are relative to the end of the two-byte operand.
    S64 delta = (S64)label_offset - (S64)(fixup->offset + sizeof(S16));
    AssertAlways(min_S16 <= delta && delta <= max_S16);
    
    memory_write16(expr.str + fixup->offset, (U16)(S16)delta);
  }
  
  scratch_end(scratch);
  return expr;
}
