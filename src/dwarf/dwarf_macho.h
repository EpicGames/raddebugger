// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef DWARF_MACHO_H
#define DWARF_MACHO_H

internal DW_Raw rb_dw_raw_from_macho_bin(String8 data, MACHO_Bin *bin);
internal RDIM_BinarySectionList rb_rdi_binary_sections_from_macho_bin(Arena *arena, MACHO_Bin *bin);

#endif // DWARF_MACHO_H
