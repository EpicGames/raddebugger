// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
// Helpers

internal U64
cv_hash_from_item_id(CV_ItemId item_id)
{
  return u64_hash_from_str8(str8_struct(&item_id));
}

internal CV_C13SubSectionKind
cv_c13_sub_section_kind_from_idx(CV_C13SubSectionIdxKind idx)
{
  switch (idx) {
#define X(n,c) case CV_C13SubSectionIdxKind_##n: return c;
    CV_C13SubSectionKindXList(X)
#undef X
  }
  return 0;
}

internal CV_C13SubSectionIdxKind
cv_c13_sub_section_idx_from_kind(CV_C13SubSectionKind kind)
{
  switch (kind) {
#define X(n,c) case CV_C13SubSectionKind_##n: return CV_C13SubSectionIdxKind_##n;
    CV_C13SubSectionKindXList(X)
#undef X
  }
  return CV_C13SubSectionIdxKind_NULL;
}

////////////////////////////////
// Parsers

internal CV_Numeric
cv_numeric_from_data_range(U8 *first, U8 *opl)
{
  CV_Numeric result = {0};
  if (first <= opl && (U64)(opl - first) >= 2) {
    U16 x = memory_read16(first);
    if (x < 0x8000) {
      result.kind         = CV_NumericKind_USHORT;
      result.val          = first;
      result.encoded_size = 2;
    } else {
      U64 val_size = 0;
      switch (x) {
      case CV_NumericKind_CHAR:       val_size = 1; break;
      case CV_NumericKind_SHORT:
      case CV_NumericKind_USHORT:     val_size = 2; break;
      case CV_NumericKind_LONG:
      case CV_NumericKind_ULONG:      val_size = 4; break;
      case CV_NumericKind_FLOAT32:    val_size = 4; break;
      case CV_NumericKind_FLOAT64:    val_size = 8; break;
      case CV_NumericKind_FLOAT80:    val_size = 10; break;
      case CV_NumericKind_FLOAT128:   val_size = 16; break;
      case CV_NumericKind_QUADWORD:
      case CV_NumericKind_UQUADWORD:  val_size = 8; break;
      case CV_NumericKind_FLOAT48:    val_size = 6; break;
      case CV_NumericKind_COMPLEX32:  val_size = 8; break;
      case CV_NumericKind_COMPLEX64:  val_size = 16; break;
      case CV_NumericKind_COMPLEX80:  val_size = 20; break;
      case CV_NumericKind_COMPLEX128: val_size = 32; break;
      case CV_NumericKind_VARSTRING:  val_size = 0; break; // TODO: ???
      case CV_NumericKind_OCTWORD:
      case CV_NumericKind_UOCTWORD:   val_size = 16; break;
      case CV_NumericKind_DECIMAL:    val_size = 0; break; // TODO: ???
      case CV_NumericKind_DATE:       val_size = 0; break; // TODO: ???
      case CV_NumericKind_UTF8STRING: val_size = 0; break; // TODO: ???
      case CV_NumericKind_FLOAT16:    val_size = 2; break;
      }
      if (val_size != 0 && val_size <= (U64)(opl - first) - 2) {
        result.kind         = x;
        result.val          = (first + 2);
        result.encoded_size = 2 + val_size;
      }
    }
  }
  return result;
}

internal U64
cv_read_numeric(String8 data, U64 offset, CV_Numeric *out)
{
  MemoryZeroStruct(out);
  if (offset >= data.size) { return 0; }
  *out = cv_numeric_from_data_range(data.str + offset, data.str + data.size);
  return out->encoded_size;
}

internal B32
cv_numeric_fits_in_u64(CV_Numeric *num)
{
  B32 result = 0;
  switch (num->kind) {
  case CV_NumericKind_USHORT:
  case CV_NumericKind_ULONG:
  case CV_NumericKind_UQUADWORD: {
    result = 1;
  } break;
  }
  return result;
}

internal B32
cv_numeric_fits_in_s64(CV_Numeric *num)
{
  B32 result = 0;
  switch (num->kind) {
  case CV_NumericKind_CHAR:
  case CV_NumericKind_SHORT:
  case CV_NumericKind_LONG:
  case CV_NumericKind_QUADWORD: {
    result = 1;
  } break;
  }
  return result;
}

internal B32
cv_numeric_fits_in_f64(CV_Numeric *num)
{
  B32 result = 0;
  switch (num->kind) {
  case CV_NumericKind_FLOAT32:
  case CV_NumericKind_FLOAT64: {
    result = 1;
  } break;
  }
  return result;
}

internal U64
cv_u64_from_numeric(CV_Numeric *num)
{
  U64 result = 0;
  switch (num->kind) {
  case CV_NumericKind_CHAR:      { result = (U64)(S64)*(S8 *)num->val; } break;
  case CV_NumericKind_SHORT:     { result = (U64)(S64)*(S16 *)num->val; } break;
  case CV_NumericKind_LONG:      { result = (U64)(S64)*(S32 *)num->val; } break;
  case CV_NumericKind_QUADWORD:  { result = (U64)(S64)*(S64 *)num->val; } break;
  case CV_NumericKind_USHORT:    { result = *(U16 *)num->val; } break;
  case CV_NumericKind_ULONG:     { result = *(U32 *)num->val; } break;
  case CV_NumericKind_UQUADWORD: { result = *(U64 *)num->val; } break;
  }
  return result;
}

internal S64
cv_s64_from_numeric(CV_Numeric *num)
{
  S64 result = 0;
  switch (num->kind) {
  case CV_NumericKind_CHAR:     { result = *(S8 *)num->val; } break;
  case CV_NumericKind_SHORT:    { result = *(S16 *)num->val; } break;
  case CV_NumericKind_LONG:     { result = *(S32 *)num->val; } break;
  case CV_NumericKind_QUADWORD: { result = *(S64 *)num->val; } break;
  }
  return (result);
}

internal F64
cv_f64_from_numeric(CV_Numeric *num)
{
  F64 result = 0;
  switch (num->kind) {
  case CV_NumericKind_FLOAT32: { result = *(F32 *)num->val; } break;
  case CV_NumericKind_FLOAT64: { result = *(F64 *)num->val; } break;
  }
  return (result);
}

internal B32
cv_read_record(String8 data, U64 offset, U64 align, CV_Record *out)
{
  MemoryZeroStruct(out);

  if (offset > data.size || sizeof(CV_RecHeader) > data.size - offset) {
    return 0;
  }

  CV_RecHeader header;
  MemoryCopy(&header, data.str + offset, sizeof(header));
  if (header.size < 2) {
    return 0;
  }

  U64 size = 2 + (U64)header.size;
  U64 pad  = AlignPadPow2(size, align);
  if (size > data.size - offset || pad > data.size - offset - size) {
    return 0;
  }

  out->header   = header;
  out->data     = str8(data.str + offset + sizeof(header), header.size - 2);
  out->consumed = size + pad;

  return 1;
}

internal U64
cv_read_leaf(String8 data, U64 offset, U64 align, CV_Leaf *out)
{
  MemoryZeroStruct(out);
  CV_Record record;
  if (!cv_read_record(data, offset, align, &record)) { return 0; }
  out->kind = record.header.kind;
  out->data = record.data;
  return record.consumed;
}

internal CV_Leaf
cv_leaf_from_string(String8 data)
{
  CV_Leaf result = {0};
  cv_read_leaf(data, 0, 1, &result);
  return result;
}

internal U64
cv_leaf_size_from_ptr(U8 *ptr)
{
  U16 size;
  MemoryCopy(&size, ptr, sizeof(size));
  return size + sizeof(size);
}

internal String8
cv_raw_leaf_from_ptr(U8 *ptr)
{
  U16 size;
  MemoryCopy(&size, ptr, sizeof(size));
  return str8(ptr, (U64)size + sizeof(size));
}

internal CV_Leaf
cv_leaf_from_ptr(U8 *ptr)
{
  return cv_leaf_from_string(cv_raw_leaf_from_ptr(ptr));
}

