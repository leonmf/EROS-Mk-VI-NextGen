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

#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
void State_ApplyControlStatus(const EROS_ControlStatus & status);
#endif

static bool g_transportInitialized = false;
static unsigned long g_lastPublishedLoopbackEchoCounter = 0;

int EROSTransport_RPCReceiveCommand(
  int type,
  int index,
  int value,
  int boolValue,
  int onSettings
)
{
  EROS_Command command;
  command.type = (EROS_CommandType)type;
  command.index = index;
  command.value = (long)value;
  command.boolValue = (boolValue != 0);
  command.onSettings = (onSettings != 0);

  return EROSM4_ReceiveCommandFromTransport(command) ? 1 : 0;
}

void EROSTransport_Setup()
{
#if EROS_BUILD_M4_CORE
  if (g_transportInitialized)
  {
    return;
  }

  SerialRPC.begin();
  RPC.bind("EROS_M4_Command", EROSTransport_RPCReceiveCommand);

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
  if (!g_transportInitialized)
  {
    EROSTransport_Setup();
  }

  // For this first real split transport phase, publish only after a loopback
  // echo changes. This proves M4-to-M7 RPC without flooding the link or trying
  // to serialize the full status packet yet.
  if (status.transportLoopbackEchoCounter == 0 ||
      status.transportLoopbackEchoCounter == g_lastPublishedLoopbackEchoCounter)
  {
    return;
  }

  g_lastPublishedLoopbackEchoCounter = status.transportLoopbackEchoCounter;

  RPC.call(
    "EROS_M7_StatusLoopback",
    (int)status.transportLoopbackRequestId,
    (int)status.transportLoopbackEchoId,
    (int)status.transportLoopbackEchoCounter,
    (int)status.transportLoopbackEchoMillis,
    (int)status.transportStatusCounter,
    (int)status.transportCommandAcceptedCounter,
    (int)status.transportCommandRejectedCounter,
    (int)status.transportCommandQueueDepth,
    (int)status.transportCommandQueueCapacity
  );
#else
  (void)status;
#endif
}
