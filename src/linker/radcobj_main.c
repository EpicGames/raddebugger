// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#define RADCOBJ_LIB 1
#define CMP_NO_OPEN 1
#define BASE_NOENTRY 1
#define BASE_NO_AUTOLINK 1
#define NO_ASYNC 1
#define NO_WIN32_RIO 1

#if !OODLE_STATIC
# error build radcobj.lib with OODLE_SDK=1 and OODLE_STATIC=1
#endif

#include "base/base_inc.h"
#include "coff/coff.h"
#include "coff/coff_parse.h"
#include "codeview/codeview.h"
#include "codeview/codeview_parse.h"
#include "base_ext/base_inc.h"
#include "thread_pool/thread_pool.h"
#include "codeview_ext/codeview.h"
#include "compression/compression.h"
#include "linker/radcobj.h"

#include "base/base_inc.c"
#include "coff/coff.c"
#include "coff/coff_parse.c"
#include "codeview/codeview.c"
#include "codeview/codeview_parse.c"
#include "base_ext/base_inc.c"
#include "base_ext/base_blake3.c"
#include "thread_pool/thread_pool.c"
#include "codeview_ext/codeview.c"
#include "compression/compression_oodle2.c"
#include "compression/compression.c"

#if OS_WINDOWS
internal BOOL CALLBACK
radcobj_init_system_info_once(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  SystemInfo *system = get_system_info();
  system->page_size = info.dwPageSize;
  system->large_page_size = GetLargePageMinimum();
  system->allocation_granularity = info.dwAllocationGranularity;
  return TRUE;
}

internal void
radcobj_init_system_info(void)
{
  local_persist INIT_ONCE once = INIT_ONCE_STATIC_INIT;
  BOOL ok = InitOnceExecuteOnce(&once, radcobj_init_system_info_once, 0, 0);
  AssertAlways(ok);
}
#endif

#include "linker/radcobj.c"

