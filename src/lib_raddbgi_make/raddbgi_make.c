// Copyright (c) 2024 Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: API Implementation Helper Macros

#define rdim_require(root, b32, else_code, error_msg)  do { if(!(b32)) {rdim_push_msg((root), (error_msg)); else_code;} }while(0)
#define rdim_requiref(root, b32, else_code, fmt, ...)  do { if(!(b32)) {rdim_push_msgf((root), (fmt), __VA_ARGS__); else_code;} }while(0)

////////////////////////////////
//~ rjf: Basic Helpers

//- rjf: memory set

#if !defined(RDIM_MEMSET_OVERRIDE)
RDI_PROC void *
rdim_memset_fallback(void *dst, RDI_U8 c, RDI_U64 size)
{
  for(RDI_U64 idx = 0; idx < size; idx += 1)
  {
    ((RDI_U8 *)dst)[idx] = c;
  }
  return dst;
}
#endif

#if !defined(RDIM_MEMCPY_OVERRIDE)
RDI_PROC void *
rdim_memcpy_fallback(void *dst, void *src, RDI_U64 size)
{
  for(RDI_U64 idx = 0; idx < size; idx += 1)
  {
    ((RDI_U8 *)dst)[idx] = ((RDI_U8 *)src)[idx];
  }
  return dst;
}
#endif

//- rjf: arenas

#if !defined (RDIM_ARENA_OVERRIDE)

RDI_PROC RDIM_Arena *
rdim_arena_alloc_fallback(void)
{
  RDIM_Arena *arena = 0;
  // TODO(rjf)
  return arena;
}

RDI_PROC void
rdim_arena_release_fallback(RDIM_Arena *arena)
{
  // TODO(rjf)
}

RDI_PROC RDI_U64
rdim_arena_pos_fallback(RDIM_Arena *arena)
{
  // TODO(rjf)
  return 0;
}

RDI_PROC void *
rdim_arena_push_fallback(RDIM_Arena *arena, RDI_U64 size)
{
  // TODO(rjf)
  return 0;
}

RDI_PROC void
rdim_arena_pop_to_fallback(RDIM_Arena *arena, RDI_U64 pos)
{
  // TODO(rjf)
}

#endif

//- rjf: thread-local scratch arenas

#if !defined (RDIM_SCRATCH_OVERRIDE)
static RDIM_THREAD_LOCAL RDIM_Arena *rdim_thread_scratches[2];

RDI_PROC RDIM_Temp
rdim_scratch_begin_fallback(RDIM_Arena **conflicts, RDI_U64 conflicts_count)
{
  if(rdim_thread_scratches[0] == 0)
  {
    rdim_thread_scratches[0] = rdim_arena_alloc();
    rdim_thread_scratches[1] = rdim_arena_alloc();
  }
  RDIM_Arena *arena = 0;
  for(RDI_U64 scratch_idx = 0; 
      scratch_idx < sizeof(rdim_thread_scratches)/sizeof(rdim_thread_scratches[0]);
      scratch_idx += 1)
  {
    RDI_S32 scratch_conflicts = 0;
    for(RDI_U64 conflict_idx = 0; conflict_idx < conflicts_count; conflict_idx += 1)
    {
      if(conflicts[conflict_idx] == rdim_thread_scratches[scratch_idx])
      {
        scratch_conflicts = 1;
        break;
      }
    }
    if(!scratch_conflicts)
    {
      arena = rdim_thread_scratches[scratch_idx];
    }
  }
  RDIM_Temp temp;
  temp.arena = arena;
  temp.pos = rdim_arena_pos(arena);
  return temp;
}

RDI_PROC void
rdim_scratch_end_fallback(RDIM_Temp temp)
{
  rdim_arena_pop_to(temp.arena, temp.pos);
}

#endif

//- rjf: strings

RDI_PROC RDIM_String8
rdim_str8(RDI_U8 *str, RDI_U64 size)
{
  RDIM_String8 result;
  result.RDIM_String8_BaseMember = str;
  result.RDIM_String8_SizeMember = size;
  return result;
}

RDI_PROC RDIM_String8
rdim_str8_copy(RDIM_Arena *arena, RDIM_String8 src)
{
  RDIM_String8 dst;
  dst.RDIM_String8_SizeMember = src.RDIM_String8_SizeMember;
  dst.RDIM_String8_BaseMember = rdim_push_array_no_zero(arena, RDI_U8, dst.RDIM_String8_SizeMember+1);
  rdim_memcpy(dst.RDIM_String8_BaseMember, src.RDIM_String8_BaseMember, src.RDIM_String8_SizeMember);
  dst.RDIM_String8_BaseMember[dst.RDIM_String8_SizeMember] = 0;
  return dst;
}

RDI_PROC RDIM_String8
rdim_str8f(RDIM_Arena *arena, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  RDIM_String8 result = rdim_str8fv(arena, fmt, args);
  va_end(args);
  return(result);
}

RDI_PROC RDIM_String8
rdim_str8fv(RDIM_Arena *arena, char *fmt, va_list args)
{
  va_list args2;
  va_copy(args2, args);
  RDI_U32 needed_bytes = rdim_vsnprintf(0, 0, fmt, args) + 1;
  RDIM_String8 result = {0};
  result.RDIM_String8_BaseMember = rdim_push_array_no_zero(arena, RDI_U8, needed_bytes);
  result.RDIM_String8_SizeMember = rdim_vsnprintf((char*)result.str, needed_bytes, fmt, args2);
  result.RDIM_String8_BaseMember[result.RDIM_String8_SizeMember] = 0;
  va_end(args2);
  return(result);
}

RDI_PROC RDI_S32
rdim_str8_match(RDIM_String8 a, RDIM_String8 b, RDIM_StringMatchFlags flags)
{
  RDI_S32 result = 0;
  if(a.RDIM_String8_SizeMember == b.RDIM_String8_SizeMember)
  {
    RDI_S32 case_insensitive = (flags & RDIM_StringMatchFlag_CaseInsensitive);
    RDI_U64 size = a.RDIM_String8_SizeMember;
    result = 1;
    for(RDI_U64 idx = 0; idx < size; idx += 1)
    {
      RDI_U8 at = a.RDIM_String8_BaseMember[idx];
      RDI_U8 bt = b.RDIM_String8_BaseMember[idx];
      if(case_insensitive)
      {
        at = ('a' <= at && at <= 'z') ? at-('a'-'A') : at;
        bt = ('a' <= bt && bt <= 'z') ? bt-('a'-'A') : bt;
      }
      if(at != bt)
      {
        result = 0;
        break;
      }
    }
  }
  return result;
}

//- rjf: string lists

RDI_PROC void
rdim_str8_list_push(RDIM_Arena *arena, RDIM_String8List *list, RDIM_String8 string)
{
  RDIM_String8Node *n = rdim_push_array(arena, RDIM_String8Node, 1);
  n->RDIM_String8Node_StringMember = string;
  RDIM_SLLQueuePush_N(list->RDIM_String8List_FirstMember, list->RDIM_String8List_LastMember, n, RDIM_String8Node_NextPtrMember);
  list->RDIM_String8List_NodeCountMember += 1;
  list->RDIM_String8List_TotalSizeMember += string.RDIM_String8_SizeMember;
}

RDI_PROC void
rdim_str8_list_push_front(RDIM_Arena *arena, RDIM_String8List *list, RDIM_String8 string)
{
  RDIM_String8Node *n = rdim_push_array(arena, RDIM_String8Node, 1);
  n->RDIM_String8Node_StringMember = string;
  RDIM_SLLQueuePushFront_N(list->RDIM_String8List_FirstMember, list->RDIM_String8List_LastMember, n, RDIM_String8Node_NextPtrMember);
  list->RDIM_String8List_NodeCountMember += 1;
  list->RDIM_String8List_TotalSizeMember += string.RDIM_String8_SizeMember;
}

RDI_PROC void
rdim_str8_list_push_align(RDIM_Arena *arena, RDIM_String8List *list, RDI_U64 align)
{
  RDI_U64 total_size_pre_align  = list->total_size;
  RDI_U64 total_size_post_align = (total_size_pre_align + (align-1))&(~(align-1));
  RDI_U64 needed_size = total_size_post_align - total_size_pre_align;
  if(needed_size != 0)
  {
    RDI_U8 *padding = rdim_push_array(arena, RDI_U8, needed_size);
    rdim_str8_list_push(arena, list, rdim_str8(padding, needed_size));
  }
}

RDI_PROC RDIM_String8
rdim_str8_list_join(RDIM_Arena *arena, RDIM_String8List *list, RDIM_String8 sep)
{
  RDIM_String8 result;
  rdim_memzero_struct(&result);
  RDI_U64 sep_count = (list->RDIM_String8List_NodeCountMember > 1) ? (list->RDIM_String8List_NodeCountMember-1) : 0;
  result.RDIM_String8_SizeMember = list->RDIM_String8List_TotalSizeMember+sep_count*sep.RDIM_String8_SizeMember;
  result.RDIM_String8_BaseMember = rdim_push_array_no_zero(arena, RDI_U8, result.RDIM_String8_SizeMember+1);
  RDI_U64 off = 0;
  for(RDIM_String8Node *node = list->RDIM_String8List_FirstMember;
      node != 0;
      node = node->RDIM_String8Node_NextPtrMember)
  {
    rdim_memcpy((RDI_U8*)result.RDIM_String8_BaseMember+off,
                node->RDIM_String8Node_StringMember.RDIM_String8_BaseMember,
                node->RDIM_String8Node_StringMember.RDIM_String8_SizeMember);
    off += node->RDIM_String8Node_StringMember.RDIM_String8_SizeMember;
    if(sep.RDIM_String8_SizeMember != 0 && node->RDIM_String8Node_NextPtrMember != 0)
    {
      rdim_memcpy((RDI_U8*)result.RDIM_String8_BaseMember+off,
                  sep.RDIM_String8_BaseMember,
                  sep.RDIM_String8_SizeMember);
      off += sep.RDIM_String8_SizeMember;
    }
  }
  result.RDIM_String8_BaseMember[off] = 0;
  return result;
}

internal void
rdim_str8_list_concat_in_place(RDIM_String8List *list, RDIM_String8List *to_push)
{
  if(to_push->node_count != 0)
  {
    if (list->last)
    {
      list->node_count += to_push->node_count;
      list->total_size += to_push->total_size;
      list->last->next = to_push->first;
      list->last = to_push->last;
    }
    else
    {
      *list = *to_push;
    }
    rdim_memzero_struct(to_push);
  }
}

//- rjf: sortable range sorting

RDI_PROC RDIM_SortKey *
rdim_sort_key_array(RDIM_Arena *arena, RDIM_SortKey *keys, RDI_U64 count)
{
  // This sort is designed to take advantage of lots of pre-existing sorted ranges.
  // Most line info is already sorted or close to already sorted.
  // Similarly most vmap data has lots of pre-sorted ranges. etc. etc.
  // Also - this sort should be a "stable" sort. In the use case of sorting vmap
  // ranges, we want to be able to rely on order, so it needs to be preserved here.
  
  RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);
  RDIM_SortKey *result = 0;
  
  if(count <= 1)
  {
    result = keys;
  }
  else
  {
    RDIM_OrderedRange *ranges_first = 0;
    RDIM_OrderedRange *ranges_last = 0;
    RDI_U64 range_count = 0;
    {
      RDI_U64 pos = 0;
      for(;pos < count;)
      {
        // identify ordered range
        RDI_U64 first = pos;
        RDI_U64 opl = pos + 1;
        for(; opl < count && keys[opl - 1].key <= keys[opl].key; opl += 1);
        
        // generate an ordered range node
        RDIM_OrderedRange *new_range = rdim_push_array(rdim_temp_arena(scratch), RDIM_OrderedRange, 1);
        SLLQueuePush(ranges_first, ranges_last, new_range);
        range_count += 1;
        new_range->first = first;
        new_range->opl = opl;
        
        // update pos
        pos = opl;
      }
    }
    
    if(range_count == 1)
    {
      result = keys;
    }
    else
    {
      RDIM_SortKey *keys_swap = rdim_push_array_no_zero(arena, RDIM_SortKey, count);
      RDIM_SortKey *src = keys;
      RDIM_SortKey *dst = keys_swap;
      RDIM_OrderedRange *src_ranges = ranges_first;
      RDIM_OrderedRange *dst_ranges = 0;
      RDIM_OrderedRange *dst_ranges_last = 0;
      
      for(;;)
      {
        // begin a pass
        for(;;)
        {
          // end pass when out of ranges
          if(src_ranges == 0)
          {
            break;
          }
          
          // get first range
          RDIM_OrderedRange *range1 = src_ranges;
          SLLStackPop(src_ranges);
          
          // if this range is the whole array, we are done
          if(range1->first == 0 && range1->opl == count)
          {
            result = src;
            goto sort_done;
          }
          
          // if there is not a second range, save this range for next time and end this pass
          if(src_ranges == 0)
          {
            RDI_U64 first = range1->first;
            rdim_memcpy(dst + first, src + first, sizeof(*src)*(range1->opl - first));
            SLLQueuePush(dst_ranges, dst_ranges_last, range1);
            break;
          }
          
          // get second range
          RDIM_OrderedRange *range2 = src_ranges;
          SLLStackPop(src_ranges);
          
          rdim_assert(range1->opl == range2->first);
          
          // merge these ranges
          RDI_U64 jd = range1->first;
          RDI_U64 j1 = range1->first;
          RDI_U64 j1_opl = range1->opl;
          RDI_U64 j2 = range2->first;
          RDI_U64 j2_opl = range2->opl;
          for(;;)
          {
            if(src[j1].key <= src[j2].key)
            {
              rdim_memcpy(dst + jd, src + j1, sizeof(*src));
              j1 += 1;
              jd += 1;
              if(j1 >= j1_opl)
              {
                break;
              }
            }
            else
            {
              rdim_memcpy(dst + jd, src + j2, sizeof(*src));
              j2 += 1;
              jd += 1;
              if(j2 >= j2_opl)
              {
                break;
              }
            }
          }
          if(j1 < j1_opl)
          {
            rdim_memcpy(dst + jd, src + j1, sizeof(*src)*(j1_opl - j1));
          }
          else
          {
            rdim_memcpy(dst + jd, src + j2, sizeof(*src)*(j2_opl - j2));
          }
          
          // save this as one range
          range1->opl = range2->opl;
          SLLQueuePush(dst_ranges, dst_ranges_last, range1);
        }
        
        // end pass by swapping buffers and range nodes
        {
          RDIM_SortKey *temp = src;
          src = dst;
          dst = temp;
        }
        src_ranges = dst_ranges;
        dst_ranges = 0;
        dst_ranges_last = 0;
      }
    }
  }
  sort_done:;
  
#if 0
  // assert sortedness
  for(RDI_U64 i = 1; i < count; i += 1)
  {
    rdim_assert(result[i - 1].key <= result[i].key);
  }
#endif
  
  scratch_end(scratch);
  return result;
}

//- rjf: rng1u64 list

RDI_PROC void
rdim_rng1u64_list_push(RDIM_Arena *arena, RDIM_Rng1U64List *list, RDIM_Rng1U64 r)
{
  RDIM_Rng1U64Node *n = rdim_push_array(arena, RDIM_Rng1U64Node, 1);
  n->v = r;
  RDIM_SLLQueuePush(list->first, list->last, n);
  list->count += 1;
  if(list->count == 1 || r.min < list->min)
  {
    list->min = r.min;
  }
}

////////////////////////////////
//~ rjf: [Building] Binary Section List Building

RDI_PROC RDIM_BinarySection *
rdim_binary_section_list_push(RDIM_Arena *arena, RDIM_BinarySectionList *list)
{
  RDIM_BinarySectionNode *n = rdim_push_array(arena, RDIM_BinarySectionNode, 1);
  RDIM_SLLQueuePush(list->first, list->last, n);
  list->count += 1;
  RDIM_BinarySection *result = &n->v;
  return result;
}

////////////////////////////////
// [Building] Checksum Info Building

RDI_PROC RDI_U64
rdim_idx_from_checksum(RDIM_Checksum *checksum)
{
  RDI_U64 idx = 0;
  if(checksum != 0 && checksum->chunk != 0)
  {
    idx = checksum->chunk->base_idx + (checksum - checksum->chunk->v) + 1;
  }
  return idx;
}

////////////////////////////////
//~ rjf: [Building] Source File Info Building

