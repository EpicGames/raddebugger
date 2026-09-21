// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#define BUILD_CONSOLE_INTERFACE 1
#define BUILD_TITLE "The RAD Debugger (Command Line Launcher)"
#include "base/base_inc.h"
#include "base/base_inc.c"

internal void
entry_point(CmdLine *cmd_line)
{
  Temp scratch = scratch_begin(0, 0);
  B32 wait_for_process = cmd_line_has_flag(cmd_line, s("cli")) || cmd_line_has_flag(cmd_line, s("bin"));
  String8List command_parts = {0};
  String8 exe_name = str8f(scratch.arena, "%S.exe", str8_chop_last_dot(cmd_line->exe_name));
  if(str8_match(exe_name, cmd_line->exe_name, 0))
  {
    fprintf(stderr,
            "ERROR: Attempting to launch %.*s recursively.\n"
            "\n"
            "This executable needs to be used with a .com extension next to the real target\n"
            "executable, with the same name, but with a .exe extension. This guarantees that\n"
            "cmd.exe will prefer launching this .com file when the executable name is used,\n"
            "allowing the same executable to be used in the Windows command-line subsystem\n"
            "style when launched from the command line, but used graphically when launched\n"
            "via Explorer.\n\n", str8_varg(exe_name));
    fflush(stderr);
  }
  else
  {
    // NOTE(rjf): sub-processes must be killed if the parent process
    // is killed, if we're waiting for it
    if(wait_for_process)
    {
#if OS_WINDOWS
      HANDLE job_handle = CreateJobObjectA(0, 0);
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = { .BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE };
      SetInformationJobObject(job_handle, JobObjectExtendedLimitInformation, &job_info, sizeof(job_info));
      AssignProcessToJobObject(job_handle, GetCurrentProcess());
#endif
    }
    str8_list_push(scratch.arena, &command_parts, exe_name);
    str8_list_push(scratch.arena, &command_parts, s("--cli"));
    for(U64 idx = 1; idx < cmd_line->argc; idx += 1)
    {
      String8 arg = str8_cstring(cmd_line->argv[idx]);
      str8_list_push(scratch.arena, &command_parts, arg);
    }
    StringJoin join = {.sep = s(" ")};
    String8 command = str8_list_join(scratch.arena, &command_parts, &join);
    Process process = launch_cmd_line(command);
    if(wait_for_process)
    {
      U64 exit_code = 0;
      process_join(process, max_U64, &exit_code);
      abort_self(exit_code);
    }
    else
    {
      process_detach(process);
    }
  }
  scratch_end(scratch);
}
