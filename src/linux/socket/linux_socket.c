// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: @per_os_impl Top-Level Layer Calls

internal void
sock_init(void)
{
  
}

internal void
sock_async_tick(void)
{
  
}

////////////////////////////////
//~ rjf: @per_os_impl Session Creation/Closing

internal SOCK_Session
sock_session_open(U16 listener_port, SOCK_WakeupFunctionType *wakeup_hook)
{
  
}

internal void
sock_session_close(SOCK_Session session)
{
  
}

////////////////////////////////
//~ rjf: @per_os_impl Sends

internal B32
sock_send(SOCK_Session session, SOCK_Protocol protocol, SOCK_Endpoint endpoint, String8 data, U64 endt_us)
{
  
}

////////////////////////////////
//~ rjf: @per_os_impl Receives

internal B32
sock_recv(Arena *arena, SOCK_Session session, SOCK_Protocol *protocol_out, SOCK_Endpoint *endpoint_out, String8 *data_out, U64 endt_us)
{
  
}