RDI_PROC RDIM_SrcFile *
rdim_src_file_chunk_list_push(RDIM_Arena *arena, RDIM_SrcFileChunkList *list, RDI_U64 cap)
{
  RDIM_SrcFileChunkNode *n = list->last;
  if(n == 0 || n->count >= n->cap)
  {
    n = rdim_push_array(arena, RDIM_SrcFileChunkNode, 1);
    n->cap = cap;
    n->base_idx = list->total_count;
    n->v = rdim_push_array(arena, RDIM_SrcFile, n->cap);
    RDIM_SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  RDIM_SrcFile *src_file = &n->v[n->count];
  src_file->chunk = n;
  src_file->fragment_list = 0;
  src_file->next_fragment = &src_file->fragment_list;
  n->count += 1;
  list->total_count += 1;
  return src_file;
}

RDI_PROC RDI_U64
rdim_idx_from_src_file(RDIM_SrcFile *src_file)
{
  RDI_U64 idx = 0;
  if(src_file != 0 && src_file->chunk != 0)
  {
    idx = src_file->chunk->base_idx + (src_file - src_file->chunk->v);
  }
  return idx;
}

RDI_PROC void
rdim_src_file_chunk_list_concat_in_place(RDIM_SrcFileChunkList *dst, RDIM_SrcFileChunkList *to_push)
{
  for(RDIM_SrcFileChunkNode *n = to_push->first; n != 0; n = n->next)
  {
    n->base_idx += dst->total_count;
  }
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->chunk_count += to_push->chunk_count;
    dst->total_count += to_push->total_count;
  }
  else if(dst->first == 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

RDI_PROC void
rdim_src_file_push_line_sequence(RDIM_Arena *arena, RDIM_SrcFileChunkList *src_files, RDIM_SrcFile *src_file, RDIM_LineSequence *seq)
{
  RDIM_SrcFileLineMapFragment *fragment = rdim_push_array(arena, RDIM_SrcFileLineMapFragment, 1);
  fragment->seq = seq;

  *src_file->next_fragment = fragment;
  src_file->next_fragment  = &fragment->next;
}

////////////////////////////////
// Lines

RDI_PROC RDIM_LineSequence *
rdim_line_sequence_list_push(RDIM_Arena *arena, RDIM_LineSequenceList *list)
{
  RDIM_LineSequenceNode *n = rdim_push_array(arena, RDIM_LineSequenceNode, 1);
  RDIM_SLLQueuePush(list->first, list->last, n);
  list->count += 1;
  return &n->v;
}

RDI_PROC RDI_U64
rdim_total_line_count_from_line_sequence_list(RDIM_LineSequenceList *list)
{
  RDI_U64 total_line_count = 0;
  if(list != 0)
  {
    for(RDIM_LineSequenceNode *n = list->first; n != 0; n = n->next)
    {
      total_line_count += n->v.line_count;
    }
  }
  return total_line_count;
}

RDI_PROC RDIM_LineSequenceList *
rdim_line_sequence_list_chunk_list_push(RDIM_Arena *arena, RDIM_LineSequenceListChunkList *list, RDI_U64 cap)
{
  RDIM_LineSequenceListChunkNode *n = list->last;
  if(n == 0 || n->count >= n->cap)
  {
    n = rdim_push_array(arena, RDIM_LineSequenceListChunkNode, 1);
    n->cap = cap;
    n->base_idx = list->total_count;
    n->v = rdim_push_array(arena, RDIM_LineSequenceList, n->cap);
    RDIM_SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  RDIM_LineSequenceList *result = &n->v[n->count];
  result->chunk = n;
  n->count += 1;
  list->total_count += 1;
  return result;
}

RDI_PROC void
rdim_line_sequence_list_chunk_list_concat_in_place(RDIM_LineSequenceListChunkList *dst, RDIM_LineSequenceListChunkList *to_push)
{
  for(RDIM_LineSequenceListChunkNode *n = to_push->first; n != 0; n = n->next)
  {
    n->base_idx += dst->total_count;
  }
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->chunk_count += to_push->chunk_count;
    dst->total_count += to_push->total_count;
  }
  else if(dst->first == 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

RDI_PROC RDI_U64
rdim_idx_from_line_sequence_list(RDIM_LineSequenceList *list)
{
  RDI_U64 idx = 0;
  if(list != 0 && list->chunk != 0)
  {
    idx = list->chunk->base_idx + (list - list->chunk->v);
  }
  return idx;
}

////////////////////////////////
// Inline Sites Building

RDI_PROC RDIM_InlineSite *
rdim_inline_site_list_push(Arena *arena, RDIM_InlineSiteChunkList *list, U64 cap)
{
  RDIM_InlineSiteChunkNode *n = list->last;
  if(n == 0 || n->count >= n->cap)
  {
    n = rdim_push_array(arena, RDIM_InlineSiteChunkNode, 1);
    n->cap = cap;
    n->base_idx = list->total_count;
    n->v = rdim_push_array(arena, RDIM_InlineSite, n->cap);
    RDIM_SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  RDIM_InlineSite *result = &n->v[n->count];
  result->chunk = n;
  n->count += 1;
  list->total_count += 1;
  return result;
}

RDI_PROC void
rdim_inline_site_chunk_list_concat_in_place(RDIM_InlineSiteChunkList *dst, RDIM_InlineSiteChunkList *to_push)
{
  for(RDIM_InlineSiteChunkNode *n = to_push->first; n != 0; n = n->next)
  {
    n->base_idx += dst->total_count;
  }
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->chunk_count += to_push->chunk_count;
    dst->total_count += to_push->total_count;
  }
  else if(dst->first == 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

RDI_PROC RDI_U64
rdim_idx_from_inline_site(RDIM_InlineSite *ptr)
{
  RDI_U64 idx = 0;
  if(ptr != 0 && ptr->chunk != 0)
  {
    idx = ptr->chunk->base_idx + (ptr - ptr->chunk->v);
  }
  return idx;
}

////////////////////////////////
//~ rjf: [Building] Unit List Building

RDI_PROC RDIM_Unit *
rdim_unit_chunk_list_push(RDIM_Arena *arena, RDIM_UnitChunkList *list, RDI_U64 cap)
{
  RDIM_UnitChunkNode *n = list->last;
  if(n == 0 || n->count >= n->cap)
  {
    n = rdim_push_array(arena, RDIM_UnitChunkNode, 1);
    n->cap = cap;
    n->base_idx = list->total_count;
    n->v = rdim_push_array(arena, RDIM_Unit, n->cap);
    RDIM_SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  RDIM_Unit *unit = &n->v[n->count];
  unit->chunk = n;
  n->count += 1;
  list->total_count += 1;
  return unit;
}

RDI_PROC RDI_U64
rdim_idx_from_unit(RDIM_Unit *unit)
{
  RDI_U64 idx = 0;
  if(unit != 0 && unit->chunk != 0)
  {
    idx = unit->chunk->base_idx + (unit - unit->chunk->v) + 1;
  }
  return idx;
}

RDI_PROC void
rdim_unit_chunk_list_concat_in_place(RDIM_UnitChunkList *dst, RDIM_UnitChunkList *to_push)
{
  for(RDIM_UnitChunkNode *n = to_push->first; n != 0; n = n->next)
  {
    n->base_idx += dst->total_count;
  }
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->chunk_count += to_push->chunk_count;
    dst->total_count += to_push->total_count;
  }
  else if(dst->first == 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

////////////////////////////////
//~ rjf: [Building] Type Info Building

RDI_PROC RDIM_Type *
rdim_type_chunk_list_push(RDIM_Arena *arena, RDIM_TypeChunkList *list, RDI_U64 cap)
{
  RDIM_TypeChunkNode *n = list->last;
  if(n == 0 || n->count >= n->cap)
  {
    n = rdim_push_array(arena, RDIM_TypeChunkNode, 1);
    n->cap = cap;
    n->base_idx = list->total_count;
    n->v = rdim_push_array(arena, RDIM_Type, n->cap);
    RDIM_SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  RDIM_Type *result = &n->v[n->count];
  result->chunk = n;
  n->count += 1;
  list->total_count += 1;
  return result;
}

RDI_PROC RDI_U64
rdim_idx_from_type(RDIM_Type *type)
{
  RDI_U64 idx = 0;
  if(type != 0 && type->chunk != 0)
  {
    idx = type->chunk->base_idx + (type - type->chunk->v) + 1;
  }
  return idx;
}

RDI_PROC void
rdim_type_chunk_list_concat_in_place(RDIM_TypeChunkList *dst, RDIM_TypeChunkList *to_push)
{
  for(RDIM_TypeChunkNode *n = to_push->first; n != 0; n = n->next)
  {
    n->base_idx += dst->total_count;
  }
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->chunk_count += to_push->chunk_count;
    dst->total_count += to_push->total_count;
  }
  else if(dst->first == 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

RDI_PROC RDIM_UDT *
rdim_udt_chunk_list_push(RDIM_Arena *arena, RDIM_UDTChunkList *list, RDI_U64 cap)
{
  RDIM_UDTChunkNode *n = list->last;
  if(n == 0 || n->count >= n->cap)
  {
    n = rdim_push_array(arena, RDIM_UDTChunkNode, 1);
    n->cap = cap;
    n->base_idx = list->total_count;
    n->v = rdim_push_array(arena, RDIM_UDT, n->cap);
    RDIM_SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  RDIM_UDT *result = &n->v[n->count];
  result->chunk = n;
  n->count += 1;
  list->total_count += 1;
  return result;
}

RDI_PROC RDI_U64
rdim_idx_from_udt(RDIM_UDT *udt)
{
  RDI_U64 idx = 0;
  if(udt != 0 && udt->chunk != 0)
  {
    idx = udt->chunk->base_idx + (udt - udt->chunk->v) + 1;
  }
  return idx;
}

RDI_PROC void
rdim_udt_chunk_list_concat_in_place(RDIM_UDTChunkList *dst, RDIM_UDTChunkList *to_push)
{
  for(RDIM_UDTChunkNode *n = to_push->first; n != 0; n = n->next)
  {
    n->base_idx += dst->total_count;
  }
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->chunk_count += to_push->chunk_count;
    dst->total_count += to_push->total_count;
    dst->total_member_count += to_push->total_member_count;
    dst->total_enum_val_count += to_push->total_enum_val_count;
  }
  else if(dst->first == 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

RDI_PROC RDIM_UDTMember *
rdim_udt_push_member(RDIM_Arena *arena, RDIM_UDTChunkList *list, RDIM_UDT *udt)
{
  RDIM_UDTMember *mem = rdim_push_array(arena, RDIM_UDTMember, 1);
  RDIM_SLLQueuePush(udt->first_member, udt->last_member, mem);
  udt->member_count += 1;
  list->total_member_count += 1;
  return mem;
}

RDI_PROC RDIM_UDTEnumVal *
rdim_udt_push_enum_val(RDIM_Arena *arena, RDIM_UDTChunkList *list, RDIM_UDT *udt)
{
  RDIM_UDTEnumVal *mem = rdim_push_array(arena, RDIM_UDTEnumVal, 1);
  RDIM_SLLQueuePush(udt->first_enum_val, udt->last_enum_val, mem);
  udt->enum_val_count += 1;
  list->total_enum_val_count += 1;
  return mem;
}

////////////////////////////////
//~ rjf: [Building] Symbol Info Building

RDI_PROC RDIM_Symbol *
rdim_symbol_chunk_list_push(RDIM_Arena *arena, RDIM_SymbolChunkList *list, RDI_U64 cap)
{
  RDIM_SymbolChunkNode *n = list->last;
  if(n == 0 || n->count >= n->cap)
  {
    n = rdim_push_array(arena, RDIM_SymbolChunkNode, 1);
    n->cap = cap;
    n->base_idx = list->total_count;
    n->v = rdim_push_array(arena, RDIM_Symbol, n->cap);
    RDIM_SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  RDIM_Symbol *result = &n->v[n->count];
  result->chunk = n;
  n->count += 1;
  list->total_count += 1;
  return result;
}

RDI_PROC RDI_U64
rdim_idx_from_symbol(RDIM_Symbol *symbol)
{
  RDI_U64 idx = 0;
  if(symbol != 0 && symbol->chunk != 0)
  {
    idx = symbol->chunk->base_idx + (symbol - symbol->chunk->v) + 1;
  }
  return idx;
}

RDI_PROC void
rdim_symbol_chunk_list_concat_in_place(RDIM_SymbolChunkList *dst, RDIM_SymbolChunkList *to_push)
{
  for(RDIM_SymbolChunkNode *n = to_push->first; n != 0; n = n->next)
  {
    n->base_idx += dst->total_count;
  }
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->chunk_count += to_push->chunk_count;
    dst->total_count += to_push->total_count;
  }
  else if(dst->first == 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

////////////////////////////////
//~ rjf: [Building] Scope Info Building

//- rjf: scopes

RDI_PROC RDIM_Scope *
rdim_scope_chunk_list_push(RDIM_Arena *arena, RDIM_ScopeChunkList *list, RDI_U64 cap)
{
  RDIM_ScopeChunkNode *n = list->last;
  if(n == 0 || n->count >= n->cap)
  {
    n = rdim_push_array(arena, RDIM_ScopeChunkNode, 1);
    n->cap = cap;
    n->base_idx = list->total_count;
    n->v = rdim_push_array(arena, RDIM_Scope, n->cap);
    RDIM_SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  RDIM_Scope *result = &n->v[n->count];
  result->chunk = n;
  n->count += 1;
  list->total_count += 1;
  return result;
}

RDI_PROC RDI_U64
rdim_idx_from_scope(RDIM_Scope *scope)
{
  RDI_U64 idx = 0;
  if(scope != 0 && scope->chunk != 0)
  {
    idx = scope->chunk->base_idx + (scope - scope->chunk->v) + 1;
  }
  return idx;
}

RDI_PROC void
rdim_scope_chunk_list_concat_in_place(RDIM_ScopeChunkList *dst, RDIM_ScopeChunkList *to_push)
{
  for(RDIM_ScopeChunkNode *n = to_push->first; n != 0; n = n->next)
  {
    n->base_idx += dst->total_count;
  }
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->chunk_count      += to_push->chunk_count;
    dst->total_count      += to_push->total_count;
    dst->scope_voff_count += to_push->scope_voff_count;
    dst->local_count      += to_push->local_count;
    dst->location_count   += to_push->location_count;
  }
  else if(dst->first == 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

RDI_PROC void
rdim_scope_push_voff_range(RDIM_Arena *arena, RDIM_ScopeChunkList *list, RDIM_Scope *scope, RDIM_Rng1U64 range)
{
  rdim_rng1u64_list_push(arena, &scope->voff_ranges, range);
  list->scope_voff_count += 2;
}

RDI_PROC RDIM_Local *
rdim_scope_push_local(RDIM_Arena *arena, RDIM_ScopeChunkList *scopes, RDIM_Scope *scope)
{
  RDIM_Local *local = rdim_push_array(arena, RDIM_Local, 1);
  RDIM_SLLQueuePush(scope->first_local, scope->last_local, local);
  scope->local_count += 1;
  scopes->local_count += 1;
  return local;
}

//- rjf: bytecode

RDI_PROC void
rdim_bytecode_push_op(RDIM_Arena *arena, RDIM_EvalBytecode *bytecode, RDI_EvalOp op, RDI_U64 p)
{
  RDI_U8 ctrlbits = rdi_eval_opcode_ctrlbits[op];
  RDI_U32 p_size = RDI_DECODEN_FROM_CTRLBITS(ctrlbits);
  
  RDIM_EvalBytecodeOp *node = rdim_push_array(arena, RDIM_EvalBytecodeOp, 1);
  node->op = op;
  node->p_size = p_size;
  node->p = p;
  
  RDIM_SLLQueuePush(bytecode->first_op, bytecode->last_op, node);
  bytecode->op_count += 1;
  bytecode->encoded_size += 1 + p_size;
}

RDI_PROC void
rdim_bytecode_push_uconst(RDIM_Arena *arena, RDIM_EvalBytecode *bytecode, RDI_U64 x)
{
  if(x <= 0xFF)
  {
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_ConstU8, x);
  }
  else if(x <= 0xFFFF)
  {
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_ConstU16, x);
  }
  else if(x <= 0xFFFFFFFF)
  {
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_ConstU32, x);
  }
  else
  {
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_ConstU64, x);
  }
}

RDI_PROC void
rdim_bytecode_push_sconst(RDIM_Arena *arena, RDIM_EvalBytecode *bytecode, RDI_S64 x)
{
  if(-0x80 <= x && x <= 0x7F)
  {
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_ConstU8, (RDI_U64)x);
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_TruncSigned, 8);
  }
  else if(-0x8000 <= x && x <= 0x7FFF)
  {
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_ConstU16, (RDI_U64)x);
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_TruncSigned, 16);
  }
  else if(-0x80000000ll <= x && x <= 0x7FFFFFFFll)
  {
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_ConstU32, (RDI_U64)x);
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_TruncSigned, 32);
  }
  else
  {
    rdim_bytecode_push_op(arena, bytecode, RDI_EvalOp_ConstU64, (RDI_U64)x);
  }
}

RDI_PROC void
rdim_bytecode_concat_in_place(RDIM_EvalBytecode *left_dst, RDIM_EvalBytecode *right_destroyed)
{
  if(right_destroyed->first_op != 0)
  {
    if(left_dst->first_op == 0)
    {
      rdim_memcpy_struct(left_dst, right_destroyed);
    }
    else
    {
      left_dst->last_op = right_destroyed->last_op;
      left_dst->op_count += right_destroyed->op_count;
      left_dst->encoded_size += right_destroyed->encoded_size;
    }
    rdim_memzero_struct(right_destroyed);
  }
}

//- rjf: individual locations

RDI_PROC RDIM_Location *
rdim_push_location_addr_bytecode_stream(RDIM_Arena *arena, RDIM_EvalBytecode *bytecode)
{
  RDIM_Location *result = rdim_push_array(arena, RDIM_Location, 1);
  result->kind = RDI_LocationKind_AddrBytecodeStream;
  result->bytecode = *bytecode;
  return result;
}

RDI_PROC RDIM_Location *
rdim_push_location_val_bytecode_stream(RDIM_Arena *arena, RDIM_EvalBytecode *bytecode)
{
  RDIM_Location *result = rdim_push_array(arena, RDIM_Location, 1);
  result->kind = RDI_LocationKind_ValBytecodeStream;
  result->bytecode = *bytecode;
  return result;
}

RDI_PROC RDIM_Location *
rdim_push_location_addr_reg_plus_u16(RDIM_Arena *arena, RDI_U8 reg_code, RDI_U16 offset)
{
  RDIM_Location *result = rdim_push_array(arena, RDIM_Location, 1);
  result->kind = RDI_LocationKind_AddrRegisterPlusU16;
  result->register_code = reg_code;
  result->offset = offset;
  return result;
}

RDI_PROC RDIM_Location *
rdim_push_location_addr_addr_reg_plus_u16(RDIM_Arena *arena, RDI_U8 reg_code, RDI_U16 offset)
{
  RDIM_Location *result = rdim_push_array(arena, RDIM_Location, 1);
  result->kind = RDI_LocationKind_AddrAddrRegisterPlusU16;
  result->register_code = reg_code;
  result->offset = offset;
  return result;
}

RDI_PROC RDIM_Location *
rdim_push_location_val_reg(RDIM_Arena *arena, RDI_U8 reg_code)
{
  RDIM_Location *result = rdim_push_array(arena, RDIM_Location, 1);
  result->kind = RDI_LocationKind_ValRegister;
  result->register_code = reg_code;
  return result;
}

//- rjf: location sets

RDI_PROC void
rdim_location_set_push_case(RDIM_Arena *arena, RDIM_ScopeChunkList *scopes, RDIM_LocationSet *locset, RDIM_Rng1U64 voff_range, RDIM_Location *location)
{
  RDIM_LocationCase *location_case = rdim_push_array(arena, RDIM_LocationCase, 1);
  SLLQueuePush(locset->first_location_case, locset->last_location_case, location_case);
  locset->location_case_count += 1;
  location_case->voff_range = voff_range;
  location_case->location   = location;
  scopes->location_count +=1;
}

////////////////////////////////
//~ rjf: [Baking Helpers] Baked VMap Building

RDI_PROC RDIM_BakeVMap
rdim_bake_vmap_from_markers(RDIM_Arena *arena, RDIM_VMapMarker *markers, RDIM_SortKey *keys, RDI_U64 marker_count)
{
  RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);
  
  //- rjf: sort markers
  RDIM_SortKey *sorted_keys = rdim_sort_key_array(scratch.arena, keys, marker_count);
  
  //- rjf: determine if an extra vmap entry for zero is needed
  RDI_U32 extra_vmap_entry = 0;
  if(marker_count > 0 && sorted_keys[0].key != 0)
  {
    extra_vmap_entry = 1;
  }
  
  //- rjf: fill output vmap entries
  RDI_U32 vmap_count_raw = marker_count - 1 + extra_vmap_entry;
  RDI_VMapEntry *vmap = rdim_push_array_no_zero(arena, RDI_VMapEntry, vmap_count_raw + 1);
  RDI_U32 vmap_entry_count_pass_1 = 0;
  {
    typedef struct RDIM_VMapRangeTracker RDIM_VMapRangeTracker;
    struct RDIM_VMapRangeTracker
    {
      RDIM_VMapRangeTracker *next;
      RDI_U32 idx;
    };
    RDI_VMapEntry *vmap_ptr = vmap;
    if(extra_vmap_entry)
    {
      vmap_ptr->voff = 0;
      vmap_ptr->idx = 0;
      vmap_ptr += 1;
    }
    RDIM_VMapRangeTracker *tracker_stack = 0;
    RDIM_VMapRangeTracker *tracker_free = 0;
    RDIM_SortKey *key_ptr = sorted_keys;
    RDIM_SortKey *key_opl = sorted_keys + marker_count;
    for(;key_ptr < key_opl;)
    {
      // rjf: get initial map state from tracker stack
      RDI_U32 initial_idx = (RDI_U32)0xffffffff;
      if(tracker_stack != 0)
      {
        initial_idx = tracker_stack->idx;
      }
      
      // rjf: update tracker stack
      //
      // * we must process _all_ of the changes that apply at this voff before moving on
      //
      RDI_U64 voff = key_ptr->key;
      
      for(;key_ptr < key_opl && key_ptr->key == voff; key_ptr += 1)
      {
        RDIM_VMapMarker *marker = (RDIM_VMapMarker*)key_ptr->val;
        RDI_U32 idx = marker->idx;
        
        // rjf: range begin -> push to stack
        if(marker->begin_range)
        {
          RDIM_VMapRangeTracker *new_tracker = tracker_free;
          if(new_tracker != 0)
          {
            RDIM_SLLStackPop(tracker_free);
          }
          else
          {
            new_tracker = rdim_push_array(scratch.arena, RDIM_VMapRangeTracker, 1);
          }
          RDIM_SLLStackPush(tracker_stack, new_tracker);
          new_tracker->idx = idx;
        }
        
        // rjf: range ending -> pop matching node from stack (not always the top)
        else
        {
          RDIM_VMapRangeTracker **ptr_in = &tracker_stack;
          RDIM_VMapRangeTracker *match = 0;
          for(RDIM_VMapRangeTracker *node = tracker_stack; node != 0;)
          {
            if(node->idx == idx)
            {
              match = node;
              break;
            }
            ptr_in = &node->next;
            node = node->next;
          }
          if(match != 0)
          {
            *ptr_in = match->next;
            RDIM_SLLStackPush(tracker_free, match);
          }
        }
      }
      
      // rjf: get final map state from tracker stack
      RDI_U32 final_idx = 0;
      if(tracker_stack != 0)
      {
        final_idx = tracker_stack->idx;
      }
      
      // rjf: if final is different from initial - emit new vmap entry
      if(final_idx != initial_idx)
      {
        vmap_ptr->voff = voff;
        vmap_ptr->idx = final_idx;
        vmap_ptr += 1;
      }
    }
    
    vmap_entry_count_pass_1 = (RDI_U32)(vmap_ptr - vmap); // TODO(rjf): @u64_to_u32
  }
  
  //- rjf: combine duplicate neighbors
  RDI_U32 vmap_entry_count = 0;
  {
    RDI_VMapEntry *vmap_ptr = vmap;
    RDI_VMapEntry *vmap_opl = vmap + vmap_entry_count_pass_1;
    RDI_VMapEntry *vmap_out = vmap;
    for(;vmap_ptr < vmap_opl;)
    {
      RDI_VMapEntry *vmap_range_first = vmap_ptr;
      RDI_U64 idx = vmap_ptr->idx;
      vmap_ptr += 1;
      for(;vmap_ptr < vmap_opl && vmap_ptr->idx == idx;) vmap_ptr += 1;
      rdim_memcpy_struct(vmap_out, vmap_range_first);
      vmap_out += 1;
    }
    vmap_entry_count = (RDI_U32)(vmap_out - vmap); // TODO(rjf): @u64_to_u32
  }
  
  //- rjf: fill result
  RDIM_BakeVMap result = {0};
  result.vmap = vmap;
  result.count = vmap_entry_count-1;
  rdim_scratch_end(scratch);
  return result;
}

////////////////////////////////
//~ rjf: [Baking Helpers] Interned / Deduplicated Blob Data Structure Helpers

//- rjf: bake string chunk lists

RDI_PROC RDIM_BakeString *
rdim_bake_string_chunk_list_push(RDIM_Arena *arena, RDIM_BakeStringChunkList *list, RDI_U64 cap)
{
  RDIM_BakeStringChunkNode *n = list->last;
  if(n == 0 || n->count >= n->cap)
  {
    n = rdim_push_array(arena, RDIM_BakeStringChunkNode, 1);
    n->cap = cap;
    n->v = rdim_push_array(arena, RDIM_BakeString, n->cap);
    RDIM_SLLQueuePush(list->first, list->last, n);
    list->chunk_count += 1;
  }
  RDIM_BakeString *s = &n->v[n->count];
  n->count += 1;
  list->total_count += 1;
  return s;
}

RDI_PROC void
rdim_bake_string_chunk_list_concat_in_place(RDIM_BakeStringChunkList *dst, RDIM_BakeStringChunkList *to_push)
{
  for(RDIM_BakeStringChunkNode *n = to_push->first; n != 0; n = n->next)
  {
    n->base_idx += dst->total_count;
  }
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->chunk_count += to_push->chunk_count;
    dst->total_count += to_push->total_count;
  }
  else if(dst->first == 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

RDI_PROC RDIM_BakeStringChunkList
rdim_bake_string_chunk_list_sorted_from_unsorted(RDIM_Arena *arena, RDIM_BakeStringChunkList *src)
{
  //- rjf: produce unsorted destination list with single chunk node
  RDIM_BakeStringChunkList dst = {0};
  for(RDIM_BakeStringChunkNode *n = src->first; n != 0; n = n->next)
  {
    for(RDI_U64 idx = 0; idx < n->count; idx += 1)
    {
      RDIM_BakeString *src_str = &n->v[idx];
      RDIM_BakeString *dst_str = rdim_bake_string_chunk_list_push(arena, &dst, src->total_count);
      rdim_memcpy_struct(dst_str, src_str);
    }
  }
  
  //- rjf: sort chunk node
  if(dst.first != 0)
  {
    RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);
    typedef struct SortTask SortTask;
    struct SortTask
    {
      SortTask *next;
      RDI_U64 string_off;
      RDIM_BakeString *v;
      RDI_U64 count;
    };
    SortTask start_task = {0, 0, dst.first->v, dst.first->count};
    SortTask *first_task = &start_task;
    SortTask *last_task = &start_task;
    
    //- rjf: for each sort task range:
    for(SortTask *t = first_task; t != 0; t = t->next)
    {
      //- rjf: loop through range, drop each element into bucket according to byte in string at task offset
      RDIM_BakeStringChunkList *buckets = rdim_push_array(scratch.arena, RDIM_BakeStringChunkList, 256);
      for(RDI_U64 idx = 0; idx < t->count; idx += 1)
      {
        U8 byte = t->string_off < t->v[idx].string.size ? t->v[idx].string.str[t->string_off] : 0;
        RDIM_BakeStringChunkList *bucket = &buckets[byte];
        RDIM_BakeString *bstr = rdim_bake_string_chunk_list_push(scratch.arena, bucket, 8);
        rdim_memcpy_struct(bstr, &t->v[idx]);
      }
      
      //- rjf: in-place mutate the original source array to reflect the order per the buckets.
      // build new sort tasks for buckets with many elements
      {
        RDI_U64 write_idx = 0;
        for(U64 bucket_idx = 0; bucket_idx < 256; bucket_idx += 1)
        {
          // rjf: write each chunk node's array into original array, detect if there is size left to sort
          RDI_U64 bucket_base_idx = write_idx;
          RDI_U64 max_size_left_to_sort = 0;
          for(RDIM_BakeStringChunkNode *n = buckets[bucket_idx].first; n != 0; n = n->next)
          {
            rdim_memcpy(t->v+write_idx, n->v, sizeof(n->v[0])*n->count);
            write_idx += n->count;
            for(RDI_U64 idx = 0; idx < n->count; idx += 1)
            {
              if(n->v[idx].string.size > t->string_off+1)
              {
                max_size_left_to_sort = Max(max_size_left_to_sort, (n->v[idx].string.size - t->string_off+1));
              }
            }
          }
          
          // rjf: if any bucket has >1 element & has some amount of size left to sort, push new task for this
          // bucket's region in the array, and for remainder of keys
          if(buckets[bucket_idx].total_count > 1 && max_size_left_to_sort > 0)
          {
            SortTask *new_task = rdim_push_array(scratch.arena, SortTask, 1);
            RDIM_SLLQueuePush(first_task, last_task, new_task);
            new_task->string_off = t->string_off+1;
            new_task->v = t->v + bucket_base_idx;
            new_task->count = write_idx-bucket_base_idx;
          }
        }
      }
    }
    scratch_end(scratch);
  }
  
  return dst;
}

//- rjf: bake string chunk list maps

RDI_PROC RDIM_BakeStringMapLoose *
rdim_bake_string_map_loose_make(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top)
{
  RDIM_BakeStringMapLoose *map = rdim_push_array(arena, RDIM_BakeStringMapLoose, 1);
  map->slots = rdim_push_array(arena, RDIM_BakeStringChunkList *, top->slots_count);
  return map;
}

RDI_PROC void
rdim_bake_string_map_loose_insert(RDIM_Arena *arena, RDIM_BakeStringMapTopology *map_topology, RDIM_BakeStringMapLoose *map, RDI_U64 chunk_cap, RDIM_String8 string)
{
  RDI_U64 hash = rdi_hash(string.RDIM_String8_BaseMember, string.RDIM_String8_SizeMember);
  RDI_U64 slot_idx = hash%map_topology->slots_count;
  RDIM_BakeStringChunkList *slot = map->slots[slot_idx];
  if(slot == 0)
  {
    slot = map->slots[slot_idx] = rdim_push_array(arena, RDIM_BakeStringChunkList, 1);
  }
  RDI_S32 is_duplicate = 0;
  for(RDIM_BakeStringChunkNode *n = slot->first; n != 0; n = n->next)
  {
    for(RDI_U64 idx = 0; idx < n->count; idx += 1)
    {
      if(rdim_str8_match(n->v[idx].string, string, 0))
      {
        is_duplicate = 1;
        goto break_all;
      }
    }
  }
  break_all:;
  if(!is_duplicate)
  {
    RDIM_BakeString *bstr = rdim_bake_string_chunk_list_push(arena, slot, chunk_cap);
    bstr->string = string;
    bstr->hash = hash;
  }
}

RDI_PROC void
rdim_bake_string_map_loose_join_in_place(RDIM_BakeStringMapTopology *map_topology, RDIM_BakeStringMapLoose *dst, RDIM_BakeStringMapLoose *src)
{
  for(RDI_U64 idx = 0; idx < map_topology->slots_count; idx += 1)
  {
    if(dst->slots[idx] == 0)
    {
      dst->slots[idx] = src->slots[idx];
    }
    else if(src->slots[idx] != 0)
    {
      rdim_bake_string_chunk_list_concat_in_place(dst->slots[idx], src->slots[idx]);
    }
  }
  rdim_memzero_struct(src);
}

RDI_PROC RDIM_BakeStringMapBaseIndices
rdim_bake_string_map_base_indices_from_map_loose(RDIM_Arena *arena, RDIM_BakeStringMapTopology *map_topology, RDIM_BakeStringMapLoose *map)
{
  RDIM_BakeStringMapBaseIndices indices = {0};
  indices.slots_base_idxs = rdim_push_array(arena, RDI_U64, map_topology->slots_count+1);
  RDI_U64 total_count = 0;
  for(RDI_U64 idx = 0; idx < map_topology->slots_count; idx += 1)
  {
    indices.slots_base_idxs[idx] += total_count;
    if(map->slots[idx] != 0)
    {
      total_count += map->slots[idx]->total_count;
    }
  }
  indices.slots_base_idxs[map_topology->slots_count] = total_count;
  return indices;
}

//- rjf: finalized bake string map

RDI_PROC RDIM_BakeStringMapTight
rdim_bake_string_map_tight_from_loose(RDIM_Arena *arena, RDIM_BakeStringMapTopology *map_topology, RDIM_BakeStringMapBaseIndices *map_base_indices, RDIM_BakeStringMapLoose *map)
{
  RDIM_BakeStringMapTight m = {0};
  m.slots_count = map_topology->slots_count;
  m.slots = rdim_push_array(arena, RDIM_BakeStringChunkList, m.slots_count);
  m.slots_base_idxs = map_base_indices->slots_base_idxs;
  for(RDI_U64 idx = 0; idx < m.slots_count; idx += 1)
  {
    if(map->slots[idx] != 0)
    {
      rdim_memcpy_struct(&m.slots[idx], map->slots[idx]);
    }
  }
  m.total_count = m.slots_base_idxs[m.slots_count];
  return m;
}

RDI_PROC RDI_U64
rdim_bake_idx_from_string(RDIM_BakeStringMapTight *map, RDIM_String8 string)
{
  RDI_U64 hash = rdi_hash(string.RDIM_String8_BaseMember, string.RDIM_String8_SizeMember);
  RDI_U64 slot_idx = hash%map->slots_count;
  RDI_U64 idx = 0;
  for(RDIM_BakeStringChunkNode *n = map->slots[slot_idx].first; n != 0; n = n->next)
  {
    for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1)
    {
      if(n->v[chunk_idx].hash == hash && rdim_str8_match(n->v[chunk_idx].string, string, 0))
      {
        idx = map->slots_base_idxs[slot_idx] + n->base_idx + chunk_idx;
        break;
      }
    }
  }
  return idx;
}

//- rjf: bake idx run map reading/writing

RDI_PROC RDI_U64
rdim_hash_from_idx_run(RDI_U32 *idx_run, RDI_U32 count)
{
  RDI_U64 hash = 5381;
  RDI_U32 *ptr = idx_run;
  RDI_U32 *opl = idx_run + count;
  for(;ptr < opl; ptr += 1)
  {
    hash = ((hash << 5) + hash) + (*ptr);
  }
  return hash;
}

RDI_PROC RDI_U32
rdim_bake_idx_from_idx_run(RDIM_BakeIdxRunMap *map, RDI_U32 *idx_run, RDI_U32 count)
{
  RDI_U64 hash = rdim_hash_from_idx_run(idx_run, count);
  RDI_U64 slot_idx = hash%map->slots_count;
  
  // rjf: find existing node
  RDIM_BakeIdxRunNode *node = 0;
  for(RDIM_BakeIdxRunNode *n = map->slots[slot_idx]; n != 0; n = n->hash_next)
  {
    if(n->hash == hash)
    {
      RDI_S32 is_match = 1;
      RDI_U32 *n_idx = n->idx_run;
      for(RDI_U32 i = 0; i < count; i += 1)
      {
        if(n_idx[i] != idx_run[i])
        {
          is_match = 0;
          break;
        }
      }
      if(is_match)
      {
        node = n;
        break;
      }
    }
  }
  
  // rjf: node -> index
  RDI_U32 result = node ? node->first_idx : 0;
  return result;
}

RDI_PROC RDI_U32
rdim_bake_idx_run_map_insert(RDIM_Arena *arena, RDIM_BakeIdxRunMap *map, RDI_U32 *idx_run, RDI_U32 count)
{
  RDI_U64 hash = rdim_hash_from_idx_run(idx_run, count);
  RDI_U64 slot_idx = hash%map->slots_count;
  
  // rjf: find existing node
  RDIM_BakeIdxRunNode *node = 0;
  for(RDIM_BakeIdxRunNode *n = map->slots[slot_idx]; n != 0; n = n->hash_next)
  {
    if(n->hash == hash)
    {
      RDI_S32 is_match = 1;
      RDI_U32 *n_idx = n->idx_run;
      for(RDI_U32 i = 0; i < count; i += 1)
      {
        if(n_idx[i] != idx_run[i])
        {
          is_match = 0;
          break;
        }
      }
      if(is_match)
      {
        node = n;
        break;
      }
    }
  }
  
  // rjf: no node -> make new node
  if(node == 0)
  {
    node = rdim_push_array_no_zero(arena, RDIM_BakeIdxRunNode, 1);
    RDI_U32 *idx_run_copy = rdim_push_array_no_zero(arena, RDI_U32, count);
    for(RDI_U32 i = 0; i < count; i += 1)
    {
      idx_run_copy[i] = idx_run[i];
    }
    node->idx_run = idx_run_copy;
    node->hash = hash;
    node->count = count;
    node->first_idx = map->idx_count;
    map->count += 1;
    map->idx_count += count;
    RDIM_SLLQueuePush_N(map->order_first, map->order_last, node, order_next);
    RDIM_SLLStackPush_N(map->slots[slot_idx], node, hash_next);
    map->slot_collision_count += (node->hash_next != 0);
  }
  
  // rjf: node -> index
  RDI_U32 result = node->first_idx;
  return result;
}

//- rjf: bake path tree reading/writing

RDI_PROC RDIM_BakePathNode *
rdim_bake_path_node_from_string(RDIM_BakePathTree *tree, RDIM_String8 string)
{
  RDIM_BakePathNode *node = &tree->root;
  RDI_U8 *ptr = string.str;
  RDI_U8 *opl = string.str + string.size;
  for(;ptr < opl && node != 0;)
  {
    // rjf: skip past slashes
    for(;ptr < opl && (*ptr == '/' || *ptr == '\\'); ptr += 1);
    
    // rjf: save beginning of non-slash range
    RDI_U8 *range_first = ptr;
    
    // rjf: skip past non-slashes
    for(;ptr < opl && !(*ptr == '/' || *ptr == '\\'); ptr += 1);
    
    // rjf: empty range -> continue
    if(range_first >= ptr)
    {
      continue;
    }
    
    // rjf: range -> sub-directory string
    RDIM_String8 sub_dir = rdim_str8(range_first, (RDI_U64)(ptr-range_first));
    
    // rjf: sub-directory string -> find child of node
    RDIM_BakePathNode *sub_dir_node = 0;
    for(RDIM_BakePathNode *child = node->first_child; child != 0; child = child->next_sibling)
    {
      if(rdim_str8_match(child->name, sub_dir, RDIM_StringMatchFlag_CaseInsensitive))
      {
        sub_dir_node = child;
      }
    }
    
    // rjf: descend to child
    node = sub_dir_node;
  }
  return node;
}

RDI_PROC RDI_U32
rdim_bake_path_node_idx_from_string(RDIM_BakePathTree *tree, RDIM_String8 string)
{
  RDIM_BakePathNode *path_node = rdim_bake_path_node_from_string(tree, string);
  return path_node ? path_node->idx : 0;
}

RDI_PROC RDIM_BakePathNode *
rdim_bake_path_tree_insert(RDIM_Arena *arena, RDIM_BakePathTree *tree, RDIM_String8 string)
{
  RDIM_BakePathNode *node = &tree->root;
  RDI_U8 *ptr = string.str;
  RDI_U8 *opl = string.str + string.size;
  for(;ptr < opl;)
  {
    // rjf: skip past slashes
    for(;ptr < opl && (*ptr == '/' || *ptr == '\\'); ptr += 1);
    
    // rjf: save beginning of non-slash range
    RDI_U8 *range_first = ptr;
    
    // rjf: skip past non-slashes
    for(;ptr < opl && !(*ptr == '/' || *ptr == '\\'); ptr += 1);
    
    // rjf: empty range -> continue
    if(range_first >= ptr)
    {
      continue;
    }
    
    // rjf: range -> sub-directory string
    RDIM_String8 sub_dir = rdim_str8(range_first, (RDI_U64)(ptr-range_first));
    
    // rjf: sub-directory string -> find child of node
    RDIM_BakePathNode *sub_dir_node = 0;
    for(RDIM_BakePathNode *child = node->first_child; child != 0; child = child->next_sibling)
    {
      if(rdim_str8_match(child->name, sub_dir, RDIM_StringMatchFlag_CaseInsensitive))
      {
        sub_dir_node = child;
      }
    }
    
    // rjf: no child -> make one
    if(sub_dir_node == 0)
    {
      sub_dir_node = rdim_push_array(arena, RDIM_BakePathNode, 1);
      RDIM_SLLQueuePush_N(tree->first, tree->last, sub_dir_node, next_order);
      sub_dir_node->parent = node;
      RDIM_SLLQueuePush_N(node->first_child, node->last_child, sub_dir_node, next_sibling);
      sub_dir_node->name = rdim_str8_copy(arena, sub_dir);
      sub_dir_node->idx = tree->count;
      tree->count += 1;
    }
    
    // rjf: descend to child
    node = sub_dir_node;
  }
  return node;
}

//- rjf: bake name maps writing

RDI_PROC void
rdim_bake_name_map_push(RDIM_Arena *arena, RDIM_BakeNameMap *map, RDIM_String8 string, RDI_U32 idx)
{
  if(string.size == 0) {return;}
  
  // rjf: hash
  RDI_U64 hash = rdi_hash(string.RDIM_String8_BaseMember, string.RDIM_String8_SizeMember);
  RDI_U64 slot_idx = hash%map->slots_count;
  
  // rjf: find existing node
  RDIM_BakeNameMapNode *node = 0;
  for(RDIM_BakeNameMapNode *n = map->slots[slot_idx]; n != 0; n = n->slot_next)
  {
    if(rdim_str8_match(string, n->string, 0))
    {
      node = n;
      break;
    }
  }
  
  // rjf: make node if necessary
  if(node == 0)
  {
    node = rdim_push_array(arena, RDIM_BakeNameMapNode, 1);
    node->string = string;
    RDIM_SLLStackPush_N(map->slots[slot_idx], node, slot_next);
    RDIM_SLLQueuePush_N(map->first, map->last, node, order_next);
    map->name_count += 1;
    map->slot_collision_count += (node->slot_next != 0);
  }
  
  // rjf: find existing idx
  RDI_S32 existing_idx = 0;
  for(RDIM_BakeNameMapValNode *n = node->val_first; n != 0; n = n->next)
  {
    for(RDI_U32 i = 0; i < sizeof(n->val)/sizeof(n->val[0]); i += 1)
    {
      if(n->val[i] == 0)
      {
        break;
      }
      if(n->val[i] == idx)
      {
        existing_idx = 1;
        break;
      }
    }
  }
  
  // rjf: insert new idx if necessary
  if(!existing_idx)
  {
    RDIM_BakeNameMapValNode *val_node = node->val_last;
    RDI_U32 insert_i = node->val_count%(sizeof(val_node->val)/sizeof(val_node->val[0]));
    if(insert_i == 0)
    {
      val_node = rdim_push_array(arena, RDIM_BakeNameMapValNode, 1);
      SLLQueuePush(node->val_first, node->val_last, val_node);
    }
    val_node->val[insert_i] = idx;
    node->val_count += 1;
  }
}

////////////////////////////////
//~ rjf: [Baking Helpers] Data Section List Building Helpers

RDI_PROC RDIM_BakeSection *
rdim_bake_section_list_push(RDIM_Arena *arena, RDIM_BakeSectionList *list)
{
  RDIM_BakeSectionNode *n = rdim_push_array(arena, RDIM_BakeSectionNode, 1);
  RDIM_SLLQueuePush(list->first, list->last, n);
  list->count += 1;
  RDIM_BakeSection *result = &n->v;
  return result;
}

RDI_PROC RDIM_BakeSection *
rdim_bake_section_list_push_new_unpacked_list(RDIM_Arena *arena, RDIM_BakeSectionList *list, RDI_DataSectionTag tag, RDIM_String8List data)
{
  RDIM_BakeSection *section = rdim_bake_section_list_push(arena, list);
  section->tag           = tag;
  section->encoding      = RDI_DataSectionEncoding_Unpacked;
  section->data          = data;
  section->unpacked_size = data.total_size;
  return section;
}

RDI_PROC RDIM_BakeSection *
rdim_bake_section_list_push_new_unpacked_raw(RDIM_Arena *arena, RDIM_BakeSectionList *list, RDI_DataSectionTag tag, void *data, RDI_U64 data_size)
{
  RDIM_String8List data_list = {0};
  rdim_str8_list_push(arena, &data_list, rdim_str8(data, data_size));
  return rdim_bake_section_list_push_new_unpacked_list(arena, list, tag, data_list);
}

RDI_PROC void
rdim_bake_section_list_concat_in_place(RDIM_BakeSectionList *dst, RDIM_BakeSectionList *to_push)
{
  if(dst->last != 0 && to_push->first != 0)
  {
    dst->last->next = to_push->first;
    dst->last = to_push->last;
    dst->count += to_push->count;
  }
  else if(to_push->first != 0)
  {
    rdim_memcpy_struct(dst, to_push);
  }
  rdim_memzero_struct(to_push);
}

////////////////////////////////
//~ rjf: [Baking] Build Artifacts -> Interned/Deduplicated Data Structures

//- rjf: basic bake string gathering passes

RDI_PROC void
rdim_bake_string_map_loose_push_top_level_info(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_TopLevelInfo *tli)
{
  rdim_bake_string_map_loose_insert(arena, top, map, 1, tli->exe_name);
}

RDI_PROC void
rdim_bake_string_map_loose_push_binary_sections(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_BinarySectionList *secs)
{
  for(RDIM_BinarySectionNode *n = secs->first; n != 0; n = n->next)
  {
    rdim_bake_string_map_loose_insert(arena, top, map, 1, n->v.name);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_path_tree(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_BakePathTree *path_tree)
{
  for(RDIM_BakePathNode *n = path_tree->first; n != 0; n = n->next_order)
  {
    rdim_bake_string_map_loose_insert(arena, top, map, 1, n->name);
  }
}

//- rjf: chunk-granularity bake string gathering passes

RDI_PROC void
rdim_bake_string_map_loose_push_src_file_slice(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_SrcFile *v, RDI_U64 count)
{
  for(RDI_U64 idx = 0; idx < count; idx += 1)
  {
    rdim_bake_string_map_loose_insert(arena, top, map, 1, v[idx].normal_full_path);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_unit_slice(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_Unit *v, RDI_U64 count)
{
  for(RDI_U64 idx = 0; idx < count; idx += 1)
  {
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].unit_name);
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].compiler_name);
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].source_file);
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].object_file);
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].archive_file);
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].build_path);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_type_slice(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_Type *v, RDI_U64 count)
{
  for(RDI_U64 idx = 0; idx < count; idx += 1)
  {
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].name);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_udt_slice(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_UDT *v, RDI_U64 count)
{
  for(RDI_U64 idx = 0; idx < count; idx += 1)
  {
    for(RDIM_UDTMember *mem = v[idx].first_member; mem != 0; mem = mem->next)
    {
      rdim_bake_string_map_loose_insert(arena, top, map, 4, mem->name);
    }
    for(RDIM_UDTEnumVal *mem = v[idx].first_enum_val; mem != 0; mem = mem->next)
    {
      rdim_bake_string_map_loose_insert(arena, top, map, 4, mem->name);
    }
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_symbol_slice(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_Symbol *v, RDI_U64 count)
{
  for(RDI_U64 idx = 0; idx < count; idx += 1)
  {
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].name);
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].link_name);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_scope_slice(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_Scope *v, RDI_U64 count)
{
  for(RDI_U64 idx = 0; idx < count; idx += 1)
  {
    for(RDIM_Local *local = v[idx].first_local; local != 0; local = local->next)
    {
      rdim_bake_string_map_loose_insert(arena, top, map, 4, local->name);
    }
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_inline_site_slice(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_InlineSite *v, RDI_U64 count)
{
  for(RDI_U64 idx = 0; idx < count; idx += 1)
  {
    rdim_bake_string_map_loose_insert(arena, top, map, 4, v[idx].name);
  }
}

//- rjf: list-granularity bake string gathering passes

RDI_PROC void
rdim_bake_string_map_loose_push_src_files(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_SrcFileChunkList *list)
{
  for(RDIM_SrcFileChunkNode *n = list->first; n != 0; n = n->next)
  {
    rdim_bake_string_map_loose_push_src_file_slice(arena, top, map, n->v, n->count);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_units(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_UnitChunkList *list)
{
  for(RDIM_UnitChunkNode *n = list->first; n != 0; n = n->next)
  {
    rdim_bake_string_map_loose_push_unit_slice(arena, top, map, n->v, n->count);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_types(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_TypeChunkList *list)
{
  for(RDIM_TypeChunkNode *n = list->first; n != 0; n = n->next)
  {
    rdim_bake_string_map_loose_push_type_slice(arena, top, map, n->v, n->count);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_udts(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_UDTChunkList *list)
{
  for(RDIM_UDTChunkNode *n = list->first; n != 0; n = n->next)
  {
    rdim_bake_string_map_loose_push_udt_slice(arena, top, map, n->v, n->count);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_symbols(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_SymbolChunkList *list)
{
  for(RDIM_SymbolChunkNode *n = list->first; n != 0; n = n->next)
  {
    rdim_bake_string_map_loose_push_symbol_slice(arena, top, map, n->v, n->count);
  }
}

RDI_PROC void
rdim_bake_string_map_loose_push_scopes(RDIM_Arena *arena, RDIM_BakeStringMapTopology *top, RDIM_BakeStringMapLoose *map, RDIM_ScopeChunkList *list)
{
  for(RDIM_ScopeChunkNode *n = list->first; n != 0; n = n->next)
  {
    rdim_bake_string_map_loose_push_scope_slice(arena, top, map, n->v, n->count);
  }
}

//- rjf: bake name map building

RDI_PROC RDIM_BakeNameMap *
rdim_bake_name_map_from_kind_params(RDIM_Arena *arena, RDI_NameMapKind kind, RDIM_BakeParams *params)
{
  RDIM_BakeNameMap *map = rdim_push_array(arena, RDIM_BakeNameMap, 1);
  switch(kind)
  {
    default:{}break;
    case RDI_NameMapKind_GlobalVariables:
    {
      map->slots_count = params->global_variables.total_count*2;
      map->slots = rdim_push_array(arena, RDIM_BakeNameMapNode *, map->slots_count);
      for(RDIM_SymbolChunkNode *n = params->global_variables.first; n != 0; n = n->next)
      {
        for(RDI_U64 idx = 0; idx < n->count; idx += 1)
        {
          RDI_U32 symbol_idx = (RDI_U32)rdim_idx_from_symbol(&n->v[idx]); // TODO(rjf): @u64_to_u32
          rdim_bake_name_map_push(arena, map, n->v[idx].name, symbol_idx);
        }
      }
    }break;
    case RDI_NameMapKind_ThreadVariables:
    {
      map->slots_count = params->thread_variables.total_count*2;
      map->slots = rdim_push_array(arena, RDIM_BakeNameMapNode *, map->slots_count);
      for(RDIM_SymbolChunkNode *n = params->thread_variables.first; n != 0; n = n->next)
      {
        for(RDI_U64 idx = 0; idx < n->count; idx += 1)
        {
          RDI_U32 symbol_idx = (RDI_U32)rdim_idx_from_symbol(&n->v[idx]); // TODO(rjf): @u64_to_u32
          rdim_bake_name_map_push(arena, map, n->v[idx].name, symbol_idx);
        }
      }
    }break;
    case RDI_NameMapKind_Procedures:
    {
      map->slots_count = params->procedures.total_count*2;
      map->slots = rdim_push_array(arena, RDIM_BakeNameMapNode *, map->slots_count);
      for(RDIM_SymbolChunkNode *n = params->procedures.first; n != 0; n = n->next)
      {
        for(RDI_U64 idx = 0; idx < n->count; idx += 1)
        {
          RDI_U32 symbol_idx = (RDI_U32)rdim_idx_from_symbol(&n->v[idx]); // TODO(rjf): @u64_to_u32
          rdim_bake_name_map_push(arena, map, n->v[idx].name, symbol_idx);
        }
      }
    }break;
    case RDI_NameMapKind_Types:
    {
      map->slots_count = params->types.total_count;
      map->slots = rdim_push_array(arena, RDIM_BakeNameMapNode *, map->slots_count);
      for(RDIM_TypeChunkNode *n = params->types.first; n != 0; n = n->next)
      {
        for(RDI_U64 idx = 0; idx < n->count; idx += 1)
        {
          RDI_U32 type_idx = (RDI_U32)rdim_idx_from_type(&n->v[idx]); // TODO(rjf): @u64_to_u32
          rdim_bake_name_map_push(arena, map, n->v[idx].name, type_idx);
        }
      }
    }break;
    case RDI_NameMapKind_LinkNameProcedures:
    {
      map->slots_count = params->procedures.total_count*2;
      map->slots = rdim_push_array(arena, RDIM_BakeNameMapNode *, map->slots_count);
      for(RDIM_SymbolChunkNode *n = params->procedures.first; n != 0; n = n->next)
      {
        for(RDI_U64 idx = 0; idx < n->count; idx += 1)
        {
          if(n->v[idx].link_name.size == 0) {continue;}
          RDI_U32 symbol_idx = (RDI_U32)rdim_idx_from_symbol(&n->v[idx]); // TODO(rjf): @u64_to_u32
          rdim_bake_name_map_push(arena, map, n->v[idx].link_name, symbol_idx);
        }
      }
    }break;
    case RDI_NameMapKind_NormalSourcePaths:
    {
      map->slots_count = params->src_files.total_count*2;
      map->slots = rdim_push_array(arena, RDIM_BakeNameMapNode *, map->slots_count);
      for(RDIM_SrcFileChunkNode *n = params->src_files.first; n != 0; n = n->next)
      {
        for(RDI_U64 idx = 0; idx < n->count; idx += 1)
        {
          RDI_U32 src_file_idx = (RDI_U32)rdim_idx_from_src_file(&n->v[idx]); // TODO(rjf): @u64_to_u32
          rdim_bake_name_map_push(arena, map, n->v[idx].normal_full_path, src_file_idx);
        }
      }
    }break;
  }
  return map;
}

//- rjf: idx run map building

RDI_PROC RDIM_BakeIdxRunMap *
rdim_bake_idx_run_map_from_params(RDIM_Arena *arena, RDIM_BakeNameMap *name_maps[RDI_NameMapKind_COUNT], RDIM_BakeParams *params)
{
  //- rjf: set up map
  RDIM_BakeIdxRunMap *idx_runs = rdim_push_array(arena, RDIM_BakeIdxRunMap, 1);
  idx_runs->slots_count = params->procedures.total_count*2 + params->global_variables.total_count*2 + params->thread_variables.total_count*2 + params->types.total_count*2;
  idx_runs->slots = rdim_push_array(arena, RDIM_BakeIdxRunNode *, idx_runs->slots_count);
  rdim_bake_idx_run_map_insert(arena, idx_runs, 0, 0);
  
  //- rjf: bake runs of function-type parameter lists
  for(RDIM_TypeChunkNode *n = params->types.first; n != 0; n = n->next)
  {
    for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1)
    {
      RDIM_Type *type = &n->v[chunk_idx];
      if(type->kind == RDI_TypeKind_Function || type->kind == RDI_TypeKind_Method)
      {
        RDI_U32 param_idx_run_count = type->count;
        RDI_U32 *param_idx_run = rdim_push_array_no_zero(arena, RDI_U32, param_idx_run_count);
        for(RDI_U32 idx = 0; idx < param_idx_run_count; idx += 1)
        {
          param_idx_run[idx] = (RDI_U32)rdim_idx_from_type(type->param_types[idx]); // TODO(rjf): @u64_to_u32
        }
        rdim_bake_idx_run_map_insert(arena, idx_runs, param_idx_run, param_idx_run_count);
      }
    }
  }
  
  //- rjf: bake runs of name map match lists
  for(RDI_NameMapKind k = (RDI_NameMapKind)(RDI_NameMapKind_NULL+1);
      k < RDI_NameMapKind_COUNT;
      k = (RDI_NameMapKind)(k+1))
  {
    RDIM_BakeNameMap *name_map = name_maps[k];
    if(name_map != 0 && name_map->name_count != 0)
    {
      for(RDIM_BakeNameMapNode *n = name_map->first; n != 0; n = n->order_next)
      {
        if(n->val_count > 1)
        {
          RDI_U32 *idx_run = rdim_push_array(arena, RDI_U32, n->val_count);
          RDI_U64 val_idx = 0;
          for(RDIM_BakeNameMapValNode *idxnode = n->val_first;
              idxnode != 0;
              idxnode = idxnode->next)
          {
            for(RDI_U32 i = 0; i < sizeof(idxnode->val)/sizeof(idxnode->val[0]); i += 1)
            {
              if(idxnode->val[i] == 0)
              {
                goto dblbreak;
              }
              idx_run[val_idx] = idxnode->val[i];
              val_idx += 1;
            }
          }
          dblbreak:;
          rdim_bake_idx_run_map_insert(arena, idx_runs, idx_run, (RDI_U32)n->val_count); // TODO(rjf): @u64_to_u32
        }
      }
    }
  }
  
  return idx_runs;
}

//- rjf: bake path tree building

RDI_PROC RDIM_BakePathTree *
rdim_bake_path_tree_from_params(RDIM_Arena *arena, RDIM_BakeParams *params)
{
  //- rjf: set up tree
  RDIM_BakePathTree *tree = rdim_push_array(arena, RDIM_BakePathTree, 1);
  rdim_bake_path_tree_insert(arena, tree, rdim_str8_lit("<nil>"));
  
  //- rjf: bake unit file paths
  RDIM_ProfScope("bake unit file paths")
  {
    for(RDIM_UnitChunkNode *n = params->units.first; n != 0; n = n->next)
    {
      for(RDI_U64 idx = 0; idx < n->count; idx += 1)
      {
        rdim_bake_path_tree_insert(arena, tree, n->v[idx].source_file);
        rdim_bake_path_tree_insert(arena, tree, n->v[idx].object_file);
        rdim_bake_path_tree_insert(arena, tree, n->v[idx].archive_file);
        rdim_bake_path_tree_insert(arena, tree, n->v[idx].build_path);
      }
    }
  }
  
  //- rjf: bake source file paths
  RDIM_ProfScope("bake source file paths")
  {
    for(RDIM_SrcFileChunkNode *n = params->src_files.first; n != 0; n = n->next)
    {
      for(RDI_U64 idx = 0; idx < n->count; idx += 1)
      {
        RDIM_BakePathNode *node = rdim_bake_path_tree_insert(arena, tree, n->v[idx].normal_full_path);
        node->src_file = &n->v[idx];
      }
    }
  }
  
  return tree;
}

////////////////////////////////
//~ rjf: [Baking] Build Artifacts -> Data Section Lists

//- rjf: top-level info

RDI_PROC RDIM_BakeSectionList
rdim_bake_top_level_info_section_list_from_params(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeParams *params)
{
  RDIM_BakeSectionList sections = {0};
  RDI_TopLevelInfo  *dst_tli = rdim_push_array(arena, RDI_TopLevelInfo, 1);
  RDIM_TopLevelInfo *src_tli = &params->top_level_info;
  dst_tli->architecture        = src_tli->arch;
  dst_tli->exe_name_string_idx = rdim_bake_idx_from_string(strings, src_tli->exe_name);
  dst_tli->exe_hash            = src_tli->exe_hash;
  dst_tli->voff_max            = src_tli->voff_max;
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_TopLevelInfo, dst_tli, sizeof(*dst_tli));
  return sections;
}

//- rjf: binary sections

RDI_PROC RDIM_BakeSectionList
rdim_bake_binary_section_section_list_from_params(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeParams *params)
{
  RDIM_BakeSectionList sections = {0};
  RDIM_BinarySectionList *src_list = &params->binary_sections;
  RDI_BinarySection *dst_base = rdim_push_array(arena, RDI_BinarySection, src_list->count+1);
  U64 dst_idx = 1;
  for(RDIM_BinarySectionNode *src_n = src_list->first; src_n != 0; src_n = src_n->next, dst_idx += 1)
  {
    RDIM_BinarySection *src = &src_n->v;
    RDI_BinarySection *dst = &dst_base[dst_idx];
    dst->name_string_idx = rdim_bake_idx_from_string(strings, src->name);
    dst->flags           = src->flags;
    dst->voff_first      = src->voff_first;
    dst->voff_opl        = src->voff_opl;
    dst->foff_first      = src->foff_first;
    dst->foff_opl        = src->foff_opl;
  }
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_BinarySections, dst_base, sizeof(*dst_base)*dst_idx);
  return sections;
}

//- rjf: units

RDI_PROC RDIM_LineInfo
rdim_bake_line_info(RDIM_Arena *arena, RDIM_LineSequenceList *line_sequences)
{
  RDIM_ProfBegin("rdim_bake_line_info");
  RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);

  //- rjf: gather up all line info into two arrays:
  //
  // [1] keys: sortable array; pairs voffs with line info records; null records are sequence enders
  // [2] recs: contains all the source coordinates for a range of voffs
  //
  typedef struct RDIM_LineRec RDIM_LineRec;
  struct RDIM_LineRec
  {
    RDI_U32 file_id;
    RDI_U32 line_num;
    RDI_U16 col_first;
    RDI_U16 col_opl;
  };
  RDI_U64 line_count = 0;
  RDI_U64 voff_count = 0;
  for(RDIM_LineSequenceNode *seq_n = line_sequences->first; seq_n != 0; seq_n = seq_n->next)
  {
    voff_count += seq_n->v.line_count + 1;
    line_count += seq_n->v.line_count;
  }
  U64 key_count = voff_count;
  RDIM_SortKey *line_keys = rdim_push_array_no_zero(scratch.arena, RDIM_SortKey, key_count);
  RDIM_LineRec *line_recs = rdim_push_array_no_zero(scratch.arena, RDIM_LineRec, line_count);
  {
    RDIM_SortKey *key_ptr = line_keys;
    RDIM_LineRec *rec_ptr = line_recs;
    for(RDIM_LineSequenceNode *seq_n = line_sequences->first; seq_n != 0; seq_n = seq_n->next)
    {
      RDIM_LineSequence *seq = &seq_n->v;
      for(RDI_U64 line_idx = 0; line_idx < seq->line_count; line_idx += 1)
      {
        key_ptr->key = seq->voffs[line_idx];
        key_ptr->val = rec_ptr;
        key_ptr += 1;
        rec_ptr->file_id = (RDI_U32)rdim_idx_from_src_file(seq->src_file); // TODO(rjf): @u64_to_u32
        rec_ptr->line_num = seq->line_nums[line_idx];
        if(seq->col_nums != 0)
        {
          rec_ptr->col_first = seq->col_nums[line_idx*2];
          rec_ptr->col_opl = seq->col_nums[line_idx*2 + 1];
        }
        rec_ptr += 1;
      }
      key_ptr->key = seq->voffs[seq->line_count];
      key_ptr->val = 0;
      key_ptr += 1;
    }
  }
  
  //- rjf: sort
  RDIM_SortKey *sorted_line_keys = 0;
  RDIM_ProfScope("sort")
  {
    sorted_line_keys = rdim_sort_key_array(scratch.arena, line_keys, key_count);
  }
  
  // TODO(rjf): do a pass over sorted keys to make sure duplicate keys
  // are sorted with null record first, and no more than one null
  // record and one non-null record
  
  //- rjf: arrange output
  RDI_U64 *arranged_voffs = rdim_push_array_no_zero(arena, RDI_U64, key_count + 1);
  RDI_Line *arranged_lines = rdim_push_array_no_zero(arena, RDI_Line, key_count);
  RDIM_ProfScope("arrange output")
  {
    for(RDI_U64 i = 0; i < key_count; i += 1)
    {
      arranged_voffs[i] = sorted_line_keys[i].key;
    }
    arranged_voffs[key_count] = ~0ull;
    for(RDI_U64 i = 0; i < key_count; i += 1)
    {
      RDIM_LineRec *rec = (RDIM_LineRec*)sorted_line_keys[i].val;
      if(rec != 0)
      {
        arranged_lines[i].file_idx = rec->file_id;
        arranged_lines[i].line_num = rec->line_num;
      }
      else
      {
        arranged_lines[i].file_idx = 0;
        arranged_lines[i].line_num = 0;
      }
    }
  }
  
  // fill out line info
  RDIM_LineInfo line_info = {0};
  line_info.voff_count = key_count + 1;
  line_info.line_count = key_count;
  line_info.col_count  = 0;
  line_info.voffs      = arranged_voffs;
  line_info.lines      = arranged_lines;
  line_info.cols       = 0;
  
  rdim_scratch_end(scratch);
  RDIM_ProfEnd();
  return line_info;
}

RDI_PROC RDIM_LineInfoLayout
rdim_make_line_info_layout(Arena *arena, RDIM_LineSequenceListChunkList lines)
{
  RDIM_ProfBegin("rdim_make_line_info_layout");

  RDI_U64 *voff_indices = rdim_push_array_no_zero(arena, RDI_U64, lines.total_count);
  RDI_U64 *line_indices = rdim_push_array_no_zero(arena, RDI_U64, lines.total_count);
  RDI_U64 *col_indices  = rdim_push_array_no_zero(arena, RDI_U64, lines.total_count);

  RDI_U64 line_info_idx = 0;
  RDI_U64 voff_cursor = 0, line_cursor = 0, col_cursor = 0;
  for(RDIM_LineSequenceListChunkNode *chunk = lines.first; chunk != 0; chunk = chunk->next)
  {
    for(U64 seq_idx = 0; seq_idx < chunk->count; seq_idx += 1, line_info_idx += 1)
    {
      voff_indices[line_info_idx] = voff_cursor;
      line_indices[line_info_idx] = line_cursor;
      col_indices[line_info_idx]  = col_cursor;

      RDIM_LineSequenceList *seq_list = chunk->v + seq_idx;

      RDI_U64 voff_count = 0;
      RDI_U64 line_count = 0;
      RDI_U64 col_count  = 0;
      for(RDIM_LineSequenceNode *seq_node = seq_list->first; seq_node != 0; seq_node = seq_node->next)
      {
        voff_count += seq_node->v.line_count > 0 ? seq_node->v.line_count + 1 : 0;
        line_count += seq_node->v.line_count;
        //col_count  += seq_node->v.col_count;
      }

      voff_cursor += voff_count + 1;
      line_cursor += voff_count;
      //col_curosr  += col_count;
    }
  }

  RDIM_LineInfoLayout layout = {0};
  layout.voff_indices = voff_indices;
  layout.line_indices = line_indices;
  layout.col_indices  = col_indices;
  layout.voff_count   = voff_cursor;
  layout.line_count   = line_cursor;
  layout.col_count    = col_cursor;

  RDIM_ProfEnd();
  return layout;
}

RDI_PROC RDIM_BakedLineChunk
rdim_bake_section_list_from_lines(RDIM_Arena *arena, RDIM_LineInfoLayout layout, RDIM_LineSequenceListChunkNode *chunk)
{
  RDIM_ProfBegin("rdim_bake_section_list_from_lines");
  RDIM_BakedLineChunk baked_chunk = {0};
  for(U64 seq_idx = 0; seq_idx < chunk->count; seq_idx += 1)
  {
    RDIM_LineSequenceList *seq           = chunk->v + seq_idx;
    RDI_U64                line_info_idx = rdim_idx_from_line_sequence_list(seq);
    RDIM_LineInfo          line_info     = rdim_bake_line_info(arena, seq);

    // fill out line info
    RDI_LineInfo *dst_line_info = rdim_push_array_no_zero(arena, RDI_LineInfo, 1);
    dst_line_info->line_count    = line_info.line_count;
    dst_line_info->col_count     = 0;
    dst_line_info->voff_data_idx = layout.voff_indices[line_info_idx];
    dst_line_info->line_data_idx = layout.line_indices[line_info_idx];
    dst_line_info->col_data_idx  = layout.col_indices[line_info_idx];

    // push line arrays
    rdim_str8_list_push(arena, &baked_chunk.line_infos, rdim_str8((RDI_U8 *)dst_line_info,   sizeof(*dst_line_info)));
    rdim_str8_list_push(arena, &baked_chunk.voffs,      rdim_str8((RDI_U8 *)line_info.voffs, sizeof(line_info.voffs[0]) * line_info.voff_count));
    rdim_str8_list_push(arena, &baked_chunk.lines,      rdim_str8((RDI_U8 *)line_info.lines, sizeof(line_info.lines[0]) * line_info.line_count));
    rdim_str8_list_push(arena, &baked_chunk.cols,       rdim_str8((RDI_U8 *)line_info.cols,  sizeof(line_info.cols[0] ) * line_info.col_count));
  }
  RDIM_ProfEnd();
  return baked_chunk;
}

RDI_PROC RDIM_BakeSectionList
rdim_bake_unit_top_level_section_list(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakePathTree *path_tree, RDIM_UnitChunkList units)
{
  RDIM_ProfBegin("rdim_bake_unit_top_level_section_list_from_params");
  RDIM_BakeSectionList sections = {0};
  RDI_Unit *dst_base = rdim_push_array(arena, RDI_Unit, units.total_count+1);
  RDI_U64   dst_idx  = 1;
  for(RDIM_UnitChunkNode *src_n = units.first; src_n != 0; src_n = src_n->next)
  {
    for(RDI_U64 src_chunk_idx = 0; src_chunk_idx < src_n->count; src_chunk_idx += 1, dst_idx += 1)
    {
      RDIM_Unit *src = src_n->v + src_chunk_idx;
      RDI_Unit  *dst = dst_base + dst_idx;

      dst->unit_name_string_idx     = rdim_bake_idx_from_string(strings, src->unit_name);
      dst->compiler_name_string_idx = rdim_bake_idx_from_string(strings, src->compiler_name);
      dst->source_file_path_node    = rdim_bake_path_node_idx_from_string(path_tree, src->source_file);
      dst->object_file_path_node    = rdim_bake_path_node_idx_from_string(path_tree, src->object_file);
      dst->archive_file_path_node   = rdim_bake_path_node_idx_from_string(path_tree, src->archive_file);
      dst->build_path_node          = rdim_bake_path_node_idx_from_string(path_tree, src->build_path);
      dst->language                 = src->language;
      dst->line_info_idx            = (RDI_U32)rdim_idx_from_line_sequence_list(src->main_line_table); // @u64_to_u32
    }
  }
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_Units, dst_base, sizeof(*dst_base)*dst_idx);
  RDIM_ProfEnd();
  return sections;
}

//- rjf: unit vmap

RDI_PROC RDIM_BakeSectionList
rdim_bake_unit_vmap_section_list_from_params(RDIM_Arena *arena, RDIM_BakeParams *params)
{
  //- rjf: build vmap from unit voff ranges
  RDIM_BakeVMap unit_vmap = {0};
  {
    RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);
    
    // rjf: count voff ranges
    RDI_U64 voff_range_count = 0;
    for(RDIM_UnitChunkNode *n = params->units.first; n != 0; n = n->next)
    {
      for(RDI_U64 idx = 0; idx < n->count; idx += 1)
      {
        RDIM_Unit *unit = &n->v[idx];
        voff_range_count += unit->voff_ranges.count;
      }
    }
    
    // rjf: count necessary markers
    RDI_U64 marker_count = voff_range_count*2;
    
    // rjf: build keys/markers arrays
    RDIM_SortKey    *keys = rdim_push_array_no_zero(scratch.arena, RDIM_SortKey, marker_count);
    RDIM_VMapMarker *markers = rdim_push_array_no_zero(scratch.arena, RDIM_VMapMarker, marker_count);
    {
      RDIM_SortKey *key_ptr = keys;
      RDIM_VMapMarker *marker_ptr = markers;
      RDI_U32 unit_idx = 1;
      for(RDIM_UnitChunkNode *unit_chunk_n = params->units.first;
          unit_chunk_n != 0;
          unit_chunk_n = unit_chunk_n->next)
      {
        for(RDI_U64 idx = 0; idx < unit_chunk_n->count; idx += 1)
        {
          RDIM_Unit *unit = &unit_chunk_n->v[idx];
          for(RDIM_Rng1U64Node *n = unit->voff_ranges.first; n != 0; n = n->next)
          {
            RDIM_Rng1U64 range = n->v;
            if(range.min < range.max)
            {
              key_ptr->key = range.min;
              key_ptr->val = marker_ptr;
              marker_ptr->idx = unit_idx;
              marker_ptr->begin_range = 1;
              key_ptr += 1;
              marker_ptr += 1;
              
              key_ptr->key = range.max;
              key_ptr->val = marker_ptr;
              marker_ptr->idx = unit_idx;
              marker_ptr->begin_range = 0;
              key_ptr += 1;
              marker_ptr += 1;
            }
          }
          unit_idx += 1;
        }
      }
    }
    
    // rjf: keys/markers -> unit vmap
    unit_vmap = rdim_bake_vmap_from_markers(arena, markers, keys, marker_count);
    rdim_scratch_end(scratch);
  }
  
  //- rjf: build section
  RDIM_BakeSectionList sections = {0};
  RDI_U64 unit_vmap_size = sizeof(unit_vmap.vmap[0])*(unit_vmap.count+1);
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_UnitVmap, unit_vmap.vmap, unit_vmap_size);
  return sections;
}

//- checksums

RDI_PROC RDIM_BakeSectionList
rdim_bake_checksums_list_from_params(RDIM_Arena *arena, RDIM_BakeParams *params, RDI_U64 **out_checksum_offsets)
{
  U64 checksum_count = (params->checksums.total_count + 1);

  *out_checksum_offsets = push_array_no_zero(arena, RDI_U64, checksum_count);

  RDI_U64 raw_checksums_size = checksum_count * sizeof(RDI_Checksum);
  for(RDIM_ChecksumChunkNode *chunk = params->checksums.first; chunk != 0; chunk = chunk->next)
  {
    for(U64 i = 0; i < chunk->count; i += 1)
    {
      raw_checksums_size += chunk->v[i].data.size;
    }
  }

  U8      *raw_checksums = push_array_no_zero(arena, U8, raw_checksums_size);
  RDI_U64  cursor        = 0;
  RDI_U64  checksum_idx  = 0;

  // write null item
  {
    RDI_Checksum *checksum = (RDI_Checksum *)(raw_checksums + cursor);
    checksum->kind = RDI_Checksum_Null;
    checksum->size = 0;

    (*out_checksum_offsets)[checksum_idx] = cursor;
    checksum_idx += 1;

    cursor += sizeof(*checksum);
  }

  for(RDIM_ChecksumChunkNode *chunk = params->checksums.first; chunk != 0; chunk = chunk->next)
  {
    for(U64 i = 0; i < chunk->count; i += 1)
    {
      RDIM_Checksum *src = chunk->v + i;
      RDI_Checksum  *dst = (RDI_Checksum *)(raw_checksums + cursor);

      // checksum size must fit into U8
      rdim_assert(src->data.size <= 0xff);

      // copy checksum
      dst->kind = src->kind;
      dst->size = (RDI_U8)src->data.size;
      MemoryCopy((U8 *)(dst + 1), src->data.str, src->data.size);

      // store checksum buffer offset
      (*out_checksum_offsets)[checksum_idx] = cursor;
      checksum_idx += 1;

      // advance cursor
      cursor += sizeof(*dst) + dst->size;
    }
  }

  rdim_assert(cursor == raw_checksums_size);
  rdim_assert(checksum_idx == checksum_count);

  // build section
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_Checksums, raw_checksums, raw_checksums_size);
  return sections;
}

//- line numbers

RDI_PROC RDIM_LineNumberMap
rdim_bake_line_number_map(RDIM_Arena *arena, RDIM_SrcFileLineMapFragment *fragment_list)
{
  RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);

  //- rjf: gather line number map
  typedef struct RDIM_SrcLineMapVoffBlock RDIM_SrcLineMapVoffBlock;
  struct RDIM_SrcLineMapVoffBlock
  {
    RDIM_SrcLineMapVoffBlock *next;
    RDI_U64 voff;
  };
  typedef struct RDIM_SrcLineMapBucket RDIM_SrcLineMapBucket;
  struct RDIM_SrcLineMapBucket
  {
    RDIM_SrcLineMapBucket *order_next;
    RDIM_SrcLineMapBucket *hash_next;
    RDI_U32 line_num;
    RDIM_SrcLineMapVoffBlock *first_voff_block;
    RDIM_SrcLineMapVoffBlock *last_voff_block;
    RDI_U64 voff_count;
  };
  RDIM_SrcLineMapBucket *first_bucket = 0;
  RDIM_SrcLineMapBucket *last_bucket = 0;
  RDI_U64 line_hash_slots_count = 2048;
  RDIM_SrcLineMapBucket **line_hash_slots = rdim_push_array(scratch.arena, RDIM_SrcLineMapBucket *, line_hash_slots_count);
  RDI_U64 line_count = 0;
  RDI_U64 voff_count = 0;
  RDI_U64 max_line_num = 0;
  {
    for(RDIM_SrcFileLineMapFragment *map_fragment = fragment_list;
        map_fragment != 0;
        map_fragment = map_fragment->next)
    {
      RDIM_LineSequence *sequence = map_fragment->seq;
      RDI_U64 *seq_voffs = sequence->voffs;
      RDI_U32 *seq_line_nums = sequence->line_nums;
      RDI_U64 seq_line_count = sequence->line_count;
      for(RDI_U64 i = 0; i < seq_line_count; i += 1)
      {
        RDI_U32 line_num = seq_line_nums[i];
        RDI_U64 voff = seq_voffs[i];
        RDI_U64 line_hash_slot_idx = line_num%line_hash_slots_count;

        // rjf: update unique voff counter & max line number
        voff_count += 1;
        max_line_num = Max(max_line_num, line_num);

        // rjf: find match
        RDIM_SrcLineMapBucket *match = 0;
        {
          for(RDIM_SrcLineMapBucket *node = line_hash_slots[line_hash_slot_idx];
              node != 0;
              node = node->hash_next)
          {
            if(node->line_num == line_num)
            {
              match = node;
              break;
            }
          }
        }

        // rjf: introduce new map if no match
        if(match == 0)
        {
          match = rdim_push_array(scratch.arena, RDIM_SrcLineMapBucket, 1);
          RDIM_SLLQueuePush_N(first_bucket, last_bucket, match, order_next);
          RDIM_SLLStackPush_N(line_hash_slots[line_hash_slot_idx], match, hash_next);
          match->line_num = line_num;
          line_count += 1;
        }

        // rjf: insert new voff
        {
          RDIM_SrcLineMapVoffBlock *block = rdim_push_array(scratch.arena, RDIM_SrcLineMapVoffBlock, 1);
          RDIM_SLLQueuePush(match->first_voff_block, match->last_voff_block, block);
          match->voff_count += 1;
          block->voff = voff;
        }
      }
    }
  }

  //- rjf: bake sortable keys array
  RDIM_SortKey *keys = rdim_push_array_no_zero(scratch.arena, RDIM_SortKey, line_count);
  {
    RDIM_SortKey *key_ptr = keys;
    for(RDIM_SrcLineMapBucket *node = first_bucket;
        node != 0;
        node = node->order_next, key_ptr += 1){
      key_ptr->key = node->line_num;
      key_ptr->val = node;
    }
  }

  //- rjf: sort keys array
  RDIM_SortKey *sorted_keys = rdim_sort_key_array(scratch.arena, keys, line_count);

  //- rjf: bake result
  RDI_U64 range_count  = line_count > 0 ? line_count + 1 : 0;
  RDI_U32 *line_nums   = rdim_push_array_no_zero(arena, RDI_U32, line_count);
  RDI_U32 *line_ranges = rdim_push_array_no_zero(arena, RDI_U32, range_count);
  RDI_U64 *voffs       = rdim_push_array_no_zero(arena, RDI_U64, voff_count);
  if(line_count > 0)
  {
    RDI_U64 *voff_ptr = voffs;
    for(RDI_U32 i = 0; i < line_count; i += 1)
    {
      line_nums[i] = sorted_keys[i].key;
      line_ranges[i] = (RDI_U32)(voff_ptr - voffs); // TODO(rjf): @u64_to_u32
      RDIM_SrcLineMapBucket *bucket = (RDIM_SrcLineMapBucket*)sorted_keys[i].val;
      for(RDIM_SrcLineMapVoffBlock *node = bucket->first_voff_block; node != 0; node = node->next)
      {
        *voff_ptr = node->voff;
        voff_ptr += 1;
      }
    }
    line_ranges[line_count] = voff_count;
  }

  //- rjf: fill output
  RDIM_LineNumberMap result = {0};
  result.line_count  = line_count;
  result.voff_count  = voff_count;
  result.range_count = range_count;
  result.line_nums   = line_nums;
  result.line_ranges = line_ranges;
  result.voffs       = voffs;

  rdim_scratch_end(scratch);
  return result;
}

RDI_PROC RDIM_BakeSectionList
rdim_bake_line_number_section_list_from_params(RDIM_Arena *arena, RDIM_BakeParams *params, RDI_U64 **line_number_map_idxs_out)
{
  RDI_U64 line_number_map_count = 1;
  for(RDIM_SrcFileChunkNode *n = params->src_files.first; n != 0; n = n->next)
  {
    for(RDI_U64 i = 0; i < n->count; i += 1)
    {
      RDIM_SrcFileLineMapFragment *fragment_list = n->v[i].fragment_list;
      for(RDIM_SrcFileLineMapFragment *frag = fragment_list; frag != 0; frag = frag->next)
      {
        if(frag->seq != 0 && frag->seq->line_count > 0)
        {
          line_number_map_count += 1;
          break;
        }
      }
    }
  }

  RDI_LineNumberMap *line_number_maps     = rdim_push_array(arena, RDI_LineNumberMap, line_number_map_count); 
  RDI_U64           *line_number_map_idxs = rdim_push_array(arena, RDI_U64,           params->src_files.total_count);

  RDIM_String8List line_data_list  = {0};
  RDIM_String8List voff_data_list  = {0};
  RDIM_String8List range_data_list = {0};

  RDI_U64 line_number_map_cursor = 1;
  RDI_U64 line_cursor            = 0;
  RDI_U64 voff_cursor            = 0;
  RDI_U64 range_cursor           = 0;

  for(RDIM_SrcFileChunkNode *n = params->src_files.first; n != 0; n = n->next)
  {
    for(RDI_U64 i = 0; i < n->count; i += 1)
    {
      RDIM_SrcFile *src_file     = &n->v[i];
      RDI_U64       src_file_idx = rdim_idx_from_src_file(src_file);

      RDIM_LineNumberMap src = rdim_bake_line_number_map(arena, src_file->fragment_list);
      if(src.line_count > 0)
      {
        // fill out line number map header
        RDI_LineNumberMap *dst = &line_number_maps[line_number_map_cursor];
        dst->line_count     = src.line_count;
        dst->voff_count     = src.voff_count;
        dst->line_data_idx  = line_cursor;
        dst->range_data_idx = range_cursor;
        dst->voff_data_idx  = voff_cursor;

        // push line numbers data
        rdim_str8_list_push(arena, &line_data_list,  rdim_str8((RDI_U8 *)src.line_nums,   sizeof(src.line_nums[0])   * src.line_count));
        rdim_str8_list_push(arena, &voff_data_list,  rdim_str8((RDI_U8 *)src.voffs,       sizeof(src.voffs[0])       * src.voff_count));
        rdim_str8_list_push(arena, &range_data_list, rdim_str8((RDI_U8 *)src.line_ranges, sizeof(src.line_ranges[0]) * src.range_count));

        // store src file -> line numbers map
        line_number_map_idxs[src_file_idx] = line_number_map_cursor;

        // advance cursors
        line_number_map_cursor += 1;
        line_cursor            += src.line_count;
        voff_cursor            += src.voff_count;
        range_cursor           += src.range_count;
      }
      else
      {
        line_number_map_idxs[src_file_idx] = 0;
      }
    }
  }

  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw (arena, &sections, RDI_DataSectionTag_LineNumberMaps, line_number_maps, sizeof(line_number_maps[0]) * line_number_map_count);
  rdim_bake_section_list_push_new_unpacked_list(arena, &sections, RDI_DataSectionTag_LineMapNumbers, line_data_list );
  rdim_bake_section_list_push_new_unpacked_list(arena, &sections, RDI_DataSectionTag_LineMapRanges , range_data_list);
  rdim_bake_section_list_push_new_unpacked_list(arena, &sections, RDI_DataSectionTag_LineMapVoffs  , voff_data_list );

  *line_number_map_idxs_out = line_number_map_idxs;

  return sections;
}

//- rjf: source files

RDI_PROC RDIM_BakeSectionList
rdim_bake_src_file_section_list_from_params(RDIM_Arena              *arena,
                                            RDIM_BakeStringMapTight *strings,
                                            RDIM_BakePathTree       *path_tree,
                                            RDI_U64                 *checksum_offsets,
                                            RDI_U64                 *line_number_map_idxs,
                                            RDIM_BakeParams         *params)
{
  ////////////////////////////
  //- rjf: iterate all source files, fill serialized version
  //
  RDI_U64         dst_files_count = params->src_files.total_count;
  RDI_SourceFile *dst_files       = rdim_push_array(arena, RDI_SourceFile, dst_files_count);
  RDI_U64         dst_file_idx    = 0;
  for(RDIM_SrcFileChunkNode *chunk_n = params->src_files.first; chunk_n != 0; chunk_n = chunk_n->next)
  {
    for(RDI_U64 idx = 0; idx < chunk_n->count; idx += 1, dst_file_idx += 1)
    {
      RDIM_SrcFile   *src_file = &chunk_n->v[idx];
      RDI_SourceFile *dst_file = &dst_files[dst_file_idx];

      RDI_U64 src_file_idx = rdim_idx_from_src_file(src_file);
      RDI_U64 checksum_idx = rdim_idx_from_checksum(src_file->checksum);
      
      dst_file->file_path_node_idx          = rdim_bake_path_node_idx_from_string(path_tree, src_file->normal_full_path);
      dst_file->normal_full_path_string_idx = rdim_bake_idx_from_string(strings, src_file->normal_full_path);
      dst_file->line_number_map_idx         = line_number_map_idxs[src_file_idx];
      dst_file->checksum_offset             = checksum_offsets[checksum_idx];
    }
  }

  ////////////////////////////
  //- rjf: build section for all source files
  //
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_SourceFiles, dst_files, sizeof(RDI_SourceFile)*dst_files_count);
  return sections;
}