internal U64
cv_read_symbol(String8 data, U64 offset, U64 align, CV_Symbol *out)
{
  MemoryZeroStruct(out);
  CV_Record record;
  if (!cv_read_record(data, offset, align, &record)) { return 0; }
  out->kind   = record.header.kind;
  out->offset = offset;
  out->data   = record.data;
  return record.consumed;
}

internal CV_Symbol
cv_symbol_from_string(String8 data)
{
  CV_Symbol result = {0};
  cv_read_symbol(data, 0, 1, &result);
  return result;
}

internal String8
cv_raw_from_symbol(void *ptr)
{
  U16 size;
  MemoryCopy(&size, ptr, sizeof(size));
  return str8(ptr, (U64)size + sizeof(size));
}

internal CV_Symbol
cv_symbol_from_ptr(U8 *ptr)
{
  return cv_symbol_from_string(cv_raw_from_symbol(ptr));
}

internal CV_Signature
cv_c13_stream_signature_from_data(String8 data)
{
  CV_Signature sig = 0;
  if (data.size >= sizeof(sig)) { MemoryCopy(&sig, data.str, sizeof(sig)); }
  return sig;
}

internal B32
cv_c13_sub_section_from_data(String8 data, U64 offset, CV_C13SubSection *out)
{
  MemoryZeroStruct(out);

  if (offset > data.size || sizeof(CV_C13SubSectionHeader) > data.size - offset) {
    out->truncated = offset != data.size;
    return 0;
  }

  CV_C13SubSectionHeader header;
  MemoryCopy(&header, data.str + offset, sizeof(header));

  U64 data_offset = offset + sizeof(header);
  U64 size        = Min(header.size, data.size - data_offset);
  U64 end         = data_offset + size;
  U64 pad         = AlignPadPow2(end, CV_C13SubSectionAlign);

  out->kind          = header.kind;
  out->declared_size = header.size;
  out->data_offset   = data_offset;
  out->data          = str8_substr(data, r1u64s(data_offset, size));
  out->next_offset   = pad <= data.size - end ? end + pad : data.size;
  out->truncated     = size != header.size;

  return 1;
}

internal B32
cv_c13_checksum_from_data(String8 data, U64 offset, CV_C13Checksum *header, String8 *value, U64 *next_offset)
{
  MemoryZeroStruct(header);
  *value = str8_zero();

  if (offset > data.size || sizeof(*header) > data.size - offset) { return 0; }

  CV_C13Checksum h;
  MemoryCopy(&h, data.str + offset, sizeof(h));

  U64 start = offset + sizeof(h);
  if (h.len > data.size - start) { return 0; }

  U64 end = start + h.len;
  U64 pad = AlignPadPow2(end, 4);

  *header = h;
  *value  = str8(data.str + start, h.len);

  if (next_offset) {
    *next_offset = end + Min(pad, data.size - end);
  }
  return 1;
}

internal CV_C13LinesIter
cv_c13_lines_iter_init(String8 data)
{
  CV_C13LinesIter it = { .data = data, .cursor = sizeof(CV_C13SubSecLinesHeader) };
  if (data.size < sizeof(it.header)) {
    it.malformed = 1;
  } else {
    MemoryCopy(&it.header, data.str, sizeof(it.header));
  }
  return it;
}

internal B32
cv_c13_lines_iter_next(CV_C13LinesIter *it)
{
  if (it->malformed || it->cursor == it->data.size) { return 0; }

  U64 off = it->cursor;
  if (off > it->data.size || sizeof(CV_C13File) > it->data.size - off) { goto malformed; }

  CV_C13File file;
  MemoryCopy(&file, it->data.str + off, sizeof(file));
  if (file.block_size < sizeof(file) || file.block_size > it->data.size - off) { goto malformed; }

  B32 columns = !!(it->header.flags & CV_C13SubSecLinesFlag_HasColumns);
  U64 stride  = sizeof(CV_C13Line) + (columns ? sizeof(CV_C13Column) : 0);
  if (file.num_lines > (file.block_size - sizeof(file)) / stride) { goto malformed; }

  it->file           = file;
  it->file_offset    = off;
  it->line_array_off = off + sizeof(file);
  it->col_array_off  = it->line_array_off + (U64)file.num_lines * sizeof(CV_C13Line);
  it->col_count      = columns ? file.num_lines : 0;
  it->cursor         = off + file.block_size;
  return 1;

malformed:
  it->malformed = 1;
  return 0;
}

internal CV_C13InlineeIter
cv_c13_inlinee_iter_init(String8 data)
{
  CV_C13InlineeIter it = { .data = data, .cursor = sizeof(CV_C13InlineeLinesSig) };
  if (data.size < sizeof(it.signature)) {
    it.malformed = 1;
  } else {
    MemoryCopy(&it.signature, data.str, sizeof(it.signature));
    it.malformed = it.signature != CV_C13InlineeLinesSig_NORMAL && it.signature != CV_C13InlineeLinesSig_EXTRA_FILES;
  }
  return it;
}

internal B32
cv_c13_inlinee_iter_next(CV_C13InlineeIter *it)
{
  if (it->malformed || it->cursor == it->data.size) { return 0; }

  U64 off = it->cursor;
  if (off > it->data.size || sizeof(CV_C13InlineeSourceLineHeader) > it->data.size - off) { goto malformed; }

  CV_C13InlineeSourceLineHeader header;
  MemoryCopy(&header, it->data.str + off, sizeof(header));

  U64 cursor = off + sizeof(header);

  CV_C13InlineeLinesParsed v = {
    .inlinee         = header.inlinee,
    .file_off        = header.file_off,
    .first_source_ln = header.first_source_ln
  };

  if (it->signature == CV_C13InlineeLinesSig_EXTRA_FILES) {
    if (sizeof(U32) > it->data.size - cursor) { goto malformed; }

    MemoryCopy(&v.extra_file_count, it->data.str + cursor, sizeof(U32));
    cursor += sizeof(U32);

    if (v.extra_file_count > (it->data.size - cursor) / sizeof(U32)) { goto malformed; }

    v.extra_files = (U32 *)(it->data.str + cursor);
    cursor += (U64)v.extra_file_count * sizeof(U32);
  }

  it->v             = v;
  it->record_offset = off;
  it->cursor        = cursor;
  return 1;

malformed:
  it->malformed = 1;
  return 0;
}

////////////////////////////////
// Iterators

internal CV_RecIter
cv_record_iter_init_ex(String8 data, CV_RecordKind kind, U64 align)
{
  CV_RecIter iter = { .data = data, .kind = kind, .align = align };
  return iter;
}

internal CV_RecIter
cv_record_iter_init(String8 data, CV_RecordKind kind)
{
  U64 align = kind == CV_RecordKind_Leaf ? CV_LeafAlign : CV_SymbolAlign;
  return cv_record_iter_init_ex(data, kind, align);
}

internal B32
cv_record_iter_next(CV_RecIter *iter, CV_Record *record_out)
{
  MemoryZeroStruct(record_out);

  while (iter->cursor < iter->data.size) {
    CV_Record record;
    if ( ! cv_read_record(iter->data, iter->cursor, iter->align, &record)) {
      iter->cursor = max_U64;
      return 0;
    }

    iter->cursor += record.consumed;

    U64 header_struct_size = iter->kind == CV_RecordKind_Leaf
                              ? cv_header_struct_size_from_leaf_kind(record.header.kind)
                              : cv_header_struct_size_from_sym_kind(record.header.kind);

    if (header_struct_size <= record.data.size) {
      *record_out = record;
      return 1;
    }
  }
  return 0;
}

