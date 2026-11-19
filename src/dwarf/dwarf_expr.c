// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Expression Evaluation Functions

internal DW_Eval
dw_eval(Arena *arena, DW_Format fmt, Arch arch, MemoryMap *memory_map, void *regs, U64 frame_base, U64 cfa, U64 tls, DW_EvalState *state, String8 expr, U64 op_idx_cap)
{
  DW_Eval result = {0};
  ARCH_Info *arch_info = arch_info_from_arch(arch);
  B32 need_a_break = 0;
  B32 done = 0;
  for(;state->off < expr.size && !need_a_break && !done;)
  {
    U64 start_off = state->off;
    U64 off = state->off;
    
    //- rjf: too many ops? -> error
    if(state->op_idx >= op_idx_cap)
    {
      result.status = DW_EvalStatus_ExecOpLimitReached;
      break;
    }
    
    //- rjf: read next opcode
    DW_ExprOp opcode = 0;
    off += str8_deserial_read_struct(expr, off, &opcode);
    
    //- rjf: unpack opcode
    U8 push_count = 0;
    U8 pop_count = 0;
    U8 operand_count = 0;
    DW_ExprOperandKind operand_kinds[2] = {DW_ExprOperandKind_Null};
    {
      DW_ExprOpInfo *op_info = dw_info_from_expr_op(DW_Version_5, DW_ExtFlag_All, opcode);
      push_count = op_info->push_count;
      pop_count = op_info->pop_count;
      operand_count = op_info->operand_count;
      MemoryCopy(operand_kinds, op_info->operand_kinds, sizeof(operand_kinds));
    }
    
    //- rjf: read operands
    DW_EvalVal operands[2] = {0};
    for(U8 operand_idx = 0; operand_idx < operand_count; operand_idx += 1)
    {
      U64 fixed_bytes_to_read = 0;
      switch(operand_kinds[operand_idx])
      {
        default:{}break;
        case DW_ExprOperandKind_U8:       {fixed_bytes_to_read = 1;}goto fixed_read;
        case DW_ExprOperandKind_U16:      {fixed_bytes_to_read = 2;}goto fixed_read;
        case DW_ExprOperandKind_U32:      {fixed_bytes_to_read = 4;}goto fixed_read;
        case DW_ExprOperandKind_U64:      {fixed_bytes_to_read = 8;}goto fixed_read;
        case DW_ExprOperandKind_S8:       {fixed_bytes_to_read = 1;}goto fixed_read;
        case DW_ExprOperandKind_S16:      {fixed_bytes_to_read = 2;}goto fixed_read;
        case DW_ExprOperandKind_S32:      {fixed_bytes_to_read = 4;}goto fixed_read;
        case DW_ExprOperandKind_S64:      {fixed_bytes_to_read = 8;}goto fixed_read;
        case DW_ExprOperandKind_Addr:     {fixed_bytes_to_read = byte_size_from_arch(arch);}goto fixed_read;
        case DW_ExprOperandKind_DwarfUInt:{fixed_bytes_to_read = dw_addr_size_from_format(fmt);}goto fixed_read;
        fixed_read:;
        {
          off += str8_deserial_read(expr, off, &operands[operand_idx].u512.u64[0], fixed_bytes_to_read, fixed_bytes_to_read);
        }break;
        case DW_ExprOperandKind_ULEB128:
        {
          off += str8_deserial_read_uleb128(expr, off, &operands[operand_idx].u512.u64[0]);
        }break;
        case DW_ExprOperandKind_SLEB128:
        {
          off += str8_deserial_read_sleb128(expr, off, &operands[operand_idx].s64);
        }break;
        case DW_ExprOperandKind_Block:
        {
          U8 block_size = 0;
          off += str8_deserial_read_struct(expr, off, &block_size);
          operands[operand_idx].data = str8_prefix(str8_skip(expr, off), block_size);
        }break;
      }
    }
    
    //- rjf: do pops
    DW_EvalVal popped_vals[3] = {0};
    {
      pop_count = Min(pop_count, ArrayCount(popped_vals));
      for(U8 pop_idx = 0; pop_idx < pop_count; pop_idx += 1)
      {
        if(state->top_val != 0)
        {
          DW_EvalValNode *popped = state->top_val;
          SLLStackPop(state->top_val);
          popped_vals[pop_idx] = popped->v;
          SLLStackPush(state->free_val, popped);
        }
      }
    }
    
    //- rjf: apply opcode
    DW_EvalVal push_vals[3] = {0};
    U64 pick_target_idx = 0;
    U64 deref_read_size = 0;
    DW_RegCode reg_code_dw = 0;
    switch(opcode)
    {
      //- rjf: unsuported ops
      case DW_ExprOp_Call2:             // DWARF function calls
      case DW_ExprOp_Call4:             // DWARF function calls
      case DW_ExprOp_CallRef:           // DWARF function calls
      case DW_ExprOp_Addrx:             // address in .debug_addr; not currently present for us, need to find how to preserve if we care
      case DW_ExprOp_Constx:            // address in .debug_addr; not currently present for us, need to find how to preserve if we care
      case DW_ExprOp_EntryValue:        // encodes a sub-expression as a data block - need to recursively evaluate, lol
      case DW_ExprOp_ConstType:         // pushes a constant - but refers into DWARF debug info to specify the type. we don't have that, need to figure out how to preserve mapping from DW type ID -> RDI type, if we care
      case DW_ExprOp_DerefType:         // same as above - refers to DWARF debug info
      case DW_ExprOp_RegvalType:        // same as above - refers to DWARF debug info
      case DW_ExprOp_Piece:             // pieces - used to stitch multiple sub-values together
      case DW_ExprOp_BitPiece:          // pieces - used to stitch multiple sub-values together
      case DW_ExprOp_XDeref:            // two entries popped from stack - second one is 'address space identifier'. not possible in this layer right now
      case DW_ExprOp_XDerefSize:        // does multiple address spaces
      case DW_ExprOp_XDerefType:        // refers to DWARF debug info, *and* does multiple address spaces
      case DW_ExprOp_PushObjectAddress: // pushes "the address of the object currently being evaluated" ???
      case DW_ExprOp_ImplicitPointer:   // refers to DWARF debug info
      default:
      {
        done = 1;
        result.status = DW_EvalStatus_UnsupportedOp;
      }break;
      
      //- rjf: address ops
      case DW_ExprOp_Deref:{deref_read_size = byte_size_from_arch(arch);}goto deref;
      case DW_ExprOp_DerefSize:{deref_read_size = operands[0].u512.u64[0];}goto deref;
      deref:;
      {
        U64 vaddr = popped_vals[0].u512.u64[0];
        U64 read_size = Min(deref_read_size, sizeof(push_vals[0].u512));
        Rng1U64 read_vaddr_range = r1u64(vaddr, vaddr+read_size);
        if(!memory_map_read(memory_map, read_vaddr_range, &push_vals[0].u512))
        {
          need_a_break = 1;
          result.status = DW_EvalStatus_FailedMemoryRead;
          result.missed_read_vaddr_range = read_vaddr_range;
        }
      }break;
      
      //- rjf: generic constants
      case DW_ExprOp_Const1U: case DW_ExprOp_Const1S:
      case DW_ExprOp_Const2U: case DW_ExprOp_Const2S:
      case DW_ExprOp_Const4U: case DW_ExprOp_Const4S:
      case DW_ExprOp_Const8U: case DW_ExprOp_Const8S:
      case DW_ExprOp_ConstU: case DW_ExprOp_ConstS:
      case DW_ExprOp_Addr:
      {
        push_vals[0] = operands[0];
      }break;
      
      //- rjf: stack ops
      case DW_ExprOp_Dup:{push_vals[0] = popped_vals[0];}break;
      case DW_ExprOp_Drop:{/* NOTE(rjf): nothing to do here - just pops */}break;
      case DW_ExprOp_Swap:{push_vals[0] = popped_vals[1]; push_vals[1] = popped_vals[0];}break;
      case DW_ExprOp_Rot: {push_vals[0] = popped_vals[0]; push_vals[1] = popped_vals[1]; push_vals[2] = popped_vals[2];}break;
      case DW_ExprOp_Over:{pick_target_idx = 1;}break;
      case DW_ExprOp_Pick:{pick_target_idx = operands[0].u512.u64[0];}goto pick_stack_val;
      pick_stack_val:;
      {
        U64 idx = 0;
        for(DW_EvalValNode *n = state->top_val; n != 0; n = n->next, idx += 1)
        {
          if(idx == pick_target_idx)
          {
            push_vals[0] = n->v;
            break;
          }
        }
      }break;
      
      //- rjf: arithmetic ops
#define UniOpU(k, op)   case k:{push_vals[0].u512.u64[0] = op(popped_vals[0].u512.u64[0]);}break
#define UniOpS(k, op)   case k:{push_vals[0].s64 = op(popped_vals[0].s64);}break
#define BinOpS(k, op)   case k:{push_vals[0].s64 = popped_vals[1].s64 op popped_vals[0].s64;}break
#define BinOpU(k, op)   case k:{push_vals[0].u512.u64[0] = popped_vals[1].u512.u64[0] op popped_vals[0].u512.u64[0];}break
#define BinOpDiv(k, op) case k:if(popped_vals[0].u512.u64[0] != 0){push_vals[0].u512.u64[0] = popped_vals[1].u512.u64[0] op popped_vals[0].u512.u64[0];}break
      UniOpS(DW_ExprOp_Abs,   abs_s64);
      UniOpS(DW_ExprOp_Neg,   -);
      UniOpU(DW_ExprOp_Not,   !);
      BinOpU(DW_ExprOp_Minus, -);
      BinOpU(DW_ExprOp_Mul,   *);
      BinOpU(DW_ExprOp_Plus,  +);
      BinOpU(DW_ExprOp_Shl,   <<);
      BinOpU(DW_ExprOp_Shr,   >>);
      BinOpU(DW_ExprOp_And,   &);
      BinOpU(DW_ExprOp_Or,    |);
      BinOpU(DW_ExprOp_Xor,   ^);
      BinOpS(DW_ExprOp_Shra,  >>);
      BinOpDiv(DW_ExprOp_Div, /);
      BinOpDiv(DW_ExprOp_Mod, %);
      BinOpU(DW_ExprOp_Eq,    ==);
      BinOpU(DW_ExprOp_Ne,    !=);
      BinOpS(DW_ExprOp_Ge,    >=);
      BinOpS(DW_ExprOp_Gt,    >);
      BinOpS(DW_ExprOp_Le,    <=);
      BinOpS(DW_ExprOp_Lt,    <);
#undef UniOpU
#undef UniOpS
#undef BinOpS
#undef BinOpU
#undef BinOpDiv
      case DW_ExprOp_PlusUConst:
      {
        push_vals[0].u512.u64[0] = popped_vals[0].u512.u64[0] + operands[0].u512.u64[0];
      }break;
      
      //- rjf: jumps/branches
      case DW_ExprOp_Bra:
      {
        if(popped_vals[0].u512.u64[0] == 0)
        {
          break;
        }
      } // fallthrough
      case DW_ExprOp_Skip:
      {
        S64 jump_delta = operands[0].s64;
        if((jump_delta < 0 && -jump_delta > off) || (off + jump_delta >= expr.size))
        {
          done = 1;
          result.status = DW_EvalStatus_Error;
        }
        else
        {
          off += jump_delta;
        }
      }break;
      
      //- rjf: literals
      case DW_ExprOp_Lit0: case DW_ExprOp_Lit1: case DW_ExprOp_Lit2: case DW_ExprOp_Lit3:
      case DW_ExprOp_Lit4: case DW_ExprOp_Lit5: case DW_ExprOp_Lit6: case DW_ExprOp_Lit7:
      case DW_ExprOp_Lit8: case DW_ExprOp_Lit9: case DW_ExprOp_Lit10: case DW_ExprOp_Lit11:
      case DW_ExprOp_Lit12: case DW_ExprOp_Lit13: case DW_ExprOp_Lit14: case DW_ExprOp_Lit15:
      case DW_ExprOp_Lit16: case DW_ExprOp_Lit17: case DW_ExprOp_Lit18: case DW_ExprOp_Lit19:
      case DW_ExprOp_Lit20: case DW_ExprOp_Lit21: case DW_ExprOp_Lit22: case DW_ExprOp_Lit23:
      case DW_ExprOp_Lit24: case DW_ExprOp_Lit25: case DW_ExprOp_Lit26: case DW_ExprOp_Lit27:
      case DW_ExprOp_Lit28: case DW_ExprOp_Lit29: case DW_ExprOp_Lit30: case DW_ExprOp_Lit31:
      {
        U8 val = (opcode - DW_ExprOp_Lit0);
        push_vals[0].u512.u8[0] = val;
      }break;
      
      //- rjf: register reads (opcode-encoded reg)
      case DW_ExprOp_Reg0: case DW_ExprOp_Reg1: case DW_ExprOp_Reg2: case DW_ExprOp_Reg3:
      case DW_ExprOp_Reg4: case DW_ExprOp_Reg5: case DW_ExprOp_Reg6: case DW_ExprOp_Reg7:
      case DW_ExprOp_Reg8: case DW_ExprOp_Reg9: case DW_ExprOp_Reg10: case DW_ExprOp_Reg11:
      case DW_ExprOp_Reg12: case DW_ExprOp_Reg13: case DW_ExprOp_Reg14: case DW_ExprOp_Reg15:
      case DW_ExprOp_Reg16: case DW_ExprOp_Reg17: case DW_ExprOp_Reg18: case DW_ExprOp_Reg19:
      case DW_ExprOp_Reg20: case DW_ExprOp_Reg21: case DW_ExprOp_Reg22: case DW_ExprOp_Reg23:
      case DW_ExprOp_Reg24: case DW_ExprOp_Reg25: case DW_ExprOp_Reg26: case DW_ExprOp_Reg27:
      case DW_ExprOp_Reg28: case DW_ExprOp_Reg29: case DW_ExprOp_Reg30: case DW_ExprOp_Reg31:
      {reg_code_dw = (opcode - DW_ExprOp_Reg0);}goto reg_read;
      case DW_ExprOp_RegX:
      {reg_code_dw = operands[0].u512.u64[0];}goto reg_read;
      reg_read:;
      {
        ARCH_RegCode reg_code = arch_reg_code_from_dw(arch, reg_code_dw);
        Rng1U16 reg_rng = arch_info->reg_code_rng_table[reg_code];
        U64 read_size = dim_1u16(reg_rng);
        read_size = Min(read_size, sizeof(push_vals[0].u512));
        reg_rng.max = reg_rng.min + read_size;
        arch_reg_block_read_range(arch_info, regs, reg_rng, &push_vals[0].u512);
      }break;
      
      //- rjf: register reads (opcode-encoded reg + offset)
      case DW_ExprOp_BReg0: case DW_ExprOp_BReg1: case DW_ExprOp_BReg2: case DW_ExprOp_BReg3:
      case DW_ExprOp_BReg4: case DW_ExprOp_BReg5: case DW_ExprOp_BReg6: case DW_ExprOp_BReg7:
      case DW_ExprOp_BReg8: case DW_ExprOp_BReg9: case DW_ExprOp_BReg10: case DW_ExprOp_BReg11:
      case DW_ExprOp_BReg12: case DW_ExprOp_BReg13: case DW_ExprOp_BReg14: case DW_ExprOp_BReg15:
      case DW_ExprOp_BReg16: case DW_ExprOp_BReg17: case DW_ExprOp_BReg18: case DW_ExprOp_BReg19:
      case DW_ExprOp_BReg20: case DW_ExprOp_BReg21: case DW_ExprOp_BReg22: case DW_ExprOp_BReg23:
      case DW_ExprOp_BReg24: case DW_ExprOp_BReg25: case DW_ExprOp_BReg26: case DW_ExprOp_BReg27:
      case DW_ExprOp_BReg28: case DW_ExprOp_BReg29: case DW_ExprOp_BReg30: case DW_ExprOp_BReg31:
      {reg_code_dw = (opcode - DW_ExprOp_BReg0);}goto based_reg_read;
      case DW_ExprOp_BRegX:
      {reg_code_dw = operands[0].u512.u64[0];}goto based_reg_read;
      based_reg_read:;
      {
        ARCH_RegCode reg_code = arch_reg_code_from_dw(arch, reg_code_dw);
        Rng1U16 reg_rng = arch_info->reg_code_rng_table[reg_code];
        U64 read_size = dim_1u16(reg_rng);
        read_size = Min(read_size, sizeof(push_vals[0].u512));
        reg_rng.max = reg_rng.min + read_size;
        arch_reg_block_read_range(arch_info, regs, reg_rng, &push_vals[0].u512);
        push_vals[0].u512.u64[0] += operands[opcode == DW_ExprOp_BRegX ? 1 : 0].s64;
      }break;
      
      //- rjf: frame base register read
      case DW_ExprOp_FBReg:
      {
        push_vals[0].u512.u64[0] = (U64)(frame_base + operands[0].s64);
      }break;
      
      //- rjf: TLS
      case DW_ExprOp_FormTlsAddress:
      {
        U64 tls_off = popped_vals[0].u512.u64[0];
        U64 tls_addr = tls + tls_off;
        push_vals[0].u512.u64[0] = tls_addr;
      }break;
      
      //- rjf: call frame CFA
      case DW_ExprOp_CallFrameCfa:
      {
        push_vals[0].u512.u64[0] = cfa;
      }break;
      
      //- rjf: implicit location descriptions (value is known, location is not)
      case DW_ExprOp_ImplicitValue:
      {
        push_vals[0] = operands[0];
      }break;
      case DW_ExprOp_StackValue:
      {
        // NOTE(rjf): nothing to do here - program is over, value (but not location) has
        // been evaluated and exists as the top entry on the stack.
      }break;
      
      //- rjf: conversions
      case DW_ExprOp_Convert:
      case DW_ExprOp_Reinterpret:
      {
        if(operands[0].u512.u64[0] == 0)
        {
          push_vals[0] = popped_vals[0];
        }
        else
        {
          // NOTE(rjf): with a nonzero operand, it is expecting us to read into the
          // DWARF debug info and push as that base type. we do not have access to that
          // in the general case here, and we need to figure out how to preserve that info
          // if we care.
          done = 1;
          result.status = DW_EvalStatus_UnsupportedOp;
        }
      }break;
    }
    
    //- rjf: do pushes
    if(!done && !need_a_break)
    {
      push_count = Min(push_count, ArrayCount(push_vals));
      for(U8 push_idx = 0; push_idx < push_count; push_idx += 1)
      {
        DW_EvalValNode *n = state->free_val;
        if(n != 0)
        {
          SLLStackPop(state->free_val);
        }
        else
        {
          n = push_array(arena, DW_EvalValNode, 1);
        }
        n->v = push_vals[push_idx];
        SLLStackPush(state->top_val, n);
      }
    }
    
    //- rjf: do not need a break? advance state.
    //
    // (if we *do* need a break, this means we need to ask the caller for
    // more info, like memory reads, and so we want to resume from the
    // offset just before this opcode)
    //
    if(!need_a_break)
    {
      state->op_idx += 1;
      state->off = off;
    }
    
    //- rjf: at the end of the expression? -> set success, fill result
    if(!need_a_break && !done && off == expr.size)
    {
      result.status = DW_EvalStatus_Good;
      if(state->top_val != 0)
      {
        result.val = state->top_val->v;
      }
    }
    
    //- rjf: did not advance? -> error
    if(off == start_off)
    {
      result.status = DW_EvalStatus_Error;
      break;
    }
  }
  return result;
}