//- rjf: type nodes

RDI_PROC RDIM_BakeSectionList
rdim_bake_type_node_section_list_from_params(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeIdxRunMap *idx_runs, RDIM_BakeParams *params)
{
  //- rjf: build all type nodes
  RDI_TypeNode *type_nodes = push_array(arena, RDI_TypeNode, params->types.total_count+1);
  RDIM_ProfScope("push all type nodes")
  {
    RDI_U32 dst_idx = 1;
    for(RDIM_TypeChunkNode *n = params->types.first; n != 0; n = n->next)
    {
      for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1, dst_idx += 1)
      {
        RDIM_Type *src = &n->v[chunk_idx];
        RDI_TypeNode *dst = &type_nodes[dst_idx];
        
        //- rjf: fill shared type node info
        dst->kind      = src->kind;
        dst->flags     = (RDI_U16)src->flags; // TODO(rjf): @u32_to_u16
        dst->byte_size = src->byte_size;
        
        //- rjf: fill built-in-only type node info
        if(RDI_TypeKind_FirstBuiltIn <= dst->kind && dst->kind <= RDI_TypeKind_LastBuiltIn)
        {
          dst->built_in.name_string_idx = rdim_bake_idx_from_string(strings, src->name);
        }
        
        //- rjf: fill constructed type node info
        else if(RDI_TypeKind_FirstConstructed <= dst->kind && dst->kind <= RDI_TypeKind_LastConstructed)
        {
          dst->constructed.direct_type_idx = (RDI_U32)rdim_idx_from_type(src->direct_type); // TODO(rjf): @u64_to_u32
          dst->constructed.count = src->count;
          if(dst->kind == RDI_TypeKind_Function || dst->kind == RDI_TypeKind_Method)
          {
            RDI_U32 param_idx_run_count = src->count;
            RDI_U32 *param_idx_run = rdim_push_array_no_zero(arena, RDI_U32, param_idx_run_count);
            for(RDI_U32 idx = 0; idx < param_idx_run_count; idx += 1)
            {
              param_idx_run[idx] = (RDI_U32)rdim_idx_from_type(src->param_types[idx]); // TODO(rjf): @u64_to_u32
            }
            dst->constructed.param_idx_run_first = rdim_bake_idx_from_idx_run(idx_runs, param_idx_run, param_idx_run_count);
          }
          else if(dst->kind == RDI_TypeKind_MemberPtr)
          {
            // TODO(rjf): member pointers not currently supported.
          }
        }
        
        //- rjf: fill user-defined-type info
        else if(RDI_TypeKind_FirstUserDefined <= dst->kind && dst->kind <= RDI_TypeKind_LastUserDefined)
        {
          dst->user_defined.name_string_idx = rdim_bake_idx_from_string(strings, src->name);
          dst->user_defined.udt_idx         = (RDI_U32)rdim_idx_from_udt(src->udt); // TODO(rjf): @u64_to_u32
          dst->user_defined.direct_type_idx = (RDI_U32)rdim_idx_from_type(src->direct_type); // TODO(rjf): @u64_to_u32
        }
        
        //- rjf: fill bitfield info
        else if(dst->kind == RDI_TypeKind_Bitfield)
        {
          dst->bitfield.direct_type_idx = (RDI_U32)rdim_idx_from_type(src->direct_type); // TODO(rjf): @u64_to_u32
          dst->bitfield.off  = src->off;
          dst->bitfield.size = src->count;
        }
      }
    }
  }
  
  
  //- rjf: build sections
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_TypeNodes, type_nodes, sizeof(RDI_TypeNode)*(params->types.total_count+1));
  return sections;
}

