// Copyright (c) 2024 Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ Win32 Implementations

#if defined(_WIN32)

//- types

typedef int BOOL;
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
typedef unsigned long DWORD;
typedef wchar_t WCHAR;
typedef char const *LPCSTR;
typedef const WCHAR *LPCWSTR, *PCWSTR;
typedef LONG HRESULT;
typedef void *HANDLE;
struct HINSTANCE__;
typedef struct HINSTANCE__ *HMODULE;
typedef __int64 INT_PTR;
typedef INT_PTR (*FARPROC)();

//- prototypes

#include <stdarg.h>

#if defined(__cplusplus)
extern "C"
{
#endif
  __declspec(dllimport) HMODULE LoadLibraryA(LPCSTR name);
  __declspec(dllimport) FARPROC GetProcAddress(HMODULE module, LPCSTR name);
  __declspec(dllimport) BOOL FreeLibrary(HMODULE mod);
  __declspec(dllimport) HANDLE GetCurrentThread(void);
  __declspec(dllimport) DWORD GetCurrentThreadId(void);
  __declspec(dllimport) void RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD nNumberOfArguments, const ULONG_PTR *lpArguments);
  long long _InterlockedCompareExchange64(long long volatile*, long long, long long);
  long long _InterlockedExchangeAdd64(long long volatile*, long long);
#pragma intrinsic(_InterlockedCompareExchange64)
#pragma intrinsic(_InterlockedExchangeAdd64)
  int raddbg_markup_vsnprintf(char *, unsigned long long, const char *, va_list);
#if defined(__cplusplus)
}
#endif

//- helpers

typedef struct RADDBG_MARKUP_UnicodeDecode RADDBG_MARKUP_UnicodeDecode;
struct RADDBG_MARKUP_UnicodeDecode
{
  unsigned __int32 inc;
  unsigned __int32 codepoint;
};
static __int8 raddbg_utf8_class[32] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,2,2,2,2,3,3,4,5};

static inline RADDBG_MARKUP_UnicodeDecode
raddbg_decode_utf8(char *str, unsigned __int64 max)
{
  RADDBG_MARKUP_UnicodeDecode result = {1, 0xffffffff};
  unsigned __int8 byte = str[0];
  unsigned __int8 byte_class = raddbg_utf8_class[byte >> 3];
  switch(byte_class)
  {
    case 1:
    {
      result.codepoint = byte;
    }break;
    case 2:
    if(2 < max)
    {
      char cont_byte = str[1];
      if(raddbg_utf8_class[cont_byte >> 3] == 0)
      {
        result.codepoint = (byte & 0x0000001f) << 6;
        result.codepoint |= (cont_byte & 0x0000003f);
        result.inc = 2;
      }
    }break;
    case 3:
    if(2 < max)
    {
      char cont_byte[2] = {str[1], str[2]};
      if(raddbg_utf8_class[cont_byte[0] >> 3] == 0 &&
         raddbg_utf8_class[cont_byte[1] >> 3] == 0)
      {
        result.codepoint = (byte & 0x0000000f) << 12;
        result.codepoint |= ((cont_byte[0] & 0x0000003f) << 6);
        result.codepoint |=  (cont_byte[1] & 0x0000003f);
        result.inc = 3;
      }
    }break;
    case 4:
    if(3 < max)
    {
      char cont_byte[3] = {str[1], str[2], str[3]};
      if(raddbg_utf8_class[cont_byte[0] >> 3] == 0 &&
         raddbg_utf8_class[cont_byte[1] >> 3] == 0 &&
         raddbg_utf8_class[cont_byte[2] >> 3] == 0)
      {
        result.codepoint = (byte & 0x00000007) << 18;
        result.codepoint |= ((cont_byte[0] & 0x0000003f) << 12);
        result.codepoint |= ((cont_byte[1] & 0x0000003f) <<  6);
        result.codepoint |=  (cont_byte[2] & 0x0000003f);
        result.inc = 4;
      }
    }
  }
  return result;
}

