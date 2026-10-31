// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#if OODLE_STATIC && !OODLE_SDK
#error OODLE_STATIC requires OODLE_SDK and the Oodle SDK headers.
#endif

#if OODLE_SDK

#define CMP_OODLE2_COMPRESSOR OodleLZ_Compressor_Kraken

#include <oodle2base.h>
#pragma push_macro("OOINLINEFUNC")
#undef OOINLINEFUNC
#define OOINLINEFUNC force_inline
#include <oodle2.h>
#pragma pop_macro("OOINLINEFUNC")

#if !OODLE_STATIC
typedef OO_BOOL  (OOFUNC2 CMP_Oodle2_CheckVersion)(OO_U32, OO_U32 *);
typedef OO_SINTa (OOFUNC2 CMP_Oodle2_Bound       )(OodleLZ_Compressor, OO_SINTa);
typedef OO_S32   (OOFUNC2 CMP_Oodle2_Scratch     )(OodleLZ_Compressor, OO_SINTa);
typedef void     (OOFUNC2 CMP_Oodle2_Validate    )(OodleLZ_CompressOptions *);
typedef OO_SINTa (OOFUNC2 CMP_Oodle2_Decode      )(const void *, OO_SINTa, void *, OO_SINTa, OodleLZ_FuzzSafe, OodleLZ_CheckCRC, OodleLZ_Verbosity, void *, OO_SINTa, OodleDecompressCallback *, void *, void *, OO_SINTa, OodleLZ_Decode_ThreadPhase);
typedef OO_SINTa (OOFUNC2 CMP_Oodle2_Compress    )(OodleLZ_Compressor, const void *, OO_SINTa, void *, OodleLZ_CompressionLevel, const OodleLZ_CompressOptions *, const void *, const void *, void *, OO_SINTa);
typedef const OodleLZ_CompressOptions *(OOFUNC2 CMP_Oodle2_Options)(OodleLZ_Compressor, OodleLZ_CompressionLevel);

#define CMP_Oodle2_XList                                   \
  X(CheckVersion, "Oodle_CheckVersion")                    \
  X(Bound,        "OodleLZ_GetCompressedBufferSizeNeeded") \
  X(Scratch,      "OodleLZDecoder_MemorySizeNeeded")       \
  X(Validate,     "OodleLZ_CompressOptions_Validate")      \
  X(Decode,       "OodleLZ_Decompress")                    \
  X(Compress,     "OodleLZ_Compress")                      \
  X(Options,      "OodleLZ_CompressOptions_GetDefault")    \
  /* end */

typedef struct
{
  Library library;
  OodleLZ_Compressor compressor;
#define X(n,...) CMP_Oodle2_##n *n;
  CMP_Oodle2_XList
#undef X
} CMP_Oodle2_API;
#endif

#if !CMP_ENCODER_ONLY
#if !CMP_NO_OPEN
internal CMP_CLOSE(cmp_close_oodle2)
{
#if OODLE_STATIC
  if (!ctx) { return 0; }
  MemoryZeroStruct(ctx);
#else
  if (ctx == 0 || MemoryIsZeroStruct(&ctx->library)) { return 0; }
  library_close(ctx->library);
  MemoryZeroStruct(&ctx->library);
  ctx->api = 0;
#endif
  return 1;
}

#endif // !CMP_NO_OPEN

internal CMP_SCRATCH(cmp_scratch_oodle2)
{
  if (size == 0 || size > max_S64) { return 0; }
#if OODLE_STATIC
  OO_S32 result = OodleLZDecoder_MemorySizeNeeded(CMP_OODLE2_COMPRESSOR, (OO_SINTa)size);
#else
  CMP_Oodle2_API *api = ctx->api;
  OO_S32 result = api->Scratch(api->compressor, (OO_SINTa)size);
#endif
  return result > 0 ? (U64)result : 0;
}

#endif // !CMP_ENCODER_ONLY

internal CMP_ENCODE(cmp_encode_oodle2)
{
  if (src.str == 0 || dst.str == 0 || src.size == 0 || src.size > max_S64) { return 0; }
#if OODLE_STATIC
  OO_SINTa bound = OodleLZ_GetCompressedBufferSizeNeeded(CMP_OODLE2_COMPRESSOR, (OO_SINTa)src.size);
  if (bound <= 0 || dst.size < (U64)bound) { return 0; }
  OO_SINTa result = OodleLZ_Compress(CMP_OODLE2_COMPRESSOR, src.str, (OO_SINTa)src.size, dst.str,
                                   OodleLZ_CompressionLevel_Normal, 0, 0, 0, 0, 0);
#else
  CMP_Oodle2_API *api = ctx->api;

  // Oodle's encoder takes no destination capacity; check its bound before calling.
  OO_SINTa bound = api->Bound(api->compressor, (OO_SINTa)src.size);
  if (bound <= 0 || dst.size < (U64)bound) { return 0; }

  OO_SINTa result = api->Compress(api->compressor, src.str, (OO_SINTa)src.size, dst.str,
                                 OodleLZ_CompressionLevel_Normal, 0, 0, 0, 0, 0);
#endif
  return result > 0 ? (U64)result : 0;
}

