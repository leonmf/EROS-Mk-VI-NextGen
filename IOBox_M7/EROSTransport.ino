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

  unsigned long nowMs = millis();

  if (g_m7LastStatusPollMs != 0)
  {
    unsigned long periodMsX100 = (nowMs - g_m7LastStatusPollMs) * 100UL;

    if (g_m7AvgStatusPollPeriodMsX100 == 0)
    {
      g_m7AvgStatusPollPeriodMsX100 = periodMsX100;
    }
    else
    {
      // Lightweight exponential moving average. 1/8 new sample.
      g_m7AvgStatusPollPeriodMsX100 = ((g_m7AvgStatusPollPeriodMsX100 * 7UL) + periodMsX100) / 8UL;
    }
  }

  g_m7LastStatusPollMs = nowMs;
  g_m7StatusPollCounter++;

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

  // Stage 12 compact status masks reported by M4.
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

  // Stage 13 compact Hitachi status from M4.
  status.hitachiCurrentOutput = EROSTransport_ReadM4StatusValue(30);

  int hitachiModes = EROSTransport_ReadM4StatusValue(31);
  status.hitachiModeOff = hitachiModes & 0xFF;
  status.hitachiModeOn = (hitachiModes >> 8) & 0xFF;

  int hitachiSetPoints = EROSTransport_ReadM4StatusValue(32);
  status.hitachiSetPointOff = hitachiSetPoints & 0xFF;
  status.hitachiSetPointOn = (hitachiSetPoints >> 8) & 0xFF;

  int hitachiMaxValues = EROSTransport_ReadM4StatusValue(33);
  status.hitachiMaxValueOff = hitachiMaxValues & 0xFF;
  status.hitachiMaxValueOn = (hitachiMaxValues >> 8) & 0xFF;

  int hitachiMinValues = EROSTransport_ReadM4StatusValue(34);
  status.hitachiMinValueOff = hitachiMinValues & 0xFF;
  status.hitachiMinValueOn = (hitachiMinValues >> 8) & 0xFF;

  status.hitachiPeriodOff = EROSTransport_ReadM4StatusValue(35);
  status.hitachiPeriodOn = EROSTransport_ReadM4StatusValue(36);

  int hitachiPreciseMask = EROSTransport_ReadM4StatusValue(37);
  status.hitachiPeriodPreciseOff = (hitachiPreciseMask & 0x01) != 0;
  status.hitachiPeriodPreciseOn = (hitachiPreciseMask & 0x02) != 0;

  status.hitachiMinRelayValue = EROSTransport_ReadM4StatusValue(38);

  // Stage 14 compact Auto settings/status from M4.
  int autoFlags = EROSTransport_ReadM4StatusValue(40);
  status.autoRunning = (autoFlags & 0x01) != 0;
  status.autoPaused = (autoFlags & 0x02) != 0;

  status.autoRemainingTime = (unsigned int)EROSTransport_ReadM4StatusValue(41);
  status.autoCurrentTime = (unsigned int)EROSTransport_ReadM4StatusValue(42);
  status.autoRunDuration = (unsigned int)EROSTransport_ReadM4StatusValue(43);
  status.autoPauseDuration = (unsigned int)EROSTransport_ReadM4StatusValue(44);
  status.autoPenaltyDuration = (unsigned int)EROSTransport_ReadM4StatusValue(45);
  status.autoIoOnTimeMs = (unsigned int)EROSTransport_ReadM4StatusValue(46);
  status.autoIoOffTimeMs = (unsigned int)EROSTransport_ReadM4StatusValue(47);

  int packedAutoModesLow = EROSTransport_ReadM4StatusValue(48);
  for (int i = 0; i < OutSize && i < 8; i++)
  {
    status.autoOutputMode[i] = (byte)((packedAutoModesLow >> (i * 4)) & 0x0F);
  }

  if (OutSize > 8)
  {
    status.autoOutputMode[8] = (byte)(EROSTransport_ReadM4StatusValue(49) & 0x0F);
  }

  int packedAutoInputIndexes = EROSTransport_ReadM4StatusValue(50);
  for (int i = 0; i < OutSize; i++)
  {
    status.autoOutputInputIndex[i] = (packedAutoInputIndexes >> (i * 2)) & 0x03;
  }

  // Stage 16 performance metrics.
  status.m4AvgLoopPeriodUs = (unsigned int)EROSTransport_ReadM4StatusValue(60);
  status.m4AvgLoopExecUs = (unsigned int)EROSTransport_ReadM4StatusValue(61);
  status.m4LoopCounter = (unsigned long)EROSTransport_ReadM4StatusValue(62);
  status.m7StatusPollCounter = g_m7StatusPollCounter;
  status.m7StatusPollAvgMsX100 = (unsigned int)g_m7AvgStatusPollPeriodMsX100;

  State_ApplyControlStatus(status);
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