//- rjf: UDTs

RDI_PROC RDIM_BakeSectionList
rdim_bake_udt_section_list_from_params(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeParams *params)
{
  //- rjf: build tables
  RDI_UDT *       udts         = push_array(arena, RDI_UDT,        params->udts.total_count+1);
  RDI_Member *    members      = push_array(arena, RDI_Member,     params->udts.total_member_count+1);
  RDI_EnumMember *enum_members = push_array(arena, RDI_EnumMember, params->udts.total_enum_val_count+1);
  {
    RDI_U32 dst_udt_idx = 1;
    RDI_U32 dst_member_idx = 1;
    RDI_U32 dst_enum_member_idx = 1;
    for(RDIM_UDTChunkNode *n = params->udts.first; n != 0; n = n->next)
    {
      for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1, dst_udt_idx += 1)
      {
        RDIM_UDT *src_udt = &n->v[chunk_idx];
        RDI_UDT *dst_udt = &udts[dst_udt_idx];
        
        //- rjf: fill basics
        dst_udt->self_type_idx = (RDI_U32)rdim_idx_from_type(src_udt->self_type); // TODO(rjf): @u64_to_u32
        dst_udt->file_idx = (RDI_U32)rdim_idx_from_src_file(src_udt->src_file); // TODO(rjf): @u64_to_u32
        dst_udt->line = src_udt->line;
        dst_udt->col  = src_udt->col;
        
        //- rjf: fill members
        if(src_udt->member_count != 0)
        {
          dst_udt->member_first = dst_member_idx;
          dst_udt->member_count = src_udt->member_count;
          for(RDIM_UDTMember *src_member = src_udt->first_member;
              src_member != 0;
              src_member = src_member->next, dst_member_idx += 1)
          {
            RDI_Member *dst_member = &members[dst_member_idx];
            dst_member->kind            = src_member->kind;
            dst_member->name_string_idx = rdim_bake_idx_from_string(strings, src_member->name);
            dst_member->type_idx        = (RDI_U32)rdim_idx_from_type(src_member->type); // TODO(rjf): @u64_to_u32
            dst_member->off             = src_member->off;
          }
        }
        
        //- rjf: fill enum members
        else if(src_udt->enum_val_count != 0)
        {
          dst_udt->flags |= RDI_UserDefinedTypeFlag_EnumMembers;
          dst_udt->member_first = dst_enum_member_idx;
          dst_udt->member_count = src_udt->enum_val_count;
          for(RDIM_UDTEnumVal *src_member = src_udt->first_enum_val;
              src_member != 0;
              src_member = src_member->next, dst_enum_member_idx += 1)
          {
            RDI_EnumMember *dst_member = &enum_members[dst_enum_member_idx];
            dst_member->name_string_idx = rdim_bake_idx_from_string(strings, src_member->name);
            dst_member->val             = src_member->val;
          }
        }
      }
    }
  }
  
  //- rjf: build sections
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_UDTs       , udts        , sizeof(RDI_UDT)        * (params->udts.total_count+1)          );
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_Members    , members     , sizeof(RDI_Member)     * (params->udts.total_member_count+1)   );
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_EnumMembers, enum_members, sizeof(RDI_EnumMember) * (params->udts.total_enum_val_count+1) );
  return sections;
}

