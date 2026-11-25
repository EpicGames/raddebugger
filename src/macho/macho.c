// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef MACHO_C
#define MACHO_C

internal U16
macho_read_u16(U8 *p)
{
  return (U16)((U16)p[0] | ((U16)p[1] << 8));
}

internal U32
macho_read_u32(U8 *p)
{
  return (U32)p[0] | ((U32)p[1] << 8) | ((U32)p[2] << 16) | ((U32)p[3] << 24);
}

internal U64
macho_read_u64(U8 *p)
{
  return (U64)macho_read_u32(p) | ((U64)macho_read_u32(p + 4) << 32);
}

internal String8
macho_name16(U8 *p)
{
  U64 size = 0;
  for(; size < 16 && p[size] != 0; size += 1) {}
  return str8(p, size);
}

internal MACHO_Bin
macho_bin_error(String8 error)
{
  return (MACHO_Bin){ .error = error };
}

internal B32
macho_is_magic(U32 magic)
{
  switch(magic) {
  case 0xfeedfaceu: case 0xcefaedfeu: // thin 32
  case 0xfeedfacfu: case 0xcffaedfeu: // thin 64
  case 0xcafebabeu: case 0xbebafecau: // fat 32
  case 0xcafebabfu: case 0xbfbafecau: // fat 64
    return 1;
  }
  return 0;
}