internal B32
cv_field_next(String8 data, CV_FieldIter *iter)
{
  U64 start = iter->next_offset;
  if (iter->malformed || start == data.size) {
    return 0;
  }

  CV_FieldIter field = {0};
  if (start > data.size || data.size - start < 2) {
    goto malformed;
  }

  field.kind = memory_read16(data.str + start);

  U64 cursor             = start + 2;
  U64 fixed_size         = 0;
  B32 has_name           = 0;
  B32 has_virtual_offset = 0;

#define field_type(T, member) field.type_offsets[field.type_offset_count++] = cursor + OffsetOf(T, member)
#define field_fixed(T) fixed_size = sizeof(T)
  switch (field.kind) {
  default: goto malformed;
  case CV_LeafKind_INDEX: {
    field_fixed(CV_LeafIndex);
    field_type(CV_LeafIndex, itype);
  } break;
  case CV_LeafKind_MEMBER: {
    field_fixed(CV_LeafMember);
    field_type(CV_LeafMember, itype);
    field.numeric_count = 1;
    has_name            = 1;
  } break;
  case CV_LeafKind_STMEMBER: {
    field_fixed(CV_LeafStMember);
    field_type(CV_LeafStMember, itype);
    has_name = 1;
  } break;
  case CV_LeafKind_METHOD: {
    field_fixed(CV_LeafMethod);
    field_type(CV_LeafMethod, list_itype);
    has_name = 1;
  } break;
  case CV_LeafKind_ONEMETHOD: {
    field_fixed(CV_LeafOneMethod);
    field_type(CV_LeafOneMethod, itype);
    has_name           = 1;
    has_virtual_offset = 1;
  } break;
  case CV_LeafKind_ENUMERATE: {
    field_fixed(CV_LeafEnumerate);
    field.numeric_count = 1;
    has_name            = 1;
  } break;
  case CV_LeafKind_NESTTYPE: {
    field_fixed(CV_LeafNestType);
    field_type(CV_LeafNestType, itype);
    has_name = 1;
  } break;
  case CV_LeafKind_NESTTYPEEX: {
    field_fixed(CV_LeafNestTypeEx);
    field_type(CV_LeafNestTypeEx, itype);
    has_name = 1;
  } break;
  case CV_LeafKind_BCLASS: {
    field_fixed(CV_LeafBClass);
    field_type(CV_LeafBClass, itype);
    field.numeric_count = 1;
  } break;
  case CV_LeafKind_BINTERFACE: {
    field_fixed(CV_LeafBInterface);
    field_type(CV_LeafBInterface, itype);
    field.numeric_count = 1;
  } break;
  case CV_LeafKind_VBCLASS:
  case CV_LeafKind_IVBCLASS: {
    field_fixed(CV_LeafVBClass);
    field_type(CV_LeafVBClass, itype);
    field_type(CV_LeafVBClass, vbptr_itype);
    field.numeric_count = 2;
  } break;
  case CV_LeafKind_VFUNCTAB: {
    field_fixed(CV_LeafVFuncTab);
    field_type(CV_LeafVFuncTab, itype);
  } break;
  case CV_LeafKind_VFUNCOFF: {
    field_fixed(CV_LeafVFuncOff);
    field_type(CV_LeafVFuncOff, itype);
  } break;
  case CV_LeafKind_FRIENDFCN: {
    field_fixed(CV_LeafFriendFcn);
    field_type(CV_LeafFriendFcn, itype);
    has_name = 1;
  } break;
  case CV_LeafKind_FRIENDCLS: {
    field_fixed(CV_LeafFriendCls);
    field_type(CV_LeafFriendCls, itype);
  } break;
  case CV_LeafKind_MEMBERMODIFY: {
    field_fixed(CV_LeafMemberModify);
    field_type(CV_LeafMemberModify, itype);
    has_name = 1;
  } break;
  }
#undef field_type
#undef field_fixed

  if (fixed_size > data.size - cursor) { goto malformed; }

  if (has_virtual_offset) {
    CV_MethodProp prop = CV_FieldAttribs_Extract_MethodProp(memory_read16(data.str + cursor));
    has_virtual_offset = prop == CV_MethodProp_Intro || prop == CV_MethodProp_PureIntro;
  }

  cursor += fixed_size;
  if (has_virtual_offset) {
    if (data.size - cursor < 4) { goto malformed; }
    field.virtual_offset = memory_read32(data.str + cursor);
    cursor += 4;
  }

  for EachIndex(i, field.numeric_count) {
    U64 size = cv_read_numeric(data, cursor, &field.numeric[i]);
    if (size == 0) { goto malformed; }
    cursor += size;
  }

  if (has_name) {
    if (cursor == data.size) { goto malformed; }
    field.name = str8_cstring_capped(data.str + cursor, data.str + data.size);
    if (field.name.size == data.size - cursor) { goto malformed; }
    cursor += field.name.size + 1;
  }

  field.data = str8(data.str + start + 2, cursor - start - 2);
  U64 pad = AlignPadPow2(cursor, 4);

  if (cursor != data.size && pad > data.size - cursor) { goto malformed; }

  field.next_offset = cursor == data.size ? cursor : cursor + pad;
  *iter = field;
  return 1;

malformed:
  iter->malformed = 1;
  return 0;
}

internal B32
cv_method_next(String8 data, CV_FieldIter *iter)
{
  U64 start = iter->next_offset;

  if (iter->malformed || start == data.size) { return 0; }
  if (start > data.size || sizeof(CV_LeafMethodListMember) > data.size - start) { goto malformed; }

  CV_LeafMethodListMember method;
  MemoryCopy(&method, data.str + start, sizeof(method));

  CV_MethodProp prop               = CV_FieldAttribs_Extract_MethodProp(method.attribs);
  U64           size               = sizeof(method);
  B32           has_virtual_offset = prop == CV_MethodProp_Intro || prop == CV_MethodProp_PureIntro;

  if (has_virtual_offset) { size += 4; }
  if (size > data.size - start) { goto malformed; }

  MemoryZeroStruct(iter);
  iter->kind              = CV_LeafKind_METHODLIST;
  iter->data              = str8(data.str + start, size);
  iter->next_offset       = start + size;
  iter->type_offset_count = 1;
  iter->type_offsets[0]   = start + OffsetOf(CV_LeafMethodListMember, itype);

  if (has_virtual_offset) {
    iter->virtual_offset = memory_read32(data.str + start + sizeof(method));
  }
  return 1;

malformed:
  iter->malformed = 1;
  return 0;
}

internal CV_C13StreamIter
cv_c13_stream_iter_init(CV_C13Stream *stream)
{
  return (CV_C13StreamIter){ .stream = stream, .next_block = stream->data.first, .index = cv_c13_stream_index(stream) };
}

internal B32
cv_c13_stream_iter_next_block(CV_C13StreamIter *it)
{
  if (it->malformed) { return 0; }
  it->block        = str8_zero();
  it->cursor       = 0;
  it->cached_block = 0;
  if (it->next_block == 0) { return 0; }

  it->block      = it->next_block->string;
  it->next_block = it->next_block->next;
  if (it->index && it->block_idx < it->index->block_count) {
    CV_C13StreamBlockIndex block = it->index->blocks[it->block_idx];
    if (block.subsection_base_idx != max_U32) {
      it->cached_block   = 1;
      it->subsection_idx = block.subsection_base_idx;
      it->subsection_opl = it->subsection_idx + block.subsection_count;
    }
  }
  it->block_idx += 1;
  return 1;
}

