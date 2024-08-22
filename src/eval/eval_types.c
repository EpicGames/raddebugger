// Copyright (c) 2024 Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Type Kind Enum Functions

internal E_TypeKind
e_type_kind_from_rdi(RDI_TypeKind kind)
{
  E_TypeKind result = E_TypeKind_Null;
  switch(kind)
  {
    default:{}break;
    case RDI_TypeKind_Void:                   {result = E_TypeKind_Void;}break;
    case RDI_TypeKind_Handle:                 {result = E_TypeKind_Handle;}break;
    case RDI_TypeKind_Char8:                  {result = E_TypeKind_Char8;}break;
    case RDI_TypeKind_Char16:                 {result = E_TypeKind_Char16;}break;
    case RDI_TypeKind_Char32:                 {result = E_TypeKind_Char32;}break;
    case RDI_TypeKind_UChar8:                 {result = E_TypeKind_UChar8;}break;
    case RDI_TypeKind_UChar16:                {result = E_TypeKind_UChar16;}break;
    case RDI_TypeKind_UChar32:                {result = E_TypeKind_UChar32;}break;
    case RDI_TypeKind_U8:                     {result = E_TypeKind_U8;}break;
    case RDI_TypeKind_U16:                    {result = E_TypeKind_U16;}break;
    case RDI_TypeKind_U32:                    {result = E_TypeKind_U32;}break;
    case RDI_TypeKind_U64:                    {result = E_TypeKind_U64;}break;
    case RDI_TypeKind_U128:                   {result = E_TypeKind_U128;}break;
    case RDI_TypeKind_U256:                   {result = E_TypeKind_U256;}break;
    case RDI_TypeKind_U512:                   {result = E_TypeKind_U512;}break;
    case RDI_TypeKind_S8:                     {result = E_TypeKind_S8;}break;
    case RDI_TypeKind_S16:                    {result = E_TypeKind_S16;}break;
    case RDI_TypeKind_S32:                    {result = E_TypeKind_S32;}break;
    case RDI_TypeKind_S64:                    {result = E_TypeKind_S64;}break;
    case RDI_TypeKind_S128:                   {result = E_TypeKind_S128;}break;
    case RDI_TypeKind_S256:                   {result = E_TypeKind_S256;}break;
    case RDI_TypeKind_S512:                   {result = E_TypeKind_S512;}break;
    case RDI_TypeKind_Bool:                   {result = E_TypeKind_Bool;}break;
    case RDI_TypeKind_F16:                    {result = E_TypeKind_F16;}break;
    case RDI_TypeKind_F32:                    {result = E_TypeKind_F32;}break;
    case RDI_TypeKind_F32PP:                  {result = E_TypeKind_F32PP;}break;
    case RDI_TypeKind_F48:                    {result = E_TypeKind_F48;}break;
    case RDI_TypeKind_F64:                    {result = E_TypeKind_F64;}break;
    case RDI_TypeKind_F80:                    {result = E_TypeKind_F80;}break;
    case RDI_TypeKind_F128:                   {result = E_TypeKind_F128;}break;
    case RDI_TypeKind_ComplexF32:             {result = E_TypeKind_ComplexF32;}break;
    case RDI_TypeKind_ComplexF64:             {result = E_TypeKind_ComplexF64;}break;
    case RDI_TypeKind_ComplexF80:             {result = E_TypeKind_ComplexF80;}break;
    case RDI_TypeKind_ComplexF128:            {result = E_TypeKind_ComplexF128;}break;
    case RDI_TypeKind_Modifier:               {result = E_TypeKind_Modifier;}break;
    case RDI_TypeKind_Ptr:                    {result = E_TypeKind_Ptr;}break;
    case RDI_TypeKind_LRef:                   {result = E_TypeKind_LRef;}break;
    case RDI_TypeKind_RRef:                   {result = E_TypeKind_RRef;}break;
    case RDI_TypeKind_Array:                  {result = E_TypeKind_Array;}break;
    case RDI_TypeKind_Function:               {result = E_TypeKind_Function;}break;
    case RDI_TypeKind_Method:                 {result = E_TypeKind_Method;}break;
    case RDI_TypeKind_MemberPtr:              {result = E_TypeKind_MemberPtr;}break;
    case RDI_TypeKind_Struct:                 {result = E_TypeKind_Struct;}break;
    case RDI_TypeKind_Class:                  {result = E_TypeKind_Class;}break;
    case RDI_TypeKind_Union:                  {result = E_TypeKind_Union;}break;
    case RDI_TypeKind_Enum:                   {result = E_TypeKind_Enum;}break;
    case RDI_TypeKind_Alias:                  {result = E_TypeKind_Alias;}break;
    case RDI_TypeKind_IncompleteStruct:       {result = E_TypeKind_IncompleteStruct;}break;
    case RDI_TypeKind_IncompleteUnion:        {result = E_TypeKind_IncompleteUnion;}break;
    case RDI_TypeKind_IncompleteClass:        {result = E_TypeKind_IncompleteClass;}break;
    case RDI_TypeKind_IncompleteEnum:         {result = E_TypeKind_IncompleteEnum;}break;
    case RDI_TypeKind_Bitfield:               {result = E_TypeKind_Bitfield;}break;
    case RDI_TypeKind_Variadic:               {result = E_TypeKind_Variadic;}break;
  }
  return result;
}

internal E_MemberKind
e_member_kind_from_rdi(RDI_MemberKind kind)
{
  E_MemberKind result = E_MemberKind_Null;
  switch(kind)
  {
    default:{}break;
    case RDI_MemberKind_DataField:            {result = E_MemberKind_DataField;}break;
    case RDI_MemberKind_StaticData:           {result = E_MemberKind_StaticData;}break;
    case RDI_MemberKind_Method:               {result = E_MemberKind_Method;}break;
    case RDI_MemberKind_StaticMethod:         {result = E_MemberKind_StaticMethod;}break;
    case RDI_MemberKind_VirtualMethod:        {result = E_MemberKind_VirtualMethod;}break;
    case RDI_MemberKind_VTablePtr:            {result = E_MemberKind_VTablePtr;}break;
    case RDI_MemberKind_Base:                 {result = E_MemberKind_Base;}break;
    case RDI_MemberKind_VirtualBase:          {result = E_MemberKind_VirtualBase;}break;
    case RDI_MemberKind_NestedType:           {result = E_MemberKind_NestedType;}break;
  }
  return result;
}

