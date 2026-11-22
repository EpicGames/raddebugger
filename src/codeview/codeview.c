// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ Generated Code

#include "generated/codeview.meta.c"

////////////////////////////////

internal CV_Arch
cv_arch_from_coff_machine(COFF_MachineType machine)
{
  CV_Arch arch = 0;
  switch(machine)
  {
    case COFF_MachineType_X64:       arch = CV_Arch_X64;    break;
    case COFF_MachineType_X86:       arch = CV_Arch_8086;   break;
    case COFF_MachineType_Am33:      arch = CV_Arch_AM33;   break;
    case COFF_MachineType_Arm:       NotImplemented;        break;
    case COFF_MachineType_Arm64:     arch = CV_Arch_ARM64;  break;
    case COFF_MachineType_ArmNt:     arch = CV_Arch_ARMNT;  break;
    case COFF_MachineType_Ebc:       arch = CV_Arch_EBC;    break;
    case COFF_MachineType_Ia64:      arch = CV_Arch_IA64;   break;
    case COFF_MachineType_M32R:      arch = CV_Arch_M32R;   break;
    case COFF_MachineType_Mips16:    arch = CV_Arch_MIPS16; break;
    case COFF_MachineType_MipsFpu:   NotImplemented;        break;
    case COFF_MachineType_MipsFpu16: NotImplemented;        break;
    case COFF_MachineType_PowerPc:   NotImplemented;        break;
    case COFF_MachineType_PowerPcFp: arch = CV_Arch_PPCFP;  break;
    case COFF_MachineType_R4000:     NotImplemented;        break;
    case COFF_MachineType_RiscV32:   NotImplemented;        break;
    case COFF_MachineType_RiscV64:   NotImplemented;        break;
    case COFF_MachineType_RiscV128:  NotImplemented;        break;
    case COFF_MachineType_Sh3:       arch = CV_Arch_SH3;    break;
    case COFF_MachineType_Sh3Dsp:    arch = CV_Arch_SH3DSP; break;
    case COFF_MachineType_Sh4:       arch = CV_Arch_SH4;    break;
    case COFF_MachineType_Sh5:       NotImplemented;        break;
    case COFF_MachineType_Thumb:     arch = CV_Arch_THUMB;  break;
    case COFF_MachineType_WceMipsV2: NotImplemented;        break;
  }
  return arch;
}

internal U64
cv_size_from_reg(CV_Arch arch, CV_Reg reg)
{
  switch(arch)
  {
    case CV_Arch_X64 : return cv_size_from_reg_x64(reg);
    default: NotImplemented;
  }
  return 0;
}

internal B32
cv_is_reg_sp(CV_Arch arch, CV_Reg reg)
{
  switch(arch)
  {
    case CV_Arch_X64:  return reg == CV_Regx64_RSP;
    default: NotImplemented;
  }
  return 0;
}

internal U64
cv_size_from_reg_x64(CV_Reg reg)
{
  switch(reg)
  {
#define X(NAME, CODE, RDI_NAME, BYTE_POS, BYTE_SIZE) case CV_Regx64_##NAME: return BYTE_SIZE;
    CV_Reg_X64_XList(X)
#undef X 
  }
  return 0;
}

internal CV_EncodedFramePtrReg
cv_pick_fp_encoding(CV_SymFrameproc *frameproc, B32 is_local_param)
{
  CV_EncodedFramePtrReg fp_reg = 0;
  if(is_local_param)
  {
    fp_reg = CV_FrameprocFlags_Extract_ParamBasePointer(frameproc->flags);
  }
  else
  {
    fp_reg = CV_FrameprocFlags_Extract_LocalBasePointer(frameproc->flags);
  }
  return fp_reg;
}

internal CV_Reg
cv_decode_fp_reg(CV_Arch arch, CV_EncodedFramePtrReg encoded_reg)
{
  CV_Reg fp_reg = 0;
  switch (arch)
  {
    case CV_Arch_X64:
    {
      switch (encoded_reg)
      {
        case CV_EncodedFramePtrReg_None    : break;
        case CV_EncodedFramePtrReg_StackPtr: fp_reg = CV_Regx64_RSP; break;
        case CV_EncodedFramePtrReg_FramePtr: fp_reg = CV_Regx64_RBP; break;
        case CV_EncodedFramePtrReg_BasePtr : fp_reg = CV_Regx64_R13; break;
        default: InvalidPath;
      }
    } break;
    default: NotImplemented;
  }
  return fp_reg;
}

internal U32
cv_map_encoded_base_pointer(CV_Arch arch, U32 encoded_frame_reg)
{
  U32 r = 0;
  switch (arch) {
    case CV_Arch_X64: {
      switch (encoded_frame_reg) {
        case 0: r = 0; break;
        case 1: r = CV_Regx64_RSP; break;
        case 2: r = CV_Regx64_RBP; break;
        case 3: r = CV_Regx64_R13; break;
      }
    } break;
    default: NotImplemented;
  }
  return r;
}

internal Rng1U64List
cv_make_defined_range_list_from_gaps(Arena *arena, Rng1U64 defrange, CV_LvarAddrGap *gaps, U64 gap_count)
{
  Rng1U64List result = {0};

  if (gap_count == 0) {
    // no gaps, push whole range
    rng1u64_list_push(arena, &result, defrange);
  } else {
    U64 cursor = defrange.min;
    for (U64 gap_idx = 0; gap_idx < gap_count; ++gap_idx) {
      // make range
      Rng1U64 range = rng_1u64(cursor, cursor + gaps[gap_idx].off);
      rng1u64_list_push(arena, &result, range);

      // advance
      cursor = defrange.min + gaps[gap_idx].off + gaps[gap_idx].len;
    }


    // emit range past last gap
    if (gap_count > 0) {
      CV_LvarAddrGap  last_gap             = gaps[gap_count - 1];
      U64             last_range_byte_size = dim_1u64(defrange) - (last_gap.off + last_gap.len);
      if (last_range_byte_size) {
        Rng1U64 last_range = rng_1u64(defrange.min + last_gap.off + last_gap.len, defrange.max);
        rng1u64_list_push(arena, &result, last_range);
      }
    }
  }

  return result;
}

internal B32
cv_is_udt_name_anon(String8 name)
{
  // corresponds to fUDTAnon from dbi/tm.cpp:817
  B32 is_anon = str8_match_lit("<unnamed-tag>",   name, 0) ||
                str8_match_lit("__unnamed",       name, 0) ||
                str8_match_lit("::<unnamed-tag>", name, StringMatchFlag_RightSideSloppy) ||
                str8_match_lit("::__unnamed",     name, StringMatchFlag_RightSideSloppy);
  return is_anon;
}

internal B32
cv_is_udt(CV_LeafKind kind)
{
  B32 is_udt = kind == CV_LeafKind_CLASS            ||
               kind == CV_LeafKind_STRUCTURE        ||
               kind == CV_LeafKind_CLASS2           ||
               kind == CV_LeafKind_STRUCT2          ||
               kind == CV_LeafKind_INTERFACE        ||
               kind == CV_LeafKind_UNION            ||
               kind == CV_LeafKind_ENUM             ||
               kind == CV_LeafKind_UDT_MOD_SRC_LINE ||
               kind == CV_LeafKind_UDT_SRC_LINE     ||
               kind == CV_LeafKind_ALIAS;
  return is_udt;
}

internal B32
cv_is_global_symbol(CV_SymKind kind)
{
  B32 is_global_symbol = kind == CV_SymKind_CONSTANT      ||
                         kind == CV_SymKind_GDATA16       ||
                         kind == CV_SymKind_GDATA32_16t   ||
                         kind == CV_SymKind_GDATA32_ST    ||
                         kind == CV_SymKind_GDATA32       ||
                         kind == CV_SymKind_GTHREAD32_16t ||
                         kind == CV_SymKind_GTHREAD32_ST  ||
                         kind == CV_SymKind_GTHREAD32;
  return is_global_symbol;
}

internal B32
cv_is_typedef(CV_SymKind kind)
{
  B32 is_typedef = kind == CV_SymKind_UDT_16t ||
                   kind == CV_SymKind_UDT_ST  ||
                   kind == CV_SymKind_UDT;
  return is_typedef;
}

internal B32
cv_is_scope_symbol(CV_SymKind kind)
{
  B32 is_scope = kind == CV_SymKind_GPROC32     ||
                 kind == CV_SymKind_LPROC32     ||
                 kind == CV_SymKind_BLOCK32     ||
                 kind == CV_SymKind_THUNK32     ||
                 kind == CV_SymKind_INLINESITE  ||
                 kind == CV_SymKind_INLINESITE2 ||
                 kind == CV_SymKind_WITH32      ||
                 kind == CV_SymKind_SEPCODE     ||
                 kind == CV_SymKind_GPROC32_ID  ||
                 kind == CV_SymKind_LPROC32_ID;
  return is_scope;
}

internal B32
cv_is_end_symbol(CV_SymKind kind)
{
  B32 is_end = kind == CV_SymKind_END         ||
               kind == CV_SymKind_PROC_ID_END ||
               kind == CV_SymKind_INLINESITE_END;
  return is_end;
}

internal B32
cv_is_leaf_type_server(CV_LeafKind kind)
{
  B32 is_type_server = kind == CV_LeafKind_TYPESERVER  ||
                       kind == CV_LeafKind_TYPESERVER2 ||
                       kind == CV_LeafKind_TYPESERVER_ST;
  return is_type_server;
}

internal B32
cv_is_leaf_pch(CV_LeafKind kind)
{
  B32 is_pch = kind == CV_LeafKind_PRECOMP    ||
               kind == CV_LeafKind_PRECOMP_ST ||
               kind == CV_LeafKind_PRECOMP_16t;
  return is_pch;
}

