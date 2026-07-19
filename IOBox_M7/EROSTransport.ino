/*
  EROSTransport.ino

  M7-side stable transport with M4 status masks and perf metrics.

  This replaces the older worker-thread transport for this test:
    - Uses stable EROS_M4_Command4 RPC, not the unstable 5-argument command.
    - Does not call SerialRPC.begin(); the main sketch owns startup/handshake.
    - Refuses to send commands until the main sketch marks M4 ready.
    - Polls transport selectors 0-9, compact M4 status masks 20-24, Hitachi selectors 30-38, and Auto selectors 40-50.
*/

#include "EROSShared.h"
#include "RPC.h"

void State_ApplyControlStatus(const EROS_ControlStatus & status);

static bool g_transportM4Ready = false;
static unsigned long g_transportCommandSendAttemptCounter = 0;
static unsigned long g_transportCommandSendAcceptedCounter = 0;
static unsigned long g_transportCommandSendFailedCounter = 0;

static unsigned long g_m7StatusPollCounter = 0;
static unsigned long g_m7LastStatusPollMs = 0;
static unsigned long g_m7AvgStatusPollPeriodMsX100 = 0;
static volatile int g_m7LastStatusSelector = -1;
static volatile unsigned long g_m7CompletedSnapshotCounter = 0;

int EROSTransport_GetLastStatusSelector()
{
  return g_m7LastStatusSelector;
}

unsigned long EROSTransport_GetCompletedSnapshotCounter()
{
  return g_m7CompletedSnapshotCounter;
}

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

  static const byte selectors[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    20, 21, 22, 23, 24,
    30, 31, 32, 33, 34, 35, 36, 37, 38,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    60, 61, 62
  };
  static const byte selectorCount = sizeof(selectors) / sizeof(selectors[0]);
  static byte selectorIndex = 0;
  static EROS_ControlStatus status;

  if (selectorIndex == 0) {
    memset(&status, 0, sizeof(status));
  }

  const int selector = selectors[selectorIndex];
  g_m7LastStatusSelector = selector;
  const int value = EROSTransport_ReadM4StatusValue(selector);

  switch (selector) {
    case 0: status.transportLoopbackRequestId = (unsigned long)value; break;
    case 1: status.transportLoopbackEchoId = (unsigned long)value; break;
    case 2: status.transportLoopbackEchoCounter = (unsigned long)value; break;
    case 3: status.transportLoopbackEchoMillis = (unsigned long)value; break;
    case 4: status.transportStatusCounter = (unsigned long)value; break;
    case 5: status.transportStatusMillis = (unsigned long)value; break;
    case 6: status.transportCommandAcceptedCounter = (unsigned long)value; break;
    case 7: status.transportCommandRejectedCounter = (unsigned long)value; break;
    case 8: status.transportCommandQueueDepth = (byte)value; break;
    case 9: status.transportCommandQueueCapacity = (byte)value; break;

    case 20:
      for (int i = 0; i < OutSize; i++)
        status.output[i] = EROSTransport_MaskBitIsSet(value, i);
      break;
    case 21:
      for (int i = 0; i < OutSize; i++)
        status.manualOutputRequest[i] = EROSTransport_MaskBitIsSet(value, i);
      break;
    case 22:
      for (int i = 0; i < InSize; i++)
        status.input[i] = EROSTransport_MaskBitIsSet(value, i);
      break;
    case 23:
      for (int i = 0; i < AssignableInSize; i++)
        status.assignableInput[i] = EROSTransport_MaskBitIsSet(value, i);
      break;
    case 24: status.mode = (byte)value; break;

    case 30: status.hitachiCurrentOutput = value; break;
    case 31:
      status.hitachiModeOff = value & 0xFF;
      status.hitachiModeOn = (value >> 8) & 0xFF;
      break;
    case 32:
      status.hitachiSetPointOff = value & 0xFF;
      status.hitachiSetPointOn = (value >> 8) & 0xFF;
      break;
    case 33:
      status.hitachiMaxValueOff = value & 0xFF;
      status.hitachiMaxValueOn = (value >> 8) & 0xFF;
      break;
    case 34:
      status.hitachiMinValueOff = value & 0xFF;
      status.hitachiMinValueOn = (value >> 8) & 0xFF;
      break;
    case 35: status.hitachiPeriodOff = value; break;
    case 36: status.hitachiPeriodOn = value; break;
    case 37:
      status.hitachiPeriodPreciseOff = (value & 0x01) != 0;
      status.hitachiPeriodPreciseOn = (value & 0x02) != 0;
      break;
    case 38: status.hitachiMinRelayValue = value; break;

    case 40:
      status.autoRunning = (value & 0x01) != 0;
      status.autoPaused = (value & 0x02) != 0;
      break;
    case 41: status.autoRemainingTime = (unsigned int)value; break;
    case 42: status.autoCurrentTime = (unsigned int)value; break;
    case 43: status.autoRunDuration = (unsigned int)value; break;
    case 44: status.autoPauseDuration = (unsigned int)value; break;
    case 45: status.autoPenaltyDuration = (unsigned int)value; break;
    case 46: status.autoIoOnTimeMs = (unsigned int)value; break;
    case 47: status.autoIoOffTimeMs = (unsigned int)value; break;
    case 48:
      for (int i = 0; i < OutSize && i < 8; i++)
        status.autoOutputMode[i] = (byte)((value >> (i * 4)) & 0x0F);
      break;
    case 49:
      if (OutSize > 8) status.autoOutputMode[8] = (byte)(value & 0x0F);
      break;
    case 50:
      for (int i = 0; i < OutSize; i++)
        status.autoOutputInputIndex[i] = (value >> (i * 2)) & 0x03;
      break;

    case 60: status.m4AvgLoopPeriodUs = (unsigned int)value; break;
    case 61: status.m4AvgLoopExecUs = (unsigned int)value; break;
    case 62: status.m4LoopCounter = (unsigned long)value; break;
  }

  selectorIndex++;
  if (selectorIndex >= selectorCount) {
    selectorIndex = 0;
    const unsigned long nowMs = millis();

    if (g_m7LastStatusPollMs != 0) {
      const unsigned long periodMsX100 =
        (nowMs - g_m7LastStatusPollMs) * 100UL;
      if (g_m7AvgStatusPollPeriodMsX100 == 0)
        g_m7AvgStatusPollPeriodMsX100 = periodMsX100;
      else
        g_m7AvgStatusPollPeriodMsX100 =
          ((g_m7AvgStatusPollPeriodMsX100 * 7UL) + periodMsX100) / 8UL;
    }

    g_m7LastStatusPollMs = nowMs;
    g_m7StatusPollCounter++;
    g_m7CompletedSnapshotCounter++;
    status.m7StatusPollCounter = g_m7StatusPollCounter;
    status.m7StatusPollAvgMsX100 =
      (unsigned int)g_m7AvgStatusPollPeriodMsX100;
    State_ApplyControlStatus(status);
  }
#endif
}

void EROSTransport_Setup()
{
  // Main sketch owns SerialRPC startup and ready handshake.
}

void EROSTransport_PublishStatusToM7(const EROS_ControlStatus & status)
{
#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
  State_ApplyControlStatus(status);
#else
  (void)status;
#endif
}
