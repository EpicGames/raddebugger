// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

internal B32
ds_push_msg(GuardedRing *out_ring, DS_Inst inst, DS_Msg *msg, U64 endt_us)
{
  B32 result = 0;
  switch(inst.kind)
  {
    default:{}break;
    case DS_InstKind_Host:
    {
      result = ds_push_msg__host(out_ring, msg, endt_us);
    }break;
  }
  return result;
}

internal B32
ds_pop_response(Arena *arena, GuardedRing *out_ring, DS_Response *response_out, U64 endt_us)
{
  B32 result = 0;
  return result;
}
