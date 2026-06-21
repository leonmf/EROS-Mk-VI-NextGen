/*
  IOBox_M4_stage4_transport.ino

  Stage 4 transport-only test for Arduino Giga R1 M4 core.

  Purpose:
    - Keep the proven Stage 3B startup/handshake pattern.
    - Add the real EROS RPC function names and signatures:
        EROS_M4_Command(type, index, value, boolValue, onSettings)
        EROS_M4_GetStatusValue(selector)
    - Exercise only the transport loopback command.

  Deliberately not included:
    - Control_Setup()
    - Control_Task()
    - State_ProcessPendingCommands()
    - Giga display / LVGL
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

int EROS_M4_Command(int type, int index, int value, int boolValue, int onSettings)
{
  (void)index;
  (void)boolValue;
  (void)onSettings;

  if (type == EROS_CMD_TRANSPORT_LOOPBACK_PING)
  {
    gTransportCommandAcceptedCounter++;
    gTransportLoopbackRequestId = (unsigned long)value;
    gTransportLoopbackEchoId = (unsigned long)value;
    gTransportLoopbackEchoCounter++;
    gTransportLoopbackEchoMillis = millis();
    RefreshTransportStatus();

    SerialRPC.println(
      String("M4: EROS_M4_Command loopback accepted, request id = ") +
      String(value)
    );

    return 1;
  }

  gTransportCommandRejectedCounter++;
  RefreshTransportStatus();

  SerialRPC.println(
    String("M4: EROS_M4_Command rejected unknown type = ") +
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

  SerialRPC.println("M4: stage 4 transport-only setup starting");

  RPC.bind("EROS_M4_Command", EROS_M4_Command);
  RPC.bind("EROS_M4_GetStatusValue", EROS_M4_GetStatusValue);

  RefreshTransportStatus();

  SerialRPC.println("M4: stage 4 EROS transport RPC.bind complete");
  SerialRPC.println("M4: stage 4 setup complete");
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
      String("M4: stage 4 heartbeat ") +
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