//- rjf: global variables

RDI_PROC RDIM_BakeSectionList
rdim_bake_global_variable_section_list_from_params(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeParams *params)
{
  //- rjf: build all global variables
  RDI_GlobalVariable *global_variables = push_array(arena, RDI_GlobalVariable, params->global_variables.total_count+1);
  {
    RDI_U32 dst_idx = 1;
    for(RDIM_SymbolChunkNode *n = params->global_variables.first; n != 0; n = n->next)
    {
      for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1, dst_idx += 1)
      {
        RDIM_Symbol *src = &n->v[chunk_idx];
        RDI_GlobalVariable *dst = &global_variables[dst_idx];
        dst->name_string_idx = rdim_bake_idx_from_string(strings, src->name);
        dst->voff            = src->offset;
        dst->type_idx        = (RDI_U32)rdim_idx_from_type(src->type); // TODO(rjf): @u64_to_u32
        if(src->is_extern)
        {
          dst->link_flags |= RDI_LinkFlag_External;
        }
        if(src->container_type != 0)
        {
          dst->link_flags |= RDI_LinkFlag_TypeScoped;
          dst->container_idx = src->container_type ? (RDI_U32)rdim_idx_from_udt(src->container_type->udt) : 0; // TODO(rjf): @u64_to_u32
        }
        else if(src->container_symbol != 0)
        {
          dst->link_flags |= RDI_LinkFlag_ProcScoped;
          dst->container_idx = (RDI_U32)rdim_idx_from_symbol(src->container_symbol); // TODO(rjf): @u64_to_u32
        }
      }
    }
  }
  
  //- rjf: build sections
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_GlobalVariables, global_variables, sizeof(RDI_GlobalVariable)*(params->global_variables.total_count+1));
  return sections;
}