internal B32
cv_is_gproc(CV_Symbol symbol)
{
  return symbol.kind == CV_SymKind_GPROC32        ||
         symbol.kind == CV_SymKind_GPROCMIPS      ||
         symbol.kind == CV_SymKind_GPROCIA64      ||
         symbol.kind == CV_SymKind_GPROC32_ID     ||
         symbol.kind == CV_SymKind_GPROCMIPS_ID   ||
         symbol.kind == CV_SymKind_GPROCIA64_ID   ||
         symbol.kind == CV_SymKind_GPROC16        ||
         symbol.kind == CV_SymKind_GPROC32_16t    ||
         symbol.kind == CV_SymKind_GPROCMIPS_16t  ||
         symbol.kind == CV_SymKind_GPROC32_ST     ||
         symbol.kind == CV_SymKind_GPROCMIPS_ST   ||
         symbol.kind == CV_SymKind_GPROCIA64_ST;
}

internal B32
cv_is_lproc(CV_Symbol symbol)
{
  return symbol.kind == CV_SymKind_LPROC32        ||
         symbol.kind == CV_SymKind_LPROCMIPS      ||
         symbol.kind == CV_SymKind_LPROCIA64      ||
         symbol.kind == CV_SymKind_LPROC32_ID     ||
         symbol.kind == CV_SymKind_LPROCMIPS_ID   ||
         symbol.kind == CV_SymKind_LPROCIA64_ID   ||
         symbol.kind == CV_SymKind_LPROC32_DPC    ||
         symbol.kind == CV_SymKind_LPROC32_DPC_ID ||
         symbol.kind == CV_SymKind_LPROC16        ||
         symbol.kind == CV_SymKind_LPROC32_16t    ||
         symbol.kind == CV_SymKind_LPROCMIPS_16t  ||
         symbol.kind == CV_SymKind_LPROC32_ST     ||
         symbol.kind == CV_SymKind_LPROCMIPS_ST   ||
         symbol.kind == CV_SymKind_LPROCIA64_ST;
}

internal B32
cv_is_obj_info(CV_Symbol symbol)
{
  return symbol.kind == CV_SymKind_OBJNAME;
}


internal CV_TypeIndexSource
cv_type_index_source_from_leaf_kind(CV_LeafKind leaf_kind)
{
  CV_TypeIndexSource source;
  if (leaf_kind == CV_LeafKind_FUNC_ID      ||
      leaf_kind == CV_LeafKind_MFUNC_ID     ||
      leaf_kind == CV_LeafKind_BUILDINFO    ||
      leaf_kind == CV_LeafKind_SUBSTR_LIST  ||
      leaf_kind == CV_LeafKind_STRING_ID    ||
      leaf_kind == CV_LeafKind_UDT_SRC_LINE ||
      leaf_kind == CV_LeafKind_UDT_MOD_SRC_LINE) {
    source = CV_TypeIndexSource_IPI;
  } else if (leaf_kind == CV_LeafKind_NOTYPE) {
    source = CV_TypeIndexSource_NULL;
  } else {
    source = CV_TypeIndexSource_TPI;
  }
  return source;
}

internal B32
cv_type_server_info_from_leaf(CV_Leaf leaf, CV_TypeServerInfo *out)
{
  MemoryZeroStruct(out);
  CV_TypeServerInfo result = {0};
  U64 name_offset = 0;
  switch (leaf.kind) {
  case CV_LeafKind_TYPESERVER: {
    CV_LeafTypeServer ts;
    if (leaf.data.size < sizeof(ts)) { return 0; }
    MemoryCopy(&ts, leaf.data.str, sizeof(ts));
    result.sig.data1 = ts.sig;
    result.age = ts.age;
    name_offset = sizeof(ts);
  } break;
  case CV_LeafKind_TYPESERVER2: {
    CV_LeafTypeServer2 ts;
    if (leaf.data.size < sizeof(ts)) { return 0; }
    MemoryCopy(&ts, leaf.data.str, sizeof(ts));
    MemoryCopy(&result.sig, &ts.sig70, sizeof(ts.sig70));
    result.age = ts.age;
    name_offset = sizeof(ts);
  } break;
  default: return 0;
  }

  if (name_offset >= leaf.data.size) { return 0; }

  result.name = str8_cstring_capped(leaf.data.str + name_offset, leaf.data.str + leaf.data.size);
  if (result.name.size == leaf.data.size - name_offset) { return 0; }

  *out = result;
  return 1;
}

internal B32
cv_precomp_info_from_leaf(CV_Leaf leaf, CV_PrecompInfo *out)
{
  MemoryZeroStruct(out);
  if (leaf.kind != CV_LeafKind_PRECOMP || leaf.data.size <= sizeof(CV_LeafPreComp)) { return 0; }

  CV_LeafPreComp precomp;
  MemoryCopy(&precomp, leaf.data.str, sizeof(precomp));

  CV_PrecompInfo result = {0};
  result.start_index = precomp.start_index;
  result.sig = precomp.sig;
  result.leaf_count = precomp.count;
  result.obj_name = str8_cstring_capped(leaf.data.str + sizeof(precomp), leaf.data.str + leaf.data.size);
  if (result.obj_name.size == leaf.data.size - sizeof(precomp)) { return 0; }

  *out = result;
  return 1;
}

internal CV_UDTInfo
cv_get_udt_info(CV_LeafKind kind, String8 data)
{
  CV_UDTInfo info     = {0};
  U64        cursor   = 0;
  B32        has_size = 0;

  if (cv_header_struct_size_from_leaf_kind(kind) > data.size) {
    return info;
  }

  switch (kind) {
  case CV_LeafKind_CLASS:
  case CV_LeafKind_STRUCTURE:
  case CV_LeafKind_INTERFACE: {
    CV_LeafStruct udt;
    cursor += str8_deserial_read_struct(data, cursor, &udt);
    info.props = udt.props;
    has_size   = 1;
  } break;

  case CV_LeafKind_CLASS2:
  case CV_LeafKind_STRUCT2: {
    CV_LeafStruct2 udt;
    cursor += str8_deserial_read_struct(data, cursor, &udt);
    info.props = udt.props;
    has_size   = 1;
  } break;

  case CV_LeafKind_UNION: {
    CV_LeafUnion udt;
    cursor += str8_deserial_read_struct(data, cursor, &udt);
    info.props = udt.props;
    has_size   = 1;
  } break;

  case CV_LeafKind_ENUM: {
    CV_LeafEnum udt;
    cursor += str8_deserial_read_struct(data, cursor, &udt);
    info.props = udt.props;
  } break;

  // dbi/tpi.cpp:1332
  case CV_LeafKind_UDT_SRC_LINE: {
    CV_LeafUDTSrcLine *src_line = str8_deserial_get_raw_ptr(data, 0, sizeof(CV_LeafUDTSrcLine));
    if (src_line) { info.name = str8_struct(&src_line->udt_itype); }
    return info;
  } break;
  case CV_LeafKind_UDT_MOD_SRC_LINE: {
    CV_LeafUDTModSrcLine *mod_src_line = str8_deserial_get_raw_ptr(data, 0, sizeof(CV_LeafUDTModSrcLine));
    if (mod_src_line) { info.name = str8_struct(&mod_src_line->udt_itype); }
    return info;
  } break;

  case CV_LeafKind_ALIAS: {
    cursor = sizeof(CV_LeafAlias);
  } break;

  default: {
    return info;
  } break;
  }

  if (has_size) {
    CV_Numeric size;
    U64 consumed = cv_read_numeric(data, cursor, &size);
    if (consumed == 0) { return (CV_UDTInfo){0}; }
    cursor += consumed;
  }

  if (cursor >= data.size) {
    return (CV_UDTInfo){0};
  }

  U64 consumed = str8_deserial_read_cstr(data, cursor, &info.name);
  if (consumed > data.size - cursor) {
    return (CV_UDTInfo){0};
  }

  cursor += consumed;

  if (info.props & CV_TypeProp_HasUniqueName) {
    if (cursor >= data.size) {
      return (CV_UDTInfo){0};
    }

    consumed = str8_deserial_read_cstr(data, cursor, &info.unique_name);

    if (consumed > data.size - cursor) {
      return (CV_UDTInfo){0};
    }
  }

  return info;
}

internal String8
cv_name_from_udt_info(CV_UDTInfo udt_info)
{
  if (udt_info.props & CV_TypeProp_HasUniqueName) {
    return udt_info.unique_name;
  }
  return udt_info.name;
}

internal CV_ObjInfo
cv_obj_info_from_symbol(CV_Symbol symbol)
{
  CV_ObjInfo result = {0};
  if(symbol.kind == CV_SymKind_OBJNAME && symbol.data.size >= sizeof(CV_SymObjName))
  {
    CV_SymObjName obj;
    MemoryCopy(&obj, symbol.data.str, sizeof(obj));
    result.sig = obj.sig;
    str8_deserial_read_cstr(symbol.data, sizeof(obj), &result.name);
  }
  return result;
}

