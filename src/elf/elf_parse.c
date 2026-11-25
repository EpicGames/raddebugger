// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef ELF_PARSE_C
#define ELF_PARSE_C

//- rjf: top-level binary parsing

internal ELF_Bin
elf_bin_from_data(Arena *arena, String8 data)
{
  ELF_Bin bin = {0};
  if(str8_match(str8_prefix(data, elf_magic_string.size), elf_magic_string, 0) &&
     data.size >= ELF_Identifier_Max)
  {
    //- rjf: parse sig/header
    U8 sig[ELF_Identifier_Max] = {0};
    str8_deserial_read(data, 0, &sig[0], sizeof(sig), 1);
    switch(sig[ELF_Identifier_Class])
    {
      default:
      case ELF_Class_None:{}break;
      case ELF_Class_32:
      {
        ELF_Hdr32 hdr32 = {0};
        U64 hdr_size = str8_deserial_read_struct(data, 0, &hdr32);
        if(hdr_size == sizeof(hdr32))
        {
          bin.hdr = elf_hdr64_from_hdr32(hdr32);
          U64 shstr_off = hdr32.e_shoff + hdr32.e_shentsize*hdr32.e_shstrndx;
          ELF_Shdr32 shdr = {0};
          U64 shdr_size = str8_deserial_read_struct(data, shstr_off, &shdr);
          if(shdr_size == sizeof(shdr))
          {
            bin.sh_name_range = rng_1u64(shdr.sh_offset, shdr.sh_offset + shdr.sh_size);
          }
        }
      }break;
      case ELF_Class_64:
      {
        ELF_Hdr64 hdr64 = {0};
        U64 hdr_size = str8_deserial_read_struct(data, 0, &hdr64);
        if(hdr_size == sizeof(hdr64))
        {
          bin.hdr = hdr64;
          U64 shstr_off = hdr64.e_shoff + hdr64.e_shentsize*hdr64.e_shstrndx;
          ELF_Shdr64 shdr = {0};
          U64 shdr_size = str8_deserial_read_struct(data, shstr_off, &shdr);
          if(shdr_size == sizeof(shdr))
          {
            bin.sh_name_range = rng_1u64(shdr.sh_offset, shdr.sh_offset + shdr.sh_size);
          }
        }
      }break;
    }
    
    //- rjf: gather all shdrs
    {
      ELF_Hdr64 *hdr = &bin.hdr;
      bin.shdrs.count = hdr->e_shnum;
      bin.shdrs.v = push_array(arena, ELF_Shdr64, hdr->e_shnum);
      Rng1U64 shdr_range = rng_1u64(hdr->e_shoff, hdr->e_shoff + hdr->e_shentsize*hdr->e_shnum);
      String8 shdr_data = str8_substr(data, shdr_range);
      for EachIndex(shdr_idx, hdr->e_shnum)
      {
        switch(hdr->e_ident[ELF_Identifier_Class])
        {
          default:
          case ELF_Class_None:
          {}break;
          case ELF_Class_32:
          {
            ELF_Shdr32 shdr32 = {0};
            str8_deserial_read_struct(shdr_data, shdr_idx * sizeof(ELF_Shdr32), &shdr32);
            bin.shdrs.v[shdr_idx] = elf_shdr64_from_shdr32(shdr32);
          }break;
          case ELF_Class_64:
          {
            str8_deserial_read_struct(shdr_data, shdr_idx * sizeof(ELF_Shdr64), &bin.shdrs.v[shdr_idx]);
          }break;
        }
      }
    }
    
    //- rjf: gather all phdrs
    {
      ELF_Hdr64 *hdr = &bin.hdr;
      bin.phdrs.count = hdr->e_phnum;
      bin.phdrs.v = push_array(arena, ELF_Phdr64, hdr->e_phnum);
      Rng1U64 phdr_range = rng_1u64(hdr->e_phoff, hdr->e_phoff + hdr->e_phentsize*hdr->e_phnum);
      String8 phdr_data = str8_substr(data, phdr_range);
      for EachIndex(phdr_idx, hdr->e_phnum)
      {
        switch(hdr->e_ident[ELF_Identifier_Class])
        {
          default:
          case ELF_Class_None:
          {}break;
          case ELF_Class_32:
          {
            ELF_Phdr32 phdr32 = {0};
            str8_deserial_read_struct(phdr_data, phdr_idx * sizeof(ELF_Phdr32), &phdr32);
            bin.phdrs.v[phdr_idx] = elf_phdr64_from_phdr32(phdr32);
          }break;
          case ELF_Class_64:
          {
            str8_deserial_read_struct(phdr_data, phdr_idx * sizeof(ELF_Phdr64), &bin.phdrs.v[phdr_idx]);
          }break;
        }
      }
    }
  }
  return bin;
}