static inline unsigned __int32
raddbg_encode_utf16(wchar_t *str, unsigned __int32 codepoint)
{
  unsigned __int32 inc = 1;
  if(codepoint == 0xffffffff)
  {
    str[0] = (wchar_t)'?';
  }
  else if(codepoint < 0x10000)
  {
    str[0] = (wchar_t)codepoint;
  }
  else
  {
    U32 v = codepoint - 0x10000;
    str[0] = (wchar_t)(0xD800 + (v >> 10));
    str[1] = (wchar_t)(0xDC00 + (v & 0x000003ff));
    inc = 2;
  }
  return inc;
}

//- implementations

static inline int
raddbg_is_attached__impl(void)
{
  // TODO(rjf)
  return 0;
}

static inline void
raddbg_thread_name__impl(char *fmt, ...)
{
  // rjf: resolve variadic arguments
  char buffer[512] = {0};
  char *name = buffer;
  {
    va_list args;
    va_start(args, fmt);
    raddbg_markup_vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
  }
  
  // rjf: get windows 10 style procedure
  HRESULT (*SetThreadDescription_function)(HANDLE hThread, PCWSTR lpThreadDescription) = 0;
  {
    static HRESULT (*global_SetThreadDescription_function)(HANDLE hThread, PCWSTR lpThreadDescription);
    static volatile __int64 global_SetThreadDescription_init_started;
    static volatile __int64 global_SetThreadDescription_init_done;
    __int64 do_init = !_InterlockedCompareExchange64(&global_SetThreadDescription_init_started, 1, 0);
    if(do_init)
    {
      HMODULE module = LoadLibraryA("kernel32.dll");
      global_SetThreadDescription_function = (HRESULT (*)(HANDLE, PCWSTR))GetProcAddress(module, "SetThreadDescription");
      FreeLibrary(module);
      _InterlockedExchangeAdd64(&global_SetThreadDescription_init_done, 1);
    }
    for(;_InterlockedExchangeAdd64(&global_SetThreadDescription_init_done, 0) == 0;)
    {
      // NOTE(rjf): busy-loop, until init is done
    }
    SetThreadDescription_function = global_SetThreadDescription_function;
  }
  
  // rjf: set thread name, windows 10 style
  if(SetThreadDescription_function)
  {
    WCHAR buffer16[1024] = {0};
    int name_length = 0;
    for(;name[name_length]; name_length += 1);
    int write_offset = 0;
    for(int idx = 0; idx < name_length;)
    {
      RADDBG_MARKUP_UnicodeDecode decode = raddbg_decode_utf8(name+idx, name_length-idx);
      write_offset += raddbg_encode_utf16(buffer16 + write_offset, decode.codepoint);
      idx += decode.inc;
    }
    SetThreadDescription_function(GetCurrentThread(), buffer16);
  }
  
  // rjf: set thread name, raise-exception style
  {
#pragma pack(push, 8)
    typedef struct THREADNAME_INFO THREADNAME_INFO;
    struct THREADNAME_INFO
    {
      DWORD dwType;
      LPCSTR szName;
      DWORD dwThreadID;
      DWORD dwFlags;
    };
#pragma pack(pop)
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = GetCurrentThreadId();
    info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try
    {
      RaiseException(0x406D1388, 0, sizeof(info) / sizeof(void *), (const ULONG_PTR *)&info);
    }
    __except(1)
    {
    }
#pragma warning(pop)
  }
}

static inline void
raddbg_thread_color__impl(unsigned int hexcode)
{
  // TODO(rjf)
}

#define raddbg_break__impl() (__debugbreak())

static inline void
raddbg_watch__impl(char *fmt, ...)
{
  // TODO(rjf)
}

static inline void
raddbg_log__impl(char *fmt, ...)
{
  // TODO(rjf)
}

#endif // defined(_WIN32)
