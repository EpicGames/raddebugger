// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef PE_MAKE_IMPORT_TABLE_H
#define PE_MAKE_IMPORT_TABLE_H

typedef struct PE_MakeImport
{
  String8 header;
  B32     make_jump_thunk;
} PE_MakeImport;

typedef struct PE_MakeImportNode
{
  PE_MakeImport             v;
  struct PE_MakeImportNode *next;
} PE_MakeImportNode;

typedef struct PE_MakeImportList
{
  U64                      count;
  PE_MakeImportNode *first;
  PE_MakeImportNode *last;
} PE_MakeImportList;

typedef struct PE_MakeImportTable
{
  COFF_TimeStamp   time_stamp;
  COFF_MachineType machine;
  String8          dll_name;
  String8          descriptor_symbol_name;
  String8          null_thunk_symbol_name;
  String8          ilt_symbol_name;
  String8          iat_symbol_name;
  String8          biat_symbol_name;
  String8          uiat_symbol_name;
  String8          tail_merge_symbol_name;
  String8          delay_load_helper_name;
  B32              is_delayed;
  B32              has_imports;
} PE_MakeImportTable;

// -----------------------------------------------------------------------------

internal void                pe_make_import_header_list_push_node(PE_MakeImportList *list, PE_MakeImportNode *node);
internal PE_MakeImportNode * pe_make_import_header_list_push(Arena *arena, PE_MakeImportList *list, PE_MakeImport header);
internal String8             pe_undecorate_import_name(String8 name);
internal String8             pe_import_name_no_prefix(String8 name);

internal COFF_ObjSymbol * pe_make_indirect_jump_thunk_x64(COFF_ObjWriter *obj_writer, COFF_ObjSection *code_sect, COFF_ObjSymbol *iat_symbol, String8 thunk_name);
internal COFF_ObjSymbol * pe_make_load_thunk_x64(COFF_ObjWriter *obj_writer, COFF_ObjSection *code_sect, COFF_ObjSymbol *imp_addr_ptr, COFF_ObjSymbol *tail_merge, String8 func_name);
internal COFF_ObjSymbol * pe_make_tail_merge_thunk_x64(COFF_ObjWriter *obj_writer, COFF_ObjSection *code_sect, String8 tail_merge_name, String8 delay_load_helper_name, COFF_ObjSymbol *dll_import_descriptor);

internal PE_MakeImportTable * pe_make_static_import_table(Arena *arena, COFF_TimeStamp time_stamp, COFF_MachineType machine, String8 dll_name);
internal PE_MakeImportTable * pe_make_delayed_import_table(Arena *arena, COFF_TimeStamp time_stamp, COFF_MachineType machine, String8 dll_name, String8 delay_load_helper_name, B32 emit_biat, B32 emit_uiat, String8 image_name);

internal String8 pe_make_static_dlldesc_obj(Arena *arena, PE_MakeImportTable *table, String8 debug_symbols);
internal String8 pe_make_static_imports_obj(Arena *arena, PE_MakeImportTable *table, PE_MakeImportList import_headers, String8 debug_symbols, B32 is_last_chunk);
internal void    pe_make_static_null_thunk(COFF_ObjWriter *obj_writer, PE_MakeImportTable *table);
internal String8 pe_make_static_null_thunk_obj(Arena *arena, PE_MakeImportTable *table, String8 debug_symbols);
internal String8 pe_make_delayed_dlldesc_obj(Arena *arena, PE_MakeImportTable *table, String8 debug_symbols);
internal String8 pe_make_delayed_imports_obj(Arena *arena, PE_MakeImportTable *table, PE_MakeImportList import_headers, String8 debug_symbols, B32 is_last_chunk);

internal String8 pe_make_import_entry_obj_delayed(Arena *arena, String8 dll_name, COFF_TimeStamp time_stamp, COFF_MachineType machine, String8 debug_symbols);
internal String8 pe_make_null_import_descriptor_delayed(Arena *arena, COFF_TimeStamp time_stamp, COFF_MachineType machine, String8 debug_symbols);

internal String8 pe_make_import_entry_obj(Arena *arena, String8 dll_name, COFF_TimeStamp time_stamp, COFF_MachineType machine, String8 debug_symbols);
internal String8 pe_make_null_import_descriptor_obj(Arena *arena, COFF_TimeStamp time_stamp, COFF_MachineType machine, String8 debug_symbols);
internal String8 pe_make_null_thunk_data_obj(Arena *arena, String8 dll_name, COFF_TimeStamp time_stamp, COFF_MachineType machine, String8 debug_symbols);

#endif // PE_MAKE_IMPORT_TABLE_H
