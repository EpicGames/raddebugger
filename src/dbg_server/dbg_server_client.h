// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef DBG_SERVER_CLIENT_H
#define DBG_SERVER_CLIENT_H

////////////////////////////////
//~ rjf: Messages/Responses

internal B32 ds_push_msg(GuardedRing *out_ring, DS_Inst inst, DS_Msg *msg, U64 endt_us);
internal B32 ds_pop_response(Arena *arena, GuardedRing *out_ring, DS_Response *response_out, U64 endt_us);

#endif // DBG_SERVER_CLIENT_H