internal B32
cv_c13_stream_iter_next_raw(CV_C13StreamIter *it)
{
  if (it->malformed) { return 0; }

  if (it->cached_block) {
    if (it->subsection_idx > it->subsection_opl ||
        it->subsection_opl > it->index->subsections.size / sizeof(CV_C13StreamSubsection)) {
      it->malformed = 1;
      return 0;
    }

    if (it->subsection_idx == it->subsection_opl) {
      it->malformed = it->cursor != it->block.size;
      return 0;
    }

    CV_C13StreamSubsection subsection;
    MemoryCopy(&subsection, it->index->subsections.str + it->subsection_idx * sizeof(subsection), sizeof(subsection));

    if (subsection.data_offset != it->cursor + sizeof(CV_C13SubSectionHeader) ||
        subsection.data_offset > it->block.size ||
        subsection.data_size > it->block.size - subsection.data_offset) {
      it->malformed = 1;
      return 0;
    }

    it->subsection_idx += 1;
    it->kind            = subsection.kind;
    it->data_offset     = subsection.data_offset;
    it->data            = str8(it->block.str + subsection.data_offset, subsection.data_size);
    it->cursor          = Min(AlignPow2((U64)subsection.data_offset + subsection.data_size, CV_C13SubSectionAlign), it->block.size);
    return 1;
  }

  CV_C13SubSection subsection;
  if ( ! cv_c13_sub_section_from_data(it->block, it->cursor, &subsection)) {
    it->malformed = subsection.truncated;
    return 0;
  }

  if (subsection.truncated) {
    it->malformed  = 1;
    return 0;
  }

  it->cursor      = subsection.next_offset;
  it->kind        = subsection.kind;
  it->data        = subsection.data;
  it->data_offset = subsection.data_offset;
  return 1;
}

internal B32
cv_c13_stream_iter_next(CV_C13StreamIter *it)
{
  while (cv_c13_stream_iter_next_raw(it)) {
    if (it->kind & CV_C13SubSectionKind_IgnoreFlag) { continue; }
    U32 idx = cv_c13_sub_section_idx_from_kind(it->kind);
    if (it->stream->ignored_kinds & (1u << idx)) { continue; }
    return 1;
  }
  return 0;
}

internal B32
cv_c13_stream_iter_next_kind(CV_C13StreamIter *it, CV_C13SubSectionKind kind)
{
  for (;;) {
    while (cv_c13_stream_iter_next(it)) {
      if (it->kind == kind) { return 1; }
    }
    if (!cv_c13_stream_iter_next_block(it)) { return 0; }
  }
}

internal void
cv_sym_top_level_info_from_symbol(Arena *arena, CV_Symbol symbol, CV_SymTopLevelInfo *info)
{
  U8 *first = symbol.data.str;
  U64 cap   = symbol.data.size;
  switch (symbol.kind) {
  case CV_SymKind_COMPILE:
    if (sizeof(CV_SymCompile) <= cap) {
      CV_SymCompile *compile = (CV_SymCompile *)first;
      String8        ver_str = cv_name_from_symbol(symbol.kind, symbol.data);
      info->arch             = compile->machine;
      info->language         = CV_CompileFlags_Extract_Language(compile->flags);
      info->compiler_name    = ver_str;
    } break;
  case CV_SymKind_COMPILE2:
    if (sizeof(CV_SymCompile2) <= cap) {
      CV_SymCompile2 *compile2      = (CV_SymCompile2 *)first;
      String8         ver_str       = cv_name_from_symbol(symbol.kind, symbol.data);
      String8         compiler_name = push_str8f(arena, "%.*s %u.%u.%u",
                                                str8_varg(ver_str),
                                                compile2->ver_major,
                                                compile2->ver_minor,
                                                compile2->ver_build);
      info->arch          = compile2->machine;
      info->language      = CV_Compile2Flags_Extract_Language(compile2->flags);
      info->compiler_name = compiler_name;
    } break;
  case CV_SymKind_COMPILE3:
    if (sizeof(CV_SymCompile3) <= cap) {
      CV_SymCompile3 *compile3      = (CV_SymCompile3 *)first;
      String8         ver_str       = cv_name_from_symbol(symbol.kind, symbol.data);
      String8         compiler_name = push_str8f(arena, "%.*s %u.%u.%u",
                                                str8_varg(ver_str),
                                                compile3->ver_major,
                                                compile3->ver_minor,
                                                compile3->ver_build);
      info->arch          = compile3->machine;
      info->language      = CV_Compile3Flags_Extract_Language(compile3->flags);
      info->compiler_name = compiler_name;
    } break;
  }
}


internal CV_C13InlineSiteDecoder
cv_c13_inline_site_decoder_init(U32 file_off, U32 first_source_ln, U64 parent_voff)
{
  CV_C13InlineSiteDecoder decoder = {0};
  decoder.parent_voff             = parent_voff;
  decoder.code_offset_base        = parent_voff;
  decoder.code_chunk_changed      = 1;
  decoder.file_off                = file_off;
  decoder.ln                      = (S32)first_source_ln;
  decoder.cn                      = 1;
  decoder.ln_changed              = 1;
  return decoder;
}