internal RDI_EvalTypeGroup
e_type_group_from_kind(E_TypeKind kind)
{
  RDI_EvalTypeGroup result = 0;
  switch(kind)
  {
    default:{}break;
    
    case E_TypeKind_Null: case E_TypeKind_Void:
    case E_TypeKind_F16:  case E_TypeKind_F32PP: case E_TypeKind_F48:
    case E_TypeKind_F80:  case E_TypeKind_F128:
    case E_TypeKind_ComplexF32: case E_TypeKind_ComplexF64:
    case E_TypeKind_ComplexF80: case E_TypeKind_ComplexF128:
    case E_TypeKind_Modifier:   case E_TypeKind_Array:
    case E_TypeKind_Struct:     case E_TypeKind_Class: case E_TypeKind_Union:
    case E_TypeKind_Enum:       case E_TypeKind_Alias:
    case E_TypeKind_IncompleteStruct: case E_TypeKind_IncompleteClass:
    case E_TypeKind_IncompleteUnion:  case E_TypeKind_IncompleteEnum:
    case E_TypeKind_Bitfield:
    case E_TypeKind_Variadic:
    {result = RDI_EvalTypeGroup_Other;}break;
    
    case E_TypeKind_Handle:
    case E_TypeKind_UChar8: case E_TypeKind_UChar16: case E_TypeKind_UChar32:
    case E_TypeKind_U8:     case E_TypeKind_U16:     case E_TypeKind_U32:
    case E_TypeKind_U64:    case E_TypeKind_U128:    case E_TypeKind_U256:
    case E_TypeKind_U512:
    case E_TypeKind_Ptr: case E_TypeKind_LRef: case E_TypeKind_RRef:
    case E_TypeKind_Function: case E_TypeKind_Method: case E_TypeKind_MemberPtr:
    {result = RDI_EvalTypeGroup_U;}break;
    
    case E_TypeKind_Char8: case E_TypeKind_Char16: case E_TypeKind_Char32:
    case E_TypeKind_S8:    case E_TypeKind_S16:    case E_TypeKind_S32:
    case E_TypeKind_S64:   case E_TypeKind_S128:   case E_TypeKind_S256:
    case E_TypeKind_S512:
    case E_TypeKind_Bool:
    {result = RDI_EvalTypeGroup_S;}break;
    
    case E_TypeKind_F32:{result = RDI_EvalTypeGroup_F32;}break;
    case E_TypeKind_F64:{result = RDI_EvalTypeGroup_F64;}break;
  }
  return result;
}

internal B32
e_type_kind_is_integer(E_TypeKind kind)
{
  B32 result = (E_TypeKind_FirstInteger <= kind && kind <= E_TypeKind_LastInteger);
  return result;
}

internal B32
e_type_kind_is_signed(E_TypeKind kind)
{
  B32 result = ((E_TypeKind_FirstSigned1 <= kind && kind <= E_TypeKind_LastSigned1) ||
                (E_TypeKind_FirstSigned2 <= kind && kind <= E_TypeKind_LastSigned2));
  return result;
}

internal B32
e_type_kind_is_basic_or_enum(E_TypeKind kind)
{
  B32 result = ((E_TypeKind_FirstBasic <= kind && kind <= E_TypeKind_LastBasic) ||
                kind == E_TypeKind_Enum);
  return result;
}

////////////////////////////////
//~ rjf: Context Selection Functions (Selection Required For All Subsequent APIs)

internal E_TypeCtx *
e_selected_type_ctx(void)
{
  return e_type_state->ctx;
}

internal void
e_select_type_ctx(E_TypeCtx *ctx)
{
  if(e_type_state == 0)
  {
    Arena *arena = arena_alloc();
    e_type_state = push_array(arena, E_TypeState, 1);
    e_type_state->arena = arena;
    e_type_state->arena_eval_start_pos = arena_pos(e_type_state->arena);
  }
  arena_pop_to(e_type_state->arena, e_type_state->arena_eval_start_pos);
  e_type_state->ctx = ctx;
  e_type_state->cons_content_slots_count = 256;
  e_type_state->cons_key_slots_count = 256;
  e_type_state->cons_content_slots = push_array(e_type_state->arena, E_ConsTypeSlot, e_type_state->cons_content_slots_count);
  e_type_state->cons_key_slots = push_array(e_type_state->arena, E_ConsTypeSlot, e_type_state->cons_key_slots_count);
}

////////////////////////////////
//~ rjf: Type Operation Functions

//- rjf: key constructors

internal E_TypeKey
e_type_key_zero(void)
{
  E_TypeKey k = zero_struct;
  return k;
}

internal E_TypeKey
e_type_key_basic(E_TypeKind kind)
{
  E_TypeKey key = {E_TypeKeyKind_Basic};
  key.u32[0] = (U32)kind;
  return key;
}

internal E_TypeKey
e_type_key_ext(E_TypeKind kind, U32 type_idx, U32 rdi_idx)
{
  E_TypeKey key = {E_TypeKeyKind_Ext};
  key.u32[0] = (U32)kind;
  if(E_TypeKind_FirstBasic <= kind && kind <= E_TypeKind_LastBasic)
  {
    key.kind = E_TypeKeyKind_Basic;
  }
  else
  {
    key.u32[1] = type_idx;
    key.u32[2] = rdi_idx;
  }
  return key;
}

internal E_TypeKey
e_type_key_reg(Architecture arch, REGS_RegCode code)
{
  E_TypeKey key = {E_TypeKeyKind_Reg};
  key.u32[0] = (U32)arch;
  key.u32[1] = (U32)code;
  return key;
}

internal E_TypeKey
e_type_key_reg_alias(Architecture arch, REGS_AliasCode code)
{
  E_TypeKey key = {E_TypeKeyKind_RegAlias};
  key.u32[0] = (U32)arch;
  key.u32[1] = (U32)code;
  return key;
}

internal E_TypeKey
e_type_key_cons(E_TypeKind kind, E_TypeKey direct_key, U64 u64)
{
  U32 buffer[] =
  {
    (U32)kind,
    (U32)direct_key.kind,
    (U32)direct_key.u32[0],
    (U32)direct_key.u32[1],
    (U32)direct_key.u32[2],
    (U32)((u64 & 0x00000000ffffffffull)>> 0),
    (U32)((u64 & 0xffffffff00000000ull)>> 32),
  };
  U64 content_hash = e_hash_from_string(str8((U8 *)buffer, sizeof(buffer)));
  U64 content_slot_idx = content_hash%e_type_state->cons_content_slots_count;
  E_ConsTypeSlot *content_slot = &e_type_state->cons_content_slots[content_slot_idx];
  E_ConsTypeNode *node = 0;
  for(E_ConsTypeNode *n = content_slot->first; n != 0; n = n->content_next)
  {
    if(e_type_kind_from_key(n->key) == kind &&
       e_type_key_match(n->direct_key, direct_key) &&
       n->u64 == u64)
    {
      node = n;
      break;
    }
  }
  E_TypeKey result = zero_struct;
  if(node == 0)
  {
    E_TypeKey key = {E_TypeKeyKind_Cons};
    key.u32[0] = (U32)kind;
    key.u32[1] = (U32)e_type_state->cons_id_gen;
    e_type_state->cons_id_gen += 1;
    U64 key_hash = e_hash_from_string(str8_struct(&key));
    U64 key_slot_idx = key_hash%e_type_state->cons_key_slots_count;
    E_ConsTypeSlot *key_slot = &e_type_state->cons_key_slots[key_slot_idx];
    E_ConsTypeNode *node = push_array(e_type_state->arena, E_ConsTypeNode, 1);
    SLLQueuePush_N(content_slot->first, content_slot->last, node, content_next);
    SLLQueuePush_N(key_slot->first, key_slot->last, node, key_next);
    node->key = key;
    node->direct_key = direct_key;
    node->u64 = u64;
    result = key;
  }
  else
  {
    result = node->key;
  }
  return result;
}