//- rjf: extra bin info extraction

internal String8
elf_name_from_shdr64(String8 data, ELF_Bin *bin, ELF_Shdr64 *shdr)
{
  String8 sh_names = str8_substr(data, bin->sh_name_range);
  String8 name = {0};
  str8_deserial_read_cstr(sh_names, shdr->sh_name, &name);
  return name;
}

internal U64
elf_base_addr_from_bin(ELF_Bin *bin)
{
  U64 base_vaddr = 0;
  B32 found_load = 0;
  if(bin->hdr.e_type != ELF_Type_Dyn)
  {
    for EachIndex(phdr_idx, bin->phdrs.count)
    {
      ELF_Phdr64 *phdr = &bin->phdrs.v[phdr_idx];
      if(phdr->p_type == ELF_PType_Load && ( ! found_load || phdr->p_vaddr < base_vaddr))
      {
        base_vaddr = phdr->p_vaddr;
        found_load = 1;
      }
    }
  }
  return base_vaddr;
}

internal ELF_SymbolArray
elf_symbols_from_bin(Arena *arena, String8 data, ELF_Bin *bin)
{
  ELF_SymbolArray result = {0};
  if (bin == 0 || data.size < ELF_Identifier_Max ||
      ! str8_match(str8_prefix(data, elf_magic_string.size), elf_magic_string, 0) ||
      data.str[ELF_Identifier_Data] != ELF_Data_2LSB) {
    return result;
  }

  ELF_Class elf_class = data.str[ELF_Identifier_Class];
  if (elf_class != ELF_Class_32 && elf_class != ELF_Class_64) {
    return result;
  }
  U64 hdr_size  = elf_hdr_size_from_class(elf_class);
  U64 shdr_size = elf_shdr_size_from_class(elf_class);
  U64 sym_size  = elf_sym_size_from_class(elf_class);
  if (data.size < hdr_size) {
    return result;
  }

  // validate the original header before using the normalized section headers
  ELF_Hdr64 hdr = {0};
  if (elf_class == ELF_Class_32) {
    ELF_Hdr32 hdr32;
    MemoryCopy(&hdr32, data.str, sizeof(hdr32));
    hdr = elf_hdr64_from_hdr32(hdr32);
  } else {
    MemoryCopy(&hdr, data.str, sizeof(hdr));
  }
  if (hdr.e_ehsize != hdr_size || hdr.e_shentsize != shdr_size ||
      hdr.e_shoff == 0 || hdr.e_shoff > data.size ||
      hdr.e_shnum > (data.size - hdr.e_shoff)/shdr_size ||
      hdr.e_shnum == 0 || bin->shdrs.count != hdr.e_shnum || bin->shdrs.v == 0) {
    return result;
  }

  // count, then fill one exact allocation; no allocation is made per raw symbol
  for (U64 pass = 0; pass < 2; pass += 1) {
    U64 count = 0;
    for (U64 table_pass = 0; table_pass < 2; table_pass += 1) {
      ELF_ShType table_type = table_pass == 0 ? ELF_ShType_Symtab : ELF_ShType_Dynsym;
      for EachIndex(shdr_idx, bin->shdrs.count) {
        ELF_Shdr64 *table = &bin->shdrs.v[shdr_idx];
        if (table->sh_type != table_type || table->sh_entsize != sym_size ||
            table->sh_size % sym_size != 0 || table->sh_offset > data.size ||
            table->sh_size > data.size - table->sh_offset) {
          continue;
        }

        U64 sym_count = table->sh_size/sym_size;
        if (pass == 0) {
          if (sym_count > max_U64/sizeof(ELF_Symbol) - count) {
            return result;
          }
          count += sym_count;
          continue;
        }

        // an invalid string table does not discard the native symbol entries
        String8 names = {0};
        if (table->sh_link < bin->shdrs.count) {
          ELF_Shdr64 *strings = &bin->shdrs.v[table->sh_link];
          if (strings->sh_type == ELF_ShType_Strtab && strings->sh_offset <= data.size &&
              strings->sh_size <= data.size - strings->sh_offset) {
            names = str8(data.str + strings->sh_offset, strings->sh_size);
          }
        }

        for EachIndex(sym_idx, sym_count) {
          U8         *sym_data = data.str + table->sh_offset + sym_idx*sym_size;
          ELF_Symbol  symbol   = {0};
          if (elf_class == ELF_Class_32) {
            ELF_Sym32 sym32;
            MemoryCopy(&sym32, sym_data, sizeof(sym32));
            symbol.raw = elf_sym64_from_sym32(sym32);
          } else {
            MemoryCopy(&symbol.raw, sym_data, sizeof(symbol.raw));
          }
          symbol.table_section_idx = shdr_idx;
          symbol.table_symbol_idx  = sym_idx;

          if (symbol.raw.st_name < names.size) {
            U8      *name_start = names.str + symbol.raw.st_name;
            U64      name_space = names.size - symbol.raw.st_name;
            String8  name       = str8_cstring_capped(name_start, name_start + name_space);
            if (name.size < name_space) {
              symbol.name       = name;
              symbol.name_valid = 1;
            }
          }
          result.v[count] = symbol;
          count += 1;
        }
      }
    }
    if (pass == 0) {
      if (count == 0) {
        return result;
      }
      result.count = count;
      result.v     = push_array(arena, ELF_Symbol, count);
    }
  }
  return result;
}