//- rjf: global vmap

RDI_PROC RDIM_BakeSectionList
rdim_bake_global_vmap_section_list_from_params(RDIM_Arena *arena, RDIM_BakeParams *params)
{
  //- rjf: build global vmap
  RDIM_BakeVMap global_vmap = {0};
  {
    RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);
    
    //- rjf: allocate keys/markers
    RDI_U64 marker_count = params->global_variables.total_count*2;
    RDIM_SortKey    *keys    = rdim_push_array_no_zero(scratch.arena, RDIM_SortKey, marker_count);
    RDIM_VMapMarker *markers = rdim_push_array_no_zero(scratch.arena, RDIM_VMapMarker, marker_count);
    
    //- rjf: fill
    {
      RDIM_SortKey *key_ptr = keys;
      RDIM_VMapMarker *marker_ptr = markers;
      
      // rjf: fill actual globals
      for(RDIM_SymbolChunkNode *n = params->global_variables.first; n != 0; n = n->next)
      {
        for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1)
        {
          RDIM_Symbol *global_var = &n->v[chunk_idx];
          RDI_U32 global_var_idx = (RDI_U32)rdim_idx_from_symbol(global_var); // TODO(rjf): @u64_to_u32
          RDI_U64 global_var_size = global_var->type ? global_var->type->byte_size : 1;
          
          RDI_U64 first = global_var->offset;
          RDI_U64 opl   = first + global_var_size;
          
          key_ptr->key = first;
          key_ptr->val = marker_ptr;
          marker_ptr->idx = global_var_idx;
          marker_ptr->begin_range = 1;
          key_ptr += 1;
          marker_ptr += 1;
          
          key_ptr->key = opl;
          key_ptr->val = marker_ptr;
          marker_ptr->idx = global_var_idx;
          marker_ptr->begin_range = 0;
          key_ptr += 1;
          marker_ptr += 1;
        }
      }
      
      // rjf: fill nil global
      {
        RDI_U32 global_idx = 0;
        RDI_U64 first = 0;
        RDI_U64 opl   = 0xffffffffffffffffull;
        key_ptr->key = first;
        key_ptr->val = marker_ptr;
        marker_ptr->idx = global_idx;
        marker_ptr->begin_range = 1;
        key_ptr += 1;
        marker_ptr += 1;
        key_ptr->key = opl;
        key_ptr->val = marker_ptr;
        marker_ptr->idx = global_idx;
        marker_ptr->begin_range = 0;
        key_ptr += 1;
        marker_ptr += 1;
      }
    }
    
    // rjf: construct vmap
    global_vmap = rdim_bake_vmap_from_markers(arena, markers, keys, marker_count);
    
    rdim_scratch_end(scratch);
  }
  
  //- rjf: build sections
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_GlobalVmap, global_vmap.vmap, sizeof(RDI_VMapEntry)*(global_vmap.count+1));
  return sections;
}