//- rjf: basic type key functions

internal B32
e_type_key_match(E_TypeKey l, E_TypeKey r)
{
  B32 result = MemoryMatchStruct(&l, &r);
  return result;
}

//- rjf: key -> info extraction

internal E_TypeKind
e_type_kind_from_key(E_TypeKey key)
{
  E_TypeKind kind = E_TypeKind_Null;
  switch(key.kind)
  {
    default:{}break;
    case E_TypeKeyKind_Basic:   {kind = (E_TypeKind)key.u32[0];}break;
    case E_TypeKeyKind_Ext:     {kind = (E_TypeKind)key.u32[0];}break;
    case E_TypeKeyKind_Cons:    {kind = (E_TypeKind)key.u32[0];}break;
    case E_TypeKeyKind_Reg:     {kind = E_TypeKind_Union;}break;
    case E_TypeKeyKind_RegAlias:{kind = E_TypeKind_Union;}break;
  }
  return kind;
}

internal E_Type *
e_type_from_key(Arena *arena, E_TypeKey key)
{
  E_Type *type = &e_type_nil;
  U64 reg_byte_count = 0;
  {
    switch(key.kind)
    {
      default:{}break;
      
      //- rjf: basic type keys
      case E_TypeKeyKind_Basic:
      {
        E_TypeKind kind = (E_TypeKind)key.u32[0];
        if(E_TypeKind_FirstBasic <= kind && kind <= E_TypeKind_LastBasic)
        {
          type = push_array(arena, E_Type, 1);
          type->kind       = kind;
          type->name       = e_kind_basic_string_table[kind];
          type->byte_size  = e_kind_basic_byte_size_table[kind];
        }
      }break;
      
      //- rjf: constructed type keys
      case E_TypeKeyKind_Cons:
      {
        U64 key_hash = e_hash_from_string(str8_struct(&key));
        U64 key_slot_idx = key_hash%e_type_state->cons_key_slots_count;
        E_ConsTypeSlot *key_slot = &e_type_state->cons_key_slots[key_slot_idx];
        for(E_ConsTypeNode *node = key_slot->first;
            node != 0;
            node = node->key_next)
        {
          if(e_type_key_match(node->key, key))
          {
            type = push_array(arena, E_Type, 1);
            type->kind             = e_type_kind_from_key(node->key);
            type->direct_type_key  = node->direct_key;
            type->count            = node->u64;
            switch(type->kind)
            {
              default:
              {
                type->byte_size = bit_size_from_arch(e_type_state->ctx->arch)/8;
              }break;
              case E_TypeKind_Array:
              {
                U64 ptee_size = e_type_byte_size_from_key(node->direct_key);
                type->byte_size = ptee_size * type->count;
              }break;
            }
          }
        }
      }break;
      
      //- rjf: external (rdi) type keys
      case E_TypeKeyKind_Ext:
      {
        U64 type_node_idx = key.u32[1];
        U32 rdi_idx = key.u32[2];
        RDI_Parsed *rdi = e_type_state->ctx->rdis[rdi_idx];
        RDI_TypeNode *rdi_type = rdi_element_from_name_idx(rdi, TypeNodes, type_node_idx);
        if(rdi_type->kind != RDI_TypeKind_NULL)
        {
          E_TypeKind kind = e_type_kind_from_rdi(rdi_type->kind);
          
          //- rjf: record types => unpack name * members & produce
          if(RDI_TypeKind_FirstRecord <= rdi_type->kind && rdi_type->kind <= RDI_TypeKind_LastRecord)
          {
            // rjf: unpack name
            String8 name = {0};
            name.str = rdi_string_from_idx(rdi, rdi_type->user_defined.name_string_idx, &name.size);
            
            // rjf: unpack UDT info
            RDI_UDT *udt = rdi_element_from_name_idx(rdi, UDTs, rdi_type->user_defined.udt_idx);
            
            // rjf: unpack members
            E_Member *members = 0;
            U32 members_count = 0;
            {
              members_count = udt->member_count;
              members = push_array(arena, E_Member, members_count);
              if(members_count != 0)
              {
                for(U32 member_idx = udt->member_first;
                    member_idx < udt->member_first+udt->member_count;
                    member_idx += 1)
                {
                  RDI_Member *src = rdi_element_from_name_idx(rdi, Members, member_idx);
                  E_TypeKind member_type_kind = E_TypeKind_Null;
                  RDI_TypeNode *member_type = rdi_element_from_name_idx(rdi, TypeNodes, src->type_idx);
                  member_type_kind = e_type_kind_from_rdi(member_type->kind);
                  E_Member *dst = &members[member_idx-udt->member_first];
                  dst->kind     = e_member_kind_from_rdi(src->kind);
                  dst->type_key = e_type_key_ext(member_type_kind, src->type_idx, rdi_idx);
                  dst->name.str = rdi_string_from_idx(rdi, src->name_string_idx, &dst->name.size);
                  dst->off      = (U64)src->off;
                }
              }
            }
            
            // rjf: produce
            type = push_array(arena, E_Type, 1);
            type->kind       = kind;
            type->name       = push_str8_copy(arena, name);
            type->byte_size  = (U64)rdi_type->byte_size;
            type->count      = members_count;
            type->members    = members;
          }
          
          //- rjf: enum types => unpack name * values & produce
          else if(rdi_type->kind == RDI_TypeKind_Enum)
          {
            // rjf: unpack name
            String8 name = {0};
            name.str = rdi_string_from_idx(rdi, rdi_type->user_defined.name_string_idx, &name.size);
            
            // rjf: unpack direct type
            E_TypeKey direct_type_key = zero_struct;
            if(rdi_type->user_defined.direct_type_idx < type_node_idx)
            {
              RDI_TypeNode *direct_type_node = rdi_element_from_name_idx(rdi, TypeNodes, rdi_type->user_defined.direct_type_idx);
              E_TypeKind direct_type_kind = e_type_kind_from_rdi(direct_type_node->kind);
              direct_type_key = e_type_key_ext(direct_type_kind, rdi_type->user_defined.direct_type_idx, rdi_idx);
            }
            
            // rjf: unpack members
            E_EnumVal *enum_vals = 0;
            U32 enum_vals_count = 0;
            {
              U32 udt_idx = rdi_type->user_defined.udt_idx;
              RDI_UDT *udt = rdi_element_from_name_idx(rdi, UDTs, udt_idx);
              enum_vals_count = udt->member_count;
              enum_vals = push_array(arena, E_EnumVal, enum_vals_count);
              for(U32 member_idx = udt->member_first;
                  member_idx < udt->member_first+udt->member_count;
                  member_idx += 1)
              {
                RDI_EnumMember *src = rdi_element_from_name_idx(rdi, EnumMembers, member_idx);
                E_EnumVal *dst = &enum_vals[member_idx-udt->member_first];
                dst->name.str = rdi_string_from_idx(rdi, src->name_string_idx, &dst->name.size);
                dst->val      = src->val;
              }
            }
            
            // rjf: produce
            type = push_array(arena, E_Type, 1);
            type->kind            = kind;
            type->name            = push_str8_copy(arena, name);
            type->byte_size       = (U64)rdi_type->byte_size;
            type->count           = enum_vals_count;
            type->enum_vals       = enum_vals;
            type->direct_type_key = direct_type_key;
          }
          
          //- rjf: constructed types
          else if(RDI_TypeKind_FirstConstructed <= rdi_type->kind && rdi_type->kind <= RDI_TypeKind_LastConstructed)
          {
            // rjf: unpack direct type
            B32 direct_type_is_good = 0;
            E_TypeKey direct_type_key = zero_struct;
            U64 direct_type_byte_size = 0;
            if(rdi_type->constructed.direct_type_idx < type_node_idx)
            {
              RDI_TypeNode *direct_type_node = rdi_element_from_name_idx(rdi, TypeNodes, rdi_type->constructed.direct_type_idx);
              E_TypeKind direct_type_kind = e_type_kind_from_rdi(direct_type_node->kind);
              direct_type_key = e_type_key_ext(direct_type_kind, rdi_type->constructed.direct_type_idx, rdi_idx);
              direct_type_is_good = 1;
              direct_type_byte_size = (U64)direct_type_node->byte_size;
            }
            
            // rjf: construct based on kind
            switch(rdi_type->kind)
            {
              case RDI_TypeKind_Modifier:
              {
                E_TypeFlags flags = 0;
                if(rdi_type->flags & RDI_TypeModifierFlag_Const)
                {
                  flags |= E_TypeFlag_Const;
                }
                if(rdi_type->flags & RDI_TypeModifierFlag_Volatile)
                {
                  flags |= E_TypeFlag_Volatile;
                }
                type = push_array(arena, E_Type, 1);
                type->kind            = kind;
                type->direct_type_key = direct_type_key;
                type->byte_size       = direct_type_byte_size;
                type->flags           = flags;
              }break;
              case RDI_TypeKind_Ptr:
              case RDI_TypeKind_LRef:
              case RDI_TypeKind_RRef:
              {
                type = push_array(arena, E_Type, 1);
                type->kind            = kind;
                type->direct_type_key = direct_type_key;
                type->byte_size       = bit_size_from_arch(e_type_state->ctx->arch)/8;
              }break;
              
              case RDI_TypeKind_Array:
              {
                type = push_array(arena, E_Type, 1);
                type->kind            = kind;
                type->direct_type_key = direct_type_key;
                type->count           = rdi_type->constructed.count;
                type->byte_size       = direct_type_byte_size * type->count;
              }break;
              case RDI_TypeKind_Function:
              {
                U32 count = rdi_type->constructed.count;
                U32 idx_run_first = rdi_type->constructed.param_idx_run_first;
                U32 check_count = 0;
                U32 *idx_run = rdi_idx_run_from_first_count(rdi, idx_run_first, count, &check_count);
                if(check_count == count)
                {
                  type = push_array(arena, E_Type, 1);
                  type->kind            = kind;
                  type->byte_size       = bit_size_from_arch(e_type_state->ctx->arch)/8;
                  type->direct_type_key = direct_type_key;
                  type->count           = count;
                  type->param_type_keys = push_array_no_zero(arena, E_TypeKey, type->count);
                  for(U32 idx = 0; idx < type->count; idx += 1)
                  {
                    U32 param_type_idx = idx_run[idx];
                    if(param_type_idx < type_node_idx)
                    {
                      RDI_TypeNode *param_type_node = rdi_element_from_name_idx(rdi, TypeNodes, param_type_idx);
                      E_TypeKind param_kind = e_type_kind_from_rdi(param_type_node->kind);
                      type->param_type_keys[idx] = e_type_key_ext(param_kind, param_type_idx, rdi_idx);
                    }
                    else
                    {
                      break;
                    }
                  }
                }
              }break;
              case RDI_TypeKind_Method:
              {
                // NOTE(rjf): for methods, the `direct` type points at the owner type.
                // the return type, instead of being encoded via the `direct` type, is
                // encoded via the first parameter.
                U32 count = rdi_type->constructed.count;
                U32 idx_run_first = rdi_type->constructed.param_idx_run_first;
                U32 check_count = 0;
                U32 *idx_run = rdi_idx_run_from_first_count(rdi, idx_run_first, count, &check_count);
                if(check_count == count)
                {
                  type = push_array(arena, E_Type, 1);
                  type->kind            = kind;
                  type->byte_size       = bit_size_from_arch(e_type_state->ctx->arch)/8;
                  type->owner_type_key  = direct_type_key;
                  type->count           = count;
                  type->param_type_keys = push_array_no_zero(arena, E_TypeKey, type->count);
                  for(U32 idx = 0; idx < type->count; idx += 1)
                  {
                    U32 param_type_idx = idx_run[idx];
                    if(param_type_idx < type_node_idx)
                    {
                      RDI_TypeNode *param_type_node = rdi_element_from_name_idx(rdi, TypeNodes, param_type_idx);
                      E_TypeKind param_kind = e_type_kind_from_rdi(param_type_node->kind);
                      type->param_type_keys[idx] = e_type_key_ext(param_kind, param_type_idx, rdi_idx);
                    }
                    else
                    {
                      break;
                    }
                  }
                  if(type->count > 0)
                  {
                    type->direct_type_key = type->param_type_keys[0];
                    type->count -= 1;
                    type->param_type_keys += 1;
                  }
                }
              }break;
              case RDI_TypeKind_MemberPtr:
              {
                // rjf: unpack owner type
                E_TypeKey owner_type_key = zero_struct;
                if(rdi_type->constructed.owner_type_idx < type_node_idx)
                {
                  RDI_TypeNode *owner_type_node = rdi_element_from_name_idx(rdi, TypeNodes, rdi_type->constructed.owner_type_idx);
                  E_TypeKind owner_type_kind = e_type_kind_from_rdi(owner_type_node->kind);
                  owner_type_key = e_type_key_ext(owner_type_kind, rdi_type->constructed.owner_type_idx, rdi_idx);
                }
                type = push_array(arena, E_Type, 1);
                type->kind            = kind;
                type->byte_size       = bit_size_from_arch(e_type_state->ctx->arch)/8;
                type->owner_type_key  = owner_type_key;
                type->direct_type_key = direct_type_key;
              }break;
            }
          }
          
          //- rjf: alias types
          else if(rdi_type->kind == RDI_TypeKind_Alias)
          {
            // rjf: unpack name
            String8 name = {0};
            name.str = rdi_string_from_idx(rdi, rdi_type->user_defined.name_string_idx, &name.size);
            
            // rjf: unpack direct type
            E_TypeKey direct_type_key = zero_struct;
            U64 direct_type_byte_size = 0;
            if(rdi_type->user_defined.direct_type_idx < type_node_idx)
            {
              RDI_TypeNode *direct_type_node = rdi_element_from_name_idx(rdi, TypeNodes, rdi_type->user_defined.direct_type_idx);
              E_TypeKind direct_type_kind = e_type_kind_from_rdi(direct_type_node->kind);
              direct_type_key = e_type_key_ext(direct_type_kind, rdi_type->user_defined.direct_type_idx, rdi_idx);
              direct_type_byte_size = direct_type_node->byte_size;
            }
            
            // rjf: produce
            type = push_array(arena, E_Type, 1);
            type->kind            = kind;
            type->name            = push_str8_copy(arena, name);
            type->byte_size       = direct_type_byte_size;
            type->direct_type_key = direct_type_key;
          }
          
          //- rjf: bitfields
          else if(RDI_TypeKind_Bitfield == rdi_type->kind)
          {
            // rjf: unpack direct type
            E_TypeKey direct_type_key = zero_struct;
            U64 direct_type_byte_size = 0;
            if(rdi_type->bitfield.direct_type_idx < type_node_idx)
            {
              RDI_TypeNode *direct_type_node = rdi_element_from_name_idx(rdi, TypeNodes, rdi_type->bitfield.direct_type_idx);
              E_TypeKind direct_type_kind = e_type_kind_from_rdi(direct_type_node->kind);
              direct_type_key = e_type_key_ext(direct_type_kind, rdi_type->bitfield.direct_type_idx, rdi_idx);
              direct_type_byte_size = direct_type_node->byte_size;
            }
            
            // rjf: produce
            type = push_array(arena, E_Type, 1);
            type->kind            = kind;
            type->byte_size       = direct_type_byte_size;
            type->direct_type_key = direct_type_key;
            type->off             = (U32)rdi_type->bitfield.off;
            type->count           = (U64)rdi_type->bitfield.size;
          }
          
          //- rjf: incomplete types
          else if(RDI_TypeKind_FirstIncomplete <= rdi_type->kind && rdi_type->kind <= RDI_TypeKind_LastIncomplete)
          {
            // rjf: unpack name
            String8 name = {0};
            name.str = rdi_string_from_idx(rdi, rdi_type->user_defined.name_string_idx, &name.size);
            
            // rjf: produce
            type = push_array(arena, E_Type, 1);
            type->kind            = kind;
            type->name            = push_str8_copy(arena, name);
          }
          
        }
      }break;
      
      //- rjf: reg type keys
      case E_TypeKeyKind_Reg:
      {
        Architecture arch = (Architecture)key.u32[0];
        REGS_RegCode code = (REGS_RegCode)key.u32[1];
        REGS_Rng rng = regs_reg_code_rng_table_from_architecture(arch)[code];
        reg_byte_count = (U64)rng.byte_size;
      }goto build_reg_type;
      case E_TypeKeyKind_RegAlias:
      {
        Architecture arch = (Architecture)key.u32[0];
        REGS_AliasCode code = (REGS_AliasCode)key.u32[1];
        REGS_Slice slice = regs_alias_code_slice_table_from_architecture(arch)[code];
        reg_byte_count = (U64)slice.byte_size;
      }goto build_reg_type;
      build_reg_type:
      {
        Temp scratch = scratch_begin(&arena, 1);
        type = push_array(arena, E_Type, 1);
        type->kind       = E_TypeKind_Union;
        type->name       = push_str8f(arena, "reg_%I64u_bit", reg_byte_count*8);
        type->byte_size  = (U64)reg_byte_count;
        
        // rjf: build register type members
        E_MemberList members = {0};
        {
          // rjf: build exact-sized members
          {
            if(type->byte_size == 16)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u128");
              mem->type_key = e_type_key_basic(E_TypeKind_U128);
            }
            if(type->byte_size == 8)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u64");
              mem->type_key = e_type_key_basic(E_TypeKind_U64);
            }
            if(type->byte_size == 4)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u32");
              mem->type_key = e_type_key_basic(E_TypeKind_U32);
            }
            if(type->byte_size == 2)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u16");
              mem->type_key = e_type_key_basic(E_TypeKind_U16);
            }
            if(type->byte_size == 1)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u8");
              mem->type_key = e_type_key_basic(E_TypeKind_U8);
            }
          }
          
          // rjf: build arrays for subdivisions
          {
            if(type->byte_size > 16 && type->byte_size%16 == 0)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u128s");
              mem->type_key = e_type_key_cons(E_TypeKind_Array, e_type_key_basic(E_TypeKind_U128), reg_byte_count/16);
            }
            if(type->byte_size > 8 && type->byte_size%8 == 0)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u64s");
              mem->type_key = e_type_key_cons(E_TypeKind_Array, e_type_key_basic(E_TypeKind_U64), reg_byte_count/8);
            }
            if(type->byte_size > 4 && type->byte_size%4 == 0)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u32s");
              mem->type_key = e_type_key_cons(E_TypeKind_Array, e_type_key_basic(E_TypeKind_U32), reg_byte_count/4);
            }
            if(type->byte_size > 2 && type->byte_size%2 == 0)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u16s");
              mem->type_key = e_type_key_cons(E_TypeKind_Array, e_type_key_basic(E_TypeKind_U16), reg_byte_count/2);
            }
            if(type->byte_size > 1)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("u8s");
              mem->type_key = e_type_key_cons(E_TypeKind_Array, e_type_key_basic(E_TypeKind_U8), reg_byte_count);
            }
            if(type->byte_size > 4 && type->byte_size%4 == 0)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("f32s");
              mem->type_key = e_type_key_cons(E_TypeKind_Array, e_type_key_basic(E_TypeKind_F32), reg_byte_count/4);
            }
            if(type->byte_size > 8 && type->byte_size%8 == 0)
            {
              E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
              SLLQueuePush(members.first, members.last, n);
              members.count += 1;
              E_Member *mem = &n->v;
              mem->kind = E_MemberKind_DataField;
              mem->name = str8_lit("f64s");
              mem->type_key = e_type_key_cons(E_TypeKind_Array, e_type_key_basic(E_TypeKind_F64), reg_byte_count/8);
            }
          }
        }
        
        // rjf: commit members
        type->count = members.count;
        type->members = push_array_no_zero(arena, E_Member, members.count);
        U64 idx = 0;
        for(E_MemberNode *n = members.first; n != 0; n = n->next, idx += 1)
        {
          MemoryCopyStruct(&type->members[idx], &n->v);
        }
        
        scratch_end(scratch);
      }break;
    }
  }
  return type;
}

