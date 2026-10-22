// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

// radcobj.lib: export the object codec; Oodle symbols are resolved by the
// embedding application. No file mapping, worker, or fault-handler setup.
#define CMP_NO_OPEN 1

#if !OODLE_STATIC
# error build radobj.lib with OODLE_SDK=1 and OODLE_STATIC=1
#endif

#include "base/base_context_cracking.h"
#include "base/base_core.h"
#include "base/base_arena.h"
#include "base/base_math.h"
#include "base/base_strings.h"
#include "base/base_dynamic_libraries.h"
#include "compression/compression.h"
#include "linker/radcobj.h"

#include "compression/compression_oodle2.c"
#include "compression/compression.c"
#include "linker/radcobj.c"

