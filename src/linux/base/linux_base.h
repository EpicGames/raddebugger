// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef LINUX_BASE_H
#define LINUX_BASE_H

////////////////////////////////
//~ rjf: Includes

#include <dirent.h>
#include <dlfcn.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <features.h>
#include <linux/limits.h>
#include <linux/futex.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <ucontext.h>

// NOTE(rjf): this is required because we need to use architecture-specific code for "base"
// OS functionality - we may want to reevaluate if such things should be in base - but for
// now just pulling in the whole x64 layer, just for linux.
#if ARCH_X64 || ARCH_X86
# include "x64/x64.h"
#endif

pid_t gettid(void);
int pthread_setname_np(pthread_t thread, const char *name);
int pthread_getname_np(pthread_t thread, char *name, size_t size);

typedef struct tm tm;
typedef struct timespec timespec;

////////////////////////////////
//~ rjf: Linux Call Interruption Retry Helper

#define LNX_RETRY_ON_EINTR(expr)             \
(__extension__({                           \
__typeof__(expr) __ret;                    \
do {                                       \
__ret = (expr);                          \
} while ((__ret == -1) && errno == EINTR); \
__ret;                                     \
}))


////////////////////////////////
//~ rjf: File Iterator

typedef struct LNX_FileIter LNX_FileIter;
struct LNX_FileIter
{
  DIR *dir;
  struct dirent *dp;
  String8 path;
};
StaticAssert(sizeof(Member(FileIter, memory)) >= sizeof(LNX_FileIter), lnx_file_iter_size_check);

////////////////////////////////
//~ rjf: Safe Call Handler Chain

typedef struct LNX_SafeCallChain LNX_SafeCallChain;
struct LNX_SafeCallChain
{
  LNX_SafeCallChain *next;
  ThreadEntryPointFunctionType *fail_handler;
  void *ptr;
};

////////////////////////////////
//~ rjf: Entities

typedef enum LNX_EntityKind
{
  LNX_EntityKind_Thread,
  LNX_EntityKind_Mutex,
  LNX_EntityKind_RWMutex,
  LNX_EntityKind_ConditionVariable,
  LNX_EntityKind_Barrier,
}
LNX_EntityKind;

typedef struct LNX_Entity LNX_Entity;
struct LNX_Entity
{
  LNX_Entity *next;
  LNX_EntityKind kind;
  union
  {
    struct
    {
      pthread_t handle;
      ThreadEntryPointFunctionType *func;
      void *ptr;
    } thread;
    pthread_mutex_t mutex_handle;
    pthread_rwlock_t rwmutex_handle;
    struct
    {
      pthread_cond_t cond_handle;
      pthread_mutex_t rwlock_mutex_handle;
    } cv;
    pthread_barrier_t barrier;
  };
};

////////////////////////////////
//~ On Demand Memory

#define LNX_MEMORY_FAULT_WORKER_LIMIT 32

typedef struct
{
  void *address;
  U32 result;
} LNX_MemoryFaultRequest;

typedef struct
{
  MemoryReadFaultFunction *fault;
  void *user_data;
  Thread workers[LNX_MEMORY_FAULT_WORKER_LIMIT];
  U32 worker_count;
  U32 stops_sent;
  U32 workers_joined;
  int requests[2];
  U32 active;
  pid_t owner_pid;
} LNX_DemandMemory;

////////////////////////////////
//~ rjf: State

typedef struct LNX_State LNX_State;
struct LNX_State
{
  Arena *arena;
  SystemInfo system_info;
  ProcessInfo process_info;
  pthread_mutex_t entity_mutex;
  Arena *entity_arena;
  LNX_Entity *entity_free;
  U64 default_env_count;
  char **default_env;
  LNX_DemandMemory demand_memory;
};

////////////////////////////////
//~ rjf: Globals

global LNX_State lnx_state = {0};
thread_static LNX_SafeCallChain *lnx_safe_call_chain = 0;
thread_static B32 lnx_in_memory_fault_callback;

////////////////////////////////
//~ rjf: Helpers

internal DateTime lnx_date_time_from_tm(tm in, U32 msec);
internal tm lnx_tm_from_date_time(DateTime dt);
internal timespec lnx_timespec_from_date_time(DateTime dt);
internal DenseTime lnx_dense_time_from_timespec(timespec in);
internal FileProperties lnx_file_properties_from_stat(struct stat *s);
internal B32 lnx_dispatch_memory_read_fault(int sig, siginfo_t *info, void *context);
internal void lnx_safe_call_sig_handler(int sig, siginfo_t *info, void *context);

////////////////////////////////
//~ rjf: Entities

internal LNX_Entity *lnx_entity_alloc(LNX_EntityKind kind);
internal void lnx_entity_release(LNX_Entity *entity);

////////////////////////////////
//~ rjf: Thread Entry Point

internal void *lnx_thread_entry_point(void *ptr);

#endif // LINUX_BASE_H
