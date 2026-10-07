// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef DBG_SERVER_HOST_H
#define DBG_SERVER_HOST_H

typedef struct DS_HostState DS_HostState;
struct DS_HostState
{
  Arena *arena;
};

internal B32 ds_push_msg__host(GuardedRing *out_ring, DS_Msg *msg, U64 endt_us);

#endif // DBG_SERVER_HOST_H