internal U64
cv_name_offset_from_symbol(CV_SymKind kind, String8 data)
{
  U64 offset = data.size;
  switch (kind) {
  case CV_SymKind_COMPILE:    offset = sizeof(CV_SymCompile); break;
  case CV_SymKind_OBJNAME:    offset = sizeof(CV_SymObjName); break;
  case CV_SymKind_BLOCK32:    offset = sizeof(CV_SymBlock32); break;
  case CV_SymKind_FILESTATIC: offset = sizeof(CV_SymFileStatic); break;
  case CV_SymKind_THUNK32: {
    offset = sizeof(CV_SymThunk32);
  } break;
  case CV_SymKind_LABEL32: {
    offset = sizeof(CV_SymLabel32);
  } break;
  case CV_SymKind_REGISTER: {
    offset = sizeof(CV_SymRegister);
  } break;
  case CV_SymKind_CONSTANT: {
    offset = sizeof(CV_SymConstant);
    CV_Numeric size;
    U64 numeric_size = cv_read_numeric(data, offset, &size);
    offset = numeric_size ? offset + numeric_size : data.size;
  } break;
  case CV_SymKind_UDT: {
    offset = sizeof(CV_SymUDT);
  } break;
  case CV_SymKind_BPREL32: {
    offset = sizeof(CV_SymBPRel32);
  } break;
  case CV_SymKind_LDATA32:
  case CV_SymKind_GDATA32: {
    offset = sizeof(CV_SymData32);
  } break;
  case CV_SymKind_PUB32: {
    offset = sizeof(CV_SymPub32);
  } break;
  case CV_SymKind_LPROC32:
  case CV_SymKind_GPROC32:
  case CV_SymKind_LPROC32_ID:
  case CV_SymKind_GPROC32_ID: {
    offset = sizeof(CV_SymProc32);
  } break;
  case CV_SymKind_REGREL32: {
    offset = sizeof(CV_SymRegrel32);
  } break;
  case CV_SymKind_LTHREAD32:
  case CV_SymKind_GTHREAD32: {
    offset = sizeof(CV_SymData32);
  } break;
  case CV_SymKind_COMPILE2: offset = sizeof(CV_SymCompile2); break;
  case CV_SymKind_COMPILE3: offset = sizeof(CV_SymCompile3); break;
  case CV_SymKind_LOCALSLOT: {
    offset = sizeof(CV_SymSlot);
  } break;
  case CV_SymKind_PROCREF:
  case CV_SymKind_LPROCREF:
  case CV_SymKind_DATAREF: {
    offset = sizeof(CV_SymRef2);
  } break;
  case CV_SymKind_TRAMPOLINE: break;
  case CV_SymKind_LOCAL: {
    offset = sizeof(CV_SymLocal);
  } break;
  }
  return Min(offset, data.size);
}

internal String8
cv_name_from_symbol(CV_SymKind kind, String8 data)
{
  U64 buf_off = cv_name_offset_from_symbol(kind, data);
  if (buf_off >= data.size) { return str8_zero(); }

  U8     *buf_ptr = data.str + buf_off;
  U8     *buf_opl = data.str + data.size;
  String8 name    = str8_cstring_capped(buf_ptr, buf_opl);

  return name;
}

internal CV_TypeIndexInfo *
cv_symbol_type_index_info_push(Arena *arena, CV_TypeIndexInfoList *list, CV_TypeIndexSource source, U64 offset)
{
  CV_TypeIndexInfo *info = push_array_no_zero(arena, CV_TypeIndexInfo, 1);
  info->next             = 0;
  info->offset           = offset;
  info->source           = source;

  SLLQueuePush(list->first, list->last, info);
  list->count += 1;

  return info;
}

internal B32
cv_get_symbol_type_index_offsets(Arena *arena, CV_SymKind kind, String8 data, CV_TypeIndexInfoList *list_out)
{
  MemoryZeroStruct(list_out);

  CV_TypeIndexSource source = CV_TypeIndexSource_TPI;
  U64 offset = 0;
  U64 count  = 1;
  switch (kind) {
  case CV_SymKind_BUILDINFO: {
    source = CV_TypeIndexSource_IPI;
    offset = OffsetOf(CV_SymBuildInfo, id);
  } break;
  case CV_SymKind_GDATA32:
  case CV_SymKind_LDATA32: {
    offset = OffsetOf(CV_SymData32, itype);
  } break;
  case CV_SymKind_LPROC32_ID:
  case CV_SymKind_GPROC32_ID:
  case CV_SymKind_LPROC32_DPC_ID: {
    source = CV_TypeIndexSource_IPI;
    offset = OffsetOf(CV_SymProc32, itype);
  } break;
  case CV_SymKind_GPROC32:
  case CV_SymKind_LPROC32:
  case CV_SymKind_LPROC32_DPC: {
    offset = OffsetOf(CV_SymProc32, itype);
  } break;
  case CV_SymKind_UDT: {
    offset = OffsetOf(CV_SymUDT, itype);
  } break;
  case CV_SymKind_GTHREAD32:
  case CV_SymKind_LTHREAD32: {
    offset = OffsetOf(CV_SymThread32, itype);
  } break;
  case CV_SymKind_FILESTATIC: {
    offset = OffsetOf(CV_SymFileStatic, itype);
  } break;
  case CV_SymKind_LOCAL: {
    offset = OffsetOf(CV_SymLocal, itype);
  } break;
  case CV_SymKind_REGREL32: {
    offset = OffsetOf(CV_SymRegrel32, itype);
  } break;
  case CV_SymKind_BPREL32: {
    offset = OffsetOf(CV_SymBPRel32, itype);
  } break;
  case CV_SymKind_REGISTER: {
    offset = OffsetOf(CV_SymRegister, itype);
  } break;
  case CV_SymKind_CONSTANT: {
    offset = OffsetOf(CV_SymConstant, itype);
  } break;
  case CV_SymKind_CALLSITEINFO: {
    offset = OffsetOf(CV_SymCallSiteInfo, itype);
  } break;
  case CV_SymKind_CALLERS:
  case CV_SymKind_CALLEES:
  case CV_SymKind_INLINEES: {
    CV_SymFunctionList func_list;
    if (data.size < sizeof(func_list)) { return 0; }
    MemoryCopy(&func_list, data.str, sizeof(func_list));
    source = CV_TypeIndexSource_IPI;
    offset = sizeof(func_list);
    count = func_list.count;
  } break;
  case CV_SymKind_INLINESITE: {
    source = CV_TypeIndexSource_IPI;
    offset = OffsetOf(CV_SymInlineSite, inlinee);
  } break;
  case CV_SymKind_INLINESITE2: {
    source = CV_TypeIndexSource_IPI;
    offset = OffsetOf(CV_SymInlineSite2, inlinee);
  } break;
  case CV_SymKind_HEAPALLOCSITE: {
    offset = OffsetOf(CV_SymHeapAllocSite, itype);
  } break;
  default: count = 0; break;
  }

  if (offset > data.size || count > (data.size - offset) / sizeof(CV_TypeIndex)) {
    return 0;
  }

  CV_TypeIndexInfoList list = {0};
  for EachIndex(i, count) {
    cv_symbol_type_index_info_push(arena, &list, source, offset + i * sizeof(CV_TypeIndex));
  }

  *list_out = list;
  return 1;
}

internal B32
cv_get_leaf_type_index_offsets(Arena *arena, CV_LeafKind leaf_kind, String8 data, CV_TypeIndexInfoList *list_out)
{
  MemoryZeroStruct(list_out);
#define push_type_index(source, offset) do {                                         \
  if ((offset) + sizeof(CV_TypeIndex) > data.size) { return 0; }                     \
  cv_symbol_type_index_info_push(arena, &list, CV_TypeIndexSource_##source, offset); \
} while (0)

#define skip_cstr(cursor) do {                                  \
  String8 name;                                                 \
  U64 name_size = str8_deserial_read_cstr(data, cursor, &name); \
  if (name_size == 0) { return 0; }                             \
  if (cursor + name_size > data.size) { return 0; }             \
  cursor += name_size;                                          \
} while (0)

