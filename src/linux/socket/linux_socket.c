// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Listener Thread

internal void
lnx_sock_listener_thread_entry_point(void *p)
{
  ThreadNameF("lnx_sock_listener_thread_%I64x", p);
  LNX_SOCK_Session *session = (LNX_SOCK_Session *)p;
  for(;;)
  {
    //- rjf: wait for next event
    struct epoll_event evts[1] = {0};
    int wait_result = LNX_RETRY_ON_EINTR(epoll_wait(session->epoll_fd, evts, ArrayCount(evts), -1));
    
    //- rjf: code is 0 -> listener is ready for accept
    if(evts[0].data.u64 == 0)
    {
      // rjf: accept new connection
      struct sockaddr_storage addr = {0};
      socklen_t addr_size = sizeof(addr);
      int new_socket = LNX_RETRY_ON_EINTR(accept(session->tcp_listen_socket, (struct sockaddr *)&addr, &addr_size));
      
      // rjf: unpack socket's endpoint info
      SOCK_Endpoint endpoint = {0};
      {
        switch(addr.ss_family)
        {
          default:{}break;
          case AF_INET:
          {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&addr;
            endpoint.port = ntohs(ipv4->sin_port);
            endpoint.address_u32[0] = ipv4->sin_addr.s_addr;
          }break;
          case AF_INET6:
          {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&addr;
            endpoint.port = ntohs(ipv6->sin6_port);
            MemoryCopy(endpoint.address_u8, ipv6->sin6_addr.s6_addr, sizeof(endpoint.address_u8));
          }break;
        }
      }
      
      // rjf: unpack endpoint
      U64 hash = u64_hash_from_str8(str8_struct(&endpoint));
      U64 slot_idx = hash%session->connection_slots_count;
      LNX_SOCK_ConnectionSlot *slot = &session->connection_slots[slot_idx];
      Stripe *stripe = stripe_from_slot_idx(&session->connection_stripes, slot_idx);
      
      // rjf: store new connection
      U64 con_ptr_u64 = 0;
      RWMutexScope(stripe->rw_mutex, 1)
      {
        LNX_SOCK_Connection *con = (LNX_SOCK_Connection *)stripe->free;
        if(con != 0)
        {
          stripe->free = con->next;
        }
        else
        {
          con = push_array(stripe->arena, LNX_SOCK_Connection, 1);
        }
        con->endpoint = endpoint;
        con->protocol = SOCK_Protocol_TCP;
        con->socket = new_socket;
        DLLPushBack(slot->first, slot->last, con);
        con_ptr_u64 = (U64)con;
      }
      
      // rjf: add socket to epoll
      struct epoll_event evt = {0};
      evt.events = EPOLLIN;
      evt.data.u64 = con_ptr_u64;
      LNX_RETRY_ON_EINTR(epoll_ctl(session->epoll_fd, EPOLL_CTL_ADD, new_socket, &evt));
    }
    
    //- rjf: code is nonzero -> ready to read on a connection.
    else
    {
      // rjf: unpack associated connection
      LNX_SOCK_Connection *con = (LNX_SOCK_Connection *)evts[0].data.u64;
      SOCK_Endpoint endpoint = con->endpoint;
      U64 hash = u64_hash_from_str8(str8_struct(&endpoint));
      U64 slot_idx = hash%session->connection_slots_count;
      LNX_SOCK_ConnectionSlot *slot = &session->connection_slots[slot_idx];
      Stripe *stripe = stripe_from_slot_idx(&session->connection_stripes, slot_idx);
      
      // rjf: do read
      U8 buffer[4096] = {0};
      ssize_t recv_result = LNX_RETRY_ON_EINTR(recv(con->socket, buffer, sizeof(buffer), MSG_DONTWAIT));
      
      // rjf: bytes received -> push result to user.
      if(recv_result >= 0)
      {
        RingGuard g = guarded_ring_open(session->s2u_ring);
        U64 header[5] =
        {
          (U64)SOCK_Protocol_TCP,
          (U64)endpoint.port,
          endpoint.address_u64[0],
          endpoint.address_u64[1],
          (U64)recv_result,
        };
        guarded_ring_write_or_wait(&g, sizeof(header), header, max_U64);
        guarded_ring_write_or_wait(&g, recv_result, buffer, max_U64);
        guarded_ring_close(&g);
      }
      
      // rjf: error -> socket was disconnected
      else RWMutexScope(stripe->rw_mutex, 1)
      {
        close(con->socket);
        DLLRemove(slot->first, slot->last, con);
        con->next = stripe->free;
        stripe->free = con;
      }
    }
  }
}