internal U64
e_type_byte_size_from_key(E_TypeKey key)
{
  U64 result = 0;
  switch(key.kind)
  {
    default:{}break;
    case E_TypeKeyKind_Basic:
    {
      E_TypeKind kind = (E_TypeKind)key.u32[0];
      result = e_kind_basic_byte_size_table[kind];
    }break;
    case E_TypeKeyKind_Ext:
    case E_TypeKeyKind_Cons:
    {
      Temp scratch = scratch_begin(0, 0);
      E_Type *type = e_type_from_key(scratch.arena, key);
      result = type->byte_size;
      scratch_end(scratch);
    }break;
  }
  return result;
}

internal E_TypeKey
e_type_direct_from_key(E_TypeKey key)
{
  E_TypeKey result = zero_struct;
  switch(key.kind)
  {
    default:{}break;
    case E_TypeKeyKind_Ext:
    case E_TypeKeyKind_Cons:
    {
      Temp scratch = scratch_begin(0, 0);
      E_Type *type = e_type_from_key(scratch.arena, key);
      result = type->direct_type_key;
      scratch_end(scratch);
    }break;
  }
  return result;
}

internal E_TypeKey
e_type_owner_from_key(E_TypeKey key)
{
  E_TypeKey result = zero_struct;
  switch(key.kind)
  {
    default:{}break;
    case E_TypeKeyKind_Ext:
    case E_TypeKeyKind_Cons:
    {
      Temp scratch = scratch_begin(0, 0);
      E_Type *type = e_type_from_key(scratch.arena, key);
      result = type->owner_type_key;
      scratch_end(scratch);
    }break;
  }
  return result;
}

