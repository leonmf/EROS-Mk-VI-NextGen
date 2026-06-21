/*
  IOBox_M7_stage8_manual_output.ino

  Stage 8 split-core test.

  Use with IOBox_M4_stage8_manual_output.

  Purpose:
    - Wait for M4_READY_FOR_RPC.
    - Verify Command4 loopback still works.
    - Send real EROS manual output commands to M4.
    - Poll scalar status selectors until the M4 status snapshot confirms
      the requested manual output and physical output state.

  Still excluded:
    - GigaDisplay / LVGL
    - old M7 transport worker thread
    - settings save/load
*/

#define EROS_BUILD_SINGLE_CORE_SIM 0
#define EROS_BUILD_M7_CORE        1
#define EROS_BUILD_M4_CORE        0

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"
#include "EROSShared.h"

static bool gSerialRPCStarted = false;
static bool gSawM4Text = false;
static bool gM4Ready = false;
static bool gStartedTests = false;

static unsigned long gLastSerialRPCBeginAttemptMs = 0;
static unsigned long gLastHeartbeatMs = 0;
static unsigned long gReadySeenMs = 0;
static unsigned long gLastCommandMs = 0;
static unsigned long gHeartbeatCounter = 0;
static unsigned long gLoopbackRequestId = 0;

// Use a dry contact output first. This avoids lock, AC, and dimmer relay outputs.
static const int TEST_OUTPUT_INDEX = OUT_DRY_1;  // OUT_DRY_1 = 4, pin 45 in the current IOBox map.
static bool gNextManualState = true;

String gSerialBuffer = "";

void PrintAndParseSerialRPCMessages()
{
  while (SerialRPC.available())
  {
    char c = (char)SerialRPC.read();
    Serial.print(c);

    gSerialBuffer += c;

    if (gSerialBuffer.length() > 300)
    {
      gSerialBuffer.remove(0, gSerialBuffer.length() - 300);
    }

    gSawM4Text = true;

    if (gSerialBuffer.indexOf("M4_READY_FOR_RPC") >= 0)
    {
      if (!gM4Ready)
      {
        gM4Ready = true;
        gReadySeenMs = millis();
        Serial.println("M7: detected M4_READY_FOR_RPC marker");
      }
    }
  }
}

void TryStartSerialRPC()
{
  if (gSerialRPCStarted)
  {
    return;
  }

  unsigned long nowMs = millis();

  if (nowMs - gLastSerialRPCBeginAttemptMs < 1000)
  {
    return;
  }

  gLastSerialRPCBeginAttemptMs = nowMs;

  Serial.println(String("M7: SerialRPC.begin attempt at millis=") + String(nowMs));
  gSerialRPCStarted = SerialRPC.begin();
  Serial.println(String("M7: SerialRPC.begin() returned ") + String(gSerialRPCStarted ? "true" : "false"));
}

int CallCommand4(int type, int index, long value, bool boolValue, bool onSettings)
{
  int flags = 0;

  if (boolValue)
  {
    flags |= 0x01;
  }

  if (onSettings)
  {
    flags |= 0x02;
  }

  return RPC.call("EROS_M4_Command4", type, index, (int)value, flags).as<int>();
}

int GetStatusValue(int selector)
{
  return RPC.call("EROS_M4_GetStatusValue", selector).as<int>();
}

bool WaitForStatusMatch(int selector, int expectedValue, unsigned long timeoutMs, int & finalValue, unsigned long & elapsedMs)
{
  unsigned long startMs = millis();
  finalValue = -9999;
  elapsedMs = 0;

  while (millis() - startMs < timeoutMs)
  {
    PrintAndParseSerialRPCMessages();

    finalValue = GetStatusValue(selector);

    if (finalValue == expectedValue)
    {
      elapsedMs = millis() - startMs;
      return true;
    }

    delay(5);
  }

  elapsedMs = millis() - startMs;
  return false;
}

void SendLoopbackPing()
{
  gLoopbackRequestId++;

  Serial.println();
  Serial.println(String("M7: sending loopback request id ") + String(gLoopbackRequestId));

  int accepted = CallCommand4(
    (int)EROS_CMD_TRANSPORT_LOOPBACK_PING,
    0,
    (long)gLoopbackRequestId,
    false,
    false
  );

  Serial.println(String("M7: EROS_M4_Command4 loopback returned ") + String(accepted));

  int finalEcho = -1;
  unsigned long elapsedMs = 0;

  bool ok = WaitForStatusMatch(1, (int)gLoopbackRequestId, 1000, finalEcho, elapsedMs);

  int request = GetStatusValue(0);
  int echo = GetStatusValue(1);
  int echoCount = GetStatusValue(2);
  int acceptedCount = GetStatusValue(6);
  int rejectedCount = GetStatusValue(7);
  int queueDepth = GetStatusValue(8);
  int queueCapacity = GetStatusValue(9);

  Serial.println(
    String("M7: loopback request=") + String(request) +
    String(", echo=") + String(echo) +
    String(", echoCount=") + String(echoCount) +
    String(", matched=") + String(ok ? "YES" : "NO") +
    String(", waitMs=") + String(elapsedMs)
  );

  Serial.println(
    String("M7: counters accepted=") + String(acceptedCount) +
    String(", rejected=") + String(rejectedCount) +
    String(", queue=") + String(queueDepth) +
    String("/") + String(queueCapacity)
  );
}

