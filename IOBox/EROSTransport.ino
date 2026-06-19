/*
  EROSTransport.ino

  In-process transport shim for the future M7/M4 bridge.

  This file is intentionally tiny. It is the adapter layer that should be
  replaced when real inter-core messaging is introduced. The rest of the
  project should continue to speak in EROS_Command and EROS_ControlStatus
  packets.
*/

#include "EROSShared.h"

bool EROSM4_ReceiveCommandFromTransport(const EROS_Command & command);
void State_ApplyControlStatus(const EROS_ControlStatus & status);

bool EROSTransport_SendCommandToM4(const EROS_Command & command)
{
  // Single-core simulation path.
  // Future real transport: serialize/copy command to M4 mailbox/queue here.
  return EROSM4_ReceiveCommandFromTransport(command);
}

void EROSTransport_PublishStatusToM7(const EROS_ControlStatus & status)
{
  // Single-core simulation path.
  // Future real transport: publish/copy status to M7 mailbox/shared buffer here.
  State_ApplyControlStatus(status);
}
