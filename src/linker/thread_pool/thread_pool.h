// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#pragma once

struct TP_Context;
#define THREAD_POOL_TASK_FUNC_(name) void name(Arena *arena, U64 worker_id, U64 task_id, void *raw_task, struct TP_Context *tp)
#define THREAD_POOL_TASK_FUNC(name) internal THREAD_POOL_TASK_FUNC_(name)
typedef THREAD_POOL_TASK_FUNC_(TP_TaskFunc);

typedef struct TP_Arena
{
  U64     count;
  Arena **v;
} TP_Arena;

typedef struct TP_Temp
{
  U64   count;
  Temp *v;
} TP_Temp;

typedef struct TP_Worker
{
  U64                id;
  struct TP_Context *pool;
  Thread             handle;
} TP_Worker;

internal Rng1U64 tp_work_range(U64 item_count, U32 range_count, U64 task_id);

typedef struct TP_Context
{
  B32          is_live;
  Semaphore    exec_semaphore;
  Semaphore    task_semaphore;
  Semaphore    main_semaphore;
  Barrier      run_barrier;
  Barrier      barrier;
  void        *broadcast;
  void       **broadcast_many;
  U32          worker_count;
  TP_Worker   *worker_arr;
  TP_Arena    *task_arena;
  TP_TaskFunc *task_func;
  void        *task_data;
  U64          task_count;
  U64          task_done;
  S64          task_left;
} TP_Context;

internal TP_Context * tp_alloc(Arena *arena, U32 worker_count, U32 max_worker_count, String8 name);
internal void         tp_release(TP_Context *pool);
internal TP_Arena *   tp_arena_alloc(TP_Context *pool);
internal void         tp_arena_release(TP_Arena **arena_ptr);
internal void         tp_release_scratch_free_blocks(TP_Context *pool);
internal TP_Temp      tp_temp_begin(TP_Arena *arena);
internal void         tp_temp_end(TP_Temp temp);
#define tp_for_parallel_prof(pool, arena, task_count, task_func, task_data, zone_name) ProfBegin(zone_name); tp_for_parallel(pool, arena, task_count, task_func, task_data); ProfEnd();
internal void         tp_for_parallel(TP_Context *pool, TP_Arena *arena, U64 task_count, TP_TaskFunc *task_func, void *task_data);

#define tp_broadcast(p) tp_broadcast_(tp, task_id, p, sizeof(*p))

#define EX(X) X
#define FE_0(F)
#define FE_1(F, X) F(X)
#define FE_2(F, X, ...) F(X) EX(FE_1(F, __VA_ARGS__))
#define FE_3(F, X, ...) F(X) EX(FE_2(F, __VA_ARGS__))
#define FE_4(F, X, ...) F(X) EX(FE_3(F, __VA_ARGS__))
#define FE_5(F, X, ...) F(X) EX(FE_4(F, __VA_ARGS__))
#define FE_6(F, X, ...) F(X) EX(FE_5(F, __VA_ARGS__))
#define FE_7(F, X, ...) F(X) EX(FE_6(F, __VA_ARGS__))
#define FE_8(F, X, ...) F(X) EX(FE_7(F, __VA_ARGS__))
#define FE_N(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N
#define THREAD_FOREACH(F, ...) EX(FE_N(__VA_ARGS__,FE_8,FE_7,FE_6,FE_5,FE_4,FE_3,FE_2,FE_1)(F,__VA_ARGS__))
#define THREAD_ADDR(var) &var,
#define THREAD_SIZEOF(var) sizeof(var),
#define tp_broadcast_many(...) tp_broadcast_many_(tp, task_id, (void *[]){THREAD_FOREACH(THREAD_ADDR, __VA_ARGS__) 0}, (U64[]){THREAD_FOREACH(THREAD_SIZEOF, __VA_ARGS__)})
