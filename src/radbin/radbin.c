// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Generated Code

#include "radbin/generated/radbin.meta.c"

////////////////////////////////
// Breakpad File Paths

internal String8
rb_breakpad_file_path(String8 prefix, String8 path)
{
  if (prefix.size == 0 || path.size <= prefix.size ||
      !str8_match(str8_prefix(path, prefix.size), prefix, StringMatchFlag_CaseInsensitive|StringMatchFlag_SlashInsensitive)) {
    return path;
  }
  return str8_skip(path, prefix.size);
}

////////////////////////////////
// Breakpad Module Identity

internal String8
rb_breakpad_cpu_from_arch(Arch arch)
{
  String8 result = {0};
  switch (arch) {
  case Arch_x64:   result = s("x86_64"); break;
  case Arch_x86:   result = s("x86");    break;
  case Arch_arm64: result = s("arm64");  break;
  default: break;
  }
  return result;
}

internal Arch
rb_pdb_arch_from_stream_table(Arena *arena, String8 data, MSF_RawStreamTable *streams)
{
  String8        dbi_data = msf_data_from_stream_number(arena, data, streams, PDB_FixedStream_Dbi);
  PDB_DbiParsed *dbi      = pdb_dbi_from_data(arena, dbi_data);
  return arch_from_coff_machine(dbi->machine_type);
}

internal String8
rb_breakpad_debug_id_from_guid_age(Arena *arena, Guid guid, U32 age)
{
  return str8f(arena, "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%X",
               guid.data1, guid.data2, guid.data3,
               guid.data4[0], guid.data4[1], guid.data4[2], guid.data4[3],
               guid.data4[4], guid.data4[5], guid.data4[6], guid.data4[7], age);
}

internal B32
rb_breakpad_elf_text_identifier(String8 data, ELF_Bin *bin, U8 identifier[16])
{
  ELF_Class elf_class = bin->hdr.e_ident[ELF_Identifier_Class];
  if (bin->hdr.e_ident[ELF_Identifier_Data] != ELF_Data_2LSB ||
      (elf_class != ELF_Class_32 && elf_class != ELF_Class_64)) {
    return 0;
  }

  U64 entry_size = elf_shdr_size_from_class(elf_class);
  if (data.size < elf_hdr_size_from_class(elf_class) ||
      bin->hdr.e_shentsize != entry_size || bin->hdr.e_shoff > data.size ||
      bin->shdrs.count > (data.size - bin->hdr.e_shoff)/entry_size ||
      bin->sh_name_range.min > bin->sh_name_range.max || bin->sh_name_range.max > data.size) {
    return 0;
  }

  // use Breakpad's section preference and fold at most one page of code
  String8 names[] = {s(".text"), s("text")};
  for EachElement(name_idx, names) {
    for EachIndex(section_idx, bin->shdrs.count) {
      ELF_Shdr64 *section = &bin->shdrs.v[section_idx];
      if (section->sh_type != ELF_ShType_ProgBits ||
          !str8_match(elf_name_from_shdr64(data, bin, section), names[name_idx], 0)) {
        continue;
      }
      if (section->sh_size == 0 || (section->sh_flags & ELF_Shf_Compressed) ||
          section->sh_offset > data.size || section->sh_size > data.size - section->sh_offset) {
        return 0;
      }

      MemoryZero(identifier, 16);
      U64 size = Min(section->sh_size, 4096);
      for EachIndex(idx, size) {
        identifier[idx % 16] ^= data.str[section->sh_offset + idx];
      }
      return 1;
    }
  }
  return 0;
}

internal RB_BreakpadIdentity
rb_breakpad_identity_from_inputs(Arena *arena, ExecutableImageKind exe_kind, String8 exe_name, String8 exe_data, String8 dbg_data, B32 dbg_is_pdb)
{
  Temp scratch = scratch_begin(&arena, 1);

  RB_BreakpadIdentity result = {0};
  if (exe_kind == ExecutableImageKind_Macho) {
    MACHO_Bin exe_bin = macho_bin_from_data(scratch.arena, exe_data);
    MACHO_Bin dbg_bin = exe_bin;

    if (exe_data.str != dbg_data.str || exe_data.size != dbg_data.size) {
      dbg_bin = macho_bin_from_data(scratch.arena, dbg_data);
    }

    if (! exe_bin.valid                         ||
        ! dbg_bin.valid                         ||
        ! exe_bin.has_uuid || !dbg_bin.has_uuid ||
        ! MemoryMatch(exe_bin.uuid, dbg_bin.uuid, sizeof(exe_bin.uuid)) ||
        exe_bin.arch != dbg_bin.arch) {
      log_user_errorf("Mach-O executable and dSYM must have matching architectures and UUIDs.\n");
      goto done;
    }

    result.os       = s("mac");
    result.cpu      = rb_breakpad_cpu_from_arch(exe_bin.arch);
    result.debug_id = str8(push_array_no_zero(arena, U8, 33), 33);

    for EachIndex(idx, 16) {
      result.debug_id.str[2*idx]   = "0123456789ABCDEF"[exe_bin.uuid[idx] >> 4];
      result.debug_id.str[2*idx+1] = "0123456789ABCDEF"[exe_bin.uuid[idx] & 15];
    }
    result.debug_id.str[32] = '0';
  }
  else if (exe_kind == ExecutableImageKind_Elf32 || exe_kind == ExecutableImageKind_Elf64) {
    result.os = s("linux");

    ELF_Bin exe_bin       = elf_bin_from_data(scratch.arena, exe_data);
    ELF_Bin dbg_bin       = exe_bin;
    String8 build_id      = elf_note_desc_from_bin(exe_data, &exe_bin, s("GNU"), GNU_NoteType_BuildId);
    B32     separate_dbg  = exe_data.str != dbg_data.str || exe_data.size != dbg_data.size;
   
    result.cpu = rb_breakpad_cpu_from_arch(arch_from_elf_machine(exe_bin.hdr.e_machine));
    if (result.cpu.size == 0) {
      log_user_errorf("unsupported ELF machine %u\n", exe_bin.hdr.e_machine);
      goto done;
    }

    if (separate_dbg) {
      dbg_bin = elf_bin_from_data(scratch.arena, dbg_data);

      if (exe_bin.hdr.e_machine != dbg_bin.hdr.e_machine ||
          exe_bin.hdr.e_ident[ELF_Identifier_Class] != dbg_bin.hdr.e_ident[ELF_Identifier_Class]) {
        log_user_errorf("ELF executable and debug file architectures do not match.\n");
        goto done;
      }

      String8 dbg_build_id = elf_note_desc_from_bin(dbg_data, &dbg_bin, s("GNU"), GNU_NoteType_BuildId);
      if (build_id.size != 0 && dbg_build_id.size != 0 && !str8_match(build_id, dbg_build_id, 0)) {
        log_user_errorf("ELF executable and debug file build IDs do not match.\n");
        goto done;
      }
      
      if (build_id.size == 0) {
        build_id = dbg_build_id;
      }
    }

    U8 text_identifier[16] = {0};
    String8 identifier = build_id;
    if (identifier.size == 0 &&
        (rb_breakpad_elf_text_identifier(exe_data, &exe_bin, text_identifier) ||
         (separate_dbg && rb_breakpad_elf_text_identifier(dbg_data, &dbg_bin, text_identifier)))) {
      identifier = str8(text_identifier, sizeof(text_identifier));
    }

    if (identifier.size) {
      // debug ID uses GUID byte ordering plus age zero; CODE_ID preserves the
      // complete build ID, or the raw 16-byte text fallback when no ID exists
      Guid guid = {0};
      MemoryCopy(&guid, identifier.str, Min(sizeof(guid), identifier.size));
      result.debug_id = rb_breakpad_debug_id_from_guid_age(arena, guid, 0);
      result.code_id  = str8(push_array_no_zero(arena, U8, identifier.size*2), identifier.size*2);
      for EachIndex(idx, identifier.size) {
        result.code_id.str[idx*2 + 0] = "0123456789ABCDEF"[identifier.str[idx] >> 4];
        result.code_id.str[idx*2 + 1] = "0123456789ABCDEF"[identifier.str[idx] & 15];
      }
    }
  } else if (exe_kind == ExecutableImageKind_CoffPe || dbg_is_pdb) {
    result.os = s("windows");

    Guid guid     = {0};
    U32  age      = 0;
    B32  got_guid = 0;
    Arch pdb_arch = Arch_Null;
    if (dbg_is_pdb) {
      MSF_RawStreamTable *st        = msf_raw_stream_table_from_data(scratch.arena, dbg_data);
      String8             info_data = msf_data_from_stream_number(scratch.arena, dbg_data, st, PDB_FixedStream_Info);
      PDB_InfoParse       info      = {0};
      pdb_info_parse_from_data(info_data, &info);
      pdb_arch = rb_pdb_arch_from_stream_table(scratch.arena, dbg_data, st);
      result.cpu = rb_breakpad_cpu_from_arch(pdb_arch);

      if (info.version >= PDB_InfoVersion_VC70_DEP) {
        guid     = info.guid;
        age      = info.age;
        got_guid = 1;
      }
    }

    if (exe_data.size) {
      PE_BinInfo pe = pe_bin_info_from_data(scratch.arena, exe_data);
      if (pdb_arch != Arch_Null && pe.arch != Arch_Null && pdb_arch != pe.arch) {
        log_user_errorf("PE executable and PDB architectures do not match.\n");
        goto done;
      }
      if (pe.arch != Arch_Null) {
        result.cpu = rb_breakpad_cpu_from_arch(pe.arch);
      }

      if (pe.image_size != 0 && exe_name.size != 0) {
        result.code_id   = str8f(arena, "%08X%X", pe.timestamp, pe.image_size);
        result.code_file = str8_skip_last_slash(exe_name);
      }

      if (pe.data_dir_count > PE_DataDirectoryIndex_DEBUG) {
        Rng1U64 range = pe.data_dir_franges[PE_DataDirectoryIndex_DEBUG];
        if (range.min <= range.max && range.max <= exe_data.size && dim_1u64(range) >= sizeof(PE_DebugDirectory)) {
          PE_DebugInfoList debug_info = pe_debug_info_list_from_raw_debug_dir(scratch.arena, exe_data, str8_substr(exe_data, range));
          for EachNode(n, PE_DebugInfoNode, debug_info.first) {
            if (n->v.cv_pdb70_header.magic == PE_CODEVIEW_PDB70_MAGIC &&
                n->v.header.size >= sizeof(PE_CvHeaderPDB70) &&
                n->v.header.foff <= exe_data.size && n->v.header.size <= exe_data.size - n->v.header.foff) {
              if (got_guid && (!MemoryMatchStruct(&guid, &n->v.guid) || age != n->v.age)) {
                log_user_errorf("PE executable and PDB GUID/age do not match.\n");
                goto done;
              }

              guid     = n->v.guid;
              age      = n->v.age;
              got_guid = 1;

              break;
            }
          }
        }
      }
    }

    if(got_guid) {
      result.debug_id = rb_breakpad_debug_id_from_guid_age(arena, guid, age);
    }
  }
  done:;

  scratch_end(scratch);
  return result;
}

////////////////////////////////
// Breakpad Image Symbols

internal B32
rb_elf_symbol_is_code(ELF_Bin *bin, ELF_Symbol *symbol)
{
  ELF_Sym64  *raw  = &symbol->raw;
  ELF_SymBind bind = ELF_ST_BIND(raw->st_info);
  ELF_SymType type = ELF_ST_TYPE(raw->st_info);
  if ( ! symbol->name_valid || symbol->name.size == 0 ||
       (bind != ELF_SymBind_Local && bind != ELF_SymBind_Global && bind != ELF_SymBind_Weak) ||
       (type != ELF_SymType_Func && type != ELF_SymType_GNU_IFunc &&
        !(type == ELF_SymType_NoType && (bind == ELF_SymBind_Global || bind == ELF_SymBind_Weak))) ||
       raw->st_shndx == ELF_SectionIndex_Undef || raw->st_shndx >= ELF_SectionIndex_LoReserve ||
       raw->st_shndx >= bin->shdrs.count) {
    return 0;
  }

  ELF_Shdr64 *section = &bin->shdrs.v[raw->st_shndx];
  U64         flags   = ELF_Shf_Alloc | ELF_Shf_ExecInstr;
  if ((section->sh_flags & flags) != flags || section->sh_size == 0 ||
      section->sh_size > max_U64 - section->sh_addr || raw->st_value < section->sh_addr) {
    return 0;
  }

  U64 off = raw->st_value - section->sh_addr;
  return off < section->sh_size && raw->st_size <= section->sh_size - off;
}

internal B32
rb_macho_symbol_is_code(MACHO_Bin *bin, MACHO_Symbol *symbol)
{
  // ignore STABS, undefined, absolute, indirect, and non-instruction symbols
  if ( ! symbol->name_valid || symbol->name.size == 0 ||
       (symbol->type & 0xe0) != 0 || (symbol->type & 0x0e) != 0x0e ||
       symbol->sect == 0 || symbol->sect > bin->section_count) {
    return 0;
  }

  MACHO_Section *section = &bin->sections[symbol->sect - 1];
  return ! section->is_debug && (section->flags & (0x80000000u | 0x400u)) != 0 && contains_1u64(section->vrange, symbol->value);
}

internal B32
rb_public_symbol_is_before(void *a, void *b)
{
  RB_PublicSymbol *l = a, *r = b;
  return l->voff < r->voff || (l->voff == r->voff && l->order < r->order);
}