////////////////////////////////
//~ rjf: @per_os_impl Top-Level Layer Calls

internal void
sock_init(void)
{
  Arena *arena = arena_alloc();
  lnx_sock_state = push_array(arena, LNX_SOCK_State, 1);
  lnx_sock_state->arena = arena;
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
  //- rjf: set up state
  Arena *arena = arena_alloc();
  LNX_SOCK_Session *session = push_array(arena, LNX_SOCK_Session, 1);
  session->arena = arena;
  session->u2s_ring = guarded_ring_alloc(arena, KB(256));
  session->s2u_ring = guarded_ring_alloc(arena, KB(256));
  session->epoll_fd = epoll_create(1);
  
  //- rjf: set up listener
  {
    session->tcp_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    U32 ipv6only = 0;
    setsockopt(session->tcp_listen_socket, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&ipv6only, sizeof(ipv6only));
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(listener_port);
    bind(session->tcp_listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(session->tcp_listen_socket, SOMAXCONN);
    struct epoll_event evt = {0};
    evt.events = EPOLLIN;
    epoll_ctl(session->epoll_fd, EPOLL_CTL_ADD, session->tcp_listen_socket, &evt);
  }
  
  //- rjf: set up connection cache
  session->connection_slots_count = 8;
  session->connection_stripes = stripe_array_alloc(arena);
  session->connection_slots = push_array(arena, LNX_SOCK_ConnectionSlot, session->connection_slots_count);
  
  //- rjf: launch listener thread
  session->listener_thread = thread_launch(lnx_sock_listener_thread_entry_point, session);
  
  //- rjf: bundle as handle
  SOCK_Session s = {(U64)session};
  return s;
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
  B32 result = 0;
  LNX_SOCK_Session *s = (LNX_SOCK_Session *)session.u64[0];
  RingGuard guard = guarded_ring_open(s->u2s_ring);
  {
    U64 header[5] = {0};
    U64 size_cap = s->u2s_ring->ring->size - sizeof(header);
    U64 size = Min(data.size, size_cap);
    {
      header[0] = (U64)protocol;
      header[1] = (U64)endpoint.port;
      header[2] = endpoint.address_u64[0];
      header[3] = endpoint.address_u64[1];
      header[4] = size;
    }
    if(guarded_ring_write_or_wait(&guard, sizeof(header), header, endt_us))
    {
      guarded_ring_write_or_wait(&guard, size, data.str, max_U64);
      result = 1;
    }
  }
  guarded_ring_close(&guard);
  if(result)
  {
    ins_atomic_u32_eval_assign(&async_loop_again, 1);
    cond_var_broadcast(async_tick_start_cond_var);
  }
  return result;
}

////////////////////////////////
//~ rjf: @per_os_impl Receives

internal B32
sock_recv(Arena *arena, SOCK_Session session, SOCK_Protocol *protocol_out, SOCK_Endpoint *endpoint_out, String8 *data_out, U64 endt_us)
{
  B32 result = 0;
  U64 header_size = sizeof(*protocol_out) + sizeof(*endpoint_out) + sizeof(U64);
  {
    LNX_SOCK_Session *s = (LNX_SOCK_Session *)session.u64[0];
    RingGuard guard = guarded_ring_open(s->s2u_ring);
    {
      U64 header[5] = {0};
      if(guarded_ring_read_or_wait(&guard, sizeof(header), header, endt_us))
      {
        U64 size_cap = s->s2u_ring->ring->size - sizeof(header);
        protocol_out[0] = (SOCK_Protocol)header[0];
        endpoint_out->port = (U16)header[1];
        endpoint_out->address_u64[0] = header[2];
        endpoint_out->address_u64[1] = header[3];
        data_out->size = Min(size_cap, header[4]);
        data_out->str = push_array(arena, U8, data_out->size);
        guarded_ring_read_or_wait(&guard, data_out->size, data_out->str, max_U64);
        result = 1;
      }
    }
    guarded_ring_close(&guard);
  }
  return result;
}