internal MACHO_Bin
macho_bin_from_data(Arena *arena, String8 data)
{
  MACHO_Bin bin = {0};
  if (data.str == 0 || data.size < 4) {
    return macho_bin_error(s("Truncated Mach-O magic"));
  }

  if (data.size > (U64)(~(size_t)0)) {
    return macho_bin_error(s("Mach-O data exceeds host address space"));
  }

  U32 magic = macho_read_u32(data.str);
  switch (magic) {
  case 0xcafebabeu:
  case 0xbebafecau:
  case 0xcafebabfu:
  case 0xbfbafecau:
    return macho_bin_error(s("Unsupported fat Mach-O (select a thin image first)"));
  case 0xcefaedfeu:
    return macho_bin_error(s("Unsupported big-endian 32-bit Mach-O"));
  case 0xfeedfaceu:
    return macho_bin_error(s("Unsupported 32-bit Mach-O"));
  case 0xcffaedfeu:
    return macho_bin_error(s("Unsupported big-endian Mach-O"));
  case 0xfeedfacfu: break;
  default: return macho_bin_error(s("Invalid Mach-O magic"));
  }

  if (data.size < 32) {
    return macho_bin_error(s("Truncated Mach-O64 header"));
  }

  switch (macho_read_u32(data.str + 4)) {
  case 0x01000007: bin.arch = Arch_x64; break;
  case 0x0100000c: bin.arch = Arch_arm64; break;
  default: return macho_bin_error(s("Unsupported Mach-O CPU (expected x86_64 or ARM64)"));
  }

  bin.file_type = macho_read_u32(data.str + 12);
  switch (bin.file_type) {
  case 1: return macho_bin_error(s("Unsupported relocatable Mach-O"));
  case 2:
  case 6:
  case 7:
  case 8:
  case 10: break;
  default: return macho_bin_error(s("Unsupported Mach-O file type"));
  }

  U64 command_count = macho_read_u32(data.str + 16);
  U64 command_size  = macho_read_u32(data.str + 20);
  if (command_size > data.size - 32 || (command_size & 7) != 0 || command_count > command_size/8) {
    return macho_bin_error(s("Invalid Mach-O load command region or count"));
  }

  U64 command_end = 32 + command_size;

  for EachIndex(pass, 2) {
    U64 cursor     = 32;
    B32 found_text = 0;

    bin.section_count = 0;
    bin.has_uuid      = 0;
    bin.has_symtab     = 0;

    for EachIndex(command_idx, command_count) {
      if (command_end - cursor < 8) {
        return macho_bin_error(s("Truncated Mach-O load command"));
      }

      U8  *cmd  = data.str + cursor;
      U32  kind = macho_read_u32(cmd);
      U64  size = macho_read_u32(cmd + 4);
      if (size < 8 || (size & 7) != 0 || size > command_end - cursor) {
        return macho_bin_error(s("Invalid Mach-O load command size or alignment"));
      }

      if (kind == 0x1b) { // LC_UUID 
        if (size != 24) {
          return macho_bin_error(s("Invalid Mach-O UUID command size"));
        }

        for EachIndex(i, 16) {
          if (bin.has_uuid && bin.uuid[i] != cmd[8 + i]) {
            return macho_bin_error(s("Conflicting Mach-O UUID commands"));
          }
          bin.uuid[i] = cmd[8 + i];
        }
        bin.has_uuid = 1;
      } else if (kind == 2) { // LC_SYMTAB
        if (size != 24) {
          return macho_bin_error(s("Invalid Mach-O symbol table command size"));
        }

        // reject even identical duplicates, so there is exactly one source of
        // symbol metadata; has_symtab is reset for each parser pass
        if (bin.has_symtab) {
          return macho_bin_error(s("Duplicate Mach-O symbol table commands"));
        }

        U64 symoff  = macho_read_u32(cmd + 8);
        U64 nsyms   = macho_read_u32(cmd + 12);
        U64 stroff  = macho_read_u32(cmd + 16);
        U64 strsize = macho_read_u32(cmd + 20);
        if (symoff > data.size || nsyms > (data.size - symoff)/16 ||
            stroff > data.size || strsize > data.size - stroff) {
          return macho_bin_error(s("Invalid Mach-O symbol or string table range"));
        }

        bin.has_symtab         = 1;
        bin.symbol_count       = nsyms;
        bin.symbol_table_range = rng_1u64(symoff, symoff + 16*nsyms);
        bin.string_table_range = rng_1u64(stroff, stroff + strsize);
      } else if(kind == 0x19) { // LC_SEGMENT_64
        if (size < 72) {
          return macho_bin_error(s("Truncated Mach-O64 segment"));
        }

        String8 segment_name  = macho_name16(cmd + 8);
        U64     vmaddr        = macho_read_u64(cmd + 24);
        U64     vmsize        = macho_read_u64(cmd + 32);
        U64     fileoff       = macho_read_u64(cmd + 40);
        U64     filesize      = macho_read_u64(cmd + 48);
        U32     init_prot     = macho_read_u32(cmd + 60);
        U64     section_count = macho_read_u32(cmd + 64);

        if (section_count > (size - 72)/80 || size != 72 + 80*section_count) {
          return macho_bin_error(s("Invalid Mach-O64 segment section count"));
        }

        if (vmsize > max_U64 - vmaddr || fileoff > data.size || filesize > data.size - fileoff || filesize > vmsize) {
          return macho_bin_error(s("Invalid Mach-O segment VM or file range"));
        }

        B32 is_dwarf    = str8_match(segment_name, s("__DWARF"), 0);
        B32 is_pagezero = str8_match(segment_name, s("__PAGEZERO"), 0);

        if (is_pagezero && (section_count != 0 || filesize != 0 || init_prot != 0)) {
          return macho_bin_error(s("Unsupported Mach-O __PAGEZERO layout"));
        }

        if (str8_match(segment_name, s("__TEXT"), 0)) {
          if (found_text) {
            return macho_bin_error(s("Multiple Mach-O __TEXT segments"));
          }

          found_text = 1;
          bin.base_vaddr = vmaddr;
        }

        for EachIndex(section_idx, section_count) {
          U8 *src = cmd + 72 + 80*section_idx;

          MACHO_Section section = {0};
          section.name         = macho_name16(src);
          section.segment_name = macho_name16(src + 16);

          U64 addr         = macho_read_u64(src + 32);
          U64 section_size = macho_read_u64(src + 40);
          U32 raw_offset   = macho_read_u32(src + 48);
          U32 align        = macho_read_u32(src + 52);
          U64 reloff       = macho_read_u32(src + 56);
          U64 nreloc       = macho_read_u32(src + 60);

          section.flags     = macho_read_u32(src + 64);
          section.init_prot = init_prot;
          section.is_debug  = is_dwarf || (section.flags & 0x02000000) != 0; // S_ATTR_DEBUG

          if ( ! str8_match(section.segment_name, segment_name, 0)) {
            return macho_bin_error(s("Mach-O section segment name mismatch"));
          }

          if (addr < vmaddr || addr - vmaddr > vmsize || section_size > vmsize - (addr - vmaddr)) {
            return macho_bin_error(s("Mach-O section outside segment VM range"));
          }

          if (align >= 64 || (addr & (((U64)1 << align) - 1)) != 0) {
            return macho_bin_error(s("Invalid Mach-O section VM alignment"));
          }

          if (nreloc != 0 && ((reloff & 3) != 0 || reloff > data.size || nreloc > (data.size - reloff)/8)) {
            return macho_bin_error(s("Invalid Mach-O section relocation range"));
          }

          section.vrange = rng_1u64(addr, addr + section_size);

          U32 section_type = section.flags & 0xff;
          B32 is_zerofill  = (section_type == 1 || section_type == 0xc || section_type == 0x12);
          B32 is_absent    = (bin.file_type == 10 && !is_dwarf && raw_offset == 0);
          if (section_size != 0 && !is_zerofill && !is_absent) {
            U64 offset = raw_offset;
            if (is_dwarf) {
              // section_64.offset is only U32. dSYM producers truncate it,
              // even though segment file offsets and section sizes are U64;
              // do not apply this mapping to sparse non-DWARF dSYM segments
              U64 delta = addr - vmaddr;
              if (delta > filesize || section_size > filesize - delta) {
                return macho_bin_error(s("Mach-O DWARF section outside segment file range"));
              }

              offset = fileoff + delta; // bounded by the validated segment
              if ((U32)offset != raw_offset) {
                return macho_bin_error(s("Unsupported Mach-O __DWARF VM-to-file layout"));
              }
            }

            if(offset < fileoff || offset - fileoff > filesize || section_size > filesize - (offset - fileoff) || offset > data.size || section_size > data.size - offset) {
              return macho_bin_error(s("Mach-O section outside segment file range or data"));
            }

            if ((offset & (((U64)1 << align) - 1)) != 0) {
              return macho_bin_error(s("Invalid Mach-O section file alignment"));
            }

            section.frange         = rng_1u64(offset, offset + section_size);
            section.is_file_backed = 1;
          }

          if (pass != 0) {
            bin.sections[bin.section_count] = section;
          }
          bin.section_count += 1; // bounded by command_size/80
        }
      }

      // unknown commands are skipped only after their generic size is checked
      cursor += size;
    }

    if (cursor != command_end) {
      return macho_bin_error(s("Mach-O load command count does not consume command region"));
    }

    if( ! found_text) {
      return macho_bin_error(s("Missing Mach-O __TEXT segment"));
    }

    if (pass == 0 && bin.section_count != 0) {
      if (bin.section_count > (U64)(~(size_t)0)/sizeof(MACHO_Section)) {
        return macho_bin_error(s("Mach-O section allocation size overflow"));
      }

      bin.sections = push_array(arena, MACHO_Section, bin.section_count);

      if(bin.sections == 0) {
        return macho_bin_error(s("Mach-O section allocation failed"));
      }
    }
  }

  bin.valid = 1;

  return bin;
}