internal B32
rb_func_range_is_before(void *a, void *b)
{
  Rng1U64 *l = a, *r = b;
  return l->min < r->min || (l->min == r->min && l->max < r->max);
}

internal String8List
rb_breakpad_public_records(Arena              *arena,
                          RDI_Parsed          *rdi,
                          ExecutableImageKind  exe_kind,
                          U64                  base_vaddr,
                          String8              exe_data,
                          String8              dbg_data)
{
  String8List result = {0};

  B32 is_elf   = exe_kind == ExecutableImageKind_Elf32 || exe_kind == ExecutableImageKind_Elf64;
  B32 is_macho = exe_kind == ExecutableImageKind_Macho;
  if ( ! is_elf && ! is_macho) {
    return result;
  }

  Temp scratch = scratch_begin(&arena, 1);

  String8           sources[]        = {exe_data, dbg_data};
  ELF_Bin           elf_bins[2]      = {0};
  MACHO_Bin         macho_bins[2]    = {0};
  ELF_SymbolArray   elf_symbols[2]   = {0};
  MACHO_SymbolArray macho_symbols[2] = {0};
  U64               source_counts[2] = {0};
  U64               capacity         = 0;
  for EachElement(source_idx, sources) {
    if (sources[source_idx].size == 0 ||
        (source_idx != 0 && sources[0].str == sources[source_idx].str && sources[0].size == sources[source_idx].size)) {
      continue;
    }

    if (is_elf) {
      elf_bins[source_idx] = elf_bin_from_data(scratch.arena, sources[source_idx]);

      ELF_Bin *bin          = &elf_bins[source_idx];
      B32      linked_image = bin->hdr.e_type == ELF_Type_Exec || bin->hdr.e_type == ELF_Type_Dyn;
      if (bin->hdr.e_type >= ELF_Type_LoOs && bin->hdr.e_type < ELF_Type_LoProc) {
        for EachIndex(idx, bin->phdrs.count) {
          linked_image |= bin->phdrs.v[idx].p_type == ELF_PType_Load;
        }
      }

      if ( ! linked_image) {
        continue;
      }

      elf_symbols[source_idx]   = elf_symbols_from_bin(scratch.arena, sources[source_idx], bin);
      source_counts[source_idx] = elf_symbols[source_idx].count;
    } else {
      macho_bins[source_idx]    = macho_bin_from_data(scratch.arena, sources[source_idx]);
      macho_symbols[source_idx] = macho_symbols_from_bin(scratch.arena, sources[source_idx], &macho_bins[source_idx]);
      source_counts[source_idx] = macho_symbols[source_idx].count;
    }
    if (source_counts[source_idx] > max_U64/sizeof(RB_PublicSymbol) - capacity) {
      log_user_errorf("Too many image symbols for Breakpad PUBLIC output.\n");
      scratch_end(scratch);
      return result;
    }
    capacity += source_counts[source_idx];
  }

  if (capacity != 0) {
    // cover exactly the bounding ranges used by emitted FUNC records, not all
    // scopes or CU ranges; PUBLIC is only a fallback where FUNC has no coverage
    U64          procedure_count = 0;
    RDI_Symbol  *procedures      = rdi_table_from_name(rdi, Procedures, &procedure_count);
    Rng1U64Array covered         = {0};
    covered.v = push_array_no_zero(scratch.arena, Rng1U64, procedure_count);
    for EachIndex(idx, procedure_count) {
      RDI_Scope *scope = rdi_element_from_name_idx(rdi, Scopes, procedures[idx].root_scope_idx);
      if (scope->voff_range_opl > scope->voff_range_first) {
        U64 min = *rdi_element_from_name_idx(rdi, ScopeVOffData, scope->voff_range_first);
        U64 max = *rdi_element_from_name_idx(rdi, ScopeVOffData, scope->voff_range_opl - 1);
        if (min < max) {
          covered.v[covered.count++] = r1u64(min, max);
        }
      }
    }

    if (covered.count > 1) {
      radsort(covered.v, covered.count, rb_func_range_is_before);
    }

    U64 merged_count = 0;
    for EachIndex(idx, covered.count) {
      Rng1U64 range = covered.v[idx];
      if (merged_count != 0 && range.min <= covered.v[merged_count - 1].max) {
        covered.v[merged_count - 1].max = Max(covered.v[merged_count - 1].max, range.max);
      } else {
        covered.v[merged_count++] = range;
      }
    }
    covered.count = merged_count;

    RB_PublicSymbol *symbols = push_array_no_zero(scratch.arena, RB_PublicSymbol, capacity);
    U64              count   = 0;
    for EachElement(source_idx, sources) {
      for EachIndex(idx, source_counts[source_idx]) {
        B32 is_code = is_elf
                      ? rb_elf_symbol_is_code(&elf_bins[source_idx], &elf_symbols[source_idx].v[idx])
                      : rb_macho_symbol_is_code(&macho_bins[source_idx], &macho_symbols[source_idx].v[idx]);
        if ( ! is_code) {
          continue;
        }
        U64 vaddr = is_elf ? elf_symbols[source_idx].v[idx].raw.st_value : macho_symbols[source_idx].v[idx].value;
        if (vaddr < base_vaddr || rng1u64_array_num_from_value__binary_search(&covered, vaddr - base_vaddr) != 0) {
          continue;
        }
        String8 name = is_elf ? elf_symbols[source_idx].v[idx].name : macho_symbols[source_idx].v[idx].name;
        if (is_macho && name.size != 0 && name.str[0] == '_') {
          name = str8_skip(name, 1); // remove exactly one Mach-O ABI prefix; no demangling
        }
        B32 single_line = name.size != 0;
        for EachIndex(char_idx, name.size) {
          if (name.str[char_idx] == '\r' || name.str[char_idx] == '\n') {
            single_line = 0;
            break;
          }
        }
        if (single_line) {
          symbols[count] = (RB_PublicSymbol){vaddr - base_vaddr, count, name};
          count += 1;
        }
      }
    }

    if (count > 1) {
      radsort(symbols, count, rb_public_symbol_is_before);
    }

    for (U64 idx = 0; idx < count;) {
      U64 end     = idx + 1;
      B32 aliases = 0;
      for (; end < count && symbols[end].voff == symbols[idx].voff; end += 1) {
        aliases |= ! str8_match(symbols[idx].name, symbols[end].name, 0);
      }
      str8_list_pushf(arena, &result, "PUBLIC %s%I64x 0 %S\n", aliases ? "m " : "", symbols[idx].voff, symbols[idx].name);
      idx = end;
    }
  }

  scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ rjf: Top-Level Entry Points

internal void
rb_entry_point(CmdLine *cmdline)
{
  Temp scratch = scratch_begin(0, 0);
  U64 threads_count = get_system_info()->logical_processor_count;
  String8 threads_count_from_cmdline_string = cmd_line_string(cmdline, str8_lit("thread_count"));
  if(threads_count_from_cmdline_string.size != 0)
  {
    U64 threads_count_from_cmdline = 0;
    if(try_u64_from_str8_c_rules(threads_count_from_cmdline_string, &threads_count_from_cmdline))
    {
      threads_count = threads_count_from_cmdline;
    }
  }
  Thread *threads = push_array(scratch.arena, Thread, threads_count);
  RB_ThreadParams *threads_params = push_array(scratch.arena, RB_ThreadParams, threads_count);
  Barrier barrier = barrier_alloc(threads_count);
  U64 broadcast_val = 0;
  for EachIndex(idx, threads_count)
  {
    threads_params[idx].cmdline = cmdline;
    threads_params[idx].lane_ctx.lane_idx         = idx;
    threads_params[idx].lane_ctx.lane_count       = threads_count;
    threads_params[idx].lane_ctx.barrier          = barrier;
    threads_params[idx].lane_ctx.broadcast_memory = &broadcast_val;
    threads[idx] = thread_launch(rb_thread_entry_point, &threads_params[idx]);
  }
  for EachIndex(idx, threads_count)
  {
    thread_join(threads[idx], max_U64);
  }
  scratch_end(scratch);
}

internal void
rb_thread_entry_point(void *p)
{
  RB_ThreadParams *params = (RB_ThreadParams *)p;
  CmdLine *cmdline = params->cmdline;
  LaneCtx lctx = params->lane_ctx;
  ThreadNameF("radbin_thread_%I64u", lctx.lane_idx);
  lane_ctx(lctx);
  Arena *arena = arena_alloc();
  Arena *bake_arena = 0;
  Log *log = log_alloc();
  log_select(log);
  log_scope_begin();
  
  //////////////////////////////
  //- rjf: set up shared state
  //
  if(lane_idx() == 0)
  {
    rb_shared = push_array(arena, RB_Shared, 1);
  }
  lane_sync();
  
  //////////////////////////////
  //- rjf: analyze & load command line input files
  //
  ProfScope("analyze & load command line input files") if(lane_idx() == 0)
  {
    String8 working_directory = get_current_path(arena);
    String8List input_file_path_tasks = str8_list_copy(arena, &cmdline->inputs);
    for(String8Node *n = input_file_path_tasks.first; n != 0; n = n->next)
    {
      //////////////////////////
      //- rjf: possibly relative -> absolute path
      //
      String8 input_file_path = n->string;
      {
        PathStyle path_style = path_style_from_str8(n->string);
        if(path_style == PathStyle_Relative)
        {
          input_file_path = path_normalized_from_string(arena, str8f(arena, "%S/%S", working_directory, n->string));
        }
      }
      
      //////////////////////////
      //- rjf: do thin analysis of file
      //
      RB_FileFormat file_format = RB_FileFormat_Null;
      RB_FileFormatFlags file_format_flags = 0;
      ProfScope("do thin analysis of file")
      {
        File file = file_open(AccessFlag_Read|AccessFlag_ShareRead, input_file_path);
        FileProperties props = properties_from_file(file);
        
        //- rjf: PDB magic -> PDB input
        if(file_format == RB_FileFormat_Null)
        {
          U8 msf20_magic_maybe[sizeof(msf_msf20_magic)] = {0};
          file_read(file, r1u64(0, sizeof(msf20_magic_maybe)), msf20_magic_maybe);
          if(MemoryMatch(msf20_magic_maybe, msf_msf20_magic, sizeof(msf20_magic_maybe)))
          {
            file_format = RB_FileFormat_PDB;
          }
        }
        if(file_format == RB_FileFormat_Null)
        {
          U8 msf70_magic_maybe[sizeof(msf_msf70_magic)] = {0};
          file_read(file, r1u64(0, sizeof(msf70_magic_maybe)), msf70_magic_maybe);
          if(MemoryMatch(msf70_magic_maybe, msf_msf70_magic, sizeof(msf70_magic_maybe)))
          {
            file_format = RB_FileFormat_PDB;
          }
        }
        
        //- rjf: PE magic -> PE input
        if(file_format == RB_FileFormat_Null)
        {
          PE_DosHeader dos_header_maybe = {0};
          file_read_struct(file, 0, &dos_header_maybe);
          if(dos_header_maybe.magic == PE_DOS_MAGIC)
          {
            U32 pe_magic_maybe = 0;
            file_read_struct(file, dos_header_maybe.coff_file_offset, &pe_magic_maybe);
            if(pe_magic_maybe == PE_MAGIC)
            {
              file_format = RB_FileFormat_PE;
            }
          }
        }
        
        //- rjf: COFF archive magic -> COFF archive input
        if(file_format == RB_FileFormat_Null)
        {
          U8 coff_archive_sig_maybe[sizeof(g_coff_archive_sig)] = {0};
          file_read(file, r1u64(0, sizeof(coff_archive_sig_maybe)), coff_archive_sig_maybe);
          if(MemoryMatch(coff_archive_sig_maybe, g_coff_archive_sig, sizeof(g_coff_archive_sig)))
          {
            file_format = RB_FileFormat_COFF_Archive;
          }
        }
        if(file_format == RB_FileFormat_Null)
        {
          U8 coff_thin_archive_sig_maybe[sizeof(g_coff_thin_archive_sig)] = {0};
          file_read(file, r1u64(0, sizeof(coff_thin_archive_sig_maybe)), coff_thin_archive_sig_maybe);
          if(MemoryMatch(coff_thin_archive_sig_maybe, g_coff_thin_archive_sig, sizeof(g_coff_thin_archive_sig)))
          {
            file_format = RB_FileFormat_COFF_ThinArchive;
          }
        }
        
        //- rjf: COFF obj magic -> COFF obj input
        if(file_format == RB_FileFormat_Null)
        {
          COFF_BigObjHeader header_maybe = {0};
          file_read_struct(file, 0, &header_maybe);
          if(header_maybe.sig1 == COFF_MachineType_Unknown &&
             header_maybe.sig2 == max_U16 &&
             header_maybe.version >= 2 &&
             MemoryMatch(header_maybe.magic, g_coff_big_header_magic, sizeof(header_maybe.magic)))
          {
            file_format = RB_FileFormat_COFF_BigOBJ;
          }
        }
        if(file_format == RB_FileFormat_Null)
        {
          Temp scratch = scratch_begin(&arena, 1);
          COFF_FileHeader header_maybe = {0};
          file_read_struct(file, 0, &header_maybe);
          U64 section_count = header_maybe.section_count;
          U64 section_hdr_opl_off = sizeof(header_maybe) + section_count*sizeof(COFF_SectionHeader);
          
          // rjf: check if machine type is valid
          B32 machine_type_is_valid = 0;
          switch(header_maybe.machine)
          {
            case COFF_MachineType_Unknown:
            case COFF_MachineType_X86:    case COFF_MachineType_X64:
            case COFF_MachineType_Am33:   case COFF_MachineType_Arm:
            case COFF_MachineType_Arm64:  case COFF_MachineType_ArmNt:
            case COFF_MachineType_Ebc:    case COFF_MachineType_Ia64:
            case COFF_MachineType_M32R:   case COFF_MachineType_Mips16:
            case COFF_MachineType_MipsFpu:case COFF_MachineType_MipsFpu16:
            case COFF_MachineType_PowerPc:case COFF_MachineType_PowerPcFp:
            case COFF_MachineType_R4000:  case COFF_MachineType_RiscV32:
            case COFF_MachineType_RiscV64:case COFF_MachineType_RiscV128:
            case COFF_MachineType_Sh3:    case COFF_MachineType_Sh3Dsp:
            case COFF_MachineType_Sh4:    case COFF_MachineType_Sh5:
            case COFF_MachineType_Thumb:  case COFF_MachineType_WceMipsV2:
            {
              machine_type_is_valid = 1;
            }break;
          }
          
          // rjf: check if sections are valid
          B32 sections_are_valid = 0;
          if(machine_type_is_valid)
          {
            if(props.size >= section_hdr_opl_off)
            {
              COFF_SectionHeader *section_hdrs = push_array(scratch.arena, COFF_SectionHeader, section_count);
              file_read(file, r1u64(sizeof(header_maybe), sizeof(header_maybe) + section_count*sizeof(COFF_SectionHeader)), section_hdrs);
              B32 section_ranges_valid = 1;
              for EachIndex(section_hdr_idx, section_count)
              {
                COFF_SectionHeader *hdr = &section_hdrs[section_hdr_idx];
                if(!(hdr->flags & COFF_SectionFlag_CntUninitializedData))
                {
                  U64 min = hdr->foff;
                  U64 max = min + hdr->fsize;
                  if(hdr->fsize > 0 && !(section_hdr_opl_off <= min && min <= max && max <= props.size))
                  {
                    section_ranges_valid = 0;
                    break;
                  }
                }
              }
              sections_are_valid = section_ranges_valid;
            }
          }
          
          // rjf: check if symbol table is valid
          B32 symbol_table_is_valid = 0;
          if(sections_are_valid)
          {
            U64 symbol_table_off = header_maybe.symbol_table_foff;
            U64 symbol_table_size = sizeof(COFF_Symbol16)*header_maybe.symbol_count;
            U64 symbol_table_opl_off = symbol_table_off+symbol_table_size;
            if(symbol_table_off == 0 && symbol_table_size == 0)
            {
              symbol_table_off = section_hdr_opl_off;
              symbol_table_opl_off = section_hdr_opl_off;
            }
            symbol_table_is_valid = (section_hdr_opl_off <= symbol_table_off &&
                                     symbol_table_off <= symbol_table_opl_off &&
                                     symbol_table_opl_off <= props.size);
          }
          
          // rjf: symbol table is valid -> is COFF OBJ
          if(symbol_table_is_valid)
          {
            file_format = RB_FileFormat_COFF_OBJ;
          }
          
          scratch_end(scratch);
        }
        
        //- rjf: ELF magic -> ELF input
        if(file_format == RB_FileFormat_Null)
        {
          U8 identifier_maybe[ELF_Identifier_Max] = {0};
          file_read(file, r1u64(0, sizeof(identifier_maybe)), identifier_maybe);
          B32 is_elf_magic = (identifier_maybe[ELF_Identifier_Mag0] == 0x7f &&
                              identifier_maybe[ELF_Identifier_Mag1] == 'E'  &&
                              identifier_maybe[ELF_Identifier_Mag2] == 'L'  &&
                              identifier_maybe[ELF_Identifier_Mag3] == 'F');
          if(is_elf_magic)
          {
            file_format = (identifier_maybe[ELF_Identifier_Class] == ELF_Class_64) ? RB_FileFormat_ELF64 : RB_FileFormat_ELF32;
          }
        }
        
        // Mach-O magic -> Mach-O input (unsupported variants get explicit errors)
        if (file_format == RB_FileFormat_Null) {
          U32 magic = 0;
          file_read_struct(file, 0, &magic);
          if (macho_is_magic(magic)) {
            file_format = RB_FileFormat_Macho;
          }
        }

        //- rjf: RDI magic -> RDI input
        if(file_format == RB_FileFormat_Null)
        {
          RDI_Header rdi_header_maybe = {0};
          file_read_struct(file, 0, &rdi_header_maybe);
          if(rdi_header_maybe.magic == RDI_MAGIC_CONSTANT)
          {
            file_format = RB_FileFormat_RDI;
          }
        }
        
        file_close(file);
      }
      
      //////////////////////////
      //- rjf: log file recognition
      //
      if(file_format != RB_FileFormat_Null)
      {
        log_infof("%S recognized as %S\n", n->string, rb_file_format_display_name_table[file_format]);
      }
      else
      {
        log_infof("%S was not recognized as a supported format.\n", n->string);
      }
      
      //////////////////////////
      //- rjf: load recognized files
      //
      File file = {0};
      FileMap file_map = {0};
      String8 file_data = {0};
      if(file_format != RB_FileFormat_Null) ProfScope("load recognized file")
      {
        file = file_open(AccessFlag_ShareRead|AccessFlag_Read, input_file_path);
        file_map = file_map_open(AccessFlag_Read, file);
        FileProperties props = properties_from_file(file);
        void *base = file_map_view_open(file_map, AccessFlag_Read, r1u64(0, props.size));
        if(base != 0)
        {
          file_data = str8((U8 *)base, props.size);
        }
      }
      
      //////////////////////////
      //- rjf: PE format => generate new implicit path tasks for PDBs
      //
      if(file_format == RB_FileFormat_PE) ProfScope("PE file => generate task for PDB")
      {
        Temp scratch = scratch_begin(&arena, 1);
        String8 file_path = input_file_path;
        PE_BinInfo pe_bin_info = pe_bin_info_from_data(scratch.arena, file_data);
        String8 raw_debug_dir = str8_substr(file_data, pe_bin_info.data_dir_franges[PE_DataDirectoryIndex_DEBUG]);
        PE_DebugInfoList debug_dir = pe_debug_info_list_from_raw_debug_dir(scratch.arena, file_data, raw_debug_dir);
        for(PE_DebugInfoNode *n = debug_dir.first; n != 0; n = n->next)
        {
          if(n->v.path.size != 0)
          {
            log_infof("Found reference to separate debug info file in %S (%S) at %S\n", file_path, rb_file_format_display_name_table[file_format], n->v.path);
            String8 ext_path_absolute = n->v.path;
            PathStyle ext_path_style = path_style_from_str8(ext_path_absolute);
            if(ext_path_style == PathStyle_Relative)
            {
              ext_path_absolute = path_absolute_dst_from_relative_dst_src(arena, ext_path_absolute, str8_chop_last_slash(input_file_path));
            }
            if ( ! file_path_exists(ext_path_absolute)) {
              String8 sibling_path = str8f(arena, "%S/%S", str8_chop_last_slash(input_file_path), str8_skip_last_slash(n->v.path));
              if (file_path_exists(sibling_path)) {
                ext_path_absolute = sibling_path;
              }
            }
            str8_list_push(arena, &input_file_path_tasks, ext_path_absolute);
          }
        }
        scratch_end(scratch);
      }
      
      //////////////////////////
      //- rjf: ELF format => generate new implicit path tasks for debug files
      //
      if(file_format == RB_FileFormat_ELF32 ||
         file_format == RB_FileFormat_ELF64)
      {
        Temp scratch = scratch_begin(&arena, 1);
        ELF_Bin bin = elf_bin_from_data(scratch.arena, file_data);
        ELF_GnuDebugLink debug_link = elf_gnu_debug_link_from_bin(file_data, &bin);
        if(debug_link.path.size != 0)
        {
          log_infof("Found reference to separate debug info file in %S (%S) at %S\n", n->string, rb_file_format_display_name_table[file_format], debug_link.path);
          String8 ext_path_absolute = debug_link.path;
          PathStyle ext_path_style = path_style_from_str8(ext_path_absolute);
          if(ext_path_style == PathStyle_Relative)
          {
            ext_path_absolute = path_absolute_dst_from_relative_dst_src(arena, ext_path_absolute, str8_chop_last_slash(input_file_path));
          }
          str8_list_push(arena, &input_file_path_tasks, ext_path_absolute);
        }
        scratch_end(scratch);
      }
      
      //////////////////////////
      //- rjf: PE => check if contains DWARF
      //
      if(file_format == RB_FileFormat_PE)
      {
        Temp scratch = scratch_begin(&arena, 1);
        PE_BinInfo pe_bin_info = pe_bin_info_from_data(scratch.arena, file_data);
        String8 raw_section_table = str8_substr(file_data, pe_bin_info.section_table_range);
        String8 string_table = str8_substr(file_data, pe_bin_info.string_table_range);
        U64 section_count = raw_section_table.size / sizeof(COFF_SectionHeader);
        COFF_SectionHeader *section_table = (COFF_SectionHeader *)raw_section_table.str;
        if(dw_is_dwarf_present_coff_section_table(string_table, section_count, section_table))
        {
          log_infof("DWARF data detected in %S (%S)\n", n->string, rb_file_format_display_name_table[file_format]);
          file_format_flags |= RB_FileFormatFlag_HasDWARF;
        }
        scratch_end(scratch);
      }
      
      //////////////////////////
      //- rjf: COFF OBJ => check if contains DWARF
      //
      if(file_format == RB_FileFormat_COFF_OBJ || file_format == RB_FileFormat_COFF_BigOBJ)
      {
        Temp scratch = scratch_begin(&arena, 1);
        COFF_FileHeaderInfo coff_obj      = coff_file_header_info_from_data(file_data);
        String8             string_table  = str8_substr(file_data, coff_obj.string_table_range);
        U64                 section_count = coff_obj.section_count_no_null;
        COFF_SectionHeader *section_table = (COFF_SectionHeader *)str8_substr(file_data, coff_obj.section_table_range).str;
        if(dw_is_dwarf_present_coff_section_table(string_table, section_count, section_table))
        {
          log_infof("DWARF data detected in %S (%S)\n", n->string, rb_file_format_display_name_table[file_format]);
          file_format_flags |= RB_FileFormatFlag_HasDWARF;
        }
        scratch_end(scratch);
      }
      
      //////////////////////////
      //- rjf: ELF => check if contains DWARF
      //
      if(file_format == RB_FileFormat_ELF32 ||
         file_format == RB_FileFormat_ELF64)
      {
        Temp scratch = scratch_begin(&arena, 1);
        ELF_Bin elf_bin = elf_bin_from_data(scratch.arena, file_data);
        if(dw_is_dwarf_present_from_elf_bin(file_data, &elf_bin))
        {
          log_infof("DWARF data detected in %S (%S)\n", n->string, rb_file_format_display_name_table[file_format]);
          file_format_flags |= RB_FileFormatFlag_HasDWARF;
        }
        scratch_end(scratch);
      }
      
      //////////////////////////
      // Mach-O => validate and check for usable DWARF
      if (file_format == RB_FileFormat_Macho) {
        Temp scratch = scratch_begin(&arena, 1);

        MACHO_Bin bin = macho_bin_from_data(scratch.arena, file_data);
        if ( ! bin.valid) {
          log_user_errorf("%S: %S\n", input_file_path, bin.error);
        } else {
          DW_Raw raw = rb_dw_raw_from_macho_bin(file_data, &bin);
          if (raw.sec[DW_SectionKind_Info].data.size != 0 && raw.sec[DW_SectionKind_Abbrev].data.size != 0) {
            file_format_flags |= RB_FileFormatFlag_HasDWARF;
            log_infof("DWARF data detected in %S (Mach-O)\n", input_file_path);
          }
        }

        scratch_end(scratch);
      }

      //- rjf: ELF => check if contains EH frame info
      //
      if(file_format == RB_FileFormat_ELF32 ||
         file_format == RB_FileFormat_ELF64)
      {
        Temp scratch = scratch_begin(&arena, 1);
        ELF_Bin elf_bin = elf_bin_from_data(scratch.arena, file_data);
        for EachIndex(sect_idx, elf_bin.hdr.e_shnum)
        {
          ELF_Shdr64 *shdr = &elf_bin.shdrs.v[sect_idx];
          String8 name = elf_name_from_shdr64(file_data, &elf_bin, shdr);
          if(str8_match(name, str8_lit(".eh_frame_hdr"), 0))
          {
            log_infof("EH frame data detected in %S (%S)\n", n->string, rb_file_format_display_name_table[file_format]);
            file_format_flags |= RB_FileFormatFlag_HasEhFrame;
            break;
          }
        }
        scratch_end(scratch);
      }
      
      //////////////////////////
      //- rjf: push to list
      //
      {
        RB_File *f = push_array(arena, RB_File, 1);
        f->format       = file_format;
        f->format_flags = file_format_flags;
        f->path         = input_file_path;
        f->file         = file;
        f->file_map     = file_map;
        f->data         = file_data;
        RB_FileNode *file_n = push_array(arena, RB_FileNode, 1);
        file_n->v = f;
        SLLQueuePush(rb_shared->input_files.first, rb_shared->input_files.last, file_n);
        rb_shared->input_files.count += 1;
      }
    }
  }
  lane_sync();
  RB_FileList input_files = rb_shared->input_files;
  
  //////////////////////////////
  //- rjf: bucket input files by format
  //
  ProfScope("bucket input files by format") if(lane_idx() == 0)
  {
    for(RB_FileNode *n = input_files.first; n != 0; n = n->next)
    {
      RB_FileNode *file_n = push_array(arena, RB_FileNode, 1);
      file_n->v = n->v;
      SLLQueuePush(rb_shared->input_files_from_format_table[n->v->format].first, rb_shared->input_files_from_format_table[n->v->format].last, file_n);
      rb_shared->input_files_from_format_table[n->v->format].count += 1;
    }
  }
  lane_sync();
  RB_FileList *input_files_from_format_table = rb_shared->input_files_from_format_table;
  
  //////////////////////////////
  //- rjf: unpack which kind of output we're producing, and to where
  //
  typedef enum OutputKind
  {
    OutputKind_Null,
    OutputKind_RDI,
    OutputKind_Dump,
    OutputKind_Breakpad,
    OutputKind_Stripped,
    OutputKind_VOff2Line,
    OutputKind_COUNT
  }
  OutputKind;
  struct
  {
    String8 arg;
    String8 title;
  }
  output_kind_info[] =
  {
    {str8_lit_comp(""),          str8_lit_comp("")},
    {str8_lit_comp("rdi"),       str8_lit_comp("RAD Debug Info (.rdi) Conversion")},
    {str8_lit_comp("dump"),      str8_lit_comp("Textual Dumping")},
    {str8_lit_comp("breakpad"),  str8_lit_comp("Breakpad Debug Info Conversion")},
    {str8_lit_comp("stripped"),  str8_lit_comp("Stripped RAD Debug Info (.rdi) Conversion")},
    {str8_lit_comp("voff2line"), str8_lit_comp("Virtual Offset -> Line Mapping")},
  };
  OutputKind output_kind = OutputKind_Null;
  String8 output_path = cmd_line_string(cmdline, str8_lit("out"));
  {
    //- rjf: user manually specified output kind
    if(output_kind == OutputKind_Null)
    {
      for EachNonZeroEnumVal(OutputKind, k)
      {
        if(cmd_line_has_flag(cmdline, output_kind_info[k].arg))
        {
          log_infof("User specified --%S; performing `%S`\n", output_kind_info[k].arg, output_kind_info[k].title);
          output_kind = k;
          break;
        }
      }
    }
    
    //- rjf: we can infer from the user-specified output path
    if(output_kind == OutputKind_Null)
    {
      if(str8_match(str8_skip_last_dot(output_path), str8_lit("rdi"), StringMatchFlag_CaseInsensitive))
      {
        output_kind = OutputKind_RDI;
        log_infof("Output path has .rdi extension; performing `%S`\n", output_kind_info[output_kind].title);
      }
      else if(str8_match(str8_skip_last_dot(output_path), str8_lit("dump"), StringMatchFlag_CaseInsensitive) ||
              str8_match(str8_skip_last_dot(output_path), str8_lit("txt"), StringMatchFlag_CaseInsensitive))
      {
        output_kind = OutputKind_Dump;
        log_infof("Output path has .dump or .txt extension; performing `%S`\n", output_kind_info[output_kind].title);
      }
      else if(str8_match(str8_skip_last_dot(output_path), str8_lit("psym"), StringMatchFlag_CaseInsensitive) ||
              str8_match(str8_skip_last_dot(output_path), str8_lit("psyms"), StringMatchFlag_CaseInsensitive))
      {
        output_kind = OutputKind_Breakpad;
        log_infof("Output path has .psym or .psyms extension; performing `%S`\n", output_kind_info[output_kind].title);
      }
    }
  }
  
  //////////////////////////////
  //- rjf: print help preamble
  //
  if(lane_idx() == 0)
  {
    if(output_kind == OutputKind_Null || cmdline->inputs.node_count == 0)
    {
      fprintf(stderr, "%s\n", BUILD_TITLE);
      fprintf(stderr, "%s\n\n", BUILD_VERSION_STRING_LITERAL);
      if(output_kind != OutputKind_Null)
      {
        fprintf(stderr, "%.*s Help\n", str8_varg(output_kind_info[output_kind].title));
        fprintf(stderr, "To see top-level options for radbin, run the binary with no arguments.\n\n");
      }
      fprintf(stderr, "-------------------------------------------------------------------------------\n\n");
    }
  }
  lane_sync();
  
  //////////////////////////////
  //- rjf: perform operation based on output kind
  //
  String8List output_blobs = {0};
  B32 output_valid = output_kind != OutputKind_Breakpad;
  switch(output_kind)
  {
    ////////////////////////////
    //- rjf: no output kind -> nothing to do. output help information to user
    //
    default:
    case OutputKind_Null:
    if(lane_idx() == 0)
    {
      fprintf(stderr, "USAGE EXAMPLES\n\n");
      
      fprintf(stderr, "radbin --rdi program.exe\n");
      fprintf(stderr, "Find the debug info for `program.exe`, if any, and converts it to RDI.\n\n");
      
      fprintf(stderr, "radbin program.pdb --out:program.rdi\n");
      fprintf(stderr, "Converts `program.pdb` to RDI, and stores it in `program.rdi`.\n\n");
      
      fprintf(stderr, "radbin --dump program.rdi\n");
      fprintf(stderr, "Outputs the textual dump of the debug information stored in `program.rdi`.\n\n");
      
      fprintf(stderr, "-------------------------------------------------------------------------------\n\n");
      
      fprintf(stderr, "DESCRIPTION\n\n");
      fprintf(stderr, "This utility provides a number of operations relating to executable image and\n");
      fprintf(stderr, "debug information formats. It can convert debug information to the RAD Debug\n");
      fprintf(stderr, "Info (.rdi) format. It can also parse and dump textualized contents of several\n");
      fprintf(stderr, "formats, including PDB, PE, ELF, Mach-O, and RDI. It also supports converting debug\n");
      fprintf(stderr, "information to the textual Breakpad format.\n\n");
      
      fprintf(stderr, "-------------------------------------------------------------------------------\n\n");
      
      fprintf(stderr, "ARGUMENTS\n\n");
      
      fprintf(stderr, "--rdi            Specifies that the utility should convert debug information\n");
      fprintf(stderr, "                 data to the RAD Debug Info (.rdi) format.\n\n");
      
      fprintf(stderr, "--dump           Specifies that the utility should dump textualized contents of\n");
      fprintf(stderr, "                 all input files.\n\n");
      
      fprintf(stderr, "--breakpad       Specifies that the utility should convert debug information\n");
      fprintf(stderr, "                 data to the textual Breakpad format.\n\n");
      fprintf(stderr, "--breakpad_strip_path_prefix:<path>  Strip a directory prefix from Breakpad FILE paths.\n\n");

      fprintf(stderr, "--stripped       Converts the same debug information subset as --breakpad,\n");
      fprintf(stderr, "                 but writes RAD Debug Info (.rdi) instead of Breakpad text.\n\n");
      
      fprintf(stderr, "--voff2line      Specifies that the utility should map a virtual offset to a\n");
      fprintf(stderr, "                 line.\n\n");
      
      fprintf(stderr, "--out:<path>     Specifies the path to which output data should be written. If\n");
      fprintf(stderr, "                 not specified, the utility will choose a fallback. If dumping\n");
      fprintf(stderr, "                 textual contents, the utility will write to `stdout`. If\n");
      fprintf(stderr, "                 converting to another format, the utility will form a path by\n");
      fprintf(stderr, "                 changing the extension of input files accordingly.\n\n");
      
      fprintf(stderr, "--deterministic  Turns off all sources of non-determinism in generated output,\n");
      fprintf(stderr, "                 like build names, versions, and dates.\n\n");
      
      fprintf(stderr, "--verbose        Outputs all log information collected during execution.\n\n");
      
      fprintf(stderr, "There are also operation-specific arguments. To see them, run this binary with\n");
      fprintf(stderr, "the operation selected, with no additional inputs (e.g. `radbin --rdi`).\n\n");
    }break;
    
    ////////////////////////////
    //- rjf: RDI, Breakpad, Debug Info Operations -> conversion based on inputs
    //
    case OutputKind_RDI:
    case OutputKind_Breakpad:
    case OutputKind_Stripped:
    case OutputKind_VOff2Line:
    {
      //- rjf: no inputs => help
      if(lane_idx() == 0 && cmdline->inputs.node_count == 0) switch(output_kind)
      {
        default:
        case OutputKind_RDI:
        {
          fprintf(stderr, "ARGUMENTS\n\n");
          
          fprintf(stderr, "--compress                       Compresses the RDI file's contents with LZB.\n");
          fprintf(stderr, "--compress-zstd[:LEVEL]          Uses Zstd (default level 9); overrides --compress.\n");
          fprintf(stderr, "                                 Negative levels favor speed; 0 selects Zstd's default (3).\n");
          fprintf(stderr, "\n");
          fprintf(stderr, "--only:<comma delimited names>   Specifies that only the named subsets of debug\n");
          fprintf(stderr, "                                 information should be generated. See below for\n");
          fprintf(stderr, "                                 a list of valid debug info subset names.\n");
          fprintf(stderr, "\n");
          fprintf(stderr, "--omit:<comma delimited names>   Specifies that the named subsets of debug\n");
          fprintf(stderr, "                                 information should not be generated. See below\n");
          fprintf(stderr, "                                 for a list of valid debug info subset names.\n");
          fprintf(stderr, "\n");
          
          fprintf(stderr, "-------------------------------------------------------------------------------\n\n");
          
          fprintf(stderr, "RAD DEBUG INFO SUBSET NAMES\n\n");
#define X(name, name_lower) fprintf(stderr, " - " #name_lower "\n");
#define Y(name, name_lower) X(name, name_lower)
          RDIM_Subset_XList
#undef X
#undef Y
        }break;
        case OutputKind_Breakpad:
        {
          fprintf(stderr, "All input files specified on the command line will be dumped. The following\n");
          fprintf(stderr, "formats are currently supported: PE, COFF, RDI, ELF, and thin 64-bit Mach-O\n\n");
          fprintf(stderr, "Pass the DWARF input file for a dSYM bundle.\n\n");
          fprintf(stderr, "--breakpad_strip_path_prefix:<path>\n");
          fprintf(stderr, "  Match a directory prefix case-insensitively, ignoring slash direction,\n");
          fprintf(stderr, "  then remove it without changing the remaining path.\n");
          fprintf(stderr, "  Unmatched paths are unchanged; no filesystem lookups are performed.\n");
          fprintf(stderr, "  Example: \"-breakpad_strip_path_prefix:c:\\devel\\projects\"\n\n");
        }break;
        case OutputKind_Stripped:
        {
          fprintf(stderr, "Converts native debug information to a stripped version of RDI,\n");
          fprintf(stderr, "that has can be used to symbolize callstacks.\n\n");
          fprintf(stderr, "Existing RDI inputs are not supported for stripping.\n\n");
          fprintf(stderr, "--compress       Compresses the RDI file's contents with LZB.\n");
          fprintf(stderr, "--compress-zstd[:LEVEL]  Uses Zstd (default level 9); overrides --compress.\n");
          fprintf(stderr, "                         Negative levels favor speed; 0 selects Zstd's default (3).\n\n");
          fprintf(stderr, "Example: radbin --stripped program.pdb --out:program.stripped.rdi\n\n");
        }break;
        case OutputKind_VOff2Line:
        {
          fprintf(stderr, "ARGUMENTS\n\n");
          fprintf(stderr, "--voff:<offset>    Specifies the virtual offset to map to a source line.\n");
          fprintf(stderr, "\n");
        }break;
      }
      
      if (output_kind == OutputKind_Stripped && cmdline->inputs.node_count == 0) {
        output_valid = 0;
        break;
      }

      if (output_kind == OutputKind_Stripped && input_files_from_format_table[RB_FileFormat_RDI].count != 0) {
        if (lane_idx() == 0) {
          log_user_errorf("--stripped does not support existing RDI inputs; provide native debug information instead.\n");
        }
        output_valid = 0;
        break;
      }

      B32 breakpad_subset = output_kind == OutputKind_Breakpad || output_kind == OutputKind_Stripped;
      String8 breakpad_strip_prefix = cmd_line_string(cmdline, s("breakpad_strip_path_prefix"));
      if (output_kind == OutputKind_Breakpad &&
          cmd_line_has_flag(cmdline, s("breakpad_strip_path_prefix")) && breakpad_strip_prefix.size == 0) {
        if (lane_idx() == 0) {
          log_user_errorf("--breakpad_strip_path_prefix requires a nonempty directory prefix.\n");
        }
        break;
      }

      //- rjf: unpack subset flags
      RDIM_SubsetFlags subset_flags = 0xffffffff;
      switch(output_kind)
      {
        default:{}break;
        case OutputKind_RDI:
        {
          String8List only_names = cmd_line_strings(cmdline, str8_lit("only"));
          if(only_names.node_count != 0)
          {
            subset_flags = 0;
          }
          for(String8Node *n = only_names.first; n != 0; n = n->next)
          {
            if(0){}
#define X(name, name_lower) else if(str8_match(n->string, str8_lit(#name_lower), 0)) { subset_flags |= RDIM_SubsetFlag_##name; }
#define Y(name, name_lower) X(name, name_lower)
            RDIM_Subset_XList
#undef X
#undef Y
          }
          String8List omit_names = cmd_line_strings(cmdline, str8_lit("omit"));
          for(String8Node *n = omit_names.first; n != 0; n = n->next)
          {
            if(0){}
#define X(name, name_lower) else if(str8_match(n->string, str8_lit(#name_lower), 0)) { subset_flags &= ~RDIM_SubsetFlag_##name; }
#define Y(name, name_lower) X(name, name_lower)
            RDIM_Subset_XList
#undef X
#undef Y
          }
        }break;
        case OutputKind_Breakpad:
        case OutputKind_Stripped:
        {
          subset_flags = (RDIM_SubsetFlag_Units|RDIM_SubsetFlag_Procedures|RDIM_SubsetFlag_LineInfo|RDIM_SubsetFlag_InlineLineInfo|RDIM_SubsetFlag_Namespaces);
        }break;
        case OutputKind_VOff2Line:
        {
          subset_flags = (RDIM_SubsetFlag_Units|RDIM_SubsetFlag_LineInfo|RDIM_SubsetFlag_InlineLineInfo|RDIM_SubsetFlag_Procedures);
        }break;
      }

      B32 compress_zstd     = cmd_line_has_flag(cmdline, str8_lit("compress-zstd"));
      S32 compression_level = 9;
#if RDI_USE_ZSTD
      if((output_kind == OutputKind_RDI || output_kind == OutputKind_Stripped) && compress_zstd)
      {
        String8 level_string = cmd_line_string(cmdline, str8_lit("compress-zstd"));
        if(level_string.size != 0)
        {
          String8 digits = {0};
          S64 sign      = sign_from_str8(level_string, &digits);
          B32 valid     = digits.size <= 10 && level_string.size - digits.size <= 1 && str8_is_integer(digits, 10);
          S64 level     = valid ? sign*(S64)u64_from_str8(digits, 10) : 0;
          S32 min_level = ZSTD_minCLevel();
          S32 max_level = ZSTD_maxCLevel();
          if( ! valid || level < min_level || level > max_level)
          {
            if(lane_idx() == 0)
            {
              log_user_errorf("--compress-zstd expects a decimal level between %d and %d (0 selects Zstd's default).\n", min_level, max_level);
            }
            output_valid = 0;
            break;
          }
          compression_level = (S32)level;
        }
      }
#else
      if((output_kind == OutputKind_RDI || output_kind == OutputKind_Stripped) &&
         cmdline->inputs.node_count != 0 && cmd_line_has_flag(cmdline, str8_lit("compress-zstd")))
      {
        if(lane_idx() == 0) { log_user_errorf("--compress-zstd requires a build with RDI_USE_ZSTD enabled.\n"); }
        output_valid = 0;
        break;
      }
#endif

      // mixed native/RDI conversion is unsupported; otherwise a single RDI can
      // be silently ignored instead of being decoded and checked
      U64 rdi_input_count = input_files_from_format_table[RB_FileFormat_RDI].count;
      if(rdi_input_count != 0 && rdi_input_count != input_files.count)
      {
        if(lane_idx() == 0)
        {
          log_user_errorf("RDI inputs cannot be mixed with native inputs for conversion.\n");
        }
        output_valid = 0;
        break;
      }

      // Validate every RDI input before any joining/conversion work begins.
      U64 rdi_inputs_valid = 1;
      if(lane_idx() == 0)
      {
        for EachNode(n, RB_FileNode, input_files_from_format_table[RB_FileFormat_RDI].first)
        {
          RDI_Parsed rdi = {0};
          RDI_ParseStatus status = rdi_parse(n->v->data.str, n->v->data.size, &rdi);
          if(status != RDI_ParseStatus_Good)
          {
            log_user_errorf("RDI parse failure for %S (status %u)\n", n->v->path, status);
            rdi_inputs_valid = 0;
          }
        }
      }
      lane_sync_u64(&rdi_inputs_valid, 0);
      if( ! rdi_inputs_valid)
      {
        output_valid = 0;
        break;
      }

      // loose conversion objects are needed only until all lanes finish baking
      Arena *conversion_arena = arena_alloc();
      
      //- rjf: convert inputs to RDI info
      B32                 convert_done          = 0;
      RDIM_BakeParams     pdb_bake_params       = {0};
      RDIM_BakeParams     dwarf_bake_params     = {0};
      ExecutableImageKind breakpad_exe_kind     = ExecutableImageKind_Null;
      U64                 breakpad_base_vaddr   = 0;
      String8             breakpad_exe_name     = {0};
      String8             breakpad_exe_data     = {0};
      String8             breakpad_dbg_data     = {0};
      String8             breakpad_module_name  = {0};
      B32                 breakpad_dbg_is_pdb   = 0;
      B32                 breakpad_mixed_inputs = 0;
      typedef struct RDIM_BakeParamsNode RDIM_BakeParamsNode;
      struct RDIM_BakeParamsNode
      {
        RDIM_BakeParamsNode *next;
        RDIM_BakeParams v;
      };
      RDIM_BakeParamsNode *first_rdi_bake_params = 0;
      RDIM_BakeParamsNode *last_rdi_bake_params = 0;
      {
        //- rjf: PE inputs w/ DWARF, or ELF inputs => DWARF -> RDI conversion
        B32 pe_w_dwarf = (input_files_from_format_table[RB_FileFormat_PE].count != 0 &&
                          input_files_from_format_table[RB_FileFormat_PE].first->v->format_flags & RB_FileFormatFlag_HasDWARF);
        B32 elf_w_dwarf = (input_files_from_format_table[RB_FileFormat_ELF32].count != 0 ||
                           input_files_from_format_table[RB_FileFormat_ELF64].count != 0);
        if(pe_w_dwarf || elf_w_dwarf)
        {
          convert_done = 1;
          if(0){}
          else if(pe_w_dwarf)  { log_infof("PEs w/ DWARF specified; converting DWARF data to RDI\n"); }
          else if(elf_w_dwarf) { log_infof("ELFs specified; converting DWARF data to RDI\n"); }
          
          // rjf: convert
          D2R_ConvertParams convert_params = {0};
          {
            B32 got_exe = 0;
            B32 got_dbg = 0;
            String8 exe_name = {0};
            String8 exe_data = {0};
            ExecutableImageKind exe_kind = ExecutableImageKind_Null;
            String8 dbg_name = {0};
            String8 dbg_data = {0};
            if(!got_exe && !got_dbg)
            {
              for(RB_FileNode *n = input_files_from_format_table[RB_FileFormat_PE].first; n != 0; n = n->next)
              {
                if(n->v->format_flags & RB_FileFormatFlag_HasDWARF)
                {
                  got_exe = 1;
                  got_dbg = 1;
                  dbg_name = n->v->path;
                  dbg_data = n->v->data;
                  exe_name = n->v->path;
                  exe_data = n->v->data;
                  exe_kind = ExecutableImageKind_CoffPe;
                  break;
                }
              }
            }
            if(!got_exe)
            {
              for(RB_FileNode *n = input_files_from_format_table[RB_FileFormat_ELF32].first; n != 0; n = n->next)
              {
                got_exe = 1;
                exe_name = n->v->path;
                exe_data = n->v->data;
                exe_kind = ExecutableImageKind_Elf32;
                if(!(n->v->format_flags & RB_FileFormatFlag_HasDWARF))
                {
                  break;
                }
              }
            }
            if(!got_exe)
            {
              for(RB_FileNode *n = input_files_from_format_table[RB_FileFormat_ELF64].first; n != 0; n = n->next)
              {
                got_exe = 1;
                exe_name = n->v->path;
                exe_data = n->v->data;
                exe_kind = ExecutableImageKind_Elf64;
                if(!(n->v->format_flags & RB_FileFormatFlag_HasDWARF))
                {
                  break;
                }
              }
            }
            if(!got_dbg)
            {
              for(RB_FileNode *n = input_files_from_format_table[RB_FileFormat_ELF32].first; n != 0; n = n->next)
              {
                if(n->v->format_flags & RB_FileFormatFlag_HasDWARF)
                {
                  got_dbg = 1;
                  dbg_name = n->v->path;
                  dbg_data = n->v->data;
                  break;
                }
              }
            }
            if(!got_dbg)
            {
              for(RB_FileNode *n = input_files_from_format_table[RB_FileFormat_ELF64].first; n != 0; n = n->next)
              {
                if(n->v->format_flags & RB_FileFormatFlag_HasDWARF)
                {
                  got_dbg = 1;
                  dbg_name = n->v->path;
                  dbg_data = n->v->data;
                  break;
                }
              }
            }

            // a linked image without DWARF can still provide PUBLIC symbols
            if ( ! got_dbg && breakpad_subset) {
              dbg_name = exe_name;
              dbg_data = exe_data;
            }

            switch(exe_kind)
            {
              default:{}break;
              case ExecutableImageKind_CoffPe:
              {
                Temp scratch = scratch_begin(&arena, 1);
                PE_BinInfo pe = pe_bin_info_from_data(scratch.arena, exe_data);
                String8 raw_sections  = str8_substr(exe_data, pe.section_table_range);
                COFF_SectionHeader *section_table = str8_deserial_get_raw_ptr(raw_sections, 0, sizeof(COFF_SectionHeader) * pe.section_count);
                String8 string_table = str8_substr(exe_data, pe.string_table_range);
                convert_params.arch = pe.arch;
                convert_params.base_vaddr = pe.image_base;
                convert_params.raw = dw_raw_from_coff_section_table(conversion_arena, exe_data, string_table, pe.section_count, section_table);
                convert_params.path_style = PathStyle_WindowsAbsolute;
                convert_params.binary_sections = c2r_rdi_binary_sections_from_coff_sections(conversion_arena, exe_data, string_table, pe.section_count, section_table);
                scratch_end(scratch);
              }break;
              case ExecutableImageKind_Elf32:
              case ExecutableImageKind_Elf64:
              {
                Temp scratch = scratch_begin(&arena, 1);
                ELF_Bin bin = elf_bin_from_data(scratch.arena, dbg_data);
                DW_Raw *shared_raw = 0;
                convert_params.arch = arch_from_elf_machine(bin.hdr.e_machine);
                convert_params.base_vaddr = elf_base_addr_from_bin(&bin);
                if (lane_idx() == 0) {
                  shared_raw = push_array(conversion_arena, DW_Raw, 1);
                  *shared_raw = dw_raw_from_elf_bin(conversion_arena, dbg_data, &bin);
                }
                lane_sync_u64(&shared_raw, 0);
                convert_params.raw = *shared_raw;
                convert_params.path_style = PathStyle_UnixAbsolute;
                convert_params.binary_sections = e2r_rdi_binary_sections_from_elf_section_table(conversion_arena, dbg_data, &bin, &bin.shdrs);
                scratch_end(scratch);
              }break;
            }
            convert_params.exe_name           = exe_name;
            convert_params.exe_data           = exe_data;
            convert_params.subset_flags       = subset_flags;
            convert_params.deterministic      = cmd_line_has_flag(cmdline, str8_lit("deterministic"));
            convert_params.skip_exe_hash      = breakpad_subset;
            convert_params.single_symbol_name = breakpad_subset;
            breakpad_exe_kind    = exe_kind;
            breakpad_base_vaddr  = convert_params.base_vaddr;
            breakpad_exe_name    = exe_name;
            breakpad_exe_data    = exe_data;
            breakpad_dbg_data    = dbg_data;
            breakpad_module_name = exe_name;
          }
          ProfScope("convert") dwarf_bake_params = d2r_convert(conversion_arena, &convert_params);
        }
        
        // thin Mach-O / dSYM inputs => DWARF conversion
        if (input_files_from_format_table[RB_FileFormat_Macho].count != 0) {
          Temp scratch = scratch_begin(&arena, 1);

          RB_File   *exe_file  = 0;
          RB_File   *dbg_file  = 0;
          MACHO_Bin  exe_bin   = {0};
          MACHO_Bin  dbg_bin   = {0};
          B32        supported = ! convert_done;
          for EachEnumVal(RB_FileFormat, format) {
            if (format != RB_FileFormat_Macho && input_files_from_format_table[format].count != 0) {
              supported = 0;
            }
          }
          for EachNode(n, RB_FileNode, input_files_from_format_table[RB_FileFormat_Macho].first) {
            MACHO_Bin bin = macho_bin_from_data(scratch.arena, n->v->data);

            if (!bin.valid) {
              supported = 0;
              continue;
            }

            if (bin.file_type == 10) { // MH_DSYM
              if (dbg_file != 0) { supported = 0; }
              dbg_file = n->v;
              dbg_bin  = bin;
            } else {
              if (exe_file != 0) { supported = 0; }
              exe_file = n->v;
              exe_bin  = bin;
            }
          }

          if (exe_file == 0) {
            exe_file = dbg_file;
            exe_bin  = dbg_bin;
          }

          if (dbg_file == 0) {
            dbg_file = exe_file;
            dbg_bin  = exe_bin;
          }

          D2R_ConvertParams convert_params = {0};
          if (exe_file == 0 || dbg_file == 0) {
            supported = 0;
          } else {
            convert_params.raw = rb_dw_raw_from_macho_bin(dbg_file->data, &dbg_bin);
            supported = supported                                &&
                        exe_bin.arch == dbg_bin.arch             &&
                        exe_bin.base_vaddr == dbg_bin.base_vaddr &&
                        exe_bin.has_uuid                         &&
                        dbg_bin.has_uuid                         &&
                        MemoryMatch(exe_bin.uuid, dbg_bin.uuid, sizeof(exe_bin.uuid)) &&
                        (breakpad_subset ||
                         (convert_params.raw.sec[DW_SectionKind_Info].data.size != 0 && convert_params.raw.sec[DW_SectionKind_Abbrev].data.size != 0));

            if (supported) {
              convert_params.exe_name        = exe_file->path;
              convert_params.exe_data        = exe_file->data;
              convert_params.arch            = dbg_bin.arch;
              convert_params.base_vaddr      = dbg_bin.base_vaddr;
              convert_params.binary_sections = rb_rdi_binary_sections_from_macho_bin(conversion_arena, &dbg_bin);
              convert_params.path_style      = PathStyle_UnixAbsolute;
              convert_params.subset_flags    = subset_flags;
              convert_params.deterministic   = cmd_line_has_flag(cmdline, str8_lit("deterministic"));
              convert_params.skip_exe_hash   = breakpad_subset;
              convert_params.single_symbol_name = breakpad_subset;
              breakpad_exe_kind    = ExecutableImageKind_Macho;
              breakpad_base_vaddr  = convert_params.base_vaddr;
              breakpad_exe_name    = exe_file->path;
              breakpad_exe_data    = exe_file->data;
              breakpad_dbg_data    = dbg_file->data;
              breakpad_module_name = exe_file->path;
            }
          }

          if (supported) {
            convert_done = 1;
            log_infof("Mach-O DWARF specified; converting debug data\n");
            ProfScope("convert") dwarf_bake_params = d2r_convert(conversion_arena, &convert_params);
          } else {
            output_valid          = 0;
            breakpad_mixed_inputs = 1;
            if (lane_idx() == 0) {
              log_user_errorf("Provide one supported thin Mach-O with DWARF and a UUID, optionally with its matching executable; mixed formats are unsupported.\n");
            }
          }

          scratch_end(scratch);
        }

        //- rjf: PDB inputs => PDB -> RDI conversion
        if(input_files_from_format_table[RB_FileFormat_PDB].count != 0)
        {
          convert_done = 1;
          log_infof("PDBs specified; converting PDB data to RDI\n");
          
          // rjf: get EXE/PDB file data
          RB_File *exe_file = rb_file_list_first(&input_files_from_format_table[RB_FileFormat_PE]);
          RB_File *pdb_file = rb_file_list_first(&input_files_from_format_table[RB_FileFormat_PDB]);
          String8 exe_path  = exe_file->path;
          String8 pdb_path  = pdb_file->path;
          String8 exe_data  = exe_file->data;
          String8 pdb_data  = pdb_file->data;
          breakpad_mixed_inputs |= breakpad_exe_kind != ExecutableImageKind_Null;
          breakpad_exe_kind     = ExecutableImageKind_CoffPe;
          breakpad_exe_name     = exe_path;
          breakpad_exe_data     = exe_data;
          breakpad_dbg_data     = pdb_data;
          breakpad_module_name  = pdb_path;
          breakpad_dbg_is_pdb   = 1;
          
          // rjf: convert
          P2R_ConvertParams convert_params = {0};
          {
            convert_params.input_pdb_name = pdb_path;
            convert_params.input_exe_name = exe_path;
            convert_params.input_pdb_data = pdb_data;
            convert_params.input_exe_data = exe_data;
            convert_params.subset_flags   = subset_flags;
            convert_params.deterministic  = cmd_line_has_flag(cmdline, str8_lit("deterministic"));
            convert_params.single_symbol_name = breakpad_subset;
          }
          ProfScope("convert") pdb_bake_params = p2r_convert2(conversion_arena, &convert_params);
        }
        
        //- rjf: RDI inputs => RDI joining
        if(input_files_from_format_table[RB_FileFormat_RDI].count > 1)
        {
          breakpad_mixed_inputs = 1;
          convert_done = 1;
          log_infof("RDIs specified; joining RDIs\n");
          
          // parse and decode every input
          RDI_Parsed *rdis = 0;
          if(lane_idx() == 0)
          {
            rdis = push_array(conversion_arena, RDI_Parsed, input_files_from_format_table[RB_FileFormat_RDI].count);
            U64 rdi_idx = 0;
            for EachNode(n, RB_FileNode, input_files_from_format_table[RB_FileFormat_RDI].first)
            {
              RB_File *f = n->v;
              RDI_Parsed *rdi = &rdis[rdi_idx++];
              RDI_ParseStatus rdi_status = rdi_parse(f->data.str, f->data.size, rdi);
              if(rdi_status == RDI_ParseStatus_Good && rdi_has_compressed_sections(rdi))
              {
                U64 decompressed_size = rdi_decompressed_size_from_parsed(rdi);
                U8 *decompressed_data = decompressed_size != 0 ? push_array_no_zero(conversion_arena, U8, decompressed_size) : 0;
                rdi_status = rdi_decompress_parsed(decompressed_data, decompressed_size, rdi);
                if(rdi_status == RDI_ParseStatus_Good)
                {
                  rdi_status = rdi_parse(decompressed_data, decompressed_size, rdi);
                }
              }
              if(rdi_status != RDI_ParseStatus_Good)
              {
                *rdi = rdi_parsed_nil;
                rdi_inputs_valid = 0;
                log_user_errorf("RDI parse/decompression failure for %S (status %u)\n", f->path, rdi_status);
              }
            }
          }
          lane_sync_u64(&rdis, 0);
          lane_sync_u64(&rdi_inputs_valid, 0);

          // rjf: produce bake params for each RDI
          if(rdi_inputs_valid) for EachIndex(rdi_idx, input_files_from_format_table[RB_FileFormat_RDI].count)
          {
            // rjf: RDI -> loose
            RDIM_BakeParams rdi_loose = rdim_loose_from_rdi(conversion_arena, subset_flags, &rdis[rdi_idx]);
            
            // rjf: add
            RDIM_BakeParamsNode *n = push_array(conversion_arena, RDIM_BakeParamsNode, 1);
            n->v = rdi_loose;
            SLLQueuePush(first_rdi_bake_params, last_rdi_bake_params, n);
          }
        }
      }
      lane_sync();

      if( ! rdi_inputs_valid)
      {
        arena_release(conversion_arena);
        output_valid = 0;
        break;
      }

      //- rjf: join conversion artifacts
      RDIM_BakeParams *bake_params = 0;
      if(lane_idx() == 0)
      {
        bake_params = push_array(conversion_arena, RDIM_BakeParams, 1);
        rdim_bake_params_concat_in_place(bake_params, &pdb_bake_params);
        rdim_bake_params_concat_in_place(bake_params, &dwarf_bake_params);
        for EachNode(n, RDIM_BakeParamsNode, first_rdi_bake_params)
        {
          rdim_bake_params_concat_in_place(bake_params, &n->v);
        }
      }
      lane_sync_u64(&bake_params, 0);
      
      //- rjf: no output path? -> pick one based on input files
      if(output_path.size == 0)
      {
        String8 output_path__noext = {0};
        if(output_path__noext.size == 0) { output_path__noext = str8_chop_last_dot(rb_file_list_first(&input_files_from_format_table[RB_FileFormat_PDB])->path); }
        if(output_path__noext.size == 0) { output_path__noext = str8_chop_last_dot(rb_file_list_first(&input_files_from_format_table[RB_FileFormat_PE])->path); }
        if(output_path__noext.size == 0) { output_path__noext = str8_chop_last_dot(rb_file_list_first(&input_files_from_format_table[RB_FileFormat_ELF64])->path); }
        if(output_path__noext.size == 0) { output_path__noext = str8_chop_last_dot(rb_file_list_first(&input_files_from_format_table[RB_FileFormat_ELF32])->path); }
        if(output_path__noext.size == 0) { output_path__noext = str8_chop_last_dot(rb_file_list_first(&input_files_from_format_table[RB_FileFormat_Macho])->path); }
        if(output_path__noext.size == 0) { output_path__noext = str8_chop_last_dot(rb_file_list_first(&input_files_from_format_table[RB_FileFormat_RDI])->path); }
        switch(output_kind)
        {
          default:{}break;
          case OutputKind_RDI:
          case OutputKind_Stripped:
          {
            output_path = str8f(arena, "%S.rdi", output_path__noext);
          }break;
          case OutputKind_Breakpad:
          {
            output_path = str8f(arena, "%S.psym", output_path__noext);
          }break;
        }
      }
      
      //- rjf: special case: only a single RDI file passed? all conversion is trivially done, just
      // package up RDI data as serialized section bundle, and let the rest of the paths use it.
      B32 noop_conversion = 0;
      if(input_files.count == 1 && rb_file_list_first(&input_files)->format == RB_FileFormat_RDI)
      {
        noop_conversion = 1;
        convert_done = 1;
        log_infof("Single RDI specified; passing through (skipping conversion)");
      }
      
      //- rjf: no viable input paths
      if(!convert_done && cmdline->inputs.node_count != 0)
      {
        log_user_errorf("Could not load debug info from the specified inputs. You must provide a valid PDB file, a PE/ELF/Mach-O file with DWARF debug info, or RDI file(s).");
        if (output_kind == OutputKind_Stripped) {
          output_valid = 0;
        }
      }
      
      //- rjf: bake
      bake_arena = arena_alloc();
      RDIM_BakeResults bake_results = {0};
      ProfScope("bake")
      {
        bake_results = rdim_bake(bake_arena, bake_params);
      }

      ProfScope("release conversion data")
      {
        lane_sync();
        arena_release(conversion_arena);
        conversion_arena = 0;
        bake_params = 0;
        lane_sync();
      }
      
      //- rjf: serialize
      RDIM_SerializedSectionBundle *serialized_section_bundle = 0;
      ProfScope("serialize") if(lane_idx() == 0)
      {
        serialized_section_bundle = push_array(arena, RDIM_SerializedSectionBundle, 1);
        serialized_section_bundle[0] = bake_results.section_bundle;
      }
      lane_sync_u64(&serialized_section_bundle, 0);
      
      //- rjf: special case: no-op conversion (single RDI file)
      if(lane_idx() == 0 && noop_conversion)
      {
        RB_File *f = rb_file_list_first(&input_files);
        RDI_Parsed rdi = {0};
        RDI_ParseStatus rdi_status = rdi_parse(f->data.str, f->data.size, &rdi);
        if(rdi_status == RDI_ParseStatus_Good && rdi_has_compressed_sections(&rdi))
        {
          U64 decompressed_size = rdi_decompressed_size_from_parsed(&rdi);
          U8 *decompressed_data = decompressed_size != 0 ? push_array_no_zero(bake_arena, U8, decompressed_size) : 0;
          rdi_status = rdi_decompress_parsed(decompressed_data, decompressed_size, &rdi);
          if(rdi_status == RDI_ParseStatus_Good)
          {
            rdi_status = rdi_parse(decompressed_data, decompressed_size, &rdi);
          }
        }
        if(rdi_status != RDI_ParseStatus_Good)
        {
          rdi_inputs_valid = 0;
          log_user_errorf("RDI parse/decompression failure for %S (status %u)\n", f->path, rdi_status);
        }
        else for EachIndex(idx, rdi.sections_count)
        {
          if(idx < RDI_SectionKind_COUNT)
          {
            serialized_section_bundle->sections[idx] = rdim_serialized_section_make_unpacked(rdi.raw_data + rdi.sections[idx].off, rdi.sections[idx].unpacked_size);
          }
        }
      }
      lane_sync_u64(&rdi_inputs_valid, 0);
      if(!rdi_inputs_valid)
      {
        output_valid = 0;
        break;
      }

      // consumers share one independent flat RDI image
      Arena *flat_rdi_arena = 0;
      RDI_Parsed *flat_rdi = 0;
      if(convert_done && (output_kind == OutputKind_Breakpad || output_kind == OutputKind_VOff2Line))
      {
        ProfScope("flatten to RDI data")
        {
          if(lane_idx() == 0)
          {
            flat_rdi_arena = arena_alloc();
            Temp scratch = scratch_begin(&arena, 1);
            String8List rdi_blobs = rdim_file_blobs_from_section_bundle(scratch.arena, serialized_section_bundle);
            String8 rdi_data = str8_list_join(flat_rdi_arena, &rdi_blobs, 0);
            flat_rdi = push_array(flat_rdi_arena, RDI_Parsed, 1);
            RDI_ParseStatus rdi_status = rdi_parse(rdi_data.str, rdi_data.size, flat_rdi);
            if(rdi_status != RDI_ParseStatus_Good)
            {
              rdi_inputs_valid = 0;
              log_user_errorf("RDI parse failure after serialization (status %u)\n", rdi_status);
            }
            scratch_end(scratch);
          }
          lane_sync_u64(&flat_rdi, 0);
          lane_sync_u64(&rdi_inputs_valid, 0);
        }

        ProfScope("release baked sections")
        {
          arena_release(bake_arena);
          bake_arena = 0;
          lane_sync();
        }
      }

      if( ! rdi_inputs_valid)
      {
        if(flat_rdi_arena != 0)
        {
          arena_release(flat_rdi_arena);
        }
        output_valid = 0;
        break;
      }

      //- rjf: convert done => generate output
      if(convert_done) switch(output_kind)
      {
        default:{}break;
        
        //- rjf: generate RDI blobs
        case OutputKind_RDI:
        case OutputKind_Stripped:
        {
          // rjf: compress
          RDIM_SerializedSectionBundle serialized_section_bundle__compressed = serialized_section_bundle[0];
          if(compress_zstd || cmd_line_has_flag(cmdline, str8_lit("compress"))) ProfScope("compress")
          {
            RDI_SectionEncoding encoding = compress_zstd ? RDI_SectionEncoding_Zstd : RDI_SectionEncoding_LZB;
            serialized_section_bundle__compressed = rdim_compress(arena, serialized_section_bundle, encoding, compression_level);
          }
          
          // rjf: serialize
          String8List blobs = rdim_file_blobs_from_section_bundle(arena, &serialized_section_bundle__compressed);
          str8_list_concat_in_place(&output_blobs, &blobs);
        }break;
        
        //- rjf: generate breakpad text
        case OutputKind_Breakpad:
        {
          RDI_Parsed *rdi = flat_rdi;
          
          //- rjf: set up shared state
          typedef struct P2B_Shared P2B_Shared;
          struct P2B_Shared
          {
            String8List dump;
            String8List *lane_file_dumps;
            String8List *lane_func_dumps;
          };
          P2B_Shared *p2b_shared = 0;
          if(lane_idx() == 0)
          {
            p2b_shared = push_array(arena, P2B_Shared, 1);
            p2b_shared->lane_file_dumps = push_array(arena, String8List, lane_count());
            p2b_shared->lane_func_dumps = push_array(arena, String8List, lane_count());
          }
          lane_sync_u64(&p2b_shared, 0);
          
          //- rjf: dump MODULE record
          U64 module_valid = 0;
          if(lane_idx() == 0)
          {
            RDI_TopLevelInfo *tli = rdi_element_from_name_idx(rdi, TopLevelInfo, 0);

            // rjf: pick name to identify module
            String8 module_name_string = breakpad_module_name;
            if(module_name_string.size == 0) {
              module_name_string = str8_from_rdi_string_idx(rdi, tli->exe_name_string_idx);
            }
            if(module_name_string.size == 0 && input_files.first != 0) {
              module_name_string = input_files.first->v->path;
            }
            module_name_string = str8_skip_last_slash(module_name_string);

            String8 cpu = {0};
            switch(tli->arch)
            {
            case RDI_Arch_X64: cpu = str8_lit("x86_64"); break;
            case RDI_Arch_X86: cpu = str8_lit("x86");    break;
            }

            RB_BreakpadIdentity identity = {0};
            if( ! breakpad_mixed_inputs)
            {
              identity = rb_breakpad_identity_from_inputs(arena, breakpad_exe_kind, breakpad_exe_name, breakpad_exe_data, breakpad_dbg_data, breakpad_dbg_is_pdb);
            }
            if(identity.cpu.size != 0)
            {
              cpu = identity.cpu;
            }
            module_valid = identity.debug_id.size != 0 && cpu.size != 0 && module_name_string.size != 0;
            if(module_valid)
            {
              output_valid = 1;
              str8_list_pushf(arena, &p2b_shared->dump, "MODULE %S %S %S %S\n", identity.os, cpu, identity.debug_id, module_name_string);
              if(identity.code_id.size != 0)
              {
                if(identity.code_file.size != 0)
                {
                  str8_list_pushf(arena, &p2b_shared->dump, "INFO CODE_ID %S %S\n", identity.code_id, identity.code_file);
                }
                else
                {
                  str8_list_pushf(arena, &p2b_shared->dump, "INFO CODE_ID %S\n", identity.code_id);
                }
              }
            }
            else
            {
              if (breakpad_mixed_inputs) {
                log_user_errorf("Cannot emit Breakpad symbols: provide one module, not mixed input formats.\n");
              } else if (cpu.size == 0) {
                log_user_errorf("Cannot emit Breakpad symbols: unsupported or missing module CPU architecture.\n");
              } else if (identity.debug_id.size == 0) {
                log_user_errorf("Cannot emit Breakpad symbols: provide an ELF build ID or file-backed text, a Mach-O UUID, or a PDB/PE GUID and age.\n");
              } else {
                log_user_errorf("Cannot emit Breakpad symbols: missing module name.\n");
              }
              output_valid = 0;
            }
          }
          lane_sync_u64(&module_valid, 0);
          if( ! module_valid)
          {
            break;
          }
          
          //- rjf: dump FILE records
          ProfScope("dump FILE records")
          {
            String8 strip_prefix = breakpad_strip_prefix;
            if (strip_prefix.size != 0 && !char_is_slash(strip_prefix.str[strip_prefix.size - 1])) {
              strip_prefix = push_str8_cat(arena, strip_prefix, s("/"));
            }
            U64 count = 0;
            RDI_SourceFile *v = rdi_table_from_name(rdi, SourceFiles, &count);
            if(count > 0)
            {
              Rng1U64 range = lane_range(count-1);
              for EachInRange(idx, range)
              {
                String8List *out = &p2b_shared->lane_file_dumps[lane_idx()];
                Temp scratch = scratch_begin(&arena, 1);
                String8 src_path = str8_from_rdi_path_node_idx(scratch.arena, rdi, v[idx+1].file_path_node_idx);
                src_path = rb_breakpad_file_path(strip_prefix, src_path);
                str8_list_pushf(arena, out, "FILE %I64u %S\n", idx, src_path);
                scratch_end(scratch);
              }
            }
          }
          
          //- rjf: dump FUNC records
          ProfScope("dump FUNC records")
          {
            U64 count = 0;
            RDI_Symbol *v = rdi_table_from_name(rdi, Procedures, &count);
            Rng1U64 range = lane_range(count);
            for EachInRange(idx, range)
            {
              // NOTE(rjf): breakpad does not support multiple voff ranges per procedure.
              String8List *out = &p2b_shared->lane_func_dumps[lane_idx()];
              RDI_Symbol *proc = &v[idx];
              RDI_Scope *root_scope = rdi_element_from_name_idx(rdi, Scopes, proc->root_scope_idx);
              if(root_scope->voff_range_opl > root_scope->voff_range_first)
              {
                // rjf: dump function record
                RDIM_Rng1U64 voff_range =
                {
                  *rdi_element_from_name_idx(rdi, ScopeVOffData, root_scope->voff_range_first),
                  *rdi_element_from_name_idx(rdi, ScopeVOffData, root_scope->voff_range_opl - 1),
                };
                {
                  Temp scratch = scratch_begin(0, 0);

                  String8 name = {0};
                  if(str8_from_rdi_string_idx(rdi, proc->name_string_idx).size == 0)
                  {
                    name = str8_from_rdi_string_idx(rdi, proc->link_name_string_idx);
                  }
                  if(name.size == 0)
                  {
                    name = fully_qualified_str8_from_rdi_symbol(scratch.arena, rdi, proc);
                  }
                  str8_list_pushf(arena, out, "FUNC %I64x %I64x %I64x %S\n", voff_range.min, voff_range.max-voff_range.min, 0ull, name);
                  scratch_end(scratch);
                }
                
                // rjf: dump function lines
                RDI_Unit *unit = rdi_unit_from_voff(rdi, voff_range.min);
                RDI_LineTable *line_table = rdi_line_table_from_unit(rdi, unit);
                RDI_ParsedLineTable line_info = {0};
                rdi_parsed_from_line_table(rdi, line_table, &line_info);

                // find the interval containing the function start
                U64 first = rdi_line_info_idx_range_from_voff(&line_info, voff_range.min, 0);
                if(first == max_U64)
                {
                  first = 0;
                  if(line_info.count > 0 && voff_range.min >= line_info.voffs[line_info.count - 1])
                  {
                    first = line_info.count - 1;
                  }
                }

                // advance by index so empty intervals and sequence-end gaps cannot stop
                // the rest of the function; only emit nonempty spans clipped to the FUNC
                for(U64 line_info_idx = first > 0 ? first - 1 : 0;
                    line_info_idx < line_info.count && line_info.voffs[line_info_idx] < voff_range.max;
                    line_info_idx += 1)
                {
                  RDI_Line *line          = &line_info.lines[line_info_idx];
                  U64       line_voff_min = Max(line_info.voffs[line_info_idx], voff_range.min);
                  U64       line_voff_opl = Min(line_info.voffs[line_info_idx+1], voff_range.max);
                  if(line->file_idx != 0 && line_voff_min < line_voff_opl)
                  {
                    str8_list_pushf(arena, out, "%I64x %I64x %I64u %I64u\n",
                                    line_voff_min,
                                    line_voff_opl-line_voff_min,
                                    (U64)line->line_num,
                                    (U64)line->file_idx - 1);
                  }
                }
              }
            }
          }
          
          //- rjf: join
          lane_sync();
          if(lane_idx() == 0)
          {
            for EachIndex(ln_idx, lane_count())
            {
              str8_list_concat_in_place(&p2b_shared->dump, &p2b_shared->lane_file_dumps[ln_idx]);
            }
            for EachIndex(ln_idx, lane_count())
            {
              str8_list_concat_in_place(&p2b_shared->dump, &p2b_shared->lane_func_dumps[ln_idx]);
            }
            ProfScope("dump PUBLIC records") {
              String8List publics = rb_breakpad_public_records(arena, rdi, breakpad_exe_kind, breakpad_base_vaddr, breakpad_exe_data, breakpad_dbg_data);
              str8_list_concat_in_place(&p2b_shared->dump, &publics);
            }
          }
          lane_sync();
          output_blobs = p2b_shared->dump;
        }break;
        
        //- rjf: generate voff -> line results
        case OutputKind_VOff2Line:
        {
          //- rjf: unpack voff arg
          U64 voff = 0;
          {
            String8 voff_str = cmd_line_string(cmdline, str8_lit("voff"));
            try_u64_from_str8_c_rules(voff_str, &voff);
            log_infof("User specified \"%S\" as virtual offset; parsed as 0x%I64x", voff_str, voff);
          }
          
          RDI_Parsed *rdi = flat_rdi;
          
          //- rjf: voff -> line
          RDI_Line line = rdi_line_from_voff(rdi, voff);
          RDI_SourceFile *src_file = rdi_element_from_name_idx(rdi, SourceFiles, line.file_idx);
          
          //- rjf: dump line info
          {
            Temp scratch = scratch_begin(0, 0);
            RDI_Scope *voff_scope = rdi_scope_from_voff(rdi, voff);
            for(RDI_Scope *scope = voff_scope, *null_scope = rdi_element_from_name_idx(rdi, Scopes, 0);
                scope != 0 && scope != null_scope;
                scope = rdi_parent_from_scope(rdi, scope))
            {
              RDI_InlineSite *inline_site = rdi_inline_site_from_scope(rdi, scope);
              RDI_InlineSite *null_inline_site = rdi_element_from_name_idx(rdi, InlineSites, 0);
              if(inline_site && inline_site != null_inline_site)
              {
                RDI_LineTable *inline_line_table = rdi_element_from_name_idx(rdi, LineTables, inline_site->line_table_idx);
                RDI_LineTable *null_inline_line_table = rdi_element_from_name_idx(rdi, LineTables, 0);
                if(inline_line_table && inline_line_table != null_inline_line_table)
                {
                  String8         inline_name          = str8_from_rdi_string_idx(rdi, inline_site->name_string_idx);
                  RDI_Line        inline_line          = rdi_line_from_line_table_voff(rdi, inline_line_table, voff);
                  RDI_SourceFile *inline_src_file      = rdi_element_from_name_idx(rdi, SourceFiles, inline_line.file_idx);
                  RDI_SourceFile *null_inline_src_file = rdi_element_from_name_idx(rdi, SourceFiles, 0);
                  if(inline_src_file && inline_src_file != null_inline_src_file)
                  {
                    String8 path = str8_from_rdi_path_node_idx(scratch.arena, rdi, src_file->file_path_node_idx);
                    str8_list_pushf(arena, &output_blobs, "[inlined] %S %S:%u\n", inline_name, path, inline_line.line_num);
                  }
                }
              }
            }
            
            String8 path = str8_from_rdi_path_node_idx(scratch.arena, rdi, src_file->file_path_node_idx);
            str8_list_pushf(arena, &output_blobs, "%S:%u\n", path, line.line_num);
            scratch_end(scratch);
          }
        }break;
      }

      // formatting copied all text into output arenas; wait for every reader before
      // lane zero releases the shared RDI image
      lane_sync();
      if(flat_rdi_arena != 0)
      {
        arena_release(flat_rdi_arena);
      }
    }break;
    
    ////////////////////////////
    //- rjf: dump -> textual dump of inputs
    //
    case OutputKind_Dump:
    {
      B32 deterministic = cmd_line_has_flag(cmdline, str8_lit("deterministic"));
      
      //- rjf: no inputs => help
      if(lane_idx() == 0 && cmdline->inputs.node_count == 0)
      {
        fprintf(stderr, "All input files specified on the command line will be dumped. Currently, only\n");
        fprintf(stderr, "RDI files are supported.\n\n");
        
        fprintf(stderr, "-------------------------------------------------------------------------------\n\n");
        
        fprintf(stderr, "ARGUMENTS\n\n");
        
        fprintf(stderr, "--only:<comma delimited names>   Specifies that only the named subsets should\n");
        fprintf(stderr, "                                 be dumped. See below for a list of valid\n");
        fprintf(stderr, "                                 subset names.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "--omit:<comma delimited names>   Specifies that the named subsets should not\n");
        fprintf(stderr, "                                 be dumped. See below for a list of valid\n");
        fprintf(stderr, "                                 subset names.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "--verbose                        dump more format details\n");
        fprintf(stderr, "\n");
        
        fprintf(stderr, "-------------------------------------------------------------------------------\n\n");
        
        fprintf(stderr, "RDI Sections:\n");
#define X(name, name_lower, title) fprintf(stderr, " - " #name_lower "\n");
        RDI_DumpSubset_XList
#undef X
        fprintf(stderr, "\n");
        
        fprintf(stderr, "-------------------------------------------------------------------------------\n\n");
        
        fprintf(stderr, "DWARF Sections:\n");
#define X(_N, _L, ...) if(DW_SectionKind_##_N != DW_SectionKind_Null) { fprintf(stderr, " - " _L "\n"); }
        DW_SectionKind_XList
#undef X
        fprintf(stderr, "\n");
      }
      
      B32 verbose = cmd_line_has_flag(cmdline, str8_lit("verbose"));
      
      //- rjf: unpack dump subset flags
      RDI_DumpSubsetFlags rdi_dump_subset_flags = RDI_DumpSubsetFlag_All;
      DW_SectionFlags     dw_dump_subset_flags  = DW_SectionFlag_All;
      EH_DumpSubsetFlags  eh_dump_subset_flags  = EH_DumpSubsetFlag_All;
      ELF_DumpSubsetFlags elf_dump_subset_flags = ELF_DumpSubsetFlag_All;
      {
        String8List only_names = cmd_line_strings(cmdline, str8_lit("only"));
        if(only_names.node_count != 0)
        {
          rdi_dump_subset_flags = 0;
          dw_dump_subset_flags = 0;
          eh_dump_subset_flags = 0;
        }
        for(String8Node *n = only_names.first; n != 0; n = n->next)
        {
          if(0){}
#define X(name, name_lower, title) else if(str8_match(n->string, str8_lit(#name_lower), 0)) { rdi_dump_subset_flags |= RDI_DumpSubsetFlag_##name; }
          RDI_DumpSubset_XList
#undef X
#define X(_N, _L, ...) else if(str8_match(n->string, str8_skip(str8_lit(_L), 1), 0)) { dw_dump_subset_flags |= DW_SectionFlag_##_N; }
          DW_SectionKind_XList
#undef X
#define X(name, name_lower, title) else if(str8_match(n->string, str8_lit(#name_lower), 0)) { eh_dump_subset_flags |= EH_DumpSubsetFlag_##name; }
          EH_DumpSubset_XList
#undef X
#define X(name, name_lower, title) else if(str8_match(n->string, str8_lit(#name_lower), 0)) { elf_dump_subset_flags |= ELF_DumpSubsetFlag_##name; }
          ELF_DumpSubset_XList
#undef X
        }
        String8List omit_names = cmd_line_strings(cmdline, str8_lit("omit"));
        for(String8Node *n = omit_names.first; n != 0; n = n->next)
        {
          if(0){}
#define X(name, name_lower, title) else if(str8_match(n->string, str8_lit(#name_lower), 0)) { rdi_dump_subset_flags &= ~RDI_DumpSubsetFlag_##name; }
          RDI_DumpSubset_XList
#undef X
#define X(_N, _L, ...) else if(str8_match(n->string, str8_skip(str8_lit(_L), 1), 0)) { dw_dump_subset_flags &= ~DW_SectionFlag_##_N; }
          DW_SectionKind_XList
#undef X
#define X(name, name_lower, title) else if(str8_match(n->string, str8_lit(#name_lower), 0)) { eh_dump_subset_flags &= ~EH_DumpSubsetFlag_##name; }
          EH_DumpSubset_XList
#undef X
#define X(name, name_lower, title) else if(str8_match(n->string, str8_lit(#name_lower), 0)) { elf_dump_subset_flags &= ~ELF_DumpSubsetFlag_##name; }
          ELF_DumpSubset_XList
#undef X
        }
      }
      
      //- rjf: dump input files in order
      for(RB_FileNode *n = input_files.first; n != 0; n = n->next)
      {
        RB_File *f = n->v;
        if(lane_idx() == 0)
        {
          if(!deterministic)
          {
            str8_list_pushf(arena, &output_blobs, "// %S (%S)\n\n", f->path, f->format ? rb_file_format_display_name_table[f->format] : str8_lit("Unsupported format"));
          }
        }
        lane_sync();
        
        //- rjf: unpack file parses
        Arch arch = Arch_Null;
        PE_BinInfo pe = {0};
        ELF_Bin elf = {0};
        COFF_FileHeaderInfo coff_obj = {0};
        DW_Raw dw = {0};
        U64 eh_frame_hdr_vaddr = 0;
        U64 eh_frame_vaddr = 0;
        String8 eh_frame_hdr = {0};
        String8 eh_frame = {0};
        {
          if(f->format == RB_FileFormat_PE)
          {
            pe   = pe_bin_info_from_data(arena, f->data);
            arch = pe.arch;
          }
          else if(f->format == RB_FileFormat_COFF_OBJ || f->format == RB_FileFormat_COFF_BigOBJ)
          {
            coff_obj = coff_file_header_info_from_data(f->data);
            String8             string_table  = str8_substr(f->data, coff_obj.string_table_range);
            U64                 section_count = coff_obj.section_count_no_null;
            COFF_SectionHeader *section_table = (COFF_SectionHeader *)str8_substr(f->data, coff_obj.section_table_range).str;
            dw = dw_raw_from_coff_section_table(arena, f->data, string_table, section_count, section_table);
          }
          else if(f->format == RB_FileFormat_ELF32 || f->format == RB_FileFormat_ELF64)
          {
            elf = elf_bin_from_data(arena, f->data);
            arch = arch_from_elf_machine(elf.hdr.e_machine);
            for EachIndex(sect_idx, elf.hdr.e_shnum)
            {
              ELF_Shdr64 *shdr = &elf.shdrs.v[sect_idx];
              String8 name = elf_name_from_shdr64(f->data, &elf, shdr);
              if(str8_match(name, str8_lit(".eh_frame_hdr"), 0))
              {
                eh_frame_hdr = str8_substr(f->data, r1u64(shdr->sh_offset, shdr->sh_offset + shdr->sh_size));
                eh_frame_hdr_vaddr = shdr->sh_addr;
              }
              else if(str8_match(name, str8_lit(".eh_frame"), 0))
              {
                eh_frame = str8_substr(f->data, r1u64(shdr->sh_offset, shdr->sh_offset + shdr->sh_size));
                eh_frame_vaddr = shdr->sh_addr;
              }
            }
          }
          else if(f->format == RB_FileFormat_Macho)
          {
            MACHO_Bin bin = macho_bin_from_data(arena, f->data);
            arch = bin.arch;
            dw = rb_dw_raw_from_macho_bin(f->data, &bin);
          }
          if(f->format_flags & RB_FileFormatFlag_HasDWARF)
          {
            if(f->format == RB_FileFormat_PE)
            {
              String8             raw_sections  = str8_substr(f->data, pe.section_table_range);
              U64                 section_count = raw_sections.size / sizeof(COFF_SectionHeader);
              COFF_SectionHeader *section_table = (COFF_SectionHeader *)raw_sections.str;
              String8 string_table = str8_substr(f->data, pe.string_table_range);
              dw = dw_raw_from_coff_section_table(arena, f->data, string_table, section_count, section_table);
            }
            else if(f->format == RB_FileFormat_ELF32 ||
                    f->format == RB_FileFormat_ELF64)
            {
              DW_Raw *shared_raw = 0;
              if (lane_idx() == 0) {
                shared_raw = push_array(arena, DW_Raw, 1);
                *shared_raw = dw_raw_from_elf_bin(arena, f->data, &elf);
              }
              lane_sync_u64(&shared_raw, 0);
              dw = *shared_raw;
            }
          }
        }
        
        //- rjf: dump file info based on format
        switch(f->format)
        {
          default:{}break;
          
          //- rjf: PDB
          case RB_FileFormat_PDB:
          {
            // TODO(rjf)
          }break;
          
          //- rjf: PE
          case RB_FileFormat_PE:
          {
            // TODO(rjf)
          }break;
          
          //- rjf: COFF OBJ
          case RB_FileFormat_COFF_OBJ:
          {
            // TODO(rjf)
          }break;
          
          //- rjf: COFF big OBJ
          case RB_FileFormat_COFF_BigOBJ:
          {
            // TODO(rjf)
          }break;
          
          //- rjf: COFF archive
          case RB_FileFormat_COFF_Archive:
          case RB_FileFormat_COFF_ThinArchive:
          {
            // TODO(rjf)
          }break;
          
          //- rjf: ELF
          case RB_FileFormat_ELF32:
          case RB_FileFormat_ELF64:
          {
            String8List elf_strings = elf_dump(arena, f->data, elf_dump_subset_flags);
            str8_list_concat_in_place(&output_blobs, &elf_strings);
          }break;
          
          //- rjf: RDI file
          case RB_FileFormat_RDI:
          {
            RDI_Parsed rdi = {0};
            RDI_ParseStatus rdi_status = rdi_parse(f->data.str, f->data.size, &rdi);
            if(rdi_status == RDI_ParseStatus_Good && rdi_has_compressed_sections(&rdi) &&
               (rdi_dump_subset_flags & ~RDI_DumpSubsetFlag_DataSections) != 0)
            {
              U64 decompressed_size = rdi_decompressed_size_from_parsed(&rdi);
              U8 *decompressed_data = decompressed_size != 0 ? push_array_no_zero(arena, U8, decompressed_size) : 0;
              rdi_status = rdi_decompress_parsed(decompressed_data, decompressed_size, &rdi);
              if(rdi_status == RDI_ParseStatus_Good)
              {
                rdi_status = rdi_parse(decompressed_data, decompressed_size, &rdi);
              }
            }
            if(rdi_status != RDI_ParseStatus_Good)
            {
              output_valid = 0;
            }
            switch(rdi_status)
            {
              default:{}break;
              case RDI_ParseStatus_HeaderDoesNotMatch:      {log_user_errorf("RDI parse failure: header does not match\n");}break;
              case RDI_ParseStatus_UnsupportedVersionNumber:{log_user_errorf("RDI parse failure: unsupported version\n");}break;
              case RDI_ParseStatus_InvalidDataSecionLayout: {log_user_errorf("RDI parse failure: invalid data section layout\n");}break;
              case RDI_ParseStatus_UnsupportedSectionEncoding: {log_user_errorf("RDI parse failure: unsupported section encoding\n");}break;
              case RDI_ParseStatus_DecompressionFailed: {log_user_errorf("RDI parse failure: section decompression failed\n");}break;
              case RDI_ParseStatus_Good:
              {
                String8List dump = rdi_dump_list_from_parsed(arena, &rdi, rdi_dump_subset_flags);
                if(lane_idx() == 0)
                {
                  str8_list_concat_in_place(&output_blobs, &dump);
                }
              }break;
            }
          }break;
        }
        
        //- rjf: dump DWARF file extension info
        if(f->format_flags & RB_FileFormatFlag_HasDWARF)
        {
          if(lane_idx() == 0)
          {
            str8_list_pushf(arena, &output_blobs, "\n// %S (%S) (DWARF)\n\n", deterministic ? str8_skip_last_slash(f->path) : f->path, f->format ? rb_file_format_display_name_table[f->format] : str8_lit("Unsupported format"));
          }
          String8List dump = dw_dump_list_from_raw(arena, &dw, arch, dw_dump_subset_flags);
          if(lane_idx() == 0)
          {
            str8_list_concat_in_place(&output_blobs, &dump);
          }
          lane_sync();
        }
        
        //- rjf: dump EH frame file extension info
        if(f->format_flags & RB_FileFormatFlag_HasEhFrame)
        {
          if(lane_idx() == 0)
          {
            String8List dump = eh_dump_list_from_data(arena, arch, eh_frame_hdr_vaddr, eh_frame_vaddr, eh_frame_hdr, eh_frame, eh_dump_subset_flags);
            str8_list_concat_in_place(&output_blobs, &dump);
          }
          lane_sync();
        }
      }
    }break;
  }
  
  //////////////////////////////
  //- rjf: write outputs
  //
  if(lane_idx() == 0 && output_valid)
  {
    if(output_path.size != 0) ProfScope("write outputs [file]")
    {
      B32 is_written = write_data_list_to_file_path(output_path, output_blobs);
      if(is_written)
      {
        log_infof("Results written to %S", output_path);
      }
      else
      {
        log_user_errorf("ERROR: failed to write file %S\n", output_path);
      }
    }
    else ProfScope("write outputs [stdout]")
    {
      for(String8Node *n = output_blobs.first; n != 0; n = n->next)
      {
        for(U64 off = 0; off < n->string.size;)
        {
          U64 size_to_write = Min(n->string.size - off, GB(2));
          fwrite(n->string.str + off, size_to_write, 1, stdout);
          off += size_to_write;
        }
      }
      log_info(str8_lit("Results written to stdout"));
    }
  }
  lane_sync();

  if(bake_arena != 0)
  {
    arena_release(bake_arena);
    bake_arena = 0;
  }
  
  //////////////////////////////
  //- rjf: write info & errors
  //
  LogScopeResult log_scope = log_scope_end(arena);
  if(lane_idx() == 0)
  {
    if(cmd_line_has_flag(cmdline, str8_lit("verbose")) && log_scope.strings[LogMsgKind_Info].size != 0)
    {
      String8List lines = wrapped_lines_from_string(arena, log_scope.strings[LogMsgKind_Info], 80, 80, 0);
      for(String8Node *n = lines.first; n != 0; n = n->next)
      {
        fprintf(stderr, "%.*s\n", str8_varg(n->string));
      }
    }
    if(log_scope.strings[LogMsgKind_UserError].size != 0)
    {
      String8List lines = wrapped_lines_from_string(arena, log_scope.strings[LogMsgKind_UserError], 80, 80, 0);
      for(String8Node *n = lines.first; n != 0; n = n->next)
      {
        fprintf(stderr, "%.*s\n", str8_varg(n->string));
      }
    }
  }
}