internal E_TypeKey
e_type_ptee_from_key(E_TypeKey key)
{
  E_TypeKey result = key;
  B32 passed_ptr = 0;
  for(;;)
  {
    E_TypeKind kind = e_type_kind_from_key(result);
    result = e_type_direct_from_key(result);
    if(kind == E_TypeKind_Ptr || kind == E_TypeKind_LRef || kind == E_TypeKind_RRef)
    {
      passed_ptr = 1;
    }
    E_TypeKind next_kind = e_type_kind_from_key(result);
    if(passed_ptr &&
       next_kind != E_TypeKind_IncompleteStruct &&
       next_kind != E_TypeKind_IncompleteUnion &&
       next_kind != E_TypeKind_IncompleteEnum &&
       next_kind != E_TypeKind_IncompleteClass &&
       next_kind != E_TypeKind_Alias &&
       next_kind != E_TypeKind_Modifier)
    {
      break;
    }
    if(kind == E_TypeKind_Null)
    {
      break;
    }
  }
  return result;
}

internal E_TypeKey
e_type_unwrap_enum(E_TypeKey key)
{
  E_TypeKey result = key;
  for(B32 good = 1; good;)
  {
    E_TypeKind kind = e_type_kind_from_key(result);
    if(kind == E_TypeKind_Enum)
    {
      result = e_type_direct_from_key(result);
    }
    else
    {
      good = 0;
    }
  }
  return result;
}

