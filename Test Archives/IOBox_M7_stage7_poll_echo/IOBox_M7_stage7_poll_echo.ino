/*
  IOBox_M7_stage7_poll_echo.ino

  Stage 7 M7 test.

  Uses the stable 4-argument EROS_M4_Command4(...) interface against the
  Stage 6 skip-settings full M4 package.

  Difference from Stage 6 M7:
    - Does not judge loopback immediately after sending the command.
    - Polls EROS_M4_GetStatusValue(...) until M4 reports the requested echo
      or until a timeout occurs.

  This matches the full M4 architecture where EROS_M4_Command4 queues the
  command and State_ProcessPendingCommands()/Control_Task() processes it later.
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
static bool gM4ReadyForRpc = false;
static bool gWaitingForEcho = false;

static unsigned long gLastSerialRpcAttemptMs = 0;
static unsigned long gFirstM4TextMs = 0;
static unsigned long gLastHeartbeatMs = 0;
static unsigned long gLastPingMs = 0;
static unsigned long gEchoWaitStartMs = 0;
static unsigned long gLastEchoPollMs = 0;
static unsigned long gHeartbeatCounter = 0;
static int gRequestId = 0;
static int gWaitingRequestId = 0;
static int gLastAccepted = 0;

const unsigned long PING_INTERVAL_MS = 3000;
const unsigned long ECHO_POLL_INTERVAL_MS = 50;
const unsigned long ECHO_TIMEOUT_MS = 1000;

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

  Serial.println(String("M7: SerialRPC.begin attempt at millis=") + String(nowMs));

  gSerialRpcStarted = SerialRPC.begin();

  Serial.println(String("M7: SerialRPC.begin() returned ") + String(gSerialRpcStarted ? "true" : "false"));

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

    if (buffer.indexOf("M4_READY_FOR_RPC") >= 0)
    {
      if (!gM4ReadyForRpc)
      {
        gM4ReadyForRpc = true;
        Serial.println("M7: detected M4_READY_FOR_RPC marker");
      }
    }
  }
}

static int GetStatusValue(int selector)
{
  return RPC.call("EROS_M4_GetStatusValue", selector).as<int>();
}

static void PrintDetailedStatus(const char * prefix, int requestId, int accepted)
{
  int m4Request = GetStatusValue(0);
  int m4Echo = GetStatusValue(1);
  int echoCount = GetStatusValue(2);
  int statusCounter = GetStatusValue(4);
  int acceptedCount = GetStatusValue(6);
  int rejectedCount = GetStatusValue(7);
  int queueDepth = GetStatusValue(8);
  int queueCapacity = GetStatusValue(9);

  bool ok = (accepted == 1 && m4Request == requestId && m4Echo == requestId);

  Serial.println(
    String(prefix) +
    String(" sent=") +
    String(requestId) +
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

static void PollForEcho()
{
  if (!gWaitingForEcho)
  {
    return;
  }

  unsigned long nowMs = millis();

  if (nowMs - gLastEchoPollMs < ECHO_POLL_INTERVAL_MS)
  {
    return;
  }

  gLastEchoPollMs = nowMs;

  int m4Request = GetStatusValue(0);
  int m4Echo = GetStatusValue(1);

  if (m4Request == gWaitingRequestId && m4Echo == gWaitingRequestId)
  {
    unsigned long elapsedMs = nowMs - gEchoWaitStartMs;

    Serial.println(
      String("M7: echo matched request id ") +
      String(gWaitingRequestId) +
      String(" after ") +
      String(elapsedMs) +
      String(" ms")
    );

    PrintDetailedStatus("M7: transport4 final status", gWaitingRequestId, gLastAccepted);

    gWaitingForEcho = false;
    return;
  }

  if (nowMs - gEchoWaitStartMs >= ECHO_TIMEOUT_MS)
  {
    Serial.println(
      String("M7: echo TIMEOUT for request id ") +
      String(gWaitingRequestId) +
      String(", last m4Request=") +
      String(m4Request) +
      String(", last echo=") +
      String(m4Echo)
    );

    PrintDetailedStatus("M7: transport4 timeout status", gWaitingRequestId, gLastAccepted);

    gWaitingForEcho = false;
  }
}

static void RunTransportLoopbackTest()
{
  if (!gSerialRpcStarted || !gSawM4Text || !gM4ReadyForRpc)
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
    Serial.println("M7: handshake delay complete, beginning EROS transport4 poll-echo test");
  }

  PollForEcho();

  if (gWaitingForEcho)
  {
    return;
  }

  if (nowMs - gLastPingMs < PING_INTERVAL_MS)
  {
    return;
  }

  gLastPingMs = nowMs;
  gRequestId++;
  gWaitingRequestId = gRequestId;

  Serial.println(String("M7: sending EROS_CMD_TRANSPORT_LOOPBACK_PING request id ") + String(gRequestId));

  gLastAccepted = RPC.call(
    "EROS_M4_Command4",
    (int)EROS_CMD_TRANSPORT_LOOPBACK_PING,
    0,
    gRequestId,
    0
  ).as<int>();

  Serial.println(String("M7: EROS_M4_Command4 returned ") + String(gLastAccepted));

  gWaitingForEcho = true;
  gEchoWaitStartMs = millis();
  gLastEchoPollMs = 0;
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
    String(", ready=") +
    String(gM4ReadyForRpc ? "true" : "false") +
    String(", rpcTest=") +
    String(gRpcTestEnabled ? "true" : "false") +
    String(", waitingEcho=") +
    String(gWaitingForEcho ? "true" : "false")
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
  Serial.println("M7: stage7 poll-echo test against skip-settings full M4 starting");
  Serial.println("M7: waits for M4_READY_FOR_RPC, sends Command4, then polls status until echo catches up");
  Serial.println("M7: use with IOBox_M4_stage6_skip_settings");
}

void loop()
{
  TryStartSerialRPC();
  PrintSerialRPCMessages();
  SendHeartbeat();
  RunTransportLoopbackTest();

  delay(1);
}