//- rjf: thread variables

RDI_PROC RDIM_BakeSectionList
rdim_bake_thread_variable_section_list_from_params(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeParams *params)
{
  //- rjf: build all thread variables
  RDI_ThreadVariable *thread_variables = push_array(arena, RDI_ThreadVariable, params->thread_variables.total_count+1);
  {
    RDI_U32 dst_idx = 1;
    for(RDIM_SymbolChunkNode *n = params->thread_variables.first; n != 0; n = n->next)
    {
      for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1, dst_idx += 1)
      {
        RDIM_Symbol *src = &n->v[chunk_idx];
        RDI_ThreadVariable *dst = &thread_variables[dst_idx];
        dst->name_string_idx = rdim_bake_idx_from_string(strings, src->name);
        dst->tls_off         = (RDI_U32)src->offset; // TODO(rjf): @u64_to_u32
        dst->type_idx        = (RDI_U32)rdim_idx_from_type(src->type);
        if(src->is_extern)
        {
          dst->link_flags |= RDI_LinkFlag_External;
        }
        if(src->container_type != 0)
        {
          dst->link_flags |= RDI_LinkFlag_TypeScoped;
          dst->container_idx = src->container_type ? (RDI_U32)rdim_idx_from_udt(src->container_type->udt) : 0; // TODO(rjf): @u64_to_u32
        }
        else if(src->container_symbol != 0)
        {
          dst->link_flags |= RDI_LinkFlag_ProcScoped;
          dst->container_idx = (RDI_U32)rdim_idx_from_symbol(src->container_symbol); // TODO(rjf): @u64_to_u32
        }
      }
    }
  }
  
  //- rjf: build sections
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_ThreadVariables, thread_variables, sizeof(RDI_ThreadVariable)*(params->thread_variables.total_count+1));
  return sections;
}

//- rjf: procedures

RDI_PROC RDIM_BakeSectionList
rdim_bake_procedure_section_list_from_params(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeParams *params)
{
  //- rjf: build all procedures
  RDI_Procedure *procedures = push_array(arena, RDI_Procedure, params->procedures.total_count+1);
  {
    RDI_U32 dst_idx = 1;
    for(RDIM_SymbolChunkNode *n = params->procedures.first; n != 0; n = n->next)
    {
      for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1, dst_idx += 1)
      {
        RDIM_Symbol *src = &n->v[chunk_idx];
        RDI_Procedure *dst = &procedures[dst_idx];
        dst->name_string_idx      = rdim_bake_idx_from_string(strings, src->name);
        dst->link_name_string_idx = rdim_bake_idx_from_string(strings, src->link_name);
        if(src->is_extern)
        {
          dst->link_flags |= RDI_LinkFlag_External;
        }
        if(src->container_type != 0)
        {
          dst->link_flags |= RDI_LinkFlag_TypeScoped;
          dst->container_idx = src->container_type ? (RDI_U32)rdim_idx_from_udt(src->container_type->udt) : 0; // TODO(rjf): @u64_to_u32
        }
        else if(src->container_symbol != 0)
        {
          dst->link_flags |= RDI_LinkFlag_ProcScoped;
          dst->container_idx = (RDI_U32)rdim_idx_from_symbol(src->container_symbol); // TODO(rjf): @u64_to_u32
        }
        dst->type_idx             = (RDI_U32)rdim_idx_from_type(src->type); // TODO(rjf): @u64_to_u32
        dst->root_scope_idx       = (RDI_U32)rdim_idx_from_scope(src->root_scope); // TODO(rjf): @u64_to_u32
      }
    }
  }
  
  //- rjf: build sections
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_Procedures, procedures, sizeof(RDI_Procedure)*(params->procedures.total_count+1));
  return sections;
}

//- rjf: scopes

RDI_PROC RDIM_BakeSectionList
rdim_bake_scope_section_list_from_params(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeParams *params)
{
  RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);
  
  ////////////////////////////
  //- rjf: build all scopes, scope voffs, locals, and location blocks
  //
  RDI_Scope *         scopes          = rdim_push_array(arena, RDI_Scope,         params->scopes.total_count+1);
  RDI_U64 *           scope_voffs     = rdim_push_array(arena, RDI_U64,           params->scopes.scope_voff_count+1);
  RDI_Local *         locals          = rdim_push_array(arena, RDI_Local,         params->scopes.local_count+1);
  RDI_LocationBlock * location_blocks = rdim_push_array(arena, RDI_LocationBlock, params->scopes.location_count+1);
  RDIM_String8List    location_data_blobs = {0};
  RDIM_ProfScope("build all scopes, scope voffs, locals, and location blocks")
  {
    RDI_U64 dst_scope_idx = 1;
    RDI_U64 dst_scope_voff_idx = 1;
    RDI_U64 dst_local_idx = 1;
    RDI_U64 dst_location_block_idx = 1;
    for(RDIM_ScopeChunkNode *chunk_n = params->scopes.first; chunk_n != 0; chunk_n = chunk_n->next)
    {
      for(RDI_U64 chunk_idx = 0; chunk_idx < chunk_n->count; chunk_idx += 1, dst_scope_idx += 1)
      {
        RDIM_Scope *src_scope = &chunk_n->v[chunk_idx];
        RDI_Scope *dst_scope = &scopes[dst_scope_idx];
        
        //- rjf: push scope's voffs
        RDI_U64 voff_idx_first = dst_scope_voff_idx;
        {
          for(RDIM_Rng1U64Node *n = src_scope->voff_ranges.first; n != 0; n = n->next)
          {
            scope_voffs[dst_scope_voff_idx] = n->v.min;
            dst_scope_voff_idx += 1;
            scope_voffs[dst_scope_voff_idx] = n->v.max;
            dst_scope_voff_idx += 1;
          }
        }
        RDI_U64 voff_idx_opl = dst_scope_voff_idx;
        
        //- rjf: push locals
        RDI_U64 local_idx_first = dst_local_idx;
        for(RDIM_Local *src_local = src_scope->first_local;
            src_local != 0;
            src_local = src_local->next, dst_local_idx += 1)
        {
          //- rjf: push local's locations
          RDI_U64 location_block_idx_first = dst_location_block_idx;
          for(RDIM_LocationCase *loccase = src_local->locset.first_location_case;
              loccase != 0;
              loccase = loccase->next, dst_location_block_idx += 1)
          {
            // rjf: fill location block
            RDI_LocationBlock *dst_locblock = &location_blocks[dst_location_block_idx];
            dst_locblock->scope_off_first   = loccase->voff_range.min;
            dst_locblock->scope_off_opl     = loccase->voff_range.max;
            dst_locblock->location_data_off = location_data_blobs.total_size;
            
            // rjf: serialize location into location data
            RDIM_Location *src_location = loccase->location;
            {
              // rjf: nil location
              if(src_location == 0)
              {
                rdim_str8_list_push_align(scratch.arena, &location_data_blobs, 8);
                rdim_str8_list_push(scratch.arena, &location_data_blobs, rdim_str8_lit("\0"));
              }
              
              // rjf: valid location
              else switch(src_location->kind)
              {
                // rjf: catchall unsupported case
                default:
                {
                  rdim_str8_list_push_align(scratch.arena, &location_data_blobs, 8);
                  rdim_str8_list_push(scratch.arena, &location_data_blobs, rdim_str8_lit("\0"));
                }break;
                
                // rjf: bytecode streams
                case RDI_LocationKind_AddrBytecodeStream:
                case RDI_LocationKind_ValBytecodeStream:
                {
                  rdim_str8_list_push(scratch.arena, &location_data_blobs, rdim_str8_copy(scratch.arena, rdim_str8_struct(&src_location->kind)));
                  for(RDIM_EvalBytecodeOp *op_node = src_location->bytecode.first_op;
                      op_node != 0;
                      op_node = op_node->next)
                  {
                    RDI_U8 op_data[9];
                    op_data[0] = op_node->op;
                    rdim_memcpy(op_data + 1, &op_node->p, op_node->p_size);
                    RDIM_String8 op_data_str = rdim_str8(op_data, 1 + op_node->p_size);
                    rdim_str8_list_push(scratch.arena, &location_data_blobs, rdim_str8_copy(scratch.arena, op_data_str));
                  }
                  {
                    RDI_U64 data = 0;
                    RDIM_String8 data_str = rdim_str8((RDI_U8 *)&data, 1);
                    rdim_str8_list_push(scratch.arena, &location_data_blobs, rdim_str8_copy(scratch.arena, data_str));
                  }
                }break;
                
                // rjf: simple addr+off cases
                case RDI_LocationKind_AddrRegisterPlusU16:
                case RDI_LocationKind_AddrAddrRegisterPlusU16:
                {
                  RDI_LocationRegisterPlusU16 loc = {0};
                  loc.kind = src_location->kind;
                  loc.register_code = src_location->register_code;
                  loc.offset = src_location->offset;
                  rdim_str8_list_push(scratch.arena, &location_data_blobs, rdim_str8_copy(scratch.arena, rdim_str8_struct(&loc)));
                }break;
                
                // rjf: register cases
                case RDI_LocationKind_ValRegister:
                {
                  RDI_LocationRegister loc = {0};
                  loc.kind = src_location->kind;
                  loc.register_code = src_location->register_code;
                  rdim_str8_list_push(scratch.arena, &location_data_blobs, rdim_str8_copy(scratch.arena, rdim_str8_struct(&loc)));
                }break;
              }
            }
          }
          RDI_U64 location_block_idx_opl = dst_location_block_idx;
          
          //- rjf: fill local
          RDI_Local *dst_local = &locals[dst_local_idx];
          dst_local->kind            = src_local->kind;
          dst_local->name_string_idx = rdim_bake_idx_from_string(strings, src_local->name);
          dst_local->type_idx        = (RDI_U32)rdim_idx_from_type(src_local->type); // TODO(rjf): @u64_to_u32
          dst_local->location_first  = (RDI_U32)location_block_idx_first; // TODO(rjf): @u64_to_u32
          dst_local->location_opl    = (RDI_U32)location_block_idx_opl;   // TODO(rjf): @u64_to_u32
        }
        RDI_U64 local_idx_opl = dst_local_idx;
        
        //- rjf: fill scope
        dst_scope->proc_idx               = (RDI_U32)rdim_idx_from_symbol(src_scope->symbol); // TODO(rjf): @u64_to_u32
        dst_scope->parent_scope_idx       = (RDI_U32)rdim_idx_from_scope(src_scope->parent_scope); // TODO(rjf): @u64_to_u32
        dst_scope->first_child_scope_idx  = (RDI_U32)rdim_idx_from_scope(src_scope->first_child); // TODO(rjf): @u64_to_u32
        dst_scope->next_sibling_scope_idx = (RDI_U32)rdim_idx_from_scope(src_scope->next_sibling); // TODO(rjf): @u64_to_u32
        dst_scope->voff_range_first       = (RDI_U32)voff_idx_first;                    // TODO(rjf): @u64_to_u32
        dst_scope->voff_range_opl         = (RDI_U32)voff_idx_opl;                      // TODO(rjf): @u64_to_u32
        dst_scope->local_first            = (RDI_U32)local_idx_first;                   // TODO(rjf): @u64_to_u32
        dst_scope->local_count            = (RDI_U32)(local_idx_opl - local_idx_first); // TODO(rjf): @u64_to_u32
        dst_scope->inline_site_idx        = (RDI_U32)rdim_idx_from_inline_site(src_scope->inline_site); // TODO: u64_to_u32
      }
    }
  }
  
  ////////////////////////////
  //- rjf: build flattened location data
  //
  RDIM_String8 location_data_blob = {0};
  RDIM_ProfScope("build flattened location data")
  {
    location_data_blob = rdim_str8_list_join(arena, &location_data_blobs, rdim_str8_lit(""));
  }
  
  ////////////////////////////
  //- rjf: push all sections
  //
  RDIM_BakeSectionList sections = {0};
  RDIM_ProfScope("push all symbol info sections")
  {
    rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_Scopes        , scopes                , sizeof(RDI_Scope)         * (params->scopes.total_count+1)      );
    rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_ScopeVoffData , scope_voffs           , sizeof(RDI_U64)           * (params->scopes.scope_voff_count+1) );
    rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_Locals        , locals                , sizeof(RDI_Local)         * (params->scopes.local_count+1)      );
    rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_LocationBlocks, location_blocks       , sizeof(RDI_LocationBlock) * (params->scopes.location_count+1)   );
    rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_LocationData  , location_data_blob.str, location_data_blob.size                                         );
  }
  rdim_scratch_end(scratch);
  return sections;
}