internal E_TypeKey
e_type_unwrap(E_TypeKey key)
{
  E_TypeKey result = key;
  for(B32 good = 1; good;)
  {
    E_TypeKind kind = e_type_kind_from_key(result);
    if((E_TypeKind_FirstIncomplete <= kind && kind <= E_TypeKind_LastIncomplete) ||
       kind == E_TypeKind_Modifier ||
       kind == E_TypeKind_Alias)
    {
      result = e_type_direct_from_key(result);
    }
    else
    {
      good = 0;
    }
  }
  return result;
}

internal E_TypeKey
e_type_promote(E_TypeKey key)
{
  E_TypeKey result = key;
  E_TypeKind kind = e_type_kind_from_key(key);
  if(kind == E_TypeKind_Bool ||
     kind == E_TypeKind_S8 ||
     kind == E_TypeKind_S16 ||
     kind == E_TypeKind_U8 ||
     kind == E_TypeKind_U16)
  {
    result = e_type_key_basic(E_TypeKind_S32);
  }
  return result;
}

internal B32
e_type_match(E_TypeKey l, E_TypeKey r)
{
  // rjf: unpack parameters
  E_TypeKey lu = e_type_unwrap(l);
  E_TypeKey ru = e_type_unwrap(r);
  
  // rjf: exact key matches -> match
  B32 result = e_type_key_match(lu, ru);
  
  // rjf: if keys don't match, type *contents* could still match,
  // so we need to unpack the type info & compare
  if(!result)
  {
    E_TypeKind luk = e_type_kind_from_key(lu);
    E_TypeKind ruk = e_type_kind_from_key(ru);
    if(luk == ruk)
    {
      switch(luk)
      {
        default:
        {
          result = 1;
        }break;
        
        case E_TypeKind_Ptr:
        case E_TypeKind_LRef:
        case E_TypeKind_RRef:
        {
          E_TypeKey lud = e_type_direct_from_key(lu);
          E_TypeKey rud = e_type_direct_from_key(ru);
          result = e_type_match(lud, rud);
        }break;
        
        case E_TypeKind_MemberPtr:
        {
          E_TypeKey lud = e_type_direct_from_key(lu);
          E_TypeKey rud = e_type_direct_from_key(ru);
          E_TypeKey luo = e_type_owner_from_key(lu);
          E_TypeKey ruo = e_type_owner_from_key(ru);
          result = (e_type_match(lud, rud) && e_type_match(luo, ruo));
        }break;
        
        case E_TypeKind_Array:
        {
          Temp scratch = scratch_begin(0, 0);
          E_Type *lt = e_type_from_key(scratch.arena, l);
          E_Type *rt = e_type_from_key(scratch.arena, r);
          if(lt->count == rt->count && e_type_match(lt->direct_type_key, rt->direct_type_key))
          {
            result = 1;
          }
          scratch_end(scratch);
        }break;
        
        case E_TypeKind_Function:
        {
          Temp scratch = scratch_begin(0, 0);
          E_Type *lt = e_type_from_key(scratch.arena, l);
          E_Type *rt = e_type_from_key(scratch.arena, r);
          if(lt->count == rt->count && e_type_match(lt->direct_type_key, rt->direct_type_key))
          {
            B32 params_match = 1;
            E_TypeKey *lp = lt->param_type_keys;
            E_TypeKey *rp = rt->param_type_keys;
            U64 count = lt->count;
            for(U64 i = 0; i < count; i += 1, lp += 1, rp += 1)
            {
              if(!e_type_match(*lp, *rp))
              {
                params_match = 0;
                break;
              }
            }
            result = params_match;
          }
          scratch_end(scratch);
        }break;
        
        case E_TypeKind_Method:
        {
          Temp scratch = scratch_begin(0, 0);
          E_Type *lt = e_type_from_key(scratch.arena, l);
          E_Type *rt = e_type_from_key(scratch.arena, r);
          if(lt->count == rt->count &&
             e_type_match(lt->direct_type_key, rt->direct_type_key) &&
             e_type_match(lt->owner_type_key, rt->owner_type_key))
          {
            B32 params_match = 1;
            E_TypeKey *lp = lt->param_type_keys;
            E_TypeKey *rp = rt->param_type_keys;
            U64 count = lt->count;
            for(U64 i = 0; i < count; i += 1, lp += 1, rp += 1)
            {
              if(!e_type_match(*lp, *rp))
              {
                params_match = 0;
                break;
              }
            }
            result = params_match;
          }
          scratch_end(scratch);
        }break;
      }
    }
  }
  
  return result;
}

internal E_Member *
e_type_member_copy(Arena *arena, E_Member *src)
{
  E_Member *dst = push_array(arena, E_Member, 1);
  MemoryCopyStruct(dst, src);
  dst->name = push_str8_copy(arena, src->name);
  dst->inheritance_key_chain = e_type_key_list_copy(arena, &src->inheritance_key_chain);
  return dst;
}

internal int
e_type_qsort_compare_members_offset(E_Member *a, E_Member *b)
{
  int result = 0;
  if(a->off < b->off)
  {
    result = -1;
  }
  else if(a->off > b->off)
  {
    result = +1;
  }
  return result;
}

