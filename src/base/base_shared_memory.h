// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef BASE_SHARED_MEMORY_H
#define BASE_SHARED_MEMORY_H

typedef struct SharedMemory SharedMemory;
struct SharedMemory
{
  U64 u64[1];
};

////////////////////////////////
//~ rjf: @per_os_impl Shared Memory

internal SharedMemory shared_memory_alloc(U64 size, String8 name);
internal SharedMemory shared_memory_open(String8 name);
internal void         shared_memory_close(SharedMemory handle);
internal void *       shared_memory_view_open(SharedMemory handle, Rng1U64 range);
internal void         shared_memory_view_close(SharedMemory handle, void *ptr, Rng1U64 range);
internal void *       shared_memory_view_replace_placeholder(SharedMemory handle, void *ptr, Rng1U64 range, AccessFlags flags);

#endif // BASE_SHARED_MEMORY_H
