internal U64
cv_size_from_leaf(String8 data, U64 align)
{
  if(data.size > CV_LeafSize_Max - sizeof(CV_LeafKind)) { return 0; }
  return AlignPow2(sizeof(CV_LeafHeader) + data.size, align);
}

internal U64
cv_write_leaf(U8 *buffer, U64 cursor, U64 size, CV_LeafKind kind, String8 data, U64 align)
{
  U64 write_size = cv_size_from_leaf(data, align);
  if(!write_size) { return 0; }
  if(buffer)
  {
    CV_LeafHeader header = {.size = (CV_LeafSize)(sizeof(kind) + data.size), .kind = kind};
    MemoryCopy(buffer + cursor, &header, sizeof(header));
    if(data.size) { MemoryCopy(buffer + cursor + sizeof(header), data.str, data.size); }
    U64 raw_size = sizeof(header) + data.size;
    for(U64 i = raw_size; i < write_size; i += 1) { buffer[cursor + i] = (U8)(0xf0 + i - raw_size); }
  }
  return write_size;
}

internal String8
cv_make_leaf(Arena *arena, CV_LeafKind kind, String8 data, U64 align)
{
  U64 size = cv_size_from_leaf(data, align);
  U8 *buffer = push_array_no_zero(arena, U8, size);
  return str8(buffer, cv_write_leaf(buffer, 0, size, kind, data, align));
}

internal String8
cv_data_from_leaf(Arena *arena, CV_Leaf *leaf, U64 align)
{
  return cv_make_leaf(arena, leaf->kind, leaf->data, align);
}

internal U64
cv_size_from_symbol(CV_Symbol *symbol, U64 align)
{
  if(symbol->data.size > CV_SymSize_Max - sizeof(CV_SymKind)) { return 0; }
  U64 size = AlignPow2(sizeof(CV_SymbolHeader) + symbol->data.size, align);
  return size - sizeof(CV_SymSize) <= CV_SymSize_Max ? size : 0;
}

internal U64
cv_write_symbol_buf(String8Node *buf, U64 *pos, CV_Symbol *symbol, U64 align)
{
  U64 size = cv_size_from_symbol(symbol, align);
  if(!size) { return 0; }
  CV_SymbolHeader header = {.size = (CV_SymSize)(size - sizeof(CV_SymSize)), .kind = symbol->kind};
  U64 result = str8_buffer_write(buf, pos, str8_struct(&header));
  if(symbol->data.str) { result += str8_buffer_write(buf, pos, symbol->data); }
  else { result += str8_buffer_write_zeroes(buf, pos, symbol->data.size); }
  result += str8_buffer_write_zeroes(buf, pos, size - sizeof(header) - symbol->data.size);
  return result;
}

internal U64
cv_write_symbol(U8 *buffer, U64 cursor, U64 size, CV_Symbol *symbol, U64 align)
{
  U64 write_size = cv_size_from_symbol(symbol, align);
  if(!write_size) { return 0; }
  if(buffer)
  {
    CV_SymbolHeader header = {.size = (CV_SymSize)(write_size - sizeof(CV_SymSize)), .kind = symbol->kind};
    U8 *dst = buffer + cursor;
    MemoryCopy(dst, &header, sizeof(header));
    MemoryZero(dst + sizeof(header), write_size - sizeof(header));
    if(symbol->data.str && symbol->data.size) { MemoryCopy(dst + sizeof(header), symbol->data.str, symbol->data.size); }
  }
  return write_size;
}

internal String8
cv_data_from_symbol(Arena *arena, CV_Symbol *symbol, U64 align)
{
  U64 size = cv_size_from_symbol(symbol, align);
  U8 *buffer = push_array_no_zero(arena, U8, size);
  return str8(buffer, cv_write_symbol(buffer, 0, size, symbol, align));
}

internal U8 *
cv_ptr_from_symbol(CV_Symbol symbol)
{
  return symbol.data.str - sizeof(CV_SymbolHeader);
}

internal CV_SymKind *
cv_kind_ptr_from_symbol(CV_Symbol symbol)
{
  return (CV_SymKind *)(cv_ptr_from_symbol(symbol) + sizeof(CV_SymSize));
}

internal B32
cv_symbol_match(CV_Symbol a, CV_Symbol b)
{
  return a.kind == b.kind && str8_match(a.data, b.data, 0);
}

internal String8
cv_make_symbol(Arena *arena, CV_SymKind kind, String8 data)
{
  CV_Symbol symbol = {.kind = kind, .data = data};
  return cv_data_from_symbol(arena, &symbol, 1);
}

