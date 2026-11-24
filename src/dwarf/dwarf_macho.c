// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

internal DW_Raw
rb_dw_raw_from_macho_bin(String8 data, MACHO_Bin *bin)
{
  DW_Raw result = {0};

  for EachIndex(idx, bin->section_count) {
    MACHO_Section *section = &bin->sections[idx];
    if (section->is_debug && str8_match(str8_prefix(section->name, 8), s("__zdebug"), 0)) {
      if (lane_idx() == 0) { log_user_errorf("Compressed Mach-O DWARF sections are not supported.\n"); }
      return (DW_Raw){0};
    }
    if (section->is_debug && section->is_file_backed) {
      // Mach-O section names are limited to 16 bytes
      DW_SectionKind kind = str8_match(section->name, s("__debug_str_offs"), 0) ? DW_SectionKind_StrOffsets : dw_section_kind_from_string(section->name);
      if (kind != DW_SectionKind_Null && result.sec[kind].data.size == 0) {
        result.sec[kind].name = section->name;
        result.sec[kind].data = str8_substr(data, section->frange);
      }
    }
  }

  return result;
}

internal RDIM_BinarySectionList
rb_rdi_binary_sections_from_macho_bin(Arena *arena, MACHO_Bin *bin)
{
  RDIM_BinarySectionList result = {0};
  for EachIndex(idx, bin->section_count) {
    MACHO_Section      *src = &bin->sections[idx];
    RDIM_BinarySection *dst = rdim_binary_section_list_push(arena, &result);
    dst->name = str8f(arena, "%S,%S", src->segment_name, src->name);

    if(src->init_prot & 1) { dst->flags |= RDI_BinarySectionFlag_Read;    }
    if(src->init_prot & 2) { dst->flags |= RDI_BinarySectionFlag_Write;   }
    if(src->init_prot & 4) { dst->flags |= RDI_BinarySectionFlag_Execute; }

    // dSYM image sections often have no file bytes but retain their VM ranges;
    // __DWARF is not part of the loaded image, even when it has VM addresses
    if ( ! src->is_debug && src->vrange.min >= bin->base_vaddr && src->vrange.min < src->vrange.max) {
      dst->voff_first = src->vrange.min - bin->base_vaddr;
      dst->voff_opl   = src->vrange.max - bin->base_vaddr;
    }
    dst->foff_first = src->frange.min;
    dst->foff_opl = src->frange.max;
  }
  return result;
}