#define skip_numeric(cursor) do {                          \
  CV_Numeric size;                                   \
  U64 numeric_size = cv_read_numeric(data, cursor, &size); \
  if (numeric_size == 0) { return 0; }                     \
  cursor += numeric_size;                                  \
} while (0)

  CV_TypeIndexInfoList list = {0};
  switch (leaf_kind) {
  case CV_LeafKind_NOTYPE:
  case CV_LeafKind_VTSHAPE:
  case CV_LeafKind_LABEL:
  case CV_LeafKind_NULL:
  case CV_LeafKind_NOTTRAN: {
    // no type indices
  } break;
  case CV_LeafKind_MODIFIER: {
    push_type_index(TPI, OffsetOf(CV_LeafModifier, itype));
  } break;
  case CV_LeafKind_POINTER: {
    if (sizeof(CV_LeafPointer) > data.size) {
      return 0;
    }

    push_type_index(TPI, OffsetOf(CV_LeafPointer, itype));

    CV_LeafPointer *ptr      = (CV_LeafPointer *)data.str;
    CV_PointerKind  ptr_kind = CV_PointerAttribs_Extract_Kind(ptr->attribs);

    if (ptr_kind == CV_PointerKind_BaseType) {
      // TODO: add CV_LeafPointerBaseType
      push_type_index(TPI, sizeof(CV_LeafPointer) + 0);
    } else {
      CV_PointerMode ptr_mode = CV_PointerAttribs_Extract_Mode(ptr->attribs);
      if (ptr_mode == CV_PointerMode_PtrMem || ptr_mode == CV_PointerMode_PtrMethod) {
        // TODO: add type for the CvLeafPointerMember to syms_cv.mc
        push_type_index(TPI, sizeof(CV_LeafPointer) + 0);
      }
    }
  } break;
  case CV_LeafKind_ARRAY: {
    push_type_index(TPI, OffsetOf(CV_LeafArray, entry_itype));
    push_type_index(TPI, OffsetOf(CV_LeafArray, index_itype));
  } break;
  case CV_LeafKind_CLASS:
  case CV_LeafKind_STRUCTURE:
  case CV_LeafKind_INTERFACE: {
    push_type_index(TPI, OffsetOf(CV_LeafStruct, field_itype));
    push_type_index(TPI, OffsetOf(CV_LeafStruct, derived_itype));
    push_type_index(TPI, OffsetOf(CV_LeafStruct, vshape_itype));
  } break;
  case CV_LeafKind_CLASS2:
  case CV_LeafKind_STRUCT2: {
    push_type_index(TPI, OffsetOf(CV_LeafStruct2, field_itype));
    push_type_index(TPI, OffsetOf(CV_LeafStruct2, derived_itype));
    push_type_index(TPI, OffsetOf(CV_LeafStruct2, vshape_itype));
  } break;
  case CV_LeafKind_UNION: {
    push_type_index(TPI, OffsetOf(CV_LeafUnion, field_itype));
  } break;
  case CV_LeafKind_ALIAS: {
    push_type_index(TPI, OffsetOf(CV_LeafAlias, itype));
  } break;
  case CV_LeafKind_FUNC_ID: {
    push_type_index(IPI, OffsetOf(CV_LeafFuncId, scope_string_id));
    push_type_index(TPI, OffsetOf(CV_LeafFuncId, itype));
  } break;
  case CV_LeafKind_MFUNC_ID: {
    push_type_index(TPI, OffsetOf(CV_LeafMFuncId, owner_itype));
    push_type_index(TPI, OffsetOf(CV_LeafMFuncId, itype));
  } break;
  case CV_LeafKind_STRING_ID: {
    push_type_index(IPI, OffsetOf(CV_LeafStringId, substr_list_id));
  } break;
  case CV_LeafKind_UDT_SRC_LINE: {
    push_type_index(TPI, OffsetOf(CV_LeafUDTSrcLine, udt_itype));
    push_type_index(IPI, OffsetOf(CV_LeafUDTSrcLine, src_string_id));
  } break;
  case CV_LeafKind_UDT_MOD_SRC_LINE: {
    push_type_index(TPI, OffsetOf(CV_LeafUDTModSrcLine, udt_itype));
    push_type_index(IPI, OffsetOf(CV_LeafUDTModSrcLine, src_string_id));
  } break;
  case CV_LeafKind_BUILDINFO: {
    if (data.size < sizeof(CV_LeafBuildInfo)) { return 0; }
    CV_LeafBuildInfo *build_info = (CV_LeafBuildInfo *)data.str;
    if (build_info->count > (data.size - sizeof(CV_LeafBuildInfo)) / sizeof(CV_ItemId)) { return 0; }
    for EachIndex(i, build_info->count) {
      push_type_index(IPI, sizeof(CV_LeafBuildInfo) + i * sizeof(CV_ItemId));
    }
  } break;
  case CV_LeafKind_ENUM: {
    push_type_index(TPI, OffsetOf(CV_LeafEnum, base_itype));
    push_type_index(TPI, OffsetOf(CV_LeafEnum, field_itype));
  } break;
  case CV_LeafKind_PROCEDURE: {
    push_type_index(TPI, OffsetOf(CV_LeafProcedure, ret_itype));
    push_type_index(TPI, OffsetOf(CV_LeafProcedure, arg_itype));
  } break;
  case CV_LeafKind_MFUNCTION: {
    push_type_index(TPI, OffsetOf(CV_LeafMFunction, ret_itype));
    push_type_index(TPI, OffsetOf(CV_LeafMFunction, class_itype));
    push_type_index(TPI, OffsetOf(CV_LeafMFunction, this_itype));
    push_type_index(TPI, OffsetOf(CV_LeafMFunction, arg_itype));
  } break;
  case CV_LeafKind_VFTABLE: {
    push_type_index(TPI, OffsetOf(CV_LeafVFTable, owner_itype));
    push_type_index(TPI, OffsetOf(CV_LeafVFTable, base_table_itype));
  } break;
  case CV_LeafKind_VFTPATH: {
    if (sizeof(CV_LeafVFPath) > data.size) { return 0; }

    CV_LeafVFPath *vfpath = (CV_LeafVFPath *)data.str;
    if (sizeof(CV_LeafVFPath) + vfpath->count * sizeof(CV_TypeId) > data.size) { return 0; }

    for EachIndex(i, vfpath->count) {
      push_type_index(TPI, sizeof(CV_LeafVFPath) + i * sizeof(CV_TypeId));
    }
  } break;
  case CV_LeafKind_TYPESERVER:
  case CV_LeafKind_TYPESERVER2:
  case CV_LeafKind_TYPESERVER_ST: {
    // no type indices
  } break;
  case CV_LeafKind_SKIP: {
    push_type_index(TPI, OffsetOf(CV_LeafSkip, itype));
  } break;
  case CV_LeafKind_SUBSTR_LIST: {
    if (sizeof(CV_LeafArgList) > data.size) { return 0; }

    CV_LeafArgList *arg_list = (CV_LeafArgList *)data.str;
    if (sizeof(CV_LeafArgList) + sizeof(CV_TypeIndex) * arg_list->count > data.size) { return 0; }

    for EachIndex(i, arg_list->count) {
      push_type_index(IPI, sizeof(CV_LeafArgList) + i * sizeof(CV_TypeIndex));
    }
  } break;
  case CV_LeafKind_ARGLIST: {
    if (sizeof(CV_LeafArgList) > data.size) { return 0; }

    CV_LeafArgList *arg_list = (CV_LeafArgList *)data.str;
    if (sizeof(CV_LeafArgList) + sizeof(CV_TypeIndex) * arg_list->count > data.size) { return 0; }

    for EachIndex(i, arg_list->count) {
      push_type_index(TPI, sizeof(CV_LeafArgList) + i * sizeof(CV_TypeIndex));
    }
  } break;
  case CV_LeafKind_LIST:
  case CV_LeafKind_FIELDLIST: {
    CV_FieldIter field = {0};
    while (cv_field_next(data, &field)) {
      for (U32 i = 0; i < field.type_offset_count; i += 1) {
        push_type_index(TPI, field.type_offsets[i]);
      }
    }
    if (field.malformed) { return 0; }
  } break;
  case CV_LeafKind_METHOD: {
    push_type_index(TPI, OffsetOf(CV_LeafMethod, list_itype));
  } break;
  case CV_LeafKind_METHODLIST: {
    CV_FieldIter method = {0};
    while (cv_method_next(data, &method)) {
      push_type_index(TPI, method.type_offsets[0]);
    }
    if (method.malformed) { return 0; }
  } break;
  case CV_LeafKind_ONEMETHOD: {
    push_type_index(TPI, OffsetOf(CV_LeafOneMethod, itype));
  } break;
  case CV_LeafKind_BITFIELD: {
    push_type_index(TPI, OffsetOf(CV_LeafBitField, itype));
  } break;
  case CV_LeafKind_PRECOMP:
  case CV_LeafKind_REFSYM: {
    // no type indices
  } break;
  case CV_LeafKind_INDEX: {
    push_type_index(TPI, OffsetOf(CV_LeafIndex, itype));
  } break;
  case CV_LeafKind_MEMBER: {
    push_type_index(TPI, OffsetOf(CV_LeafMember, itype));
  } break;
  case CV_LeafKind_VFUNCTAB: {
    push_type_index(TPI, OffsetOf(CV_LeafVFuncTab, itype));
  } break;
  case CV_LeafKind_VFUNCOFF: {
    push_type_index(TPI, OffsetOf(CV_LeafVFuncOff, itype));
  } break;
  case CV_LeafKind_NESTTYPE: {
    push_type_index(TPI, OffsetOf(CV_LeafNestType, itype));
  } break;
  case CV_LeafKind_NESTTYPEEX: {
    push_type_index(TPI, OffsetOf(CV_LeafNestTypeEx, itype));
  } break;
  case CV_LeafKind_ENDPRECOMP: {} break;

  default: return 0;
  }

  *list_out = list;
#undef push_type_index
#undef skip_cstr
#undef skip_numeric
  return 1;
}

internal CV_TypeIndexInfoList
cv_get_inlinee_type_index_offsets(Arena *arena, String8 raw_data)
{
  CV_TypeIndexInfoList list = {0};
  for (CV_C13InlineeIter it = cv_c13_inlinee_iter_init(raw_data); cv_c13_inlinee_iter_next(&it);) {
    cv_symbol_type_index_info_push(arena, &list, CV_TypeIndexSource_IPI, it.record_offset + OffsetOf(CV_C13InlineeSourceLineHeader, inlinee));
  }
  return list;
}

internal String8Array
cv_get_data_around_type_indices(Arena *arena, CV_TypeIndexInfoList ti_list, String8 data)
{
  String8Array result;
  if (ti_list.count > 0) {
    result.count = ti_list.count + 1;
    result.v     = push_array_no_zero(arena, String8, result.count);

    U64 cursor = 0;
    U64 ti_idx = 0;

    for (CV_TypeIndexInfo *ti_info = ti_list.first; ti_info != 0; ti_info = ti_info->next, ++ti_idx) {
      result.v[ti_idx].size = ti_info->offset - cursor;
      result.v[ti_idx].str  = data.str + cursor;
      cursor = ti_info->offset + sizeof(CV_TypeIndex);
    }

    result.v[result.count - 1].size = data.size - cursor;
    result.v[result.count - 1].str  = data.str + cursor;
  } else {
    result.count = 1;
    result.v     = push_array_no_zero(arena, String8, 1);
    result.v[0]  = data;
  }
  return result;
}

internal U64
cv_decode_inline_annot_u32(String8 data, U64 offset, U32 *out_value)
{
  // validate the complete compressed integer before consuming any bytes
  if (offset >= data.size) { return 0; }

  U8  first        = data.str[offset];
  U64 encoded_size = first < 0x80 ? 1 : first < 0xc0 ? 2 : first < 0xe0 ? 4 : 0;
  if (encoded_size == 0 || encoded_size > data.size - offset) { return 0; }

  U64 cursor = offset;

  // read header
  U8 header = 0;
  cursor += str8_deserial_read_struct(data, cursor, &header);

  // decode value
  U32 value = 0;
  {
    // 1 byte
    if ((header & 0x80) == 0) {
      value = header;
    } else if ((header & 0xC0) == 0x80 && cursor + 1 <= data.size) {
      // 2 bytes
      U8 second_byte;
      cursor += str8_deserial_read_struct(data, cursor, &second_byte);
      value = ((header & 0x3F) << 8) | second_byte;
    } else if ((header & 0xE0) == 0xC0 && cursor + 3 <= data.size) {
      // 4 bytes
      U8 second_byte, third_byte, fourth_byte;
      cursor += str8_deserial_read_struct(data, cursor, &second_byte);
      cursor += str8_deserial_read_struct(data, cursor, &third_byte);
      cursor += str8_deserial_read_struct(data, cursor, &fourth_byte);
      value = (((U32)header & 0x1F) << 24) | ((U32)second_byte << 16) | ((U32)third_byte << 8) | (U32)fourth_byte;
    } else if ((header & 0xE0) == 0xE0) {
      // bad encode
      value = max_U32;
    }
  }

  // output results
  if (out_value) {
    *out_value = value;
  }

  U64 read_size = cursor - offset;
  return read_size;
}