internal CV_C13InlineSiteDecoderStep
cv_c13_inline_site_decoder_step(CV_C13InlineSiteDecoder *decoder, String8 binary_annots)
{
  CV_C13InlineSiteDecoderStep result = {0};

#define read_annot(kind, dst) do {                                                      \
  U64 read_size = cv_decode_inline_annot_##kind(binary_annots, decoder->cursor, (dst)); \
  if (read_size == 0) { goto bad_annotation; }                                          \
  decoder->cursor += read_size;                                                         \
} while (0)

  for (; decoder->cursor < binary_annots.size && result.flags == 0; ) {
    U32 op = CV_InlineBinaryAnnotation_Null;
    read_annot(u32, &op);

    switch (op) {
    case CV_InlineBinaryAnnotation_Null: {
      decoder->cursor              = binary_annots.size;
      // this is last run, append range with left over code bytes
      decoder->code_length         = decoder->code_offset - decoder->code_offset_lo;
      decoder->code_length_changed = 1;
    } break;
    case CV_InlineBinaryAnnotation_CodeOffset: {
      read_annot(u32, &decoder->code_offset);
      decoder->code_offset_lo         = decoder->code_offset;
      decoder->code_offset_lo_changed = 1;
      decoder->code_offset_changed    = 1;
    } break;
    case CV_InlineBinaryAnnotation_ChangeCodeOffsetBase: {
      // MSVC PGO starts new offsets in the selected chunk; source state
      // (file, line and column) survives the switch
      U32 chunk_idx = 0;
      read_annot(u32, &chunk_idx);
      if (chunk_idx > decoder->separated_code_voffs.count) { goto bad_annotation; }
      U64 base = decoder->parent_voff;
      if (chunk_idx != 0) {
        if (decoder->separated_code_voffs.v == 0) { goto bad_annotation; }
        base = decoder->separated_code_voffs.v[chunk_idx - 1];
      }
      if (base == max_U64) { goto bad_annotation; }
      decoder->code_offset_base       = base;
      decoder->code_offset            = 0;
      decoder->code_offset_lo         = 0;
      decoder->code_length            = 0;
      decoder->code_offset_changed    = 0;
      decoder->code_offset_lo_changed = 0;
      decoder->code_length_changed    = 0;
      decoder->code_chunk_changed     = 1;
      decoder->ln_changed             = 1;
      decoder->file_off_changed       = 1; // end the previous line sequence even if the file hasn't changed
    } break;
    case CV_InlineBinaryAnnotation_ChangeCodeOffset: {
      U32 delta = 0;
      read_annot(u32, &delta);

      decoder->code_offset += delta;

      if (!decoder->code_offset_lo_changed) {
        decoder->code_offset_lo         = decoder->code_offset;
        decoder->code_offset_lo_changed = 1;
      }
      decoder->code_offset_changed = 1;
    } break;
    case CV_InlineBinaryAnnotation_ChangeCodeLength: {
      decoder->code_length = 0;
      read_annot(u32, &decoder->code_length);
      decoder->code_length_changed = 1;
    } break;
    case CV_InlineBinaryAnnotation_ChangeFile: {
      U32 old_file_off = decoder->file_off;
      read_annot(u32, &decoder->file_off);
      decoder->file_off_changed = old_file_off != decoder->file_off;
      // compiler isn't obligated to terminate code sequence before chaning files,
      // so we have to always force emit code range on file change
      decoder->code_length_changed = decoder->file_off_changed;
    } break;
    case CV_InlineBinaryAnnotation_ChangeLineOffset: {
      S32 delta = 0;
      read_annot(s32, &delta);

      decoder->ln         += delta;
      decoder->ln_changed  = 1;
    } break;
    case CV_InlineBinaryAnnotation_ChangeLineEndDelta: {
      goto bad_annotation;
      // S32 end_delta = 1;
      // decoder->cursor += cv_decode_inline_annot_s32(binary_annots, decoder->cursor, &end_delta);
      // decoder->ln += end_delta;
    } break;
    case CV_InlineBinaryAnnotation_ChangeRangeKind: {
      goto bad_annotation;
      // decoder->cursor += cv_decode_inline_annot_u32(binary_annots, decoder->cursor, &range_kind);
    } break;
    case CV_InlineBinaryAnnotation_ChangeColumnStart: {
      goto bad_annotation;
      // S32 delta;
      // decoder->cursor += cv_decode_inline_annot_s32(binary_annots, decoder->cursor, &delta);
      // decoder->cn += delta;
    } break;
    case CV_InlineBinaryAnnotation_ChangeColumnEndDelta: {
      goto bad_annotation;
      // S32 end_delta;
      // decoder->cursor += cv_decode_inline_annot_s32(binary_annots, decoder->cursor, &end_delta);
      // decoder->cn += end_delta;
    } break;
    case CV_InlineBinaryAnnotation_ChangeCodeOffsetAndLineOffset: {
      U32 code_offset_and_line_offset = 0;
      read_annot(u32, &code_offset_and_line_offset);

      S32 line_delta = cv_inline_annot_signed_from_unsigned_operand(code_offset_and_line_offset >> 4);
      U32 code_delta = (code_offset_and_line_offset & 0xf);

      decoder->code_offset += code_delta;
      decoder->ln          += line_delta;

      if (!decoder->code_offset_lo_changed) {
        decoder->code_offset_lo         = decoder->code_offset;
        decoder->code_offset_lo_changed = 1;
      }

      decoder->code_offset_changed = 1;
      decoder->ln_changed          = 1;
    } break;
    case CV_InlineBinaryAnnotation_ChangeCodeLengthAndCodeOffset: {
      U32 offset_delta = 0;
      read_annot(u32, &decoder->code_length);
      read_annot(u32, &offset_delta);

      decoder->code_offset += offset_delta;

      if (!decoder->code_offset_lo_changed) {
        decoder->code_offset_lo         = decoder->code_offset;
        decoder->code_offset_lo_changed = 1;
      }

      decoder->code_offset_changed = 1;
      decoder->code_length_changed = 1;
    } break;
    case CV_InlineBinaryAnnotation_ChangeColumnEnd: {
      goto bad_annotation;
      // U32 column_end = 0;
      // decoder->cursor += cv_decode_inline_annot_u32(binary_annots, decoder->cursor, &column_end);
    } break;
    default: goto bad_annotation;
    }

    U64 line_code_offset = decoder->code_offset;

    if (decoder->code_length_changed) {
      // compute upper bound of the range
      U64 code_offset_hi = (U64)decoder->code_offset + decoder->code_length;
      if (code_offset_hi > max_U32 || code_offset_hi > max_U64 - decoder->code_offset_base) { goto bad_annotation; }

      // can last code range be extended to cover current sequence too?
      if (!decoder->code_chunk_changed && decoder->last_range.max == decoder->code_offset_base + decoder->code_offset_lo) {
        decoder->last_range.max = decoder->code_offset_base + code_offset_hi;

        result.flags |= CV_C13InlineSiteDecoderStepFlag_ExtendLastRange;
        result.range  = decoder->last_range;
      } else {
        decoder->last_range      = rng_1u64(decoder->code_offset_base + decoder->code_offset_lo, decoder->code_offset_base + code_offset_hi);
        decoder->file_last_range = decoder->last_range;

        result.flags |= CV_C13InlineSiteDecoderStepFlag_EmitRange;
        result.range  = decoder->last_range;
      }

      // update state
      decoder->code_offset_lo         = code_offset_hi;
      decoder->code_offset           += decoder->code_length;
      decoder->code_offset_lo_changed = 0;
      decoder->code_length_changed    = 0;
      decoder->code_length            = 0;
      decoder->code_chunk_changed     = 0;
    }

    if (decoder->file_off_changed || (decoder->file_count == 0)) {
      result.flags    |= CV_C13InlineSiteDecoderStepFlag_EmitFile;
      result.file_off  = decoder->file_off;

      // update state
      decoder->file_last_range   = decoder->last_range;
      decoder->file_off_changed  = 0;
      decoder->file_count       += 1;
      decoder->file_line_count   = 0;
    }

    if (decoder->code_offset_changed && decoder->ln_changed) {
      if (decoder->file_line_count == 0 || decoder->file_last_ln != decoder->ln) {
        result.flags |= CV_C13InlineSiteDecoderStepFlag_EmitLine;
        result.ln     = decoder->ln;
        result.cn     = decoder->cn;
        if (line_code_offset > max_U64 - decoder->code_offset_base) { goto bad_annotation; }
        result.line_voff     = decoder->code_offset_base + line_code_offset;
        result.line_voff_end = decoder->last_range.max;

        // update state
        decoder->file_line_count += 1;
        decoder->file_last_ln     = decoder->ln;
      }

      // update state
      decoder->code_offset_changed = 0;
      decoder->ln_changed          = 0;
    }
  }

  return result;

bad_annotation:
  decoder->error  = 1;
  decoder->cursor = binary_annots.size;
  MemoryZeroStruct(&result);
  result.flags = CV_C13InlineSiteDecoderStepFlag_Error;
  return result;
#undef read_annot
}

////////////////////////////////
// Stream Builders

internal CV_SymbolStream *
cv_symbol_stream_from_data(Arena *arena, String8 sym_data, U64 sym_align)
{
  ProfBeginFunction();
  CV_SymbolStream *result = push_array(arena, CV_SymbolStream, 1);
  result->data            = sym_data;
  result->sym_align       = sym_align;

  CV_Record record;
  for (U64 cursor = 0; cv_read_record(sym_data, cursor, sym_align, &record);) {
    CV_Symbol symbol = { .kind = record.header.kind, .offset = cursor, .data = record.data };
    result->count += 1;
    cv_sym_top_level_info_from_symbol(arena, symbol, &result->info);
    cursor += record.consumed;
  }

  ProfEnd();
  return result;
}

internal B32
cv_make_leaf_stream_checked(String8 data, U64 align, U64 count, U32 *offsets, CV_LeafStream *out)
{
  MemoryZeroStruct(out);

  if (data.size > max_U32 - 2 || count > data.size / sizeof(CV_RecHeader)) {
    return 0;
  }

  *out = (CV_LeafStream){data, count, offsets, align};

  return 1;
}

internal B32
cv_leaf_stream_from_data_checked(Arena *arena, String8 data, U64 align, CV_LeafStream *out)
{
  if ( ! cv_make_leaf_stream_checked(data, align, 0, 0, out)) { return 0; }

  MemoryZeroStruct(out);

  // count leaf records
  U64 count  = 0;
  U64 cursor = 0;
  CV_Record record;
  while (cursor < data.size) {
    if ( ! cv_read_record(data, cursor, align, &record)) { return 0; }
    cursor += record.consumed;
    count  += 1;
  }

  U32 *offsets = count ? push_array_no_zero(arena, U32, count) : 0;
  cursor = 0;
  for EachIndex(idx, count) {
    offsets[idx] = (U32)cursor;
    B32 ok = cv_read_record(data, cursor, align, &record);
    Assert(ok);
    cursor += record.consumed;
  }

  *out = (CV_LeafStream){data, count, offsets, align};
  
  return 1;
}

internal B32
cv_c13_stream_from_data_c13_checked(Arena *arena, String8 data, CV_C13Stream *out)
{
  MemoryZeroStruct(out);
  CV_C13StreamIter it = { .block = data };
  while (cv_c13_stream_iter_next_raw(&it)) {}
  if (it.malformed) { return 0; }
  str8_list_push(arena, &out->data, data);
  return 1;
}

