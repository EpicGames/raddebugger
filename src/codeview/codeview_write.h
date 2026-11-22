// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef CODEVIEW_WRITE_H
#define CODEVIEW_WRITE_H

internal U64     cv_size_from_leaf(String8 data, U64 align);
internal U64     cv_write_leaf(U8 *buffer, U64 cursor, U64 size, CV_LeafKind kind, String8 data, U64 align);
internal String8 cv_make_leaf(Arena *arena, CV_LeafKind kind, String8 data, U64 align);
internal String8 cv_data_from_leaf(Arena *arena, CV_Leaf *leaf, U64 align);

internal U64         cv_size_from_symbol(CV_Symbol *symbol, U64 align);
internal U64         cv_write_symbol_buf(String8Node *buf, U64 *pos, CV_Symbol *symbol, U64 align);
internal U64         cv_write_symbol(U8 *buffer, U64 cursor, U64 size, CV_Symbol *symbol, U64 align);
internal String8     cv_data_from_symbol(Arena *arena, CV_Symbol *symbol, U64 align);
internal U8*         cv_ptr_from_symbol(CV_Symbol symbol);
internal CV_SymKind* cv_kind_ptr_from_symbol(CV_Symbol symbol);
internal B32         cv_symbol_match(CV_Symbol a, CV_Symbol b);
internal String8     cv_make_symbol(Arena *arena, CV_SymKind kind, String8 data);

internal String8   cv_make_obj_name(Arena *arena, String8 obj_path, U32 sig);
internal String8   cv_make_comp3(Arena *arena, CV_Compile3Flags flags, CV_Language lang, CV_Arch arch, U16 ver_fe_major, U16 ver_fe_minor, U16 ver_fe_build, U16 ver_feqfe, U16 ver_major, U16 ver_minor, U16 ver_build, U16 ver_qfe, String8 version_string);
internal String8   cv_make_envblock(Arena *arena, String8List strings);
internal String8   cv_make_proc32(Arena *arena, CV_SymProc32 proc, String8 name);
internal String8   cv_make_end(Arena *arena);
internal String8   cv_make_data32(Arena *arena, CV_SymData32 data, String8 name);
internal String8   cv_make_const(Arena *arena, CV_SymConstant v, U16 value, String8 name);
internal String8   cv_make_udt(Arena *arena, CV_SymUDT v, String8 name);
internal String8   cv_make_inline_site(Arena *arena, CV_SymInlineSite site, String8 annots);
internal String8   cv_make_inline_site_end(Arena *arena);
internal CV_Symbol cv_make_proc_ref(Arena *arena, CV_ModIndex imod, U32 stream_offset, String8 name, B32 is_local);
internal CV_Symbol cv_make_pub32(Arena *arena, CV_Pub32Flags flags, U32 off, U16 isect, String8 name);

#endif // CODEVIEW_WRITE_H
