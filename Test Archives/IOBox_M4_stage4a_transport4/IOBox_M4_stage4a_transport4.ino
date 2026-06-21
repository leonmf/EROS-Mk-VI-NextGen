/*
  IOBox_M4_stage4a_transport4.ino

  Stage 4A transport-only test for Arduino Giga R1 M4 core.

  Difference from failed Stage 4:
    - Avoids the 5-argument RPC.bind() function.
    - Uses a 4-argument command function instead:
        EROS_M4_Command4(type, index, value, flags)
    - Keeps EROS_M4_GetStatusValue(selector).

  Deliberately not included:
    - Control_Setup()
    - Control_Task()
    - State_ProcessPendingCommands()
    - physical IO setup
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"
#include "EROSShared.h"

static unsigned long gHeartbeatCounter = 0;
static unsigned long gLastHeartbeatMs = 0;

static unsigned long gTransportStatusCounter = 0;
static unsigned long gTransportStatusMillis = 0;
static unsigned long gTransportCommandAcceptedCounter = 0;
static unsigned long gTransportCommandRejectedCounter = 0;
static unsigned long gTransportLoopbackRequestId = 0;
static unsigned long gTransportLoopbackEchoId = 0;
static unsigned long gTransportLoopbackEchoCounter = 0;
static unsigned long gTransportLoopbackEchoMillis = 0;

static const byte TRANSPORT_QUEUE_DEPTH = 0;
static const byte TRANSPORT_QUEUE_CAPACITY = 16;

static void RefreshTransportStatus()
{
  gTransportStatusCounter++;
  gTransportStatusMillis = millis();
}

int EROS_M4_Command4(int type, int index, int value, int flags)
{
  (void)index;
  (void)flags;

  if (type == EROS_CMD_TRANSPORT_LOOPBACK_PING)
  {
    gTransportCommandAcceptedCounter++;
    gTransportLoopbackRequestId = (unsigned long)value;
    gTransportLoopbackEchoId = (unsigned long)value;
    gTransportLoopbackEchoCounter++;
    gTransportLoopbackEchoMillis = millis();
    RefreshTransportStatus();

    SerialRPC.println(
      String("M4: EROS_M4_Command4 loopback accepted, request id = ") +
      String(value)
    );

    return 1;
  }

  gTransportCommandRejectedCounter++;
  RefreshTransportStatus();

  SerialRPC.println(
    String("M4: EROS_M4_Command4 rejected unknown type = ") +
    String(type)
  );

  return 0;
}

int EROS_M4_GetStatusValue(int selector)
{
  RefreshTransportStatus();

  switch (selector)
  {
    case 0: return (int)gTransportLoopbackRequestId;
    case 1: return (int)gTransportLoopbackEchoId;
    case 2: return (int)gTransportLoopbackEchoCounter;
    case 3: return (int)gTransportLoopbackEchoMillis;
    case 4: return (int)gTransportStatusCounter;
    case 5: return (int)gTransportStatusMillis;
    case 6: return (int)gTransportCommandAcceptedCounter;
    case 7: return (int)gTransportCommandRejectedCounter;
    case 8: return (int)TRANSPORT_QUEUE_DEPTH;
    case 9: return (int)TRANSPORT_QUEUE_CAPACITY;
    default: return 0;
  }
}

void setup()
{
  SerialRPC.begin();

  SerialRPC.println("M4: stage 4A transport4 setup starting");

  RPC.bind("EROS_M4_Command4", EROS_M4_Command4);
  SerialRPC.println("M4: stage 4A Command4 bind complete");

  RPC.bind("EROS_M4_GetStatusValue", EROS_M4_GetStatusValue);
  SerialRPC.println("M4: stage 4A GetStatusValue bind complete");

  RefreshTransportStatus();

  SerialRPC.println("M4: stage 4A setup complete");
}

void loop()
{
  unsigned long nowMs = millis();

  if (nowMs - gLastHeartbeatMs >= 2000)
  {
    gLastHeartbeatMs = nowMs;
    gHeartbeatCounter++;

    RefreshTransportStatus();

    SerialRPC.println(
      String("M4: stage 4A heartbeat ") +
      String(gHeartbeatCounter) +
      String(", millis=") +
      String(nowMs) +
      String(", accepted=") +
      String(gTransportCommandAcceptedCounter) +
      String(", rejected=") +
      String(gTransportCommandRejectedCounter) +
      String(", echo=") +
      String(gTransportLoopbackEchoId)
    );
  }

  delay(1);
}