internal CV_C13Stream
cv_c13_stream_from_data_c13(Arena *arena, String8 data)
{
  CV_C13Stream result = {0};
  AssertAlways(cv_c13_stream_from_data_c13_checked(arena, data, &result));
  return result;
}

internal B32
cv_c13_stream_from_data_checked(Arena *arena, String8 data, CV_C13Stream *out)
{
  MemoryZeroStruct(out);
  if (data.size >= sizeof(CV_Signature) && cv_c13_stream_signature_from_data(data) == CV_Signature_C13) {
    return cv_c13_stream_from_data_c13_checked(arena, str8_skip(data, sizeof(CV_Signature)), out);
  }
  return 0;
}

internal CV_C13Stream
cv_c13_stream_from_data(Arena *arena, String8 data)
{
  CV_C13Stream result = {0};
  AssertAlways(cv_c13_stream_from_data_checked(arena, data, &result));
  return result;
}

internal void
cv_parse_checksum_data(Arena *arena, CV_ChecksumList *list, String8 checksum_data)
{
  for (U64 cursor = 0; cursor < checksum_data.size;) {
    CV_C13Checksum  checked_header;
    CV_C13Checksum *header = (CV_C13Checksum *)(checksum_data.str + cursor);

    String8         value;
    if (!cv_c13_checksum_from_data(checksum_data, cursor, &checked_header, &value, &cursor)) { break; }

    CV_ChecksumNode *node = push_array(arena, CV_ChecksumNode, 1);
    node->data.header     = header;
    node->data.value      = value;
    SLLQueuePush(list->first, list->last, node);
    list->count += 1;
  }
}

internal CV_ChecksumList
cv_c13_parse_checksum_data_list(Arena *arena, String8List checksum_data_list)
{
  CV_ChecksumList result = {0};
  for EachNode(n, String8Node, checksum_data_list.first) {
    cv_parse_checksum_data(arena, &result, n->string);
  }
  return result;
}

internal String8List
cv_c13_collect_source_file_names(Arena *arena, CV_ChecksumList checksum_list, String8 string_data)
{
  String8List result = {0};
  for EachNode(node, CV_ChecksumNode, checksum_list.first) {
    CV_C13Checksum header;
    MemoryCopy(&header, node->data.header, sizeof(header));

    String8 name;
    if (!cv_c13_name_from_string_table(string_data, header.name_off, &name)) { continue; }

    str8_list_push(arena, &result, name);
  }
  return result;
}

internal void
cv_c13_lines_header_list_concat_in_place(CV_C13LinesHeaderList *list, CV_C13LinesHeaderList *to_concat)
{
  if (to_concat->first) {
    if (list->last) {
      list->last->next = to_concat->first;
    } else {
      list->first = to_concat->first;
    }
    list->last   = to_concat->last;
    list->count += to_concat->count;
  }
  MemoryZeroStruct(to_concat);
}

internal CV_C13LinesHeaderList
cv_c13_lines_from_sub_sections(Arena *arena, String8 c13_data, Rng1U64 ss_range)
{
  ProfBeginFunction();
  CV_C13LinesHeaderList result        = {0};
  String8               sub_sect_data = str8_substr(c13_data, ss_range);
  for (CV_C13LinesIter it = cv_c13_lines_iter_init(sub_sect_data); cv_c13_lines_iter_next(&it);) {
    CV_C13LinesHeaderNode *node = push_array(arena, CV_C13LinesHeaderNode, 1);
    node->v.header_off          = ss_range.min;
    node->v.sec_idx             = it.header.sec;
    node->v.sec_off_lo          = it.header.sec_off;
    node->v.sec_off_hi          = (U64)it.header.sec_off + it.header.len;
    node->v.file_off            = it.file.file_off;
    node->v.line_count          = it.file.num_lines;
    node->v.col_count           = it.col_count;
    node->v.line_array_off      = ss_range.min + it.line_array_off;
    node->v.col_array_off       = ss_range.min + it.col_array_off;
    SLLQueuePush(result.first, result.last, node);
    result.count += 1;
  }
  ProfEnd();
  return result;
}

internal CV_LineArray
cv_c13_line_array_from_data(Arena *arena, String8 c13_data, U64 sec_base, CV_C13LinesHeader parsed_lines)
{
  CV_LineArray result = {0};

  result.file_off   = parsed_lines.file_off;
  result.line_count = parsed_lines.line_count;
  result.col_count  = parsed_lines.col_count;
  result.voffs      = push_array_no_zero(arena, U64, parsed_lines.line_count + 1);
  result.line_nums  = push_array_no_zero(arena, U32, parsed_lines.line_count);
  for EachIndex(line_idx, parsed_lines.line_count) {
    CV_C13Line line;
    MemoryCopy(&line, c13_data.str + parsed_lines.line_array_off + line_idx * sizeof(line), sizeof(line));
    result.voffs[line_idx]     = sec_base + parsed_lines.sec_off_lo + line.off;
    result.line_nums[line_idx] = CV_C13LineFlags_Extract_LineNumber(line.flags);
  }
  result.voffs[result.line_count] = sec_base + parsed_lines.sec_off_hi;

  if (result.col_count) {
    result.col_nums = push_array_no_zero(arena, U16, 2 * result.col_count);
    MemoryCopy(result.col_nums, c13_data.str + parsed_lines.col_array_off, result.col_count * sizeof(CV_C13Column));
  }

  return result;
}

internal CV_C13InlineeLinesParsedList
cv_c13_inlinee_lines_from_sub_sections(Arena *arena, CV_C13Stream *stream)
{
  ProfBeginFunction();

  CV_C13InlineeLinesParsedList result = {0};
  for (CV_C13StreamIter it = cv_c13_stream_iter_init(stream);
       cv_c13_stream_iter_next_kind(&it, CV_C13SubSectionKind_InlineeLines);) {
    for (CV_C13InlineeIter inlinee = cv_c13_inlinee_iter_init(it.data); cv_c13_inlinee_iter_next(&inlinee);) {
      CV_C13InlineeLinesParsedNode *node = push_array(arena, CV_C13InlineeLinesParsedNode, 1);
      node->v = inlinee.v;
      SLLQueuePush(result.first, result.last, node);
      result.count += 1;
    }
  }

  ProfEnd();
  return result;
}

