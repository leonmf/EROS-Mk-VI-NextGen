/*
  EROSTransport.ino

  Stage 11 M7-side stable transport with M4 status masks.

  This replaces the older worker-thread transport for this test:
    - Uses stable EROS_M4_Command4 RPC, not the unstable 5-argument command.
    - Does not call SerialRPC.begin(); the main sketch owns startup/handshake.
    - Refuses to send commands until the main sketch marks M4 ready.
    - Polls transport selectors 0-9 plus compact M4 status masks 20-24.
*/

#include "EROSShared.h"
#include "RPC.h"

void State_ApplyControlStatus(const EROS_ControlStatus & status);

static bool g_transportM4Ready = false;
static unsigned long g_transportCommandSendAttemptCounter = 0;
static unsigned long g_transportCommandSendAcceptedCounter = 0;
static unsigned long g_transportCommandSendFailedCounter = 0;

void EROSTransport_SetM4Ready(bool ready)
{
  g_transportM4Ready = ready;
}

static int EROSTransport_CommandFlags(const EROS_Command & command)
{
  int flags = 0;

  if (command.boolValue)
  {
    flags |= 0x01;
  }

  if (command.onSettings)
  {
    flags |= 0x02;
  }

  return flags;
}

bool EROSTransport_SendCommandToM4(const EROS_Command & command)
{
#if EROS_BUILD_M7_CORE
  g_transportCommandSendAttemptCounter++;

  if (!g_transportM4Ready)
  {
    g_transportCommandSendFailedCounter++;
    return false;
  }

  int accepted = RPC.call(
    "EROS_M4_Command4",
    (int)command.type,
    command.index,
    (int)command.value,
    EROSTransport_CommandFlags(command)
  ).as<int>();

  if (accepted != 0)
  {
    g_transportCommandSendAcceptedCounter++;
    return true;
  }

  g_transportCommandSendFailedCounter++;
  return false;
#else
  (void)command;
  return false;
#endif
}

static int EROSTransport_ReadM4StatusValue(int selector)
{
#if EROS_BUILD_M7_CORE
  if (!g_transportM4Ready)
  {
    return 0;
  }

  return RPC.call("EROS_M4_GetStatusValue", selector).as<int>();
#else
  (void)selector;
  return 0;
#endif
}

static bool EROSTransport_MaskBitIsSet(int mask, int index)
{
  return (mask & (1 << index)) != 0;
}

void EROSTransport_PollM4Status()
{
#if EROS_BUILD_M7_CORE
  if (!g_transportM4Ready)
  {
    return;
  }

  EROS_ControlStatus status;
  memset(&status, 0, sizeof(status));

  status.transportLoopbackRequestId = (unsigned long)EROSTransport_ReadM4StatusValue(0);
  status.transportLoopbackEchoId = (unsigned long)EROSTransport_ReadM4StatusValue(1);
  status.transportLoopbackEchoCounter = (unsigned long)EROSTransport_ReadM4StatusValue(2);
  status.transportLoopbackEchoMillis = (unsigned long)EROSTransport_ReadM4StatusValue(3);

  status.transportStatusCounter = (unsigned long)EROSTransport_ReadM4StatusValue(4);
  status.transportStatusMillis = (unsigned long)EROSTransport_ReadM4StatusValue(5);
  status.transportCommandAcceptedCounter = (unsigned long)EROSTransport_ReadM4StatusValue(6);
  status.transportCommandRejectedCounter = (unsigned long)EROSTransport_ReadM4StatusValue(7);
  status.transportCommandQueueDepth = (byte)EROSTransport_ReadM4StatusValue(8);
  status.transportCommandQueueCapacity = (byte)EROSTransport_ReadM4StatusValue(9);

  // Stage 11 compact status masks reported by M4.
  // These make display indicators reflect actual M4 state rather than M7 intent.
  int outputMask = EROSTransport_ReadM4StatusValue(20);
  int manualMask = EROSTransport_ReadM4StatusValue(21);
  int inputMask = EROSTransport_ReadM4StatusValue(22);
  int assignableInputMask = EROSTransport_ReadM4StatusValue(23);

  for (int i = 0; i < OutSize; i++)
  {
    status.output[i] = EROSTransport_MaskBitIsSet(outputMask, i);
    status.manualOutputRequest[i] = EROSTransport_MaskBitIsSet(manualMask, i);
  }

  for (int i = 0; i < InSize; i++)
  {
    status.input[i] = EROSTransport_MaskBitIsSet(inputMask, i);
  }

  for (int i = 0; i < AssignableInSize; i++)
  {
    status.assignableInput[i] = EROSTransport_MaskBitIsSet(assignableInputMask, i);
  }

  status.mode = (byte)EROSTransport_ReadM4StatusValue(24);

  State_ApplyControlStatus(status);
#endif
}

void EROSTransport_Setup()
{
  // Main sketch owns SerialRPC startup and ready handshake in Stage 9.
}

void EROSTransport_PublishStatusToM7(const EROS_ControlStatus & status)
{
#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
  State_ApplyControlStatus(status);
#else
  (void)status;
#endif
}
