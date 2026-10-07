// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

internal B32
ds_push_msg__host(GuardedRing *out_ring, DS_Msg *msg, U64 endt_us)
{
  B32 result = 1;
  switch(msg->kind)
  {
    default:{}break;
    
    //- rjf: launching
    case DS_MsgKind_Launch:
    {
      ProcessLaunchParams params = {0};
      {
        params.cmd_line           = msg->command_line;
        params.path               = msg->path;
        params.env                = msg->env;
        params.inherit_env        = !!(msg->flags & DS_MsgFlag_InheritEnv);
        params.debug_subprocesses = !!(msg->flags & DS_MsgFlag_DebugSubprocesses);
      }
      U32 pid = dmn_ctrl_launch(0, &params);
    }break;
    
  }
  return result;
}
