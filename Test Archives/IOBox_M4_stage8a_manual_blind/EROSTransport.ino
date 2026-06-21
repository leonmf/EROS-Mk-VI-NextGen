/*
  EROSTransport.ino

  M4-side transport shim for the EROS Mk VI bridge.

  Current real split mode:
    - Receives command packets from the M7 over Arduino RPC.
    - Publishes a minimal loopback/debug status packet back to the M7 over RPC.

  This phase intentionally transports only the loopback/debug subset of the
  status packet. Full status serialization comes next after the pipe is proven
  stable.
*/

#include "EROSShared.h"
#include "RPC.h"
#include "SerialRPC.h"

bool EROSM4_ReceiveCommandFromTransport(const EROS_Command & command);
void State_CopyControlStatus(EROS_ControlStatus & status);

#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
void State_ApplyControlStatus(const EROS_ControlStatus & status);
#endif

static bool g_transportInitialized = false;
static unsigned long g_lastPublishedLoopbackEchoCounter = 0;

int EROSTransport_RPCReceiveCommand4(
  int type,
  int index,
  int value,
  int flags
)
{
  EROS_Command command;
  command.type = (EROS_CommandType)type;
  command.index = index;
  command.value = (long)value;

  // Packed flags keep the RPC signature at 4 arguments.
  // Arduino Giga RPC has proven stable with 4 args, while the 5-arg
  // EROS_M4_Command(...) binding prevented M4 startup in Stage 4.
  command.boolValue = ((flags & 0x01) != 0);
  command.onSettings = ((flags & 0x02) != 0);

  return EROSM4_ReceiveCommandFromTransport(command) ? 1 : 0;
}

int EROSTransport_RPCGetStatusValue(int selector)
{
  EROS_ControlStatus status;
  memset(&status, 0, sizeof(status));
  State_CopyControlStatus(status);

  switch (selector)
  {
    case 0: return (int)status.transportLoopbackRequestId;
    case 1: return (int)status.transportLoopbackEchoId;
    case 2: return (int)status.transportLoopbackEchoCounter;
    case 3: return (int)status.transportLoopbackEchoMillis;
    case 4: return (int)status.transportStatusCounter;
    case 5: return (int)status.transportStatusMillis;
    case 6: return (int)status.transportCommandAcceptedCounter;
    case 7: return (int)status.transportCommandRejectedCounter;
    case 8: return (int)status.transportCommandQueueDepth;
    case 9: return (int)status.transportCommandQueueCapacity;
    default: return 0;
  }
}

void EROSTransport_Setup()
{
#if EROS_BUILD_M4_CORE
  if (g_transportInitialized)
  {
    return;
  }

  // SerialRPC.begin() is intentionally called once from IOBox_M4 setup().
  // Do not call it again here.
  RPC.bind("EROS_M4_Command4", EROSTransport_RPCReceiveCommand4);
  RPC.bind("EROS_M4_GetStatusValue", EROSTransport_RPCGetStatusValue);

  g_transportInitialized = true;
#endif
}

bool EROSTransport_SendCommandToM4(const EROS_Command & command)
{
#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
  // Single-core simulation path.
  return EROSM4_ReceiveCommandFromTransport(command);
#else
  // M4 builds do not send commands to M4.
  (void)command;
  return false;
#endif
}

void EROSTransport_PublishStatusToM7(const EROS_ControlStatus & status)
{
#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
  // Single-core simulation path.
  State_ApplyControlStatus(status);
#elif EROS_BUILD_M4_CORE
  // Real split mode now uses an M7-pull model for the first bridge test:
  // M7 sends commands and polls EROS_M4_GetStatusValue(...). Avoid making
  // synchronous M4-to-M7 RPC calls here, because a blocked reverse call can
  // stall the M4 control loop.
  (void)status;
#else
  (void)status;
#endif
}