internal MACHO_SymbolArray
macho_symbols_from_bin(Arena *arena, String8 data, MACHO_Bin *bin)
{
  MACHO_SymbolArray result = {0};
  if (bin == 0 || ! bin->valid || ! bin->has_symtab) {
    return result;
  }

  // validate ranges against this input before reading or allocating
  Rng1U64 symbol_range = bin->symbol_table_range;
  Rng1U64 string_range = bin->string_table_range;
  if (data.str == 0 || data.size > (U64)(~(size_t)0) ||
      symbol_range.min > symbol_range.max || symbol_range.max > data.size ||
      string_range.min > string_range.max || string_range.max > data.size) {
    return result;
  }

  U64 symbol_size = symbol_range.max - symbol_range.min;
  if (symbol_size % 16 != 0 || bin->symbol_count != symbol_size/16 ||
      bin->symbol_count > (U64)(~(size_t)0)/sizeof(MACHO_Symbol) ||
      bin->symbol_count == 0) {
    return result;
  }

  result.v = push_array_no_zero(arena, MACHO_Symbol, bin->symbol_count);
  if (result.v == 0) {
    return result;
  }
  result.count = bin->symbol_count;

  U64 string_size = bin->string_table_range.max - bin->string_table_range.min;
  U8 *strings     = data.str + bin->string_table_range.min;
  for EachIndex(symbol_idx, result.count) {
    U8           *src    = data.str + symbol_range.min + 16*symbol_idx;
    MACHO_Symbol *symbol = &result.v[symbol_idx];
    symbol->strx       = macho_read_u32(src);
    symbol->type       = src[4];
    symbol->sect       = src[5];
    symbol->desc       = macho_read_u16(src + 6);
    symbol->value      = macho_read_u64(src + 8);
    symbol->name       = (String8){0};
    symbol->name_valid = 0;

    // preserve every entry, even when its name is out of bounds or unterminated
    U64 strx = symbol->strx;
    if (strx < string_size) {
      U64 name_end = strx;
      for (; name_end < string_size && strings[name_end] != 0; name_end += 1) {}
      if (name_end < string_size) {
        symbol->name       = str8(strings + strx, name_end - strx);
        symbol->name_valid = 1;
      }
    }
  }

  return result;
}

#endif // MACHO_C