internal U64
cv_decode_inline_annot_s32(String8 data, U64 offset, S32 *out_value)
{
  U32 value     = 0;
  U64 read_size = cv_decode_inline_annot_u32(data, offset, &value);
  if (read_size == 0) { return 0; }

  if (value & 1) {
    value = -(value >> 1);
  } else {
    value = value >> 1;
  }

  *out_value = (S32)value;
  return read_size;
}

internal S32
cv_inline_annot_signed_from_unsigned_operand(U32 value)
{
  if (value & 1) {
    value = -(value >> 1);
  } else {
    value = value >> 1;
  }
  S32 result = (S32)value;
  return result;
}

////////////////////////////////
//~ rjf: Enum -> String

internal String8
cv_string_from_inline_range_kind(CV_InlineRangeKind kind)
{
  switch (kind) {
    case CV_InlineRangeKind_Expr: return str8_lit("Expr");
    case CV_InlineRangeKind_Stmt: return str8_lit("Stmt");
  }
  return str8_zero();
}

internal String8
cv_string_from_type_index_source(CV_TypeIndexSource ti_source)
{
  switch (ti_source) {
    case CV_TypeIndexSource_NULL:  return str8_lit("");    break;
    case CV_TypeIndexSource_TPI:   return str8_lit("TPI"); break;
    case CV_TypeIndexSource_IPI:   return str8_lit("IPI"); break;
    case CV_TypeIndexSource_COUNT: break;
  }
  return str8_zero();
}

internal String8
cv_string_from_language(CV_Language x)
{
  switch (x) {
#define X(_n,_i) case _i: return str8_lit(Stringify(_n));
    CV_LanguageXList(X)
#undef X
  }
  return str8_zero();
}

internal String8 
cv_string_from_reg_id(Arena *arena, CV_Arch arch, U32 id)
{
  String8 result = str8_zero();
  switch (arch) {
    case CV_Arch_X64: {
      switch (id) {
#define X(_N, _ID, ...) case _ID: result = str8_lit(Stringify(_N)); break;
        CV_Reg_X64_XList(X)
#undef X
      }
    } break;
    default: NotImplemented; break;
  }
  if (result.size == 0) {
    result = push_str8f(arena, "%x", id);
  }
  return result;
}

internal String8
cv_string_from_member_access(CV_MemberAccess x)
{
  switch (x) {
    case CV_MemberAccess_Null:      break;
    case CV_MemberAccess_Private:   return str8_lit("Private");
    case CV_MemberAccess_Protected: return str8_lit("Protected");
    case CV_MemberAccess_Public:    return str8_lit("Public");
  }
  return str8_zero();
}

internal String8
cv_string_from_method_prop(CV_MethodProp x)
{
  switch (x) {
    case CV_MethodProp_Vanilla:     return str8_lit("Vanilla");
    case CV_MethodProp_Virtual:     return str8_lit("Virtual");
    case CV_MethodProp_Static:      return str8_lit("Static");
    case CV_MethodProp_Friend:      return str8_lit("Friend");
    case CV_MethodProp_Intro:       return str8_lit("Intro");
    case CV_MethodProp_PureVirtual: return str8_lit("PureVirtual");
    case CV_MethodProp_PureIntro:   return str8_lit("PureIntro");
  }
  return str8_zero();
}

internal String8
cv_string_from_hfa(CV_HFAKind x)
{
  switch (x) {
    case CV_HFAKind_None:   return str8_lit("None");
    case CV_HFAKind_Float:  return str8_lit("Float");
    case CV_HFAKind_Double: return str8_lit("Double");
    case CV_HFAKind_Other:  return str8_lit("Other");
  }
  return str8_zero();
}

internal String8
cv_string_from_mcom(CV_MoComUDTKind x)
{
  switch (x) {
    case CV_MoComUDTKind_None:      return str8_lit("None");
    case CV_MoComUDTKind_Ref:       return str8_lit("Ref");
    case CV_MoComUDTKind_Value:     return str8_lit("Value");
    case CV_MoComUDTKind_Interface: return str8_lit("Interface");
  }
  return str8_zero();
}

internal String8
cv_string_from_binary_opcode(CV_InlineBinaryAnnotation x)
{
  switch (x) {
    case CV_InlineBinaryAnnotation_Null:                          break;
    case CV_InlineBinaryAnnotation_CodeOffset:                    return str8_lit("CodeOffset");
    case CV_InlineBinaryAnnotation_ChangeCodeOffsetBase:          return str8_lit("ChangeCodeOffsetBase");
    case CV_InlineBinaryAnnotation_ChangeCodeOffset:              return str8_lit("ChangeCodeOffset");
    case CV_InlineBinaryAnnotation_ChangeCodeLength:              return str8_lit("ChangeCodeLength");
    case CV_InlineBinaryAnnotation_ChangeFile:                    return str8_lit("ChangeFile");
    case CV_InlineBinaryAnnotation_ChangeLineOffset:              return str8_lit("ChangeLineOffset");
    case CV_InlineBinaryAnnotation_ChangeLineEndDelta:            return str8_lit("ChangeLineEndDelta");
    case CV_InlineBinaryAnnotation_ChangeRangeKind:               return str8_lit("ChangeRangeKind");
    case CV_InlineBinaryAnnotation_ChangeColumnStart:             return str8_lit("ChangeColumnStart");
    case CV_InlineBinaryAnnotation_ChangeColumnEndDelta:          return str8_lit("ChangeColumnEndDelta");
    case CV_InlineBinaryAnnotation_ChangeCodeOffsetAndLineOffset: return str8_lit("ChangeCodeOffsetAndLineOffset");
    case CV_InlineBinaryAnnotation_ChangeCodeLengthAndCodeOffset: return str8_lit("ChangeCodeLengthAndCodeOffset");
    case CV_InlineBinaryAnnotation_ChangeColumnEnd:               return str8_lit("ChangeColumnEnd");
  }
  return str8_zero();
}

internal String8
cv_string_from_thunk_ordinal(CV_ThunkOrdinal x)
{
  switch (x) {
    case CV_ThunkOrdinal_NoType:            return str8_lit("NoType");
    case CV_ThunkOrdinal_Adjustor:          return str8_lit("Adjustor");
    case CV_ThunkOrdinal_VCall:             return str8_lit("VCall");
    case CV_ThunkOrdinal_PCode:             return str8_lit("PCode");
    case CV_ThunkOrdinal_Load:              return str8_lit("Load");
    case CV_ThunkOrdinal_TrampIncremental:  return str8_lit("TrampIncremental");
    case CV_ThunkOrdinal_TrampBranchIsland: return str8_lit("TrampBranchIsland");
  }
  return str8_zero();
}

internal String8
cv_string_from_frame_cookie_kind(CV_FrameCookieKind x)
{
  switch (x) {
    case CV_FrameCookieKind_Copy:  return str8_lit("Copy");
    case CV_FrameCookieKind_XorSP: return str8_lit("XorSP");
    case CV_FrameCookieKind_XorBP: return str8_lit("XorR13");
  }
  return str8_zero();
}

internal String8
cv_string_from_generic_style(CV_GenericStyle x)
{
  switch (x) {
    case CV_GenericStyle_VOID:   return str8_lit("VOID");
    case CV_GenericStyle_REG:    return str8_lit("REG");
    case CV_GenericStyle_ICAN:   return str8_lit("ICAN");
    case CV_GenericStyle_ICAF:   return str8_lit("ICAF");
    case CV_GenericStyle_IRAN:   return str8_lit("IRAN");
    case CV_GenericStyle_IRAF:   return str8_lit("IRAF");
    case CV_GenericStyle_UNUSED: return str8_lit("UNUSED");
  }
  return str8_zero();
}

internal String8
cv_string_from_trampoline_kind(CV_TrampolineKind x)
{
  switch (x) {
    case CV_TrampolineKind_Incremental:  return str8_lit("Incremental");
    case CV_TrampolineKind_BranchIsland: return str8_lit("BranchIsland");
  }
  return str8_zero();
}

internal String8
cv_string_from_virtual_table_shape_kind(CV_VirtualTableShape x)
{
  switch (x) {
    case CV_VirtualTableShape_Near:   return str8_lit("Near");
    case CV_VirtualTableShape_Far:    return str8_lit("Far");
    case CV_VirtualTableShape_Thin:   return str8_lit("Thin");
    case CV_VirtualTableShape_Outer:  return str8_lit("Outer");
    case CV_VirtualTableShape_Meta:   return str8_lit("Meta");
    case CV_VirtualTableShape_Near32: return str8_lit("Near32");
    case CV_VirtualTableShape_Far32:  return str8_lit("Far32");
  }
  return str8_zero();
}

internal String8
cv_string_from_call_kind(CV_CallKind x)
{
  switch (x) {
    case CV_CallKind_NearC:          return str8_lit("NearC");
    case CV_CallKind_FarC:           return str8_lit("FarC");
    case CV_CallKind_NearPascal:     return str8_lit("NearPascal");
    case CV_CallKind_FarPascal:      return str8_lit("FarPascal");
    case CV_CallKind_NearFast:       return str8_lit("NearFast");
    case CV_CallKind_FarFast:        return str8_lit("FarFast");
    case CV_CallKind_UNUSED:         return str8_lit("UNUSED");
    case CV_CallKind_NearStd:        return str8_lit("NearStd");
    case CV_CallKind_FarStd:         return str8_lit("FarStd");
    case CV_CallKind_NearSys:        return str8_lit("NearSys");
    case CV_CallKind_FarSys:         return str8_lit("FarSys");
    case CV_CallKind_This:           return str8_lit("This");
    case CV_CallKind_Mips:           return str8_lit("Mips");
    case CV_CallKind_Generic:        return str8_lit("Generic");
    case CV_CallKind_Alpha:          return str8_lit("Alpha");
    case CV_CallKind_PPC:            return str8_lit("PPC");
    case CV_CallKind_HitachiSuperH:  return str8_lit("HitachiSuperH");
    case CV_CallKind_Arm:            return str8_lit("Arm");
    case CV_CallKind_AM33:           return str8_lit("AM33");
    case CV_CallKind_TriCore:        return str8_lit("TriCore");
    case CV_CallKind_HitachiSuperH5: return str8_lit("HitachiSuperH5");
    case CV_CallKind_M32R:           return str8_lit("M32R");
    case CV_CallKind_Clr:            return str8_lit("Clr");
    case CV_CallKind_Inline:         return str8_lit("Inline");
    case CV_CallKind_NearVector:     return str8_lit("NearVector");
  }
  return str8_zero();
}

