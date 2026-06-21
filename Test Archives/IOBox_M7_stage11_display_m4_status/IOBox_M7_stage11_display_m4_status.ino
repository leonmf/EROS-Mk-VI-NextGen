/*
  IOBox_M7_stage11_display_m4_status.ino

  Stage 11 split-core M7 display test with M4-reported status masks.

  Purpose:
    - Keep the known-good M7/M4 ready handshake.
    - Use the stable 4-argument EROS_M4_Command4 transport.
    - Start the Giga Display only after:
        1) SerialRPC is started
        2) M4 text has been seen
        3) M4_READY_FOR_RPC has been seen
        4) loopback has been proven
    - Avoid the old M7 transport worker thread.

  Use with:
    IOBox_M4_stage11_status_masks
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

// These includes are intentionally in the main sketch so the later .ino tabs
// see LVGL/display types during Arduino's sketch concatenation step.
#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"

#include "EROSShared.h"

void GigaDisplay_Setup();
void GigaDisplay_Task();
void EROSTransport_SetM4Ready(bool ready);
void EROSTransport_PollM4Status();

static bool g_serialRpcStarted = false;
static bool g_sawM4Text = false;
static bool g_m4Ready = false;
static bool g_loopbackProven = false;
static bool g_displayStarted = false;

static unsigned long g_lastSerialRpcAttemptMs = 0;
static unsigned long g_lastHeartbeatMs = 0;
static unsigned long g_m4ReadySeenMs = 0;
static unsigned long g_lastLoopbackMs = 0;
static unsigned long g_lastStatusPollMs = 0;
static unsigned long g_loopbackRequestId = 0;

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
      if (!g_m4Ready)
      {
        g_m4ReadySeenMs = millis();
        Serial.println("M7: detected M4_READY_FOR_RPC marker");
      }

      g_m4Ready = true;
      EROSTransport_SetM4Ready(true);
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

  if (accepted == 0)
  {
    return false;
  }

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

  Serial.println("M7: loopback timeout for request id " + String(requestId));
  return false;
}

static void StartDisplayIfReady()
{
  if (g_displayStarted)
  {
    return;
  }

  if (!g_m4Ready)
  {
    return;
  }

  if (millis() - g_m4ReadySeenMs < 3000)
  {
    return;
  }

  if (!g_loopbackProven)
  {
    g_loopbackRequestId++;
    g_loopbackProven = ProveLoopback(g_loopbackRequestId);
    g_lastLoopbackMs = millis();

    if (!g_loopbackProven)
    {
      Serial.println("M7: display startup held because loopback did not pass");
      return;
    }
  }

  Serial.println("M7: starting GigaDisplay_Setup after M4 ready + loopback pass");
  GigaDisplay_Setup();
  g_displayStarted = true;
  Serial.println("M7: GigaDisplay_Setup complete");
}

static void RunHealthChecks()
{
  if (!g_displayStarted)
  {
    return;
  }

  unsigned long nowMs = millis();

  if (nowMs - g_lastStatusPollMs >= 250)
  {
    g_lastStatusPollMs = nowMs;
    EROSTransport_PollM4Status();
  }

  if (nowMs - g_lastLoopbackMs >= 10000)
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
    ", loopback=" +
    String(g_loopbackProven ? "true" : "false") +
    ", display=" +
    String(g_displayStarted ? "true" : "false")
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
  Serial.println("M7: stage11 display with M4-reported status masks starting");
  Serial.println("M7: no display startup until M4_READY_FOR_RPC and loopback pass");
  Serial.println("M7: uses stable Command4 transport and M4-reported output/input masks");
}

void loop()
{
  TryStartSerialRPC();

  if (g_serialRpcStarted)
  {
    PrintSerialRPCMessages();
    StartDisplayIfReady();
    RunHealthChecks();
  }

  if (g_displayStarted)
  {
    GigaDisplay_Task();
  }

  Heartbeat();
  delay(1);
}