internal E_MemberArray
e_type_data_members_from_key(Arena *arena, E_TypeKey key)
{
  Temp scratch = scratch_begin(&arena, 1);
  E_TypeKind root_type_kind = e_type_kind_from_key(key);
  
  //- rjf: walk type tree; gather members list
  E_MemberList members_list = {0};
  B32 members_need_offset_sort = 0;
  {
    E_Type *root_type = e_type_from_key(scratch.arena, key);
    typedef struct Task Task;
    struct Task
    {
      Task *next;
      U64 base_off;
      E_TypeKeyList inheritance_chain;
      E_TypeKey type_key;
      E_Type *type;
    };
    Task start_task = {0, 0, {0}, key, root_type};
    Task *first_task = &start_task;
    Task *last_task = &start_task;
    for(Task *task = first_task; task != 0; task = task->next)
    {
      E_Type *type = task->type;
      if(type->members != 0)
      {
        U64 last_member_off = 0;
        for(U64 member_idx = 0; member_idx < type->count; member_idx += 1)
        {
          if(type->members[member_idx].kind == E_MemberKind_DataField)
          {
            E_MemberNode *n = push_array(scratch.arena, E_MemberNode, 1);
            MemoryCopyStruct(&n->v, &type->members[member_idx]);
            n->v.off += task->base_off;
            n->v.inheritance_key_chain = task->inheritance_chain;
            SLLQueuePush(members_list.first, members_list.last, n);
            members_list.count += 1;
            members_need_offset_sort = members_need_offset_sort || (n->v.off < last_member_off);
            last_member_off = n->v.off;
          }
          else if(type->members[member_idx].kind == E_MemberKind_Base)
          {
            Task *t = push_array(scratch.arena, Task, 1);
            t->base_off = type->members[member_idx].off + task->base_off;
            t->inheritance_chain = e_type_key_list_copy(scratch.arena, &task->inheritance_chain);
            e_type_key_list_push(scratch.arena, &t->inheritance_chain, type->members[member_idx].type_key);
            t->type_key = type->members[member_idx].type_key;
            t->type = e_type_from_key(scratch.arena, type->members[member_idx].type_key);
            SLLQueuePush(first_task, last_task, t);
            members_need_offset_sort = 1;
          }
        }
      }
    }
  }
  
  //- rjf: convert to array
  E_MemberArray members = {0};
  {
    members.count = members_list.count;
    members.v = push_array(arena, E_Member, members.count);
    U64 idx = 0;
    for(E_MemberNode *n = members_list.first; n != 0; n = n->next)
    {
      MemoryCopyStruct(&members.v[idx], &n->v);
      members.v[idx].name = push_str8_copy(arena, members.v[idx].name);
      members.v[idx].inheritance_key_chain = e_type_key_list_copy(arena, &members.v[idx].inheritance_key_chain);
      idx += 1;
    }
  }
  
  //- rjf: sort array by offset if needed
  if(members_need_offset_sort)
  {
    quick_sort(members.v, members.count, sizeof(E_Member), e_type_qsort_compare_members_offset);
  }
  
  //- rjf: find all padding instances
  typedef struct PaddingNode PaddingNode;
  struct PaddingNode
  {
    PaddingNode *next;
    U64 off;
    U64 size;
    U64 prev_member_idx;
  };
  PaddingNode *first_padding = 0;
  PaddingNode *last_padding = 0;
  U64 padding_count = 0;
  if(root_type_kind == E_TypeKind_Struct || root_type_kind == E_TypeKind_Class)
  {
    for(U64 idx = 0; idx < members.count; idx += 1)
    {
      E_Member *member = &members.v[idx];
      if(idx+1 < members.count)
      {
        U64 member_byte_size = e_type_byte_size_from_key(member->type_key);
        Rng1U64 member_byte_range = r1u64(member->off, member->off + member_byte_size);
        if(member[1].off > member_byte_range.max)
        {
          PaddingNode *n = push_array(scratch.arena, PaddingNode, 1);
          SLLQueuePush(first_padding, last_padding, n);
          n->off = member_byte_range.max;
          n->size = member[1].off - member_byte_range.max;
          n->prev_member_idx = idx;
          padding_count += 1;
        }
      }
    }
  }
  
  //- rjf: produce new members array, if we have any padding
  if(padding_count != 0)
  {
    E_MemberArray new_members = {0};
    new_members.count = members.count + padding_count;
    new_members.v = push_array(arena, E_Member, new_members.count);
    MemoryCopy(new_members.v, members.v, sizeof(E_Member)*members.count);
    U64 padding_idx = 0;
    for(PaddingNode *n = first_padding; n != 0; n = n->next)
    {
      if(members.count+padding_idx > n->prev_member_idx+1)
      {
        MemoryCopy(new_members.v + n->prev_member_idx + padding_idx + 2,
                   new_members.v + n->prev_member_idx + padding_idx + 1,
                   sizeof(E_Member) * (members.count + padding_idx - (n->prev_member_idx + padding_idx + 1)));
      }
      E_Member *padding_member = &new_members.v[n->prev_member_idx+padding_idx+1];
      MemoryZeroStruct(padding_member);
      padding_member->kind = E_MemberKind_Padding;
      padding_member->type_key = e_type_key_cons(E_TypeKind_Array, e_type_key_basic(E_TypeKind_U8), n->size);
      padding_member->off = n->off;
      padding_member->name = str8_lit("padding");
      padding_idx += 1;
    }
    members = new_members;
  }
  
  scratch_end(scratch);
  return members;
}