#if !CMP_ENCODER_ONLY
internal CMP_DECODE(cmp_decode_oodle2)
{
  if (src.str == 0 || dst.str == 0 || src.size == 0 || dst.size == 0 ||
      src.size > max_S64 || dst.size > max_S64 || scratch.size > max_S64 ||
      (scratch.size != 0 && scratch.str == 0)) { return 0; }
#if OODLE_STATIC
  OO_SINTa result = OodleLZ_Decompress(src.str, (OO_SINTa)src.size, dst.str, (OO_SINTa)dst.size,
                                      OodleLZ_FuzzSafe_Yes, OodleLZ_CheckCRC_No, OodleLZ_Verbosity_None,
                                      0, 0, 0, 0, scratch.str, (OO_SINTa)scratch.size,
                                      OodleLZ_Decode_Unthreaded);
#else
  CMP_Oodle2_API *api = ctx->api;

  // dst.size is the exact original size, rather than just the output capacity.
  OO_SINTa result = api->Decode(src.str, (OO_SINTa)src.size, dst.str, (OO_SINTa)dst.size,
                               OodleLZ_FuzzSafe_Yes, OodleLZ_CheckCRC_No, OodleLZ_Verbosity_None,
                               0, 0, 0, 0, scratch.str, (OO_SINTa)scratch.size,
                               OodleLZ_Decode_Unthreaded);
#endif
  return result == (OO_SINTa)dst.size;
}

#endif // !CMP_ENCODER_ONLY

internal CMP_BOUND(cmp_bound_oodle2)
{
  if (size == 0 || size > max_S64) { return 0; }
#if OODLE_STATIC
  OO_SINTa result = OodleLZ_GetCompressedBufferSizeNeeded(CMP_OODLE2_COMPRESSOR, (OO_SINTa)size);
#else
  CMP_Oodle2_API *api = ctx->api;
  OO_SINTa result = api->Bound(api->compressor, (OO_SINTa)size);
#endif
  return result > 0 ? (U64)result : 0;
}

#if !CMP_ENCODER_ONLY && !CMP_NO_OPEN
internal CMP_OPEN(cmp_open_oodle2)
{
#if OODLE_STATIC
  *ctx_out = 0;
  if (!Oodle_CheckVersion(OODLE_HEADER_VERSION, 0)) { return "library version mismatch"; }
  CMP_Ctx *ctx = push_array(arena, CMP_Ctx, 1);
  ctx->sdk_type = CMP_SdkType_Oodle2;
  *ctx_out = ctx;
  return 0;
#else
  char *error = 0;
  *ctx_out = 0;

  // load oodle2
  Library library = library_open(library_path);
  if (MemoryIsZeroStruct(&library)) { error = "failed to load the library"; goto exit; }

  CMP_Oodle2_API *api = push_array(arena, CMP_Oodle2_API, 1);

  // load SDK API
#define X(n, sdk_name)                                                 \
  api->n      = (void*)library_load_proc(library, str8_lit(sdk_name)); \
  if (api->n == 0) { error = "failed to load Oodle2 API"; goto exit; } \
  /* end */
  CMP_Oodle2_XList
#undef X

  // version check SDK
  if (api->CheckVersion(OODLE_HEADER_VERSION, 0) == 0) {
    error = "library version mismatch";
    goto exit;
  }

  // set default compressor
  api->compressor = CMP_OODLE2_COMPRESSOR;

  // fill out compression context
  CMP_Ctx *ctx = push_array(arena, CMP_Ctx, 1);
  ctx->sdk_type = CMP_SdkType_Oodle2;
  ctx->library  = library;
  ctx->api      = api;
  *ctx_out = ctx;

  exit:;
  if (error && !MemoryIsZeroStruct(&library)) { library_close(library); }
  return error;
#endif
}

#endif // !CMP_ENCODER_ONLY && !CMP_NO_OPEN

#else
#if !CMP_ENCODER_ONLY
#if !CMP_NO_OPEN
internal CMP_OPEN(cmp_open_oodle2)       { *ctx_out = 0; return "Oodle2 SDK is not enabled"; }
internal CMP_CLOSE(cmp_close_oodle2)     { return 0; }
#endif
internal CMP_SCRATCH(cmp_scratch_oodle2) { return 0; }
internal CMP_DECODE(cmp_decode_oodle2)   { return 0; }
#endif
internal CMP_ENCODE(cmp_encode_oodle2)   { return 0; }
internal CMP_BOUND(cmp_bound_oodle2)     { return 0; }
#endif
