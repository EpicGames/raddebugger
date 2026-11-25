// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef MACHO_H
#define MACHO_H

typedef struct MACHO_Section
{
  String8 name;
  String8 segment_name;
  Rng1U64 vrange;
  Rng1U64 frange;
  U32     flags;
  U32     init_prot;
  B32     is_debug;
  B32     is_file_backed;
} MACHO_Section;

typedef struct MACHO_Bin
{
  B32            valid;
  String8        error;
  Arch           arch;
  U32            file_type;
  U64            base_vaddr;
  B32            has_uuid;
  U8             uuid[16];
  B32            has_symtab;
  U64            symbol_count;
  Rng1U64        symbol_table_range;
  Rng1U64        string_table_range;
  U64            section_count;
  MACHO_Section *sections;
} MACHO_Bin;

typedef struct MACHO_Symbol
{
  U32     strx;
  U8      type;
  U8      sect;
  U16     desc;
  U64     value;
  String8 name;
  B32     name_valid;
} MACHO_Symbol;

typedef struct MACHO_SymbolArray
{
  U64           count;
  MACHO_Symbol *v;
} MACHO_SymbolArray;

internal B32 macho_is_magic(U32 magic);
internal MACHO_Bin macho_bin_from_data(Arena *arena, String8 data);
internal MACHO_SymbolArray macho_symbols_from_bin(Arena *arena, String8 data, MACHO_Bin *bin);

#endif // MACHO_H
