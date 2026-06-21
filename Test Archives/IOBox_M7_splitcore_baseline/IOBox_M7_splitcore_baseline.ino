/*
  IOBox_M7_splitcore_baseline.ino

  Split-core baseline M7 supervisor test.

  Use with IOBox_M4_splitcore_baseline.

  Purpose:
    - Keep the known-good Stage 6 M4 transport/status implementation.
    - Wait for M4_READY_FOR_RPC before any RPC.call().
    - Prove loopback first.
    - Then send EROS_CMD_SET_MANUAL_OUTPUT to OUT_DRY_1.

  Note:
    This test does not add extra M4 status selectors. The relay click is the
    proof of manual output execution. The extra selectors added in Stage 8
    appear to destabilize M4 startup during EROSTransport_Setup().
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"
#include "EROSShared.h"

static bool g_serialRpcStarted = false;
static bool g_sawM4Text = false;
static bool g_m4Ready = false;
static bool g_startedTests = false;

static unsigned long g_lastSerialRpcAttemptMs = 0;
static unsigned long g_lastHeartbeatMs = 0;
static unsigned long g_m4ReadySeenMs = 0;
static unsigned long g_lastLoopbackMs = 0;
static unsigned long g_lastManualMs = 0;

static unsigned long g_loopbackRequestId = 0;
static bool g_manualState = false;

const int TEST_MANUAL_OUTPUT_INDEX = OUT_DRY_1;

static void PrintSerialRPCMessages()
{
  String buffer = "";

  while (SerialRPC.available())
  {
    buffer += (char)SerialRPC.read();
  }

  if (buffer.length() > 0)
  {
    Serial.print(buffer);
    g_sawM4Text = true;

    if (buffer.indexOf("M4_READY_FOR_RPC") >= 0)
    {
      g_m4Ready = true;
      g_m4ReadySeenMs = millis();
      Serial.println("M7: detected M4_READY_FOR_RPC marker");
    }
  }
}

static void TryStartSerialRPC()
{
  if (g_serialRpcStarted)
  {
    return;
  }

  unsigned long nowMs = millis();

  if (nowMs - g_lastSerialRpcAttemptMs < 1000)
  {
    return;
  }

  g_lastSerialRpcAttemptMs = nowMs;

  Serial.println("M7: SerialRPC.begin attempt at millis=" + String(nowMs));

  g_serialRpcStarted = SerialRPC.begin();

  Serial.println("M7: SerialRPC.begin() returned " + String(g_serialRpcStarted ? "true" : "false"));
}

static int Command4(int type, int index, int value, int flags)
{
  return RPC.call("EROS_M4_Command4", type, index, value, flags).as<int>();
}

static int GetStatusValue(int selector)
{
  return RPC.call("EROS_M4_GetStatusValue", selector).as<int>();
}

static bool ProveLoopback(unsigned long requestId)
{
  Serial.println("M7: proving loopback request id " + String(requestId));

  int accepted = Command4(
    EROS_CMD_TRANSPORT_LOOPBACK_PING,
    0,
    (int)requestId,
    0
  );

  Serial.println("M7: loopback Command4 returned " + String(accepted));

  unsigned long startMs = millis();

  while (millis() - startMs < 1000)
  {
    PrintSerialRPCMessages();

    int m4Request = GetStatusValue(0);
    int echo = GetStatusValue(1);
    int echoCount = GetStatusValue(2);

    if (m4Request == (int)requestId && echo == (int)requestId)
    {
      Serial.println(
        "M7: loopback matched request id " +
        String(requestId) +
        ", echoCount=" +
        String(echoCount)
      );
      return true;
    }

    delay(25);
  }

  int finalRequest = GetStatusValue(0);
  int finalEcho = GetStatusValue(1);

  Serial.println(
    "M7: loopback timeout, sent=" +
    String(requestId) +
    ", m4Request=" +
    String(finalRequest) +
    ", echo=" +
    String(finalEcho)
  );

  return false;
}

static void SendManualOutput(bool state)
{
  int flags = state ? 0x01 : 0x00; // bit 0 = boolValue

  Serial.println(
    "M7: sending manual output OUT_DRY_1 index " +
    String(TEST_MANUAL_OUTPUT_INDEX) +
    " -> " +
    String(state ? "ON" : "OFF")
  );

  int accepted = Command4(
    EROS_CMD_SET_MANUAL_OUTPUT,
    TEST_MANUAL_OUTPUT_INDEX,
    0,
    flags
  );

  Serial.println("M7: manual output Command4 returned " + String(accepted));
}

static void RunTests()
{
  if (!g_startedTests)
  {
    if (!g_m4Ready)
    {
      return;
    }

    if (millis() - g_m4ReadySeenMs < 3000)
    {
      return;
    }

    g_startedTests = true;
    Serial.println("M7: ready delay complete, beginning split-core baseline test");

    g_loopbackRequestId++;
    ProveLoopback(g_loopbackRequestId);

    g_lastLoopbackMs = millis();
    g_lastManualMs = millis();
    return;
  }

  unsigned long nowMs = millis();

  if (nowMs - g_lastManualMs >= 3000)
  {
    g_lastManualMs = nowMs;
    g_manualState = !g_manualState;
    SendManualOutput(g_manualState);
  }

  if (nowMs - g_lastLoopbackMs >= 6000)
  {
    g_lastLoopbackMs = nowMs;
    g_loopbackRequestId++;
    ProveLoopback(g_loopbackRequestId);
  }
}

static void Heartbeat()
{
  unsigned long nowMs = millis();

  if (nowMs - g_lastHeartbeatMs < 2000)
  {
    return;
  }

  g_lastHeartbeatMs = nowMs;

  Serial.println(
    "M7: heartbeat, millis=" +
    String(nowMs) +
    ", SerialRPC=" +
    String(g_serialRpcStarted ? "true" : "false") +
    ", sawM4=" +
    String(g_sawM4Text ? "true" : "false") +
    ", ready=" +
    String(g_m4Ready ? "true" : "false") +
    ", startedTests=" +
    String(g_startedTests ? "true" : "false")
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
  Serial.println("M7: split-core baseline M7 starting");
  Serial.println("M7: uses stable Command4 transport, ready handshake, no display");
  Serial.println("M7: waits for M4_READY_FOR_RPC before any RPC.call()");
}

void loop()
{
  TryStartSerialRPC();

  if (g_serialRpcStarted)
  {
    PrintSerialRPCMessages();
    RunTests();
  }

  Heartbeat();
  delay(1);
}