internal String8
cv_record_named_data(Arena *arena, String8 fixed, String8 name)
{
  U64 size = fixed.size + name.size + 1;
  U8 *buffer = push_array(arena, U8, size);
  MemoryCopy(buffer, fixed.str, fixed.size);
  MemoryCopy(buffer + fixed.size, name.str, name.size);
  return str8(buffer, size);
}

internal String8
cv_make_obj_name(Arena *arena, String8 obj_path, U32 sig)
{
  CV_SymObjName obj = {.sig = sig};
  return cv_record_named_data(arena, str8_struct(&obj), obj_path);
}

internal String8
cv_make_comp3(Arena *arena, CV_Compile3Flags flags, CV_Language lang, CV_Arch arch,
              U16 ver_fe_major, U16 ver_fe_minor, U16 ver_fe_build, U16 ver_feqfe,
              U16 ver_major, U16 ver_minor, U16 ver_build, U16 ver_qfe, String8 version_string)
{
  CV_SymCompile3 comp = {0};
  comp.flags = flags | lang;
  comp.machine = arch;
  comp.ver_fe_major = ver_fe_major;
  comp.ver_fe_minor = ver_fe_minor;
  comp.ver_fe_build = ver_fe_build;
  comp.ver_feqfe = ver_feqfe;
  comp.ver_major = ver_major;
  comp.ver_minor = ver_minor;
  comp.ver_build = ver_build;
  comp.ver_qfe = ver_qfe;
  return cv_record_named_data(arena, str8_struct(&comp), version_string);
}

internal String8
cv_make_envblock(Arena *arena, String8List strings)
{
  U64 size = sizeof(CV_SymEnvBlock) + strings.total_size + strings.node_count;
  U8 *buffer = push_array(arena, U8, size);
  U64 cursor = sizeof(CV_SymEnvBlock);
  for(String8Node *node = strings.first; node; node = node->next)
  {
    MemoryCopy(buffer + cursor, node->string.str, node->string.size);
    cursor += node->string.size + 1;
  }
  return str8(buffer, size);
}

internal String8
cv_make_proc32(Arena *arena, CV_SymProc32 proc, String8 name)
{
  return cv_record_named_data(arena, str8_struct(&proc), name);
}

internal String8
cv_make_end(Arena *arena)
{
  return str8_zero();
}

internal String8
cv_make_data32(Arena *arena, CV_SymData32 data, String8 name)
{
  return cv_record_named_data(arena, str8_struct(&data), name);
}

internal String8
cv_make_const(Arena *arena, CV_SymConstant v, U16 value, String8 name)
{
  U8 fixed[sizeof(v) + sizeof(value)];
  MemoryCopy(fixed, &v, sizeof(v));
  MemoryCopy(fixed + sizeof(v), &value, sizeof(value));
  return cv_record_named_data(arena, str8_array_fixed(fixed), name);
}

internal String8
cv_make_udt(Arena *arena, CV_SymUDT v, String8 name)
{
  return cv_record_named_data(arena, str8_struct(&v), name);
}

internal String8
cv_make_inline_site(Arena *arena, CV_SymInlineSite site, String8 annots)
{
  U64 size = sizeof(site) + annots.size;
  U8 *buffer = push_array_no_zero(arena, U8, size);
  MemoryCopy(buffer, &site, sizeof(site));
  MemoryCopy(buffer + sizeof(site), annots.str, annots.size);
  return str8(buffer, size);
}

internal String8
cv_make_inline_site_end(Arena *arena)
{
  return str8_zero();
}

internal CV_Symbol
cv_make_proc_ref(Arena *arena, CV_ModIndex imod, U32 stream_offset, String8 name, B32 is_local)
{
  CV_SymRef2 ref = {.sym_off = stream_offset, .imod = imod + 1};
  CV_Symbol result = {.kind = is_local ? CV_SymKind_LPROCREF : CV_SymKind_PROCREF, .offset = max_U64};
  result.data = cv_record_named_data(arena, str8_struct(&ref), name);
  return result;
}

internal CV_Symbol
cv_make_pub32(Arena *arena, CV_Pub32Flags flags, U32 off, U16 isect, String8 name)
{
  CV_SymPub32 pub = {.flags = flags, .off = off, .sec = isect};
  CV_Symbol result = {.kind = CV_SymKind_PUB32, .offset = max_U64};
  result.data = cv_record_named_data(arena, str8_struct(&pub), name);
  return result;
}

