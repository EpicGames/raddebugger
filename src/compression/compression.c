// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#if !CMP_ENCODER_ONLY

internal char *
cfm_status_string(CFM_Status status)
{
  switch (status) {
#define X(n,...) case CFM_##n: return Stringify(n);
    CFM_Status_XList
#undef X
  }
  return 0;
}

#if !CMP_NO_OPEN
internal char *
cmp_open(Arena *arena, CMP_SdkType sdk_type, String8 sdk_path, CMP_Ctx **ctx_out)
{
  switch (sdk_type) {
  case CMP_SdkType_Null: break;
  case CMP_SdkType_Oodle2: return cmp_open_oodle2(arena, sdk_path, ctx_out);
  default: InvalidPath;
  }
  return 0;
}

internal B32
cmp_close(CMP_Ctx *ctx)
{
  switch (ctx->sdk_type) {
  case CMP_SdkType_Null: break;
  case CMP_SdkType_Oodle2: return cmp_close_oodle2(ctx);
  default: InvalidPath;
  }
  return 0;
}

#endif // !CMP_NO_OPEN

internal U64
cmp_scratch(CMP_Ctx *ctx, U64 size)
{
  switch (ctx->sdk_type) {
  case CMP_SdkType_Null: break;
  case CMP_SdkType_Oodle2: return cmp_scratch_oodle2(ctx, size);
  default: InvalidPath;
  }
  return 0;
}

#endif // !CMP_ENCODER_ONLY

internal U64
cmp_encode(CMP_Ctx *ctx, String8 src, String8 dst)
{
  switch (ctx->sdk_type) {
  case CMP_SdkType_Null: break;
  case CMP_SdkType_Oodle2: return cmp_encode_oodle2(ctx, src, dst);
  default: InvalidPath;
  }
  return 0;
}

#if !CMP_ENCODER_ONLY
internal B32
cmp_decode(CMP_Ctx *ctx, String8 src, String8 dst, String8 scratch)
{
  switch (ctx->sdk_type) {
  case CMP_SdkType_Null: break;
  case CMP_SdkType_Oodle2: return cmp_decode_oodle2(ctx, src, dst, scratch);
  default: InvalidPath;
  }
  return 0;
}

#endif // !CMP_ENCODER_ONLY

internal U64
cmp_bound(CMP_Ctx *ctx, U64 size)
{
  switch (ctx->sdk_type) {
  case CMP_SdkType_Null: break;
  case CMP_SdkType_Oodle2: return cmp_bound_oodle2(ctx, size);
  default: InvalidPath;
  }
  return 0;
}

