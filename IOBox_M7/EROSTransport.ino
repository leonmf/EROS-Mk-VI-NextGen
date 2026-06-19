/*
  EROSTransport.ino

  M7-side transport shim for the EROS Mk VI bridge.

  Current real split mode:
    - Sends command packets to the M4 over Arduino RPC.
    - Receives a minimal status/loopback packet back from the M4 over RPC.

  This phase intentionally transports only the loopback/debug subset of the
  status packet. Full command/status serialization comes next after the pipe is
  proven stable.
*/

#include "EROSShared.h"
#include "RPC.h"
#include "SerialRPC.h"

#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
bool EROSM4_ReceiveCommandFromTransport(const EROS_Command & command);
#endif

void State_ApplyControlStatus(const EROS_ControlStatus & status);

static bool g_transportInitialized = false;

int EROSTransport_RPCReceiveStatusLoopback(
  int requestId,
  int echoId,
  int echoCounter,
  int echoMillis,
  int statusCounter,
  int m4Accepted,
  int m4Rejected,
  int queueDepth,
  int queueCapacity
)
{
  EROS_ControlStatus status;
  memset(&status, 0, sizeof(status));

  status.transportLoopbackRequestId = (unsigned long)requestId;
  status.transportLoopbackEchoId = (unsigned long)echoId;
  status.transportLoopbackEchoCounter = (unsigned long)echoCounter;
  status.transportLoopbackEchoMillis = (unsigned long)echoMillis;

  status.transportStatusCounter = (unsigned long)statusCounter;
  status.transportStatusMillis = (unsigned long)echoMillis;
  status.transportCommandAcceptedCounter = (unsigned long)m4Accepted;
  status.transportCommandRejectedCounter = (unsigned long)m4Rejected;
  status.transportCommandQueueDepth = (byte)queueDepth;
  status.transportCommandQueueCapacity = (byte)queueCapacity;

  State_ApplyControlStatus(status);
  return 1;
}

void EROSTransport_Setup()
{
#if EROS_BUILD_M7_CORE
  if (g_transportInitialized)
  {
    return;
  }

  // SerialRPC initializes the RPC transport used between the GIGA M7 and M4
  // cores. On the M7 it also starts the M4 firmware when present.
  SerialRPC.begin();
  RPC.bind("EROS_M7_StatusLoopback", EROSTransport_RPCReceiveStatusLoopback);

  g_transportInitialized = true;
#endif
}

bool EROSTransport_SendCommandToM4(const EROS_Command & command)
{
#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
  // Single-core simulation path.
  return EROSM4_ReceiveCommandFromTransport(command);
#elif EROS_BUILD_M7_CORE
  if (!g_transportInitialized)
  {
    EROSTransport_Setup();
  }

  int accepted = RPC.call(
    "EROS_M4_Command",
    (int)command.type,
    command.index,
    (int)command.value,
    command.boolValue ? 1 : 0,
    command.onSettings ? 1 : 0
  ).as<int>();

  return accepted != 0;
#else
  (void)command;
  return false;
#endif
}

void EROSTransport_PublishStatusToM7(const EROS_ControlStatus & status)
{
#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
  // Single-core simulation path.
  State_ApplyControlStatus(status);
#else
  // M7 builds do not publish M4 status.
  (void)status;
#endif
}
