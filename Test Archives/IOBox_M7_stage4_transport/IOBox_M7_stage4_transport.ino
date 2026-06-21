/*
  IOBox_M7_stage4_transport.ino

  Stage 4 transport-only test for Arduino Giga R1 M7 core.

  Purpose:
    - Use the proven Stage 3B safe startup sequence.
    - Wait for SerialRPC.begin() to succeed.
    - Wait until M4 has actually spoken over SerialRPC.
    - Then call the real EROS M4 transport RPC functions.

  Deliberately not included:
    - GigaDisplay_Setup()
    - GigaDisplay_Task()
    - old M7 transport worker thread
    - LVGL
*/

#define EROS_BUILD_SINGLE_CORE_SIM 0
#define EROS_BUILD_M7_CORE 1
#define EROS_BUILD_M4_CORE 0

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"
#include "EROSShared.h"

static bool gSerialRpcStarted = false;
static bool gSawM4Text = false;
static bool gRpcTestEnabled = false;

static unsigned long gLastSerialRpcAttemptMs = 0;
static unsigned long gFirstM4TextMs = 0;
static unsigned long gLastHeartbeatMs = 0;
static unsigned long gLastPingMs = 0;
static unsigned long gHeartbeatCounter = 0;
static int gRequestId = 0;

static bool TryStartSerialRPC()
{
  if (gSerialRpcStarted)
  {
    return true;
  }

  unsigned long nowMs = millis();

  if (gLastSerialRpcAttemptMs != 0 && nowMs - gLastSerialRpcAttemptMs < 1000)
  {
    return false;
  }

  gLastSerialRpcAttemptMs = nowMs;

  Serial.println(
    String("M7: SerialRPC.begin attempt at millis=") +
    String(nowMs)
  );

  gSerialRpcStarted = SerialRPC.begin();

  Serial.println(
    String("M7: SerialRPC.begin() returned ") +
    String(gSerialRpcStarted ? "true" : "false")
  );

  return gSerialRpcStarted;
}

static void PrintSerialRPCMessages()
{
  if (!gSerialRpcStarted)
  {
    return;
  }

  String buffer = "";

  while (SerialRPC.available())
  {
    buffer += (char)SerialRPC.read();
  }

  if (buffer.length() > 0)
  {
    Serial.print(buffer);

    if (!gSawM4Text)
    {
      gSawM4Text = true;
      gFirstM4TextMs = millis();
      Serial.println("M7: detected first M4 SerialRPC text");
    }
  }
}

static int GetStatusValue(int selector)
{
  return RPC.call("EROS_M4_GetStatusValue", selector).as<int>();
}

static void RunTransportLoopbackTest()
{
  if (!gSerialRpcStarted || !gSawM4Text)
  {
    return;
  }

  unsigned long nowMs = millis();

  if (!gRpcTestEnabled)
  {
    if (nowMs - gFirstM4TextMs < 3000)
    {
      return;
    }

    gRpcTestEnabled = true;
    Serial.println("M7: handshake delay complete, beginning EROS transport loopback test");
  }

  if (nowMs - gLastPingMs < 3000)
  {
    return;
  }

  gLastPingMs = nowMs;
  gRequestId++;

  Serial.println(
    String("M7: sending EROS_CMD_TRANSPORT_LOOPBACK_PING request id ") +
    String(gRequestId)
  );

  int accepted = RPC.call(
    "EROS_M4_Command",
    (int)EROS_CMD_TRANSPORT_LOOPBACK_PING,
    0,
    gRequestId,
    0,
    0
  ).as<int>();

  Serial.println(
    String("M7: EROS_M4_Command returned ") +
    String(accepted)
  );

  int m4Request = GetStatusValue(0);
  int m4Echo = GetStatusValue(1);
  int echoCount = GetStatusValue(2);
  int echoMillis = GetStatusValue(3);
  int statusCounter = GetStatusValue(4);
  int statusMillis = GetStatusValue(5);
  int acceptedCount = GetStatusValue(6);
  int rejectedCount = GetStatusValue(7);
  int queueDepth = GetStatusValue(8);
  int queueCapacity = GetStatusValue(9);

  bool ok = (accepted == 1 && m4Request == gRequestId && m4Echo == gRequestId);

  Serial.println(
    String("M7: transport status sent=") +
    String(gRequestId) +
    String(", m4Request=") +
    String(m4Request) +
    String(", echo=") +
    String(m4Echo) +
    String(", echoCount=") +
    String(echoCount) +
    String(", ok=") +
    String(ok ? "YES" : "NO")
  );

  Serial.println(
    String("M7: counters statusCounter=") +
    String(statusCounter) +
    String(", statusMillis=") +
    String(statusMillis) +
    String(", echoMillis=") +
    String(echoMillis) +
    String(", accepted=") +
    String(acceptedCount) +
    String(", rejected=") +
    String(rejectedCount) +
    String(", queue=") +
    String(queueDepth) +
    String("/") +
    String(queueCapacity)
  );
}

static void SendHeartbeat()
{
  unsigned long nowMs = millis();

  if (nowMs - gLastHeartbeatMs < 2000)
  {
    return;
  }

  gLastHeartbeatMs = nowMs;
  gHeartbeatCounter++;

  Serial.println(
    String("M7: heartbeat ") +
    String(gHeartbeatCounter) +
    String(", millis=") +
    String(nowMs) +
    String(", SerialRPC=") +
    String(gSerialRpcStarted ? "true" : "false") +
    String(", sawM4=") +
    String(gSawM4Text ? "true" : "false") +
    String(", rpcTest=") +
    String(gRpcTestEnabled ? "true" : "false")
  );
}

void setup()
{
  Serial.begin(115200);

  unsigned long serialStartMs = millis();
  while (!Serial && millis() - serialStartMs < 3000)
  {
    delay(10);
  }

  Serial.println();
  Serial.println("M7: stage 4 EROS transport loopback test starting");
  Serial.println("M7: starts SerialRPC, waits for M4 text, then calls EROS_M4_Command");
  Serial.println("M7: no display, no old M7 transport thread, no control loop");
}

void loop()
{
  TryStartSerialRPC();
  PrintSerialRPCMessages();
  SendHeartbeat();
  RunTransportLoopbackTest();

  delay(1);
}