internal String8
cv_string_from_member_pointer_kind(CV_MemberPointerKind x)
{
  switch (x) {
    case CV_MemberPointerKind_Undef:        return str8_lit("Undef");
    case CV_MemberPointerKind_DataSingle:   return str8_lit("DataSingle");
    case CV_MemberPointerKind_DataMultiple: return str8_lit("DataMultiple");
    case CV_MemberPointerKind_DataVirtual:  return str8_lit("DataVirtual");
    case CV_MemberPointerKind_DataGeneral:  return str8_lit("DataGeneral");
    case CV_MemberPointerKind_FuncSingle:   return str8_lit("FuncSingle");
    case CV_MemberPointerKind_FuncMultiple: return str8_lit("FuncMultiple");
    case CV_MemberPointerKind_FuncGeneral:  return str8_lit("FuncGeneral");
  }
  return str8_zero();
}

internal String8
cv_string_from_pointer_kind(CV_PointerKind x)
{
  switch (x) {
    case CV_PointerKind_Near:        return str8_lit("Near");
    case CV_PointerKind_Far:         return str8_lit("Far");
    case CV_PointerKind_Huge:        return str8_lit("Huge");
    case CV_PointerKind_BaseSeg:     return str8_lit("BaseSeg");
    case CV_PointerKind_BaseVal:     return str8_lit("BaseVal");
    case CV_PointerKind_BaseSegVal:  return str8_lit("BaseSegVal");
    case CV_PointerKind_BaseAddr:    return str8_lit("BaseAddr");
    case CV_PointerKind_BaseSegAddr: return str8_lit("BaseSegAddr");
    case CV_PointerKind_BaseType:    return str8_lit("BaseType");
    case CV_PointerKind_BaseSelf:    return str8_lit("BaseSelf");
    case CV_PointerKind_Near32:      return str8_lit("Near32");
    case CV_PointerKind_Far32:       return str8_lit("Far32");
    case CV_PointerKind_64:          return str8_lit("64Bit");
  }
  return str8_zero();
}

internal String8
cv_string_from_pointer_mode(CV_PointerMode x)
{
  switch (x) {
    case CV_PointerMode_Ptr:       return str8_lit("Ptr");
    case CV_PointerMode_LRef:      return str8_lit("LRef");
    case CV_PointerMode_PtrMem:    return str8_lit("PtrMem");
    case CV_PointerMode_PtrMethod: return str8_lit("PtrMethod");
    case CV_PointerMode_RRef:      return str8_lit("RRef");
  }
  return str8_zero();
}

internal String8
cv_string_from_c13_checksum_kind(CV_C13ChecksumKind x)
{
  switch (x) {
    case CV_C13ChecksumKind_Null:   break;
    case CV_C13ChecksumKind_MD5:    return str8_lit("MD5");
    case CV_C13ChecksumKind_SHA1:   return str8_lit("SHA1");
    case CV_C13ChecksumKind_SHA256: return str8_lit("SHA256");
  }
  return str8_zero();
}

internal String8
cv_string_from_label_kind(Arena *arena, CV_LabelKind x)
{
  switch (x) {
    case CV_LabelKind_Near: return str8_lit("Near");
    case CV_LabelKind_Far:  return str8_lit("Far");
  }
  return push_str8f(arena, "%#x", x);
}

internal String8
cv_string_from_c13_subsection_kind(CV_C13SubSectionKind x)
{
  switch (x) {
#define X(_N, _ID) case CV_C13SubSectionKind_##_N: return str8_lit(Stringify(_N));
    CV_C13SubSectionKindXList(X)
#undef X
  }
  return str8_zero();
}