internal void
e_type_lhs_string_from_key(Arena *arena, E_TypeKey key, String8List *out, U32 prec, B32 skip_return)
{
  String8 keyword = {0};
  E_TypeKind kind = e_type_kind_from_key(key);
  switch(kind)
  {
    default:
    {
      Temp scratch = scratch_begin(&arena, 1);
      E_Type *type = e_type_from_key(scratch.arena, key);
      str8_list_push(arena, out, push_str8_copy(arena, type->name));
      str8_list_push(arena, out, str8_lit(" "));
      scratch_end(scratch);
    }break;
    
    case E_TypeKind_Bitfield:
    {
      Temp scratch = scratch_begin(&arena, 1);
      E_Type *type = e_type_from_key(scratch.arena, key);
      e_type_lhs_string_from_key(arena, type->direct_type_key, out, prec, skip_return);
      str8_list_pushf(arena, out, ": %I64u", type->count);
      scratch_end(scratch);
    }break;
    
    case E_TypeKind_Modifier:
    {
      Temp scratch = scratch_begin(&arena, 1);
      E_Type *type = e_type_from_key(scratch.arena, key);
      E_TypeKey direct = type->direct_type_key;
      e_type_lhs_string_from_key(arena, direct, out, 1, skip_return);
      if(type->flags & E_TypeFlag_Const)
      {
        str8_list_push(arena, out, str8_lit("const "));
      }
      if(type->flags & E_TypeFlag_Volatile)
      {
        str8_list_push(arena, out, str8_lit("volatile "));
      }
      scratch_end(scratch);
    }break;
    
    case E_TypeKind_Variadic:
    {
      str8_list_push(arena, out, str8_lit("..."));
    }break;
    
    case E_TypeKind_Struct:
    case E_TypeKind_Union:
    case E_TypeKind_Enum:
    case E_TypeKind_Class:
    case E_TypeKind_Alias:
    {
      Temp scratch = scratch_begin(&arena, 1);
      E_Type *type = e_type_from_key(scratch.arena, key);
      str8_list_push(arena, out, push_str8_copy(arena, type->name));
      str8_list_push(arena, out, str8_lit(" "));
      scratch_end(scratch);
    }break;
    
    case E_TypeKind_IncompleteStruct: keyword = str8_lit("struct"); goto fwd_udt;
    case E_TypeKind_IncompleteUnion:  keyword = str8_lit("union"); goto fwd_udt;
    case E_TypeKind_IncompleteEnum:   keyword = str8_lit("enum"); goto fwd_udt;
    case E_TypeKind_IncompleteClass:  keyword = str8_lit("class"); goto fwd_udt;
    fwd_udt:;
    {
      Temp scratch = scratch_begin(&arena, 1);
      E_Type *type = e_type_from_key(scratch.arena, key);
      str8_list_push(arena, out, keyword);
      str8_list_push(arena, out, str8_lit(" "));
      str8_list_push(arena, out, push_str8_copy(arena, type->name));
      str8_list_push(arena, out, str8_lit(" "));
      scratch_end(scratch);
    }break;
    
    case E_TypeKind_Array:
    {
      E_TypeKey direct = e_type_direct_from_key(key);
      e_type_lhs_string_from_key(arena, direct, out, 2, skip_return);
      if(prec == 1)
      {
        str8_list_push(arena, out, str8_lit("("));
      }
    }break;
    
    case E_TypeKind_Function:
    {
      if(!skip_return)
      {
        E_TypeKey direct = e_type_direct_from_key(key);
        e_type_lhs_string_from_key(arena, direct, out, 2, 0);
      }
      if(prec == 1)
      {
        str8_list_push(arena, out, str8_lit("("));
      }
    }break;
    
    case E_TypeKind_Ptr:
    {
      E_TypeKey direct = e_type_direct_from_key(key);
      e_type_lhs_string_from_key(arena, direct, out, 1, skip_return);
      str8_list_push(arena, out, str8_lit("*"));
    }break;
    
    case E_TypeKind_LRef:
    {
      E_TypeKey direct = e_type_direct_from_key(key);
      e_type_lhs_string_from_key(arena, direct, out, 1, skip_return);
      str8_list_push(arena, out, str8_lit("&"));
    }break;
    
    case E_TypeKind_RRef:
    {
      E_TypeKey direct = e_type_direct_from_key(key);
      e_type_lhs_string_from_key(arena, direct, out, 1, skip_return);
      str8_list_push(arena, out, str8_lit("&&"));
    }break;
    
    case E_TypeKind_MemberPtr:
    {
      Temp scratch = scratch_begin(&arena, 1);
      E_Type *type = e_type_from_key(scratch.arena, key);
      E_TypeKey direct = type->direct_type_key;
      e_type_lhs_string_from_key(arena, direct, out, 1, skip_return);
      E_Type *container = e_type_from_key(scratch.arena, type->owner_type_key);
      if(container->kind != E_TypeKind_Null)
      {
        str8_list_push(arena, out, push_str8_copy(arena, container->name));
      }
      else
      {
        str8_list_push(arena, out, str8_lit("<unknown-class>"));
      }
      str8_list_push(arena, out, str8_lit("::*"));
      scratch_end(scratch);
    }break;
  }
}

internal void
e_type_rhs_string_from_key(Arena *arena, E_TypeKey key, String8List *out, U32 prec)
{
  E_TypeKind kind = e_type_kind_from_key(key);
  switch(kind)
  {
    default:{}break;
    
    case E_TypeKind_Bitfield:
    {
      E_TypeKey direct = e_type_direct_from_key(key);
      e_type_rhs_string_from_key(arena, direct, out, prec);
    }break;
    
    case E_TypeKind_Modifier:
    case E_TypeKind_Ptr:
    case E_TypeKind_LRef:
    case E_TypeKind_RRef:
    case E_TypeKind_MemberPtr:
    {
      E_TypeKey direct = e_type_direct_from_key(key);
      e_type_rhs_string_from_key(arena, direct, out, 1);
    }break;
    
    case E_TypeKind_Array:
    {
      Temp scratch = scratch_begin(&arena, 1);
      E_Type *type = e_type_from_key(scratch.arena, key);
      if(prec == 1)
      {
        str8_list_push(arena, out, str8_lit(")"));
      }
      String8 count_str = str8_from_u64(arena, type->count, 10, 0, 0);
      str8_list_push(arena, out, str8_lit("["));
      str8_list_push(arena, out, count_str);
      str8_list_push(arena, out, str8_lit("]"));
      E_TypeKey direct = e_type_direct_from_key(key);
      e_type_rhs_string_from_key(arena, direct, out, 2);
      scratch_end(scratch);
    }break;
    
    case E_TypeKind_Function:
    {
      Temp scratch = scratch_begin(&arena, 1);
      E_Type *type = e_type_from_key(scratch.arena, key);
      if(prec == 1)
      {
        str8_list_push(arena, out, str8_lit(")"));
      }
      if(type->count == 0)
      {
        str8_list_push(arena, out, str8_lit("(void)"));
      }
      else
      {
        str8_list_push(arena, out, str8_lit("("));
        U64 param_count = type->count;
        E_TypeKey *param_type_keys = type->param_type_keys;
        for(U64 param_idx = 0; param_idx < param_count; param_idx += 1)
        {
          E_TypeKey param_type_key = param_type_keys[param_idx];
          String8 param_str = e_type_string_from_key(arena, param_type_key);
          String8 param_str_trimmed = str8_skip_chop_whitespace(param_str);
          str8_list_push(arena, out, param_str_trimmed);
          if(param_idx+1 < param_count)
          {
            str8_list_push(arena, out, str8_lit(", "));
          }
        }
        str8_list_push(arena, out, str8_lit(")"));
      }
      E_TypeKey direct = e_type_direct_from_key(key);
      e_type_rhs_string_from_key(arena, direct, out, 2);
      scratch_end(scratch);
    }break;
  }
}

internal String8
e_type_string_from_key(Arena *arena, E_TypeKey key)
{
  Temp scratch = scratch_begin(&arena, 1);
  String8List list = {0};
  e_type_lhs_string_from_key(scratch.arena, key, &list, 0, 0);
  e_type_rhs_string_from_key(scratch.arena, key, &list, 0);
  String8 result = str8_list_join(arena, &list, 0);
  scratch_end(scratch);
  return result;
}

//- rjf: type key data structures

internal void
e_type_key_list_push(Arena *arena, E_TypeKeyList *list, E_TypeKey key)
{
  E_TypeKeyNode *n = push_array(arena, E_TypeKeyNode, 1);
  n->v = key;
  SLLQueuePush(list->first, list->last, n);
  list->count += 1;
}

internal E_TypeKeyList
e_type_key_list_copy(Arena *arena, E_TypeKeyList *src)
{
  E_TypeKeyList dst = {0};
  for(E_TypeKeyNode *n = src->first; n != 0; n = n->next)
  {
    e_type_key_list_push(arena, &dst, n->v);
  }
  return dst;
}