//- rjf: scope vmap

RDI_PROC RDIM_BakeSectionList
rdim_bake_scope_vmap_section_list_from_params(RDIM_Arena *arena, RDIM_BakeParams *params)
{
  //- rjf: build scope vmap
  RDIM_BakeVMap scope_vmap = {0};
  {
    RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);
    
    // rjf: allocate keys/markers
    RDI_U64 marker_count = params->scopes.scope_voff_count;
    RDIM_SortKey    *keys    = rdim_push_array_no_zero(scratch.arena, RDIM_SortKey, marker_count);
    RDIM_VMapMarker *markers = rdim_push_array_no_zero(scratch.arena, RDIM_VMapMarker, marker_count);
    
    // rjf: fill
    {
      RDIM_SortKey *key_ptr = keys;
      RDIM_VMapMarker *marker_ptr = markers;
      for(RDIM_ScopeChunkNode *chunk_n = params->scopes.first; chunk_n != 0; chunk_n = chunk_n->next)
      {
        for(RDI_U64 chunk_idx = 0; chunk_idx < chunk_n->count; chunk_idx += 1)
        {
          RDIM_Scope *src_scope = &chunk_n->v[chunk_idx];
          RDI_U32 scope_idx = (RDI_U32)rdim_idx_from_scope(src_scope); // TODO(rjf): @u64_to_u32
          for(RDIM_Rng1U64Node *n = src_scope->voff_ranges.first; n != 0; n = n->next)
          {
            key_ptr->key = n->v.min;
            key_ptr->val = marker_ptr;
            marker_ptr->idx = scope_idx;
            marker_ptr->begin_range = 1;
            key_ptr += 1;
            marker_ptr += 1;
            
            key_ptr->key = n->v.max;
            key_ptr->val = marker_ptr;
            marker_ptr->idx = scope_idx;
            marker_ptr->begin_range = 0;
            key_ptr += 1;
            marker_ptr += 1;
          }
        }
      }
    }
    
    // rjf: produce vmap
    scope_vmap = rdim_bake_vmap_from_markers(arena, markers, keys, marker_count);
    rdim_scratch_end(scratch);
  }
  
  //- rjf: build sections
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_ScopeVmap, scope_vmap.vmap, sizeof(RDI_VMapEntry)*(scope_vmap.count+1));
  return sections;
}

//- inline sites

RDI_PROC RDIM_BakeSectionList
rdim_bake_inline_sites_section_list_from_params(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeParams *params)
{
  RDI_U64         inline_site_count = params->inline_sites.total_count;
  RDI_InlineSite *inline_sites      = push_array_no_zero(arena, RDI_InlineSite, inline_site_count);

  // fill out inline sites
  RDI_U64 dst_idx = 0;
  for(RDIM_InlineSiteChunkNode *chunk = params->inline_sites.first; chunk != 0; chunk = chunk->next)
  {
    for(RDI_U64 src_idx = 0; src_idx < chunk->count; src_idx += 1, dst_idx += 1)
    {
      RDIM_InlineSite *src = chunk->v + src_idx;
      RDI_InlineSite  *dst = inline_sites + dst_idx;
      dst->name_string_idx   = rdim_bake_idx_from_string(strings, src->name);
      dst->call_src_file_idx = rdim_idx_from_src_file(src->call_src_file);
      dst->call_line_num     = src->call_line_num;
      dst->call_col_num      = src->call_col_num;
      dst->type_idx          = rdim_idx_from_type(src->type);
      dst->owner_type_idx    = rdim_idx_from_type(src->owner);
      dst->line_info_idx     = rdim_idx_from_line_sequence_list(src->line_info);
    }
  }

  // build section
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_InlineSites, inline_sites, sizeof(inline_sites[0]) * inline_site_count);
  return sections;
}

//- rjf: name maps

RDI_PROC RDIM_NameMap
rdim_make_name_map(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakeIdxRunMap *idx_runs, RDIM_BakeNameMap *bake_map)
{
  RDIM_Temp scratch = rdim_scratch_begin(&arena, 1);

  RDI_U32            baked_buckets_count = bake_map->name_count;
  RDI_U32            baked_nodes_count   = bake_map->name_count;
  RDI_NameMapBucket *baked_buckets       = rdim_push_array(arena, RDI_NameMapBucket, baked_buckets_count);
  RDI_NameMapNode   *baked_nodes         = rdim_push_array_no_zero(arena, RDI_NameMapNode, baked_nodes_count);
    
  // rjf: setup the final bucket layouts
  typedef struct RDIM_NameMapSemiNode RDIM_NameMapSemiNode;
  struct RDIM_NameMapSemiNode
  {
    RDIM_NameMapSemiNode *next;
    RDIM_BakeNameMapNode *node;
  };
  typedef struct RDIM_NameMapSemiBucket RDIM_NameMapSemiBucket;
  struct RDIM_NameMapSemiBucket
  {
    RDIM_NameMapSemiNode *first;
    RDIM_NameMapSemiNode *last;
    RDI_U64 count;
  };
  RDIM_NameMapSemiBucket *sbuckets = rdim_push_array(scratch.arena, RDIM_NameMapSemiBucket, baked_buckets_count);
  for(RDIM_BakeNameMapNode *node = bake_map->first; node != 0; node = node->order_next)
  {
    RDI_U64 hash = rdi_hash(node->string.str, node->string.size);
    RDI_U64 bi = hash%baked_buckets_count;
    RDIM_NameMapSemiNode *snode = rdim_push_array(scratch.arena, RDIM_NameMapSemiNode, 1);
    SLLQueuePush(sbuckets[bi].first, sbuckets[bi].last, snode);
    snode->node = node;
    sbuckets[bi].count += 1;
  }
  
  // rjf: convert to serialized buckets & nodes
  {
    RDI_NameMapBucket *bucket_ptr = baked_buckets;
    RDI_NameMapNode *node_ptr = baked_nodes;
    for(RDI_U32 i = 0; i < baked_buckets_count; i += 1, bucket_ptr += 1)
    {
      bucket_ptr->first_node = (RDI_U32)(node_ptr - baked_nodes);
      bucket_ptr->node_count = sbuckets[i].count;
      for(RDIM_NameMapSemiNode *snode = sbuckets[i].first;
          snode != 0;
          snode = snode->next)
      {
        RDIM_BakeNameMapNode *node = snode->node;
        
        // rjf: cons name and index(es)
        RDI_U32 string_idx = rdim_bake_idx_from_string(strings, node->string);
        RDI_U32 match_count = node->val_count;
        RDI_U32 idx = 0;
        if(match_count == 1)
        {
          idx = node->val_first->val[0];
        }
        else
        {
          RDI_U64 temp_pos = rdim_arena_pos(scratch.arena);
          RDI_U32 *idx_run = rdim_push_array_no_zero(scratch.arena, RDI_U32, match_count);
          RDI_U32 *idx_ptr = idx_run;
          for(RDIM_BakeNameMapValNode *idxnode = node->val_first;
              idxnode != 0;
              idxnode = idxnode->next)
          {
            for(RDI_U32 i = 0; i < sizeof(idxnode->val)/sizeof(idxnode->val[0]); i += 1)
            {
              if(idxnode->val[i] == 0)
              {
                goto dblbreak;
              }
              *idx_ptr = idxnode->val[i];
              idx_ptr += 1;
            }
          }
          dblbreak:;
          idx = rdim_bake_idx_from_idx_run(idx_runs, idx_run, match_count);
          rdim_arena_pop_to(scratch.arena, temp_pos);
        }
        
        // rjf: write to node
        node_ptr->string_idx = string_idx;
        node_ptr->match_count = match_count;
        node_ptr->match_idx_or_idx_run_first = idx;
        node_ptr += 1;
      }
    }
  }

  RDIM_NameMap result = {0};
  result.buckets      = baked_buckets;
  result.nodes        = baked_nodes;
  result.bucket_count = baked_buckets_count;
  result.node_count   = baked_nodes_count;
    
  rdim_scratch_end(scratch);
  return result;
}

RDI_PROC RDIM_BakeSectionList
rdim_bake_name_maps_sections(RDIM_Arena *arena, RDIM_NameMap name_maps[RDI_NameMapKind_COUNT])
{
  RDIM_String8List name_maps_list = {0};
  RDIM_String8List buckets_list   = {0};
  RDIM_String8List nodes_list     = {0};
  for(RDI_U64 name_map_idx = 0, buckets_cursor = 0, nodes_cursor = 0;
      name_map_idx < RDI_NameMapKind_COUNT;
      name_map_idx += 1)
  {
    RDIM_NameMap *src = &name_maps[name_map_idx];
    if(src->bucket_count > 0)
    {
      // fill out dst map
      RDI_NameMap *dst = rdim_push_array(arena, RDI_NameMap, 1);
      dst->kind             = (RDI_NameMapKind)name_map_idx;
      dst->bucket_data_idx  = buckets_cursor;
      dst->bucket_count     = src->bucket_count;
      dst->node_data_idx    = nodes_cursor;
      dst->node_count       = src->node_count;

      // push
      rdim_str8_list_push(arena, &name_maps_list, rdim_str8_struct(dst));
      rdim_str8_list_push(arena, &buckets_list,   rdim_str8((RDI_U8 *)src->buckets, sizeof(src->buckets[0]) * src->bucket_count));
      rdim_str8_list_push(arena, &nodes_list,     rdim_str8((RDI_U8 *)src->nodes  , sizeof(src->nodes[0]  ) * src->node_count));

      // advance
      buckets_cursor += src->bucket_count;
      nodes_cursor   += src->node_count;
    }
  }
  
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_list(arena, &sections, RDI_DataSectionTag_NameMaps       , name_maps_list);
  rdim_bake_section_list_push_new_unpacked_list(arena, &sections, RDI_DataSectionTag_NameMapsBuckets, buckets_list  );
  rdim_bake_section_list_push_new_unpacked_list(arena, &sections, RDI_DataSectionTag_NameMapsNodes  , nodes_list    );

  return sections;
}

//- rjf: file paths

RDI_PROC RDIM_BakeSectionList
rdim_bake_file_path_section_list_from_path_tree(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings, RDIM_BakePathTree *path_tree)
{
  RDI_U32 dst_nodes_count = path_tree->count;
  RDI_FilePathNode *dst_nodes = rdim_push_array(arena, RDI_FilePathNode, dst_nodes_count);
  {
    RDI_U32 dst_node_idx = 0;
    for(RDIM_BakePathNode *src_node = path_tree->first;
        src_node != 0;
        src_node = src_node->next_order, dst_node_idx += 1)
    {
      RDI_FilePathNode *dst_node = &dst_nodes[dst_node_idx];
      dst_node->name_string_idx = rdim_bake_idx_from_string(strings, src_node->name);
      dst_node->source_file_idx = rdim_idx_from_src_file(src_node->src_file);
      if(src_node->parent != 0)
      {
        dst_node->parent_path_node = src_node->parent->idx;
      }
      if(src_node->first_child != 0)
      {
        dst_node->first_child = src_node->first_child->idx;
      }
      if(src_node->next_sibling != 0)
      {
        dst_node->next_sibling = src_node->next_sibling->idx;
      }
    }
  }
  RDIM_BakeSectionList sections = {0};
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_FilePathNodes, dst_nodes, sizeof(RDI_FilePathNode)*dst_nodes_count);
  return sections;
}

//- rjf: strings

RDI_PROC RDIM_BakeSectionList
rdim_bake_string_section_list_from_string_map(RDIM_Arena *arena, RDIM_BakeStringMapTight *strings)
{
  RDIM_BakeSectionList sections = {0};
  RDI_U32 *str_offs = rdim_push_array_no_zero(arena, RDI_U32, strings->total_count + 1);
  RDI_U32 off_cursor = 0;
  {
    RDI_U32 *off_ptr = str_offs;
    *off_ptr = 0;
    off_ptr += 1;
    for(RDI_U64 slot_idx = 0; slot_idx < strings->slots_count; slot_idx += 1)
    {
      for(RDIM_BakeStringChunkNode *n = strings->slots[slot_idx].first; n != 0; n = n->next)
      {
        for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1)
        {
          RDIM_BakeString *bake_string = &n->v[chunk_idx];
          off_cursor += bake_string->string.size;
          *off_ptr = off_cursor;
          off_ptr += 1;
        }
      }
    }
  }
  RDI_U8 *buf = rdim_push_array(arena, RDI_U8, off_cursor);
  {
    RDI_U8 *ptr = buf;
    for(RDI_U64 slot_idx = 0; slot_idx < strings->slots_count; slot_idx += 1)
    {
      for(RDIM_BakeStringChunkNode *n = strings->slots[slot_idx].first; n != 0; n = n->next)
      {
        for(RDI_U64 chunk_idx = 0; chunk_idx < n->count; chunk_idx += 1)
        {
          RDIM_BakeString *bake_string = &n->v[chunk_idx];
          rdim_memcpy(ptr, bake_string->string.str, bake_string->string.size);
          ptr += bake_string->string.size;
        }
      }
    }
  }
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_StringTable, str_offs, sizeof(RDI_U32)*(strings->total_count+1));
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_StringData , buf     , off_cursor);
  return sections;
}

//- rjf: index runs

RDI_PROC RDIM_BakeSectionList
rdim_bake_idx_run_section_list_from_idx_run_map(RDIM_Arena *arena, RDIM_BakeIdxRunMap *idx_runs)
{
  RDIM_BakeSectionList sections = {0};
  RDI_U32 *idx_data = rdim_push_array_no_zero(arena, RDI_U32, idx_runs->idx_count);
  {
    RDI_U32 *out_ptr = idx_data;
    RDI_U32 *opl = out_ptr + idx_runs->idx_count;
    for(RDIM_BakeIdxRunNode *node = idx_runs->order_first;
        node != 0 && out_ptr < opl;
        node = node->order_next)
    {
      rdim_memcpy(out_ptr, node->idx_run, sizeof(*node->idx_run)*node->count);
      out_ptr += node->count;
    }
  }
  rdim_bake_section_list_push_new_unpacked_raw(arena, &sections, RDI_DataSectionTag_IndexRuns, idx_data, sizeof(RDI_U32)*idx_runs->idx_count);
  return sections;
}

////////////////////////////////
//~ rjf: [Serializing] Baked Data Section List -> Serialized Binary Strings

RDI_PROC RDIM_String8List
rdim_serialized_strings_from_params_bake_section_list(RDIM_Arena *arena, RDIM_BakeSectionList *sections)
{
  RDIM_String8List strings; rdim_memzero_struct(&strings);
  
  //- rjf: make table for actually laid out sections
  RDIM_BakeSection *bake_sections[RDI_DataSectionTag_PRIMARY_COUNT] = {0};
  for(RDIM_BakeSectionNode *n = sections->first; n != 0; n = n->next)
  {
    RDIM_BakeSection *bake_section = &n->v;
    if(0 <= bake_section->tag && bake_section->tag < rdim_array_count(bake_sections))
    {
      if(bake_sections[bake_section->tag] != 0)
      {
        // TODO(rjf): error - malformed input! we have a duplicate section.
      }
      else
      {
        bake_sections[bake_section->tag] = bake_section;
      }
    }
  }
  
  //- rjf: push empty header & data section table
  RDI_Header *rdi_header = rdim_push_array(arena, RDI_Header, 1);
  RDI_DataSection *rdi_sections = rdim_push_array(arena, RDI_DataSection, rdim_array_count(bake_sections));
  rdim_str8_list_push(arena, &strings, rdim_str8_struct(rdi_header));
  rdim_str8_list_push_align(arena, &strings, 8);
  U32 data_section_off = (U32)strings.total_size;
  rdim_str8_list_push(arena, &strings, rdim_str8((RDI_U8 *)rdi_sections, sizeof(RDI_DataSection)*rdim_array_count(bake_sections)));
  
  // rjf: fill baked header
  {
    rdi_header->magic              = RDI_MAGIC_CONSTANT;
    rdi_header->encoding_version   = RDI_ENCODING_VERSION;
    rdi_header->data_section_off   = data_section_off;
    rdi_header->data_section_count = rdim_array_count(bake_sections);
  }
  
  // rjf: fill baked data section table
  U64 dst_idx = 0;
  for(RDI_U64 src_idx = 0; src_idx < rdim_array_count(bake_sections); src_idx += 1, dst_idx += 1)
  {
    RDIM_BakeSection *src = bake_sections[src_idx];
    if(src == 0) { continue; }

    // align section start bytes
    if(src->data.total_size != 0)
    {
      rdim_str8_list_push_align(arena, &strings, 8);
    }

    RDI_U64 data_section_off  = strings.total_size;
    RDI_U64 data_section_size = src->data.total_size;
    rdim_str8_list_concat_in_place(&strings, &src->data);

    RDI_DataSection *dst = rdi_sections+dst_idx;
    dst->tag           = src->tag;
    dst->encoding      = src->encoding;
    dst->off           = data_section_off;
    dst->encoded_size  = data_section_size;
    dst->unpacked_size = src->unpacked_size;
  }
    
  return strings;
}