void SendManualOutputCommand()
{
  bool requestedState = gNextManualState;
  gNextManualState = !gNextManualState;

  Serial.println();
  Serial.println(
    String("M7: sending manual output command OUT_DRY_1 index ") +
    String(TEST_OUTPUT_INDEX) +
    String(" -> ") +
    String(requestedState ? "ON" : "OFF")
  );

  int accepted = CallCommand4(
    (int)EROS_CMD_SET_MANUAL_OUTPUT,
    TEST_OUTPUT_INDEX,
    0,
    requestedState,
    false
  );

  Serial.println(String("M7: EROS_M4_Command4 manual output returned ") + String(accepted));

  int expected = requestedState ? 1 : 0;
  int outputSelector = 100 + TEST_OUTPUT_INDEX;
  int manualSelector = 120 + TEST_OUTPUT_INDEX;

  int finalManual = -1;
  int finalOutput = -1;
  unsigned long manualWaitMs = 0;
  unsigned long outputWaitMs = 0;

  bool manualOk = WaitForStatusMatch(manualSelector, expected, 1000, finalManual, manualWaitMs);
  bool outputOk = WaitForStatusMatch(outputSelector, expected, 1000, finalOutput, outputWaitMs);

  int acceptedCount = GetStatusValue(6);
  int rejectedCount = GetStatusValue(7);
  int queueDepth = GetStatusValue(8);
  int queueCapacity = GetStatusValue(9);

  Serial.println(
    String("M7: manual status expected=") + String(expected) +
    String(", manual=") + String(finalManual) +
    String(", output=") + String(finalOutput) +
    String(", manualOk=") + String(manualOk ? "YES" : "NO") +
    String(", outputOk=") + String(outputOk ? "YES" : "NO") +
    String(", waits=") + String(manualWaitMs) + String("/") + String(outputWaitMs) + String(" ms")
  );

  Serial.println(
    String("M7: counters accepted=") + String(acceptedCount) +
    String(", rejected=") + String(rejectedCount) +
    String(", queue=") + String(queueDepth) +
    String("/") + String(queueCapacity)
  );
}

void RunStage8Tests()
{
  if (!gSerialRPCStarted || !gM4Ready)
  {
    return;
  }

  unsigned long nowMs = millis();

  if (!gStartedTests)
  {
    if (nowMs - gReadySeenMs < 1500)
    {
      return;
    }

    gStartedTests = true;
    gLastCommandMs = 0;
    Serial.println("M7: handshake delay complete, beginning Stage 8 command tests");
  }

  if (nowMs - gLastCommandMs < 3000)
  {
    return;
  }

  gLastCommandMs = nowMs;

  // Alternate between a transport loopback and a real manual output command.
  static bool runLoopbackNext = true;

  if (runLoopbackNext)
  {
    SendLoopbackPing();
  }
  else
  {
    SendManualOutputCommand();
  }

  runLoopbackNext = !runLoopbackNext;
}

void SendHeartbeat()
{
  unsigned long nowMs = millis();

  if (nowMs - gLastHeartbeatMs < 2000)
  {
    return;
  }

  gLastHeartbeatMs = nowMs;
  gHeartbeatCounter++;

  Serial.println(
    String("M7: heartbeat ") + String(gHeartbeatCounter) +
    String(", millis=") + String(nowMs) +
    String(", SerialRPC=") + String(gSerialRPCStarted ? "true" : "false") +
    String(", sawM4=") + String(gSawM4Text ? "true" : "false") +
    String(", ready=") + String(gM4Ready ? "true" : "false") +
    String(", startedTests=") + String(gStartedTests ? "true" : "false")
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
  Serial.println("M7: stage8 manual-output command test starting");
  Serial.println("M7: waits for M4_READY_FOR_RPC, proves loopback, then toggles OUT_DRY_1 via Command4");
  Serial.println("M7: use with IOBox_M4_stage8_manual_output");
}

void loop()
{
  TryStartSerialRPC();

  if (gSerialRPCStarted)
  {
    PrintAndParseSerialRPCMessages();
  }

  SendHeartbeat();
  RunStage8Tests();

  delay(1);
}