internal ELF_GnuDebugLink
elf_gnu_debug_link_from_bin(String8 raw_data, ELF_Bin *bin)
{
  ELF_GnuDebugLink result = {0};
  for EachIndex(idx, bin->shdrs.count)
  {
    ELF_Shdr64 *shdr = &bin->shdrs.v[idx];
    String8 name = elf_name_from_shdr64(raw_data, bin, shdr);
    if(str8_match(name, str8_lit(".gnu_debuglink"), 0))
    {
      Rng1U64 raw_data_range = rng_1u64(shdr->sh_offset, shdr->sh_offset + shdr->sh_size);
      String8 data = str8_substr(raw_data, raw_data_range);
      String8 path = {0};
      U32 checksum = 0;
      {
        U64 cursor = 0;
        cursor += str8_deserial_read_cstr(data, cursor, &path);
        cursor = AlignPow2(cursor, 4);
        cursor += str8_deserial_read_struct(data, cursor, &checksum);
      }
      result.path = path;
      result.checksum = checksum;
      break;
    }
  }
  return result;
}

internal String8
elf_note_desc_from_bin(String8 raw_data, ELF_Bin *bin, String8 owner, ELF_NoteType type)
{
  String8 result = {0};

  ELF_Class elf_class = bin->hdr.e_ident[ELF_Identifier_Class];

  if (bin->hdr.e_ident[ELF_Identifier_Data] == ELF_Data_2LSB && (elf_class == ELF_Class_32 || elf_class == ELF_Class_64)) {
    Temp scratch = scratch_begin(0, 0);

    for (U64 pass = 0; pass < 2 && result.size == 0; pass += 1) {
      U64 count         = pass == 0 ? bin->shdrs.count     : bin->phdrs.count;
      U64 table_off     = pass == 0 ? bin->hdr.e_shoff     : bin->hdr.e_phoff;
      U64 entry_size    = pass == 0 ? bin->hdr.e_shentsize : bin->hdr.e_phentsize;
      U64 expected_size = pass == 0 ? (elf_class == ELF_Class_32 ? sizeof(ELF_Shdr32) : sizeof(ELF_Shdr64)) :
                                      (elf_class == ELF_Class_32 ? sizeof(ELF_Phdr32) : sizeof(ELF_Phdr64));

      if (entry_size != expected_size || table_off > raw_data.size || count > (raw_data.size - table_off)/expected_size) {
        continue;
      }

      for (U64 idx = 0; idx < count && result.size == 0; idx += 1) {
        B32 is_note = pass == 0 ? bin->shdrs.v[idx].sh_type == ELF_ShType_Note : bin->phdrs.v[idx].p_type == ELF_PType_Note;
        if (!is_note) {
          continue;
        }

        U64 off  = pass == 0 ? bin->shdrs.v[idx].sh_offset : bin->phdrs.v[idx].p_offset;
        U64 size = pass == 0 ? bin->shdrs.v[idx].sh_size   : bin->phdrs.v[idx].p_filesz;
        if (off > raw_data.size || size > raw_data.size - off) {
          continue;
        }

        Temp note_temp = temp_begin(scratch.arena);

        ELF_NoteList notes = elf_parse_note(scratch.arena, str8_substr(raw_data, r1u64(off, off + size)), elf_class, bin->hdr.e_machine);
        for EachNode(n, ELF_NoteNode, notes.first) {
          if (n->v.type == type && str8_match(n->v.owner, owner, 0) && n->v.desc.size != 0) {
            result = n->v.desc;
            break;
          }
        }

        temp_end(note_temp);
      }
    }

    scratch_end(scratch);
  }

  return result;
}

