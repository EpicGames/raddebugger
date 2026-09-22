// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef X64_EXCEPTIONS_H
#define X64_EXCEPTIONS_H

////////////////////////////////
//~ Exception Codes

typedef enum X64_ExceptionCode
{
  X64_ExceptionCode_DE     = 0,  // Divide error: division by zero or quotient overflow
  X64_ExceptionCode_DB     = 1,  // Debug exception (single stepping, hardware breakpoint)
  X64_ExceptionCode_NMI    = 2,  // Non-maskable interrupt
  X64_ExceptionCode_BP     = 3,  // Breakpoint (int3)
  X64_ExceptionCode_OF     = 4,  // Overflow
  X64_ExceptionCode_BR     = 5,  // Bound range exceeded
  X64_ExceptionCode_UD     = 6,  // Invalid opcode
  X64_ExceptionCode_NM     = 7,  // Device not available
  X64_ExceptionCode_DF     = 8,  // Double fault
  X64_ExceptionCode_9      = 9,  // Coprocessor segment overrun (obsolete)
  X64_ExceptionCode_TS     = 10, // Invalid task-state segment
  X64_ExceptionCode_NP     = 11, // Segment not present
  X64_ExceptionCode_SS     = 12, // Stack-segment fault
  X64_ExceptionCode_GP     = 13, // General-protection fault
  X64_ExceptionCode_PF     = 14, // Page fault
  X64_ExceptionCode_15     = 15, // Reserved; Linux names it X86_TRAP_SPURIOUS
  X64_ExceptionCode_MF     = 16, // x87 floating-point exception
  X64_ExceptionCode_AC     = 17, // Alignment check
  X64_ExceptionCode_MC     = 18, // Machine check
  X64_ExceptionCode_XF     = 19, // SIMD floating-point exception
  X64_ExceptionCode_VE     = 20, // Virtualization exception
  X64_ExceptionCode_CP     = 21, // Control-protection exception
  X64_ExceptionCode_Amd_HV = 28, // Hypervisor injection exception
  X64_ExceptionCode_Amd_VC = 29, // VMM communication exception
  X64_ExceptionCode_Amd_SX = 30, // Security exception
}
X64_ExceptionCode;

////////////////////////////////
//~ Page Fault Flags

typedef enum X64_PageFaultError
{
  X64_PageFaultError_Protection       = (1u << 0),
  X64_PageFaultError_Write            = (1u << 1),
  X64_PageFaultError_User             = (1u << 2),
  X64_PageFaultError_ReservedBit      = (1u << 3),
  X64_PageFaultError_InstructionFetch = (1u << 4),
  X64_PageFaultError_ProtectionKey    = (1u << 5),
  X64_PageFaultError_ShadowStack      = (1u << 6),
}
X64_PageFaultError;

#endif // X64_EXCEPTIONS_H