internal String8
cv_string_from_modifier_flags(Arena *arena, CV_ModifierFlags x)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List list = {0};
  if (x & CV_ModifierFlag_Const) {
    str8_list_pushf(scratch.arena, &list, "Const");
  }
  if (x & CV_ModifierFlag_Volatile) {
    str8_list_pushf(scratch.arena, &list, "Volatile");
  }
  if (x & CV_ModifierFlag_Unaligned) {
    str8_list_pushf(scratch.arena, &list, "Unaligned");
  }
  String8 result = str8_list_join(arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_pointer_attribs(Arena *arena, CV_PointerAttribs x)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  String8List list = {0};
  if (x & CV_PointerAttrib_IsFlat) {
    x &= ~CV_PointerAttrib_IsFlat;
    str8_list_pushf(scratch.arena, &list, "IsFlat");
  }
  if (x & CV_PointerAttrib_Volatile) {
    x &= ~CV_PointerAttrib_Volatile;
    str8_list_pushf(scratch.arena, &list, "Volatile");
  }
  if (x & CV_PointerAttrib_Const) {
    x &= ~CV_PointerAttrib_Const;
    str8_list_pushf(scratch.arena, &list, "Const");
  }
  if (x & CV_PointerAttrib_Unaligned) {
    x &= ~CV_PointerAttrib_Unaligned;
    str8_list_pushf(scratch.arena, &list, "Unaligned");
  }
  if (x & CV_PointerAttrib_Restricted) {
    x &= ~CV_PointerAttrib_Restricted;
    str8_list_pushf(scratch.arena, &list, "Restricted");
  }
  if (x & CV_PointerAttrib_MOCOM) {
    x &= ~CV_PointerAttrib_MOCOM;
    str8_list_pushf(scratch.arena, &list, "MOCOM");
  }
  if (x & CV_PointerAttrib_LRef) {
    x &= ~CV_PointerAttrib_LRef;
    str8_list_pushf(scratch.arena, &list, "LRef");
  }
  if (x & CV_PointerAttrib_RRef) {
    x &= ~CV_PointerAttrib_RRef;
    str8_list_pushf(scratch.arena, &list, "RRef");
  }
  
  CV_PointerKind kind = CV_PointerAttribs_Extract_Kind(x);
  CV_PointerMode mode = CV_PointerAttribs_Extract_Mode(x);
  U64            size = CV_PointerAttribs_Extract_Size(x);
  
  x &= ~(0x1f|(0x7<<5)|(0x3f<<13));
  
  if (kind) {
    String8 kind_str = cv_string_from_pointer_kind(kind);
    str8_list_pushf(scratch.arena, &list, "Kind=%S", kind_str);
  }
  if (mode) {
    String8 mode_str = cv_string_from_pointer_mode(mode);
    str8_list_pushf(scratch.arena, &list, "Mode=%S", mode_str);
  }
  if (size) {
    str8_list_pushf(scratch.arena, &list, "Size=%llu", size);
  }
  
  if (x != 0) {
    str8_list_pushf(scratch.arena, &list, "Unknown=%x", x);
  }
  
  String8 result = str8_list_join(arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_function_attribs(Arena *arena, CV_FunctionAttribs x)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List list = {0};
  if (x & CV_FunctionAttrib_CxxReturnUDT) {
    str8_list_pushf(scratch.arena, &list, "CxxReturnUDT");
  }
  if (x & CV_FunctionAttrib_Constructor) {
    str8_list_pushf(scratch.arena, &list, "Constructor");
  }
  if (x & CV_FunctionAttrib_ConstructorVBase) {
    str8_list_pushf(scratch.arena, &list, "ConstructorVBase");
  }
  String8 result = str8_list_join(arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_export_flags(Arena *arena, CV_ExportFlags x)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List list = {0};
  if (x & CV_ExportFlag_Constant) {
    str8_list_pushf(scratch.arena, &list, "Constant");
  }
  if (x & CV_ExportFlag_Data) {
    str8_list_pushf(scratch.arena, &list, "Data");
  }
  if (x & CV_ExportFlag_Private) {
    str8_list_pushf(scratch.arena, &list, "Private");
  }
  if (x & CV_ExportFlag_NoName) {
    str8_list_pushf(scratch.arena, &list, "NoName");
  }
  if (x & CV_ExportFlag_Ordinal) {
    str8_list_pushf(scratch.arena, &list, "Ordinal");
  }
  if (x & CV_ExportFlag_Forwarder) {
    str8_list_pushf(scratch.arena, &list, "Forwarder");
  }
  String8 result = str8_list_join(arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_sepcode(Arena *arena, CV_SepcodeFlags x)
{
  Temp scratch = scratch_begin(&arena,1);
  String8List list = {0};
  if (x & CV_SepcodeFlag_IsLexicalScope) {
    str8_list_pushf(scratch.arena, &list, "IsLexicalScope");
  }
  if (x & CV_SepcodeFlag_ReturnsToParent) {
    str8_list_pushf(scratch.arena, &list, "ReturnsToParent");
  }
  String8 result = str8_list_join(arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_pub32_flags(Arena *arena, CV_Pub32Flags x)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List list = {0};
  if (x & CV_Pub32Flag_Code) {
    str8_list_pushf(scratch.arena, &list, "Code");
  }
  if (x & CV_Pub32Flag_Function) {
    str8_list_pushf(scratch.arena, &list, "Function");
  }
  if (x & CV_Pub32Flag_ManagedCode) {
    str8_list_pushf(scratch.arena, &list, "ManagedCode");
  }
  if (x & CV_Pub32Flag_MSIL) {
    str8_list_pushf(scratch.arena, &list, "MSIL");
  }
  String8 result = str8_list_join(scratch.arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_generic_flags(Arena *arena, CV_GenericFlags x)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List list = {0};
  if (x & CV_GenericFlags_CSTYLE) {
    str8_list_pushf(scratch.arena, &list, "CSTYLE");
  }
  if (x & CV_GenericFlags_RSCLEAN) {
    str8_list_pushf(scratch.arena, &list, "RSCLEAN");
  }
  String8 result = str8_list_join(arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_frame_proc_flags(Arena *arena, CV_FrameprocFlags x)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List list = {0};
  if (x & CV_FrameprocFlag_UsesAlloca) {
    str8_list_pushf(scratch.arena, &list, "UsesAlloca");
  }
  if (x & CV_FrameprocFlag_UsesSetJmp) {
    str8_list_pushf(scratch.arena, &list, "UsesSetJmp");
  }
  if (x & CV_FrameprocFlag_UsesLongJmp) {
    str8_list_pushf(scratch.arena, &list, "UsesLongJmp");
  }
  if (x & CV_FrameprocFlag_UsesInlAsm) {
    str8_list_pushf(scratch.arena, &list, "UsesInlAsm");
  }
  if (x & CV_FrameprocFlag_UsesEH) {
    str8_list_pushf(scratch.arena, &list, "UsesEH");
  }
  if (x & CV_FrameprocFlag_Inline) {
    str8_list_pushf(scratch.arena, &list, "Inline");
  }
  if (x & CV_FrameprocFlag_HasSEH) {
    str8_list_pushf(scratch.arena, &list, "HasSEH");
  }
  if (x & CV_FrameprocFlag_Naked) {
    str8_list_pushf(scratch.arena, &list, "Naked");
  }
  if (x & CV_FrameprocFlag_HasSecurityChecks) {
    str8_list_pushf(scratch.arena, &list, "HasSecurityChecks");
  }
  if (x & CV_FrameprocFlag_AsyncEH) {
    str8_list_pushf(scratch.arena, &list, "AsyncEH");
  }
  if (x & CV_FrameprocFlag_GSNoStackOrdering) {
    str8_list_pushf(scratch.arena, &list, "GSNoStackOrdering");
  }
  if (x & CV_FrameprocFlag_WasInlined) {
    str8_list_pushf(scratch.arena, &list, "WasInlined");
  }
  if (x & CV_FrameprocFlag_GSCheck) {
    str8_list_pushf(scratch.arena, &list, "GSCheck");
  }
  if (x & CV_FrameprocFlag_SafeBuffers) {
    str8_list_pushf(scratch.arena, &list, "SafeBuffers");
  }
  if (x & CV_FrameprocFlag_PogoOn) {
    str8_list_pushf(scratch.arena, &list, "PogoOn");
  }
  if (x & CV_FrameprocFlag_PogoCountsValid) {
    str8_list_pushf(scratch.arena, &list, "PogoCountsValid");
  }
  if (x & CV_FrameprocFlag_OptSpeed) {
    str8_list_pushf(scratch.arena, &list, "OptSpeed");
  }
  if (x & CV_FrameprocFlag_HasCFG) {
    str8_list_pushf(scratch.arena, &list, "HasCFG");
  }
  if (x & CV_FrameprocFlag_HasCFW) {
    str8_list_pushf(scratch.arena, &list, "HasCFW");
  }
  String8 result = str8_list_join(arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_type_props(Arena *arena, CV_TypeProps32 x)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  U32 hfa  = CV_TypeProps_Extract_HFA(x);
  U32 mcom = CV_TypeProps_Extract_MOCOM(x);
  
  String8 hfa_str  = cv_string_from_hfa(hfa);
  String8 mcom_str = cv_string_from_mcom(mcom);
  
  String8 flags_str;
  {
    String8List list = {0};
    if (x & CV_TypeProp_Packed) {
      x &= ~CV_TypeProp_Packed;
      str8_list_pushf(scratch.arena, &list, "Packed");
    }
    if (x & CV_TypeProp_HasConstructorsDestructors) {
      x &= ~CV_TypeProp_HasConstructorsDestructors;
      str8_list_pushf(scratch.arena, &list, "HasConstructorsDestructors");
    }
    if (x & CV_TypeProp_OverloadedOperators) {
      x &= ~CV_TypeProp_OverloadedOperators;
      str8_list_pushf(scratch.arena, &list, "OverloadedOperators");
    }
    if (x & CV_TypeProp_IsNested) {
      x &= ~CV_TypeProp_IsNested;
      str8_list_pushf(scratch.arena, &list, "IsNested");
    }
    if (x & CV_TypeProp_ContainsNested) {
      x &= ~CV_TypeProp_ContainsNested;
      str8_list_pushf(scratch.arena, &list, "ContainsNested");
    }
    if (x & CV_TypeProp_OverloadedAssignment) {
      x &= ~CV_TypeProp_OverloadedAssignment;
      str8_list_pushf(scratch.arena, &list, "OverloadedAssignment");
    }
    if (x & CV_TypeProp_OverloadedCasting) {
      x &= ~CV_TypeProp_OverloadedCasting;
      str8_list_pushf(scratch.arena, &list, "OverloadedCasting");
    }
    if (x & CV_TypeProp_FwdRef) {
      x &= ~CV_TypeProp_FwdRef;
      str8_list_pushf(scratch.arena, &list, "FwdRef");
    }
    if (x & CV_TypeProp_Scoped) {
      x &= ~CV_TypeProp_Scoped;
      str8_list_pushf(scratch.arena, &list, "Scoped");
    }
    if (x & CV_TypeProp_HasUniqueName) {
      x &= ~CV_TypeProp_HasUniqueName;
      str8_list_pushf(scratch.arena, &list, "HasUniqueName");
    }
    if (x & CV_TypeProp_Sealed) {
      x &= ~CV_TypeProp_Sealed;
      str8_list_pushf(scratch.arena, &list, "Sealed");
    }
    if (x & CV_TypeProp_Intrinsic) {
      x &= ~CV_TypeProp_Intrinsic;
      str8_list_pushf(scratch.arena, &list, "Intrinsic");
    }
    if (x != 0) {
      str8_list_pushf(scratch.arena, &list, "%x", x);
    }
    flags_str = str8_list_join(scratch.arena, &list, &(StringJoin){.sep=str8_lit(", ") });
    
    if (hfa) {
      str8_list_pushf(scratch.arena, &list, "HFA = %S", hfa_str);
    }
    if (mcom) {
      str8_list_pushf(scratch.arena, &list, "MCOM = %S", mcom_str);
    }
  }
  
  String8 result = push_str8f(arena, "%S", flags_str);
  
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_local_flags(Arena *arena, CV_LocalFlags x)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List list = {0};
  if (x & CV_LocalFlag_Param) {
    str8_list_pushf(scratch.arena, &list, "Param");
  }
  if (x & CV_LocalFlag_AddrTaken) {
    str8_list_pushf(scratch.arena, &list, "AddrTaken");
  }
  if (x & CV_LocalFlag_Compgen) {
    str8_list_pushf(scratch.arena, &list, "Compgen");
  }
  if (x & CV_LocalFlag_Aggregate) {
    str8_list_pushf(scratch.arena, &list, "Aggregate");
  }
  if (x & CV_LocalFlag_PartOfAggregate) {
    str8_list_pushf(scratch.arena, &list, "PartOfAggregate");
  }
  if (x & CV_LocalFlag_Aliased) {
    str8_list_pushf(scratch.arena, &list, "Aliased");
  }
  if (x & CV_LocalFlag_Alias) {
    str8_list_pushf(scratch.arena, &list, "Alias");
  }
  if (x & CV_LocalFlag_Retval) {
    str8_list_pushf(scratch.arena, &list, "Retval");
  }
  if (x & CV_LocalFlag_OptOut) {
    str8_list_pushf(scratch.arena, &list, "OptOut");
  }
  if (x & CV_LocalFlag_Global) {
    str8_list_pushf(scratch.arena, &list, "Global");
  }
  if (x & CV_LocalFlag_Static) {
    str8_list_pushf(scratch.arena, &list, "Static");
  }
  String8 result = str8_list_join(arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_proc_flags(Arena *arena, CV_ProcFlags x)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List list = {0};
  if (x & CV_ProcFlag_NoFPO) {
    x &= ~CV_ProcFlag_NoFPO;
    str8_list_pushf(scratch.arena, &list, "NoFPO");
  }
  if (x & CV_ProcFlag_IntReturn) {
    x &= ~CV_ProcFlag_IntReturn;
    str8_list_pushf(scratch.arena, &list, "IntReturn");
  }
  if (x & CV_ProcFlag_FarReturn) {
    x &= ~CV_ProcFlag_FarReturn;
    str8_list_pushf(scratch.arena, &list, "FarReturn");
  }
  if (x & CV_ProcFlag_NeverReturn) {
    x &= ~CV_ProcFlag_NeverReturn;
    str8_list_pushf(scratch.arena, &list, "NeverReturn");
  }
  if (x & CV_ProcFlag_NotReached) {
    x &= ~CV_ProcFlag_NotReached;
    str8_list_pushf(scratch.arena, &list, "NotReached");
  }
  if (x & CV_ProcFlag_CustomCall) {
    x &= ~CV_ProcFlag_CustomCall;
    str8_list_pushf(scratch.arena, &list, "CustomCall");
  }
  if (x & CV_ProcFlag_NoInline) {
    x &= ~CV_ProcFlag_NoInline;
    str8_list_pushf(scratch.arena, &list, "NoInline");
  }
  if (x & CV_ProcFlag_OptDbgInfo) {
    x &= ~CV_ProcFlag_OptDbgInfo;
    str8_list_pushf(scratch.arena, &list, "OptDbgInfo");
  }
  if (x != 0) {
    str8_list_pushf(scratch.arena, &list, "%#x", x);
  }
  String8 result;
  if (list.node_count == 0) {
    result = str8_lit("None");
  } else {
    result = str8_list_join(arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  }
  temp_end(scratch);
  return result;
}

internal String8
cv_string_from_range_attribs(Arena *arena, CV_RangeAttribs x)
{ (void)arena;
  String8 result = str8_lit("None");
  if (x == CV_RangeAttrib_Maybe) {
    result = str8_lit("Maybe");
  }
  return result;
}

internal String8
cv_string_from_defrange_register_rel_flags(Arena *arena, CV_DefrangeRegisterRelFlags x)
{ (void)arena;
  String8 result = str8_lit("None");
  if (x == CV_DefrangeRegisterRelFlag_SpilledOutUDTMember) {
    result = str8_lit("SpilledOutUDTMember");
  }
  return result;
}

internal String8
cv_string_from_field_attribs(Arena *arena, CV_FieldAttribs attribs)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  U32 access = CV_FieldAttribs_Extract_Access(attribs);
  U32 mprop  = CV_FieldAttribs_Extract_MethodProp(attribs);
  attribs &= ~(0x3 | 0x7);
  
  String8 access_str = cv_string_from_member_access(access);
  String8 mprop_str  = cv_string_from_method_prop(mprop);
  
  String8List list = {0};
  {
    if (attribs & CV_FieldAttrib_Pseudo) {
      attribs &= ~CV_FieldAttrib_Pseudo;
      str8_list_pushf(scratch.arena, &list, "Pseudo");
    }
    if (attribs & CV_FieldAttrib_NoInherit) {
      attribs &= ~CV_FieldAttrib_NoInherit;
      str8_list_pushf(scratch.arena, &list, "NoInherit");
    }
    if (attribs & CV_FieldAttrib_NoConstruct) {
      attribs &= ~CV_FieldAttrib_NoConstruct;
      str8_list_pushf(scratch.arena, &list, "NoConstruct");
    }
    if (attribs & CV_FieldAttrib_CompilerGenated) {
      attribs &= ~CV_FieldAttrib_CompilerGenated;
      str8_list_pushf(scratch.arena, &list, "CompilerGenerated");
    }
    if (attribs & CV_FieldAttrib_Sealed) {
      attribs &= ~CV_FieldAttrib_Sealed;
      str8_list_pushf(scratch.arena, &list, "Sealed");
    }
    if (attribs) {
      str8_list_pushf(scratch.arena, &list, "Unknown: %x", attribs);
    }
  }
  
  if (access) {
    str8_list_pushf(scratch.arena, &list, "%S", access_str);
  }
  if (mprop) {
    str8_list_pushf(scratch.arena, &list, "%S", mprop_str);
  }
  
  String8 result = str8_list_join(scratch.arena, &list, &(StringJoin){.sep=str8_lit(", ")});
  
  scratch_end(scratch);
  return result;
}

internal String8
cv_string_from_itype(Arena *arena, CV_TypeIndex min_itype, CV_TypeIndex itype)
{
  String8 result = str8_zero();
  if (itype < min_itype) {
    String8 n = cv_type_name_from_basic_type((CV_BasicType)itype);
    if (n.size) {
      Temp scratch = scratch_begin(&arena, 1);
      
      U64 type = CV_BasicTypeFromTypeId(itype);
      char *type_str = "???";
      switch (type) {
        case CV_BasicType_NOTYPE:     type_str = "NOTYPE";     break;
        case CV_BasicType_ABS:        type_str = "ABS";        break;
        case CV_BasicType_SEGMENT:    type_str = "SEGMENT";    break;
        case CV_BasicType_VOID:       type_str = "VOID";       break;
        case CV_BasicType_CURRENCY:   type_str = "CURRENCY";   break;
        case CV_BasicType_NBASICSTR:  type_str = "NBASICSTR";  break;
        case CV_BasicType_FBASICSTR:  type_str = "FBASICSTR";  break;
        case CV_BasicType_NOTTRANS:   type_str = "NOTTRANS";   break;
        case CV_BasicType_HRESULT:    type_str = "HRESULT";    break;
        case CV_BasicType_CHAR:       type_str = "CHAR";       break;
        case CV_BasicType_SHORT:      type_str = "SHORT";      break;
        case CV_BasicType_LONG:       type_str = "LONG";       break;
        case CV_BasicType_QUAD:       type_str = "QUAD";       break;
        case CV_BasicType_OCT:        type_str = "OCT";        break;
        case CV_BasicType_UCHAR:      type_str = "UCHAR";      break;
        case CV_BasicType_USHORT:     type_str = "USHORT";     break;
        case CV_BasicType_ULONG:      type_str = "ULONG";      break;
        case CV_BasicType_UQUAD:      type_str = "UQUAD";      break;
        case CV_BasicType_UOCT:       type_str = "UOCT";       break;
        case CV_BasicType_BOOL8:      type_str = "BOOL8";      break;
        case CV_BasicType_BOOL16:     type_str = "BOOL16";     break;
        case CV_BasicType_BOOL32:     type_str = "BOOL32";     break;
        case CV_BasicType_BOOL64:     type_str = "BOOL64";     break;
        case CV_BasicType_FLOAT32:    type_str = "FLOAT32";    break;
        case CV_BasicType_FLOAT64:    type_str = "FLOAT64";    break;
        case CV_BasicType_FLOAT80:    type_str = "FLOAT80";    break;
        case CV_BasicType_FLOAT128:   type_str = "FLOAT128";   break;
        case CV_BasicType_FLOAT48:    type_str = "FLOAT48";    break;
        case CV_BasicType_FLOAT32PP:  type_str = "FLOAT32PP";  break;
        case CV_BasicType_FLOAT16:    type_str = "FLOAT16";    break;
        case CV_BasicType_COMPLEX32:  type_str = "COMPLEX32";  break;
        case CV_BasicType_COMPLEX64:  type_str = "COMPLEX64";  break;
        case CV_BasicType_COMPLEX80:  type_str = "COMPLEX80";  break;
        case CV_BasicType_COMPLEX128: type_str = "COMPLEX128"; break;
        case CV_BasicType_BIT:        type_str = "BIT";        break;
        case CV_BasicType_PASCHAR:    type_str = "PASCHAR";    break;
        case CV_BasicType_BOOL32FF:   type_str = "BOOL32FF";   break;
        case CV_BasicType_INT8:       type_str = "INT8";       break;
        case CV_BasicType_UINT8:      type_str = "UINT8";      break;
        case CV_BasicType_RCHAR:      type_str = "RCHAR";      break;
        case CV_BasicType_WCHAR:      type_str = "WCHAR";      break;
        case CV_BasicType_INT16:      type_str = "INT16";      break;
        case CV_BasicType_UINT16:     type_str = "UINT16";     break;
        case CV_BasicType_INT32:      type_str = "INT32";      break;
        case CV_BasicType_UINT32:     type_str = "UINT32";     break;
        case CV_BasicType_INT64:      type_str = "INT64";      break;
        case CV_BasicType_UINT64:     type_str = "UINT64";     break;
        case CV_BasicType_INT128:     type_str = "INT128";     break;
        case CV_BasicType_UINT128:    type_str = "UINT128";    break;
        case CV_BasicType_CHAR16:     type_str = "CHAR16";     break;
        case CV_BasicType_CHAR32:     type_str = "CHAR32";     break;
        case CV_BasicType_CHAR8:      type_str = "CHAR8";      break;
        case CV_BasicType_PTR:        type_str = "PTR";        break;
      }
      
      U64 ptr = CV_BasicPointerKindFromTypeId(itype);
      char *ptr_str = "";
      switch (ptr) {
        case 0x1: ptr_str = "P";    break;
        case 0x2: ptr_str = "PF";   break;
        case 0x3: ptr_str = "PH";   break;
        case 0x4: ptr_str = "32P";  break;
        case 0x5: ptr_str = "32PF"; break;
        case 0x6: ptr_str = "64P";  break;
      }
      
      n = upper_from_str8(scratch.arena, n);
      result = push_str8f(arena, "T_%s%s(%x)", ptr_str, type_str, itype);
      scratch_end(scratch);
    } else {
      result = push_str8f(arena, "%x", itype);
    }
  } else {
    result = push_str8f(arena, "%x", itype);
  }
  return result;
}

internal String8
cv_string_from_itemid(Arena *arena, CV_ItemId itemid)
{
  String8 result = push_str8f(arena, "%x", itemid);
  return result;
}

internal String8
cv_string_from_symbol_type(Arena *arena, CV_SymKind symbol_type)
{
  String8 str    = cv_string_from_sym_kind(symbol_type);
  String8 result = push_str8f(arena, "S_%S", str);
  return result;
}

internal String8
cv_string_from_symbol_kind(Arena *arena, CV_SymKind kind)
{
  String8 str    = cv_string_from_sym_kind(kind);
  String8 result = push_str8f(arena, "S_%S", str);
  return result;
}

internal String8
cv_string_from_leaf_name(Arena *arena, U32 leaf_type)
{
  String8 str    = cv_string_from_leaf_kind(leaf_type);
  String8 result = push_str8f(arena, "LF_%S", str);
  return result;
}

internal String8 
cv_string_sec_off(Arena *arena, U32 sec, U32 off)
{
  return push_str8f(arena, "%04x:%08x", sec, off);
}

////////////////////////////////

internal CV_SymKind
cv_final_symbol_kind(CV_SymKind kind)
{
  switch (kind) {
  case CV_SymKind_PROC_ID_END:    return CV_SymKind_END;
  case CV_SymKind_LPROC32_ID:     return CV_SymKind_LPROC32;
  case CV_SymKind_GPROC32_ID:     return CV_SymKind_GPROC32;
  case CV_SymKind_LPROC32_DPC_ID: return CV_SymKind_LPROC32_DPC;
  case CV_SymKind_LPROCMIPS_ID:   return CV_SymKind_LPROCMIPS;
  case CV_SymKind_GPROCMIPS_ID:   return CV_SymKind_GPROCMIPS;
  case CV_SymKind_LPROCIA64_ID:   return CV_SymKind_LPROCIA64;
  case CV_SymKind_GPROCIA64_ID:   return CV_SymKind_GPROCIA64;
  }
  return kind;
}

