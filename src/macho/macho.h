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
  U64            section_count;
  MACHO_Section *sections;
} MACHO_Bin;

internal B32 macho_is_magic(U32 magic);
internal MACHO_Bin macho_bin_from_data(Arena *arena, String8 data);

#endif // MACHO_H
