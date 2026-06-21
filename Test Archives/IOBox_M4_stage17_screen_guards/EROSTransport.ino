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
unsigned long EROSPerf_GetM4LoopCounter();
unsigned long EROSPerf_GetM4AvgLoopPeriodUs();
unsigned long EROSPerf_GetM4AvgLoopExecUs();

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

static int EROSTransport_BoolArrayToMask(const bool * values, int count)
{
  int mask = 0;

  for (int i = 0; i < count; i++)
  {
    if (values[i])
    {
      mask |= (1 << i);
    }
  }

  return mask;
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

    // Stage 11 compact M4-to-M7 display status.
    // These masks allow the M7 display to render actual M4 state without
    // using a large multi-field RPC return or many per-point selectors.
    case 20: return EROSTransport_BoolArrayToMask(status.output, OutSize);
    case 21: return EROSTransport_BoolArrayToMask(status.manualOutputRequest, OutSize);
    case 22: return EROSTransport_BoolArrayToMask(status.input, InSize);
    case 23: return EROSTransport_BoolArrayToMask(status.assignableInput, AssignableInSize);
    case 24: return (int)status.mode;

    // Stage 13 compact Hitachi status.
    // Pair selectors pack OFF in low byte and ON in high byte where possible.
    case 30: return status.hitachiCurrentOutput;
    case 31: return ((status.hitachiModeOn & 0xFF) << 8) | (status.hitachiModeOff & 0xFF);
    case 32: return ((status.hitachiSetPointOn & 0xFF) << 8) | (status.hitachiSetPointOff & 0xFF);
    case 33: return ((status.hitachiMaxValueOn & 0xFF) << 8) | (status.hitachiMaxValueOff & 0xFF);
    case 34: return ((status.hitachiMinValueOn & 0xFF) << 8) | (status.hitachiMinValueOff & 0xFF);
    case 35: return status.hitachiPeriodOff;
    case 36: return status.hitachiPeriodOn;
    case 37: return (status.hitachiPeriodPreciseOff ? 0x01 : 0x00) | (status.hitachiPeriodPreciseOn ? 0x02 : 0x00);
    case 38: return status.hitachiMinRelayValue;

    // Stage 14 compact Auto settings/status from M4.
    // These let the M7 Auto Settings screen rebuild from M4 truth instead
    // of falling back to zero/default UI values when the screen is recreated.
    case 40: return (status.autoRunning ? 0x01 : 0x00) | (status.autoPaused ? 0x02 : 0x00);
    case 41: return (int)status.autoRemainingTime;
    case 42: return (int)status.autoCurrentTime;
    case 43: return (int)status.autoRunDuration;
    case 44: return (int)status.autoPauseDuration;
    case 45: return (int)status.autoPenaltyDuration;
    case 46: return (int)status.autoIoOnTimeMs;
    case 47: return (int)status.autoIoOffTimeMs;

    // Pack auto output modes for outputs 0-7 into 4-bit nibbles.
    // Output 8 is returned separately because 9 outputs would require 36 bits.
    case 48:
    {
      int packed = 0;
      for (int i = 0; i < OutSize && i < 8; i++)
      {
        packed |= ((int)status.autoOutputMode[i] & 0x0F) << (i * 4);
      }
      return packed;
    }

    case 49:
      return (OutSize > 8) ? ((int)status.autoOutputMode[8] & 0x0F) : 0;

    // Pack assigned input indexes into 2-bit fields. AssignableInSize is 3,
    // so 2 bits per output is sufficient for outputs 0-8.
    case 50:
    {
      int packed = 0;
      for (int i = 0; i < OutSize; i++)
      {
        packed |= ((int)status.autoOutputInputIndex[i] & 0x03) << (i * 2);
      }
      return packed;
    }

    // Stage 16 M4 performance metrics.
    // 60: average loop period, including the loop yield, in microseconds.
    // 61: average active control-loop execution time, excluding the loop yield, in microseconds.
    // 62: loop counter, truncated to int range by the existing RPC return type.
    case 60: return (int)EROSPerf_GetM4AvgLoopPeriodUs();
    case 61: return (int)EROSPerf_GetM4AvgLoopExecUs();
    case 62: return (int)(EROSPerf_GetM4LoopCounter() & 0x7FFFFFFF);

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