internal ELF_NoteList
elf_parse_note(Arena *arena, String8 raw_note, ELF_Class elf_class, ELF_MachineKind e_machine)
{
  ELF_NoteList result = {0};
  for (U64 cursor = 0; cursor < raw_note.size; ) {
    U32 owner_size;
    U64 owner_size_size = str8_deserial_read_struct(raw_note, cursor, &owner_size);
    if (owner_size_size == 0) { goto exit; }
    cursor += owner_size_size;
    
    U32 desc_size;
    U64 desc_size_size = str8_deserial_read_struct(raw_note, cursor, &desc_size);
    if (desc_size_size == 0) { goto exit; }
    cursor += desc_size_size;
    
    ELF_NoteType type;
    U64 type_size = str8_deserial_read_struct(raw_note, cursor, &type);
    if (type_size == 0) { goto exit; }
    cursor += type_size;
    
    U64 owner_padded_size = AlignPow2((U64)owner_size, 4);
    if (owner_padded_size > raw_note.size - cursor) { goto exit; }
    String8 owner = str8_cstring_capped(raw_note.str + cursor, raw_note.str + cursor + owner_size);
    cursor += owner_padded_size;
    
    U64 desc_padded_size = AlignPow2((U64)desc_size, 4);
    if (desc_size > raw_note.size - cursor) { goto exit; }
    String8 desc = str8_substr(raw_note, r1u64(cursor, cursor + desc_size));
    cursor += Min(desc_padded_size, raw_note.size - cursor);
    
    ELF_NoteNode *n = push_array(arena, ELF_NoteNode, 1);
    n->v.owner = owner;
    n->v.desc  = desc;
    n->v.type  = type;
    
    SLLQueuePush(result.first, result.last, n);
    result.count += 1;
  }
  exit:;
  return result;
}

#endif // ELF_PARSE_C