internal CV_InlineBinaryAnnotsParsed
cv_c13_parse_inline_binary_annots(Arena                    *arena,
                                  U64                       parent_voff,
                                  CV_C13InlineeLinesParsed *inlinee_parsed,
                                  String8                   binary_annots,
                                  U64Array                  separated_code_voffs)
{
  Temp                        scratch = scratch_begin(&arena, 1);
  CV_InlineBinaryAnnotsParsed result  = {0};

  struct SourceLine {
    struct SourceLine *next;
    U64                voff;
    U64                ln;
  };
  struct SourceFile {
    struct SourceFile *next;
    struct SourceLine *line_first;
    struct SourceLine *line_last;
    U64                line_count;
    U64                checksum_off;
    Rng1U64            last_code_range;
  };
  Rng1U64List             code_ranges = {0};
  struct SourceFile      *file_first  = 0;
  struct SourceFile      *file_last   = 0;
  U64                     file_count  = 0;
  CV_C13InlineSiteDecoder decoder     = cv_c13_inline_site_decoder_init(inlinee_parsed->file_off, inlinee_parsed->first_source_ln, parent_voff);
  decoder.separated_code_voffs        = separated_code_voffs;

  for (;;) {
    CV_C13InlineSiteDecoderStep step = cv_c13_inline_site_decoder_step(&decoder, binary_annots);
    if (decoder.error || (step.flags & CV_C13InlineSiteDecoderStepFlag_Error)) { goto exit; }
    if (step.flags == 0) { break; }
    if (step.flags & CV_C13InlineSiteDecoderStepFlag_EmitRange) {
      rng1u64_list_push(arena, &code_ranges, step.range);
    }
    if (step.flags & CV_C13InlineSiteDecoderStepFlag_ExtendLastRange) {
      if (code_ranges.last) { code_ranges.last->v = step.range; }
    }

    // a file change can also finish the preceding sequence's range
    if (file_last && (step.flags & (CV_C13InlineSiteDecoderStepFlag_EmitRange | CV_C13InlineSiteDecoderStepFlag_ExtendLastRange))) {
      file_last->last_code_range = step.range;
    }

    if (step.flags & CV_C13InlineSiteDecoderStepFlag_EmitFile) {
      struct SourceFile *file = push_array(scratch.arena, struct SourceFile, 1);
      file->checksum_off = step.file_off;
      SLLQueuePush(file_first, file_last, file);
      file_count += 1;
    }

    if (step.flags & CV_C13InlineSiteDecoderStepFlag_EmitLine) {
      if (step.flags & (CV_C13InlineSiteDecoderStepFlag_EmitRange | CV_C13InlineSiteDecoderStepFlag_ExtendLastRange)) {
        file_last->last_code_range = step.range;
      }
      struct SourceLine *line = push_array(scratch.arena, struct SourceLine, 1);
      line->voff = step.line_voff;
      line->ln   = step.ln;
      SLLQueuePush(file_last->line_first, file_last->line_last, line);
      file_last->line_count += 1;
    }
  }

  CV_LineArray *lines     = push_array(arena, CV_LineArray, file_count);
  U64           lines_idx = 0;
  for (struct SourceFile *file = file_first; file != 0; file = file->next, lines_idx += 1) {
    CV_LineArray *l = lines + lines_idx;
    l->file_off     = file->checksum_off;
    l->line_count   = file->line_count;
    if (file->line_count > 0) {
      l->voffs     = push_array_no_zero(arena, U64, file->line_count + 1);
      l->line_nums = push_array_no_zero(arena, U32, file->line_count);
      U64 line_idx = 0;
      for (struct SourceLine *line = file->line_first; line != 0; line = line->next, line_idx += 1) {
        l->voffs[line_idx]     = line->voff;
        l->line_nums[line_idx] = (U32)line->ln;
      }
      Assert(line_idx == file->line_count);
      l->voffs[line_idx] = file->last_code_range.max;
    }
  }
  result.lines_count = file_count;
  result.lines       = lines;
  result.code_ranges = code_ranges;

exit:;
  scratch_end(scratch);
  return result;
}

////////////////////////////////
// Lookups

internal B32
cv_leaf_stream_read_record(CV_LeafStream *stream, U64 ordinal, CV_Record *out)
{
  MemoryZeroStruct(out);
  U64 offset = stream->offsets[ordinal];
  U64 opl    = ordinal + 1 < stream->count ? stream->offsets[ordinal + 1] : stream->data.size;
  if (offset > opl || opl > stream->data.size) { return 0; }
  return cv_read_record(str8_prefix(stream->data, opl), offset, stream->align, out);
}

internal B32
cv_leaf_stream_get_leaf(CV_LeafStream *stream, U64 ordinal, CV_Leaf *out)
{
  MemoryZeroStruct(out);
  CV_Record record;
  if (!cv_leaf_stream_read_record(stream, ordinal, &record) ||
      cv_header_struct_size_from_leaf_kind(record.header.kind) > record.data.size) { return 0; }
  out->kind = record.header.kind;
  out->data = record.data;
  return 1;
}

internal CV_RecRange
cv_leaf_stream_rec_range(CV_LeafStream *stream, U64 ordinal)
{
  CV_RecRange result = {0};
  CV_Record   record;
  if (cv_leaf_stream_read_record(stream, ordinal, &record) &&
      cv_header_struct_size_from_leaf_kind(record.header.kind) <= record.data.size) {
    result.off = stream->offsets[ordinal] + 2;
    result.hdr = record.header;
  }
  return result;
}

internal CV_C13StreamIndex *
cv_c13_stream_index(CV_C13Stream *stream)
{
  CV_C13StreamIndex *index = stream->index;
  if (index && (index->block_count != stream->data.node_count || index->last_block != stream->data.last || (stream->data.last && stream->data.last->next))) {
    index = 0;
  }
  return index;
}

internal void
cv_c13_stream_stats_accumulate(CV_C13StreamStats *stats, CV_C13SubSectionKind kind, U64 size)
{
  if (kind & CV_C13SubSectionKind_IgnoreFlag) { return; }
  U32 idx = cv_c13_sub_section_idx_from_kind(kind);
  stats->size[idx]  += size;
  stats->count[idx] += 1;
  stats->framed_size[idx] += AlignPow2(sizeof(CV_C13SubSectionHeader) + size, CV_C13SubSectionAlign);
  if (kind == CV_C13SubSectionKind_Lines) {
    stats->lines_size += AlignPow2(sizeof(CV_C13SubSectionHeader) + size, CV_C13SubSectionAlign);
  }
}

internal CV_C13StreamStats
cv_c13_stream_stats(CV_C13Stream *stream)
{
  CV_C13StreamIndex *index = cv_c13_stream_index(stream);
  if (index) {
    CV_C13StreamStats result = index->stats;
    for EachElement(i, result.size) {
      if (stream->ignored_kinds & (1u << i)) { result.size[i] = 0; result.count[i] = 0; result.framed_size[i] = 0; }
    }
    if (stream->ignored_kinds & (1u << CV_C13SubSectionIdxKind_Lines)) { result.lines_size = 0; }
    return result;
  }
  CV_C13StreamStats result = {0};
  CV_C13StreamIter  it     = cv_c13_stream_iter_init(stream);
  while (cv_c13_stream_iter_next_block(&it)) {
    while (cv_c13_stream_iter_next(&it)) {
      cv_c13_stream_stats_accumulate(&result, it.kind, it.data.size);
    }
  }
  return result;
}

internal B32
cv_c13_stream_find_first(CV_C13Stream *stream, CV_C13SubSectionKind kind, String8 *out)
{
  CV_C13StreamIndex *index = cv_c13_stream_index(stream);
  if (index && (kind == CV_C13SubSectionKind_StringTable || kind == CV_C13SubSectionKind_FileChksms)) {
    U32 idx = cv_c13_sub_section_idx_from_kind(kind);
    if ((stream->ignored_kinds & (1u << idx)) || index->stats.count[idx] == 0) { return 0; }
    *out = kind == CV_C13SubSectionKind_StringTable ? index->string_table : index->file_chksms;
    return 1;
  }
  CV_C13StreamIter it = cv_c13_stream_iter_init(stream);
  if (!cv_c13_stream_iter_next_kind(&it, kind)) { return 0; }
  *out = it.data;
  return 1;
}

internal String8
cv_c13_stream_string_table(CV_C13Stream *stream)
{
  String8 result = str8_zero();
  cv_c13_stream_find_first(stream, CV_C13SubSectionKind_StringTable, &result);
  return result;
}

internal String8
cv_c13_stream_file_chksms(CV_C13Stream *stream)
{
  String8 result = str8_zero();
  cv_c13_stream_find_first(stream, CV_C13SubSectionKind_FileChksms, &result);
  return result;
}

internal B32
cv_c13_name_from_string_table(String8 data, U64 offset, String8 *out)
{
  *out = str8_zero();
  if (offset >= data.size) { return 0; }

  U64 end = offset;
  for (; end < data.size && data.str[end] != 0; end += 1) {}

  if (end == data.size) {
    return 0;
  }

  *out = str8(data.str + offset, end - offset);
  return 1;
}

internal B32
cv_c13_file_info_from_checksum(String8 checksums, String8 strings, U64 offset, CV_C13FileInfo *out)
{
  MemoryZeroStruct(out);

  CV_C13Checksum header;
  CV_C13FileInfo info = {0};
  if (!cv_c13_checksum_from_data(checksums, offset, &header, &info.checksum, 0) ||
      !cv_c13_name_from_string_table(strings, header.name_off, &info.name)) {
    return 0;
  }

  info.checksum_kind = header.kind;
  *out = info;
  return 1;
}

internal int
cv_c13_voff_map_compar(const void *raw_a, const void *raw_b)
{
  CV_Line *a = (CV_Line *)raw_a;
  CV_Line *b = (CV_Line *)raw_b;
  return a->voff < b->voff ? -1 : a->voff > b->voff ? +1 : 0;
}

internal CV_LinesAccel *
cv_c13_make_lines_accel(Arena *arena, U64 lines_count, CV_LineArray *lines)
{
  ProfBeginFunction();
  U64 total_voff_count = 0;
  for (U64 arr_idx = 0; arr_idx < lines_count; arr_idx += 1) {
    if (lines[arr_idx].line_count) { total_voff_count += lines[arr_idx].line_count + 1; }
  }

  CV_Line *map     = push_array_no_zero(arena, CV_Line, total_voff_count);
  U64      map_idx = 0;
  for (U64 line_idx = 0; line_idx < lines_count; line_idx += 1) {
    CV_LineArray *l = lines + line_idx;
    if (l->line_count > 0) {
      for (U64 voff_idx = 0; voff_idx < l->line_count; voff_idx += 1) {
        map[map_idx].voff     = l->voffs[voff_idx];
        map[map_idx].file_off = l->file_off;
        map[map_idx].line_num = l->line_nums[voff_idx];
        map[map_idx].col_num  = l->col_nums && voff_idx < l->col_count ? l->col_nums[2 * voff_idx] : 0;
        map_idx += 1;
      }
      map[map_idx].voff     = l->voffs[l->line_count];
      map[map_idx].file_off = l->file_off;
      map[map_idx].line_num = 0;
      map[map_idx].col_num  = 0;
      map_idx += 1;
    }
  }
  Assert(map_idx == total_voff_count);
  quick_sort(map, total_voff_count, sizeof(map[0]), cv_c13_voff_map_compar);

  CV_LinesAccel *accel = push_array(arena, CV_LinesAccel, 1);
  accel->map_count     = total_voff_count;
  accel->map           = map;
  ProfEnd();
  return accel;
}

internal U64
cv_nearest_line(CV_Line *arr, U64 count, U64 value)
{
  if (count > 1 && arr[0].voff <= value && value < arr[count - 1].voff) {
    U64 l = 0, r = count;
    for (; l < r;) {
      U64 m = l + (r - l) / 2;
      if (arr[m].voff <= value) {
        l = m + 1;
      } else {
        r = m;
      }
    }
    return l - 1;
  } else if (count == 1 && arr[0].voff == value) {
    return 0;
  }
  return max_U64;
}

internal CV_Line *
cv_line_from_voff(CV_LinesAccel *accel, U64 voff, U64 *out_line_count)
{
  ProfBeginFunction();
  U64      voff_line_count = 0;
  CV_Line *lines           = 0;
  U64      map_idx         = cv_nearest_line(accel->map, accel->map_count, voff);
  if (map_idx < accel->map_count) {
    U64 near_voff = accel->map[map_idx].voff;
    for (; map_idx > 0; map_idx -= 1) {
      if (accel->map[map_idx - 1].voff != near_voff) { break; }
    }
    lines = accel->map + map_idx;
    for (; map_idx < accel->map_count; map_idx += 1) {
      if (accel->map[map_idx].voff != near_voff) { break; }
      voff_line_count += 1;
    }
  }
  *out_line_count = voff_line_count;
  ProfEnd();
  return lines;
}

internal B32
cv_c13_inlinee_lines_accel_push(CV_InlineeLinesAccel *accel, CV_C13InlineeLinesParsed *parsed)
{
  U64 best_idx = cv_hash_from_item_id(parsed->inlinee) % accel->bucket_max;
  U64 idx      = best_idx;
  do {
    if (accel->buckets[idx] == 0) {
      accel->buckets[idx]  = parsed;
      accel->bucket_count += 1;
      return 1;
    }
    // keep the first definition; DIA selects the first matching inlinee
    if (accel->buckets[idx]->inlinee == parsed->inlinee) { return 0; }
    idx = (idx + 1) % accel->bucket_max;
  } while (idx != best_idx);
  return 0;
}

internal CV_C13InlineeLinesParsed *
cv_c13_inlinee_lines_accel_find(CV_InlineeLinesAccel *accel, CV_ItemId inlinee)
{
  U64 best_idx = cv_hash_from_item_id(inlinee) % accel->bucket_max;
  U64 idx      = best_idx;
  do {
    if (accel->buckets[idx] == 0) { break; }
    if (accel->buckets[idx]->inlinee == inlinee) { return accel->buckets[idx]; }
    idx = (idx + 1) % accel->bucket_max;
  } while (idx != best_idx);
  return 0;
}

internal CV_InlineeLinesAccel *
cv_c13_make_inlinee_lines_accel(Arena *arena, CV_C13InlineeLinesParsedList inlinee_lines)
{
  ProfBeginFunction();
  CV_InlineeLinesAccel *accel = push_array(arena, CV_InlineeLinesAccel, 1);
  accel->bucket_max           = Max((U64)((F64)inlinee_lines.count * 2.5), 1);
  accel->buckets              = push_array(arena, CV_C13InlineeLinesParsed *, accel->bucket_max);
  for (CV_C13InlineeLinesParsedNode *inlinee = inlinee_lines.first; inlinee != 0; inlinee = inlinee->next) {
    cv_c13_inlinee_lines_accel_push(accel, &inlinee->v);
  }
  ProfEnd();
  return accel;
}

////////////////////////////////
// C13 Construction/Serialization

internal String8
cv_c13_stream_alloc_subsection(Arena *arena, CV_C13Stream *stream, CV_C13SubSectionKind kind, U64 size)
{
  stream->index = 0;
  CV_C13SubSectionHeader header     = { .kind = kind, .size = safe_cast_u32(size) };
  U64                    block_size = AlignPow2(sizeof(header) + size, CV_C13SubSectionAlign);
  U8                    *block      = push_array(arena, U8, block_size);
  MemoryCopy(block, &header, sizeof(header));
  str8_list_push(arena, &stream->data, str8(block, block_size));
  return str8(block + sizeof(header), size);
}

internal void
cv_c13_stream_ignore_kind(CV_C13Stream *stream, CV_C13SubSectionKind kind)
{
  U32 idx = cv_c13_sub_section_idx_from_kind(kind);
  stream->ignored_kinds |= 1u << idx;
}

internal void
cv_c13_stream_concat_in_place(CV_C13Stream *dst, CV_C13Stream *src)
{
  if (dst->data.node_count == 0) {
    dst->ignored_kinds = src->ignored_kinds;
    dst->index         = cv_c13_stream_index(src);
  } else if (src->data.node_count) {
    dst->index = 0;
  }
  str8_list_concat_in_place(&dst->data, &src->data);
  MemoryZeroStruct(src);
}

internal U64
cv_c13_stream_size(CV_C13Stream *stream)
{
  U64               size  = 0;
  CV_C13StreamStats stats = cv_c13_stream_stats(stream);
  for EachElement(i, stats.framed_size) { size += stats.framed_size[i]; }
  return size;
}

internal String8List
cv_c13_stream_data(Arena *arena, CV_C13Stream *stream, B32 write_sig)
{
  String8List srl = {0};
  str8_serial_begin(arena, &srl);
  if (write_sig) { str8_serial_push_u32(arena, &srl, CV_Signature_C13); }
  CV_C13StreamIter it = cv_c13_stream_iter_init(stream);
  while (cv_c13_stream_iter_next_block(&it)) {
    while (cv_c13_stream_iter_next(&it)) {
      str8_serial_push_u32(arena, &srl, it.kind);
      str8_serial_push_u32(arena, &srl, safe_cast_u32(it.data.size));
      str8_serial_push_string(arena, &srl, it.data);
      str8_serial_push_align(arena, &srl, CV_C13SubSectionAlign);
    }
  }
  return srl;
}
