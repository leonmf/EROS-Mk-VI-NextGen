/*
  IOBox_M7.ino

  EROS Mk VI M7-side RPC loopback test.

  Purpose:
    - Keep M7 simple and stable.
    - Do not start LVGL / Giga Display yet.
    - Do not start the older M7 transport worker thread yet.
    - Talk directly to the M4 RPC endpoints exposed by the current IOBox_M4 build:
        EROS_M4_Command(...)
        EROS_M4_GetStatusValue(selector)

  Upload order:
    1. Upload IOBox_M4 to the M4 core.
    2. Upload this IOBox_M7 to the M7 core.
    3. Open Serial Monitor at 115200 baud.
*/

#include "EROSShared.h"
#include "RPC.h"
#include "SerialRPC.h"

static unsigned long g_lastLoopbackSendMs = 0;
static unsigned long g_lastStatusPollMs = 0;
static unsigned long g_loopbackRequestId = 0;

static const unsigned long LOOPBACK_SEND_INTERVAL_MS = 1000;
static const unsigned long STATUS_POLL_INTERVAL_MS = 500;

static int ReadM4StatusValue(int selector)
{
  return RPC.call("EROS_M4_GetStatusValue", selector).as<int>();
}

static int SendM4Command(
  EROS_CommandType type,
  int index,
  long value,
  bool boolValue,
  bool onSettings
)
{
  return RPC.call(
    "EROS_M4_Command",
    (int)type,
    index,
    (int)value,
    boolValue ? 1 : 0,
    onSettings ? 1 : 0
  ).as<int>();
}

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
  }
}

static void SendLoopbackPing()
{
  unsigned long nowMs = millis();

  if (nowMs - g_lastLoopbackSendMs < LOOPBACK_SEND_INTERVAL_MS)
  {
    return;
  }

  g_lastLoopbackSendMs = nowMs;
  g_loopbackRequestId++;

  Serial.println();
  Serial.println(
    "M7: sending loopback ping request id " +
    String(g_loopbackRequestId)
  );

  int accepted = SendM4Command(
    EROS_CMD_TRANSPORT_LOOPBACK_PING,
    0,
    (long)g_loopbackRequestId,
    false,
    false
  );

  Serial.println(
    "M7: EROS_M4_Command returned " +
    String(accepted)
  );
}

static void PollM4Status()
{
  unsigned long nowMs = millis();

  if (nowMs - g_lastStatusPollMs < STATUS_POLL_INTERVAL_MS)
  {
    return;
  }

  g_lastStatusPollMs = nowMs;

  unsigned long m4RequestId = (unsigned long)ReadM4StatusValue(0);
  unsigned long m4EchoId = (unsigned long)ReadM4StatusValue(1);
  unsigned long m4EchoCounter = (unsigned long)ReadM4StatusValue(2);
  unsigned long m4EchoMillis = (unsigned long)ReadM4StatusValue(3);
  unsigned long m4StatusCounter = (unsigned long)ReadM4StatusValue(4);
  unsigned long m4StatusMillis = (unsigned long)ReadM4StatusValue(5);
  unsigned long m4Accepted = (unsigned long)ReadM4StatusValue(6);
  unsigned long m4Rejected = (unsigned long)ReadM4StatusValue(7);
  int m4QueueDepth = ReadM4StatusValue(8);
  int m4QueueCapacity = ReadM4StatusValue(9);

  bool loopbackOk =
    (g_loopbackRequestId > 0) &&
    (m4EchoId == g_loopbackRequestId);

  Serial.println(
    "M7: status "
    "sent=" + String(g_loopbackRequestId) +
    ", m4Request=" + String(m4RequestId) +
    ", echo=" + String(m4EchoId) +
    ", echoCount=" + String(m4EchoCounter) +
    ", ok=" + String(loopbackOk ? "YES" : "WAIT")
  );

  Serial.println(
    "M7: m4 statusCounter=" + String(m4StatusCounter) +
    ", statusAgeMs=" + String(nowMs - m4StatusMillis) +
    ", echoAgeMs=" + String(nowMs - m4EchoMillis) +
    ", accepted=" + String(m4Accepted) +
    ", rejected=" + String(m4Rejected) +
    ", queue=" + String(m4QueueDepth) +
    "/" + String(m4QueueCapacity)
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
  Serial.println("M7: IOBox_M7 RPC loopback test starting");
  Serial.println("M7: initializing SerialRPC");

  if (!SerialRPC.begin())
  {
    Serial.println("M7: SerialRPC initialization failed");
  }
  else
  {
    Serial.println("M7: SerialRPC initialization complete");
  }

  // Give M4 time to finish setup and bind RPC endpoints.
  delay(3000);

  Serial.println("M7: starting loopback polling");
}

void loop()
{
  PrintSerialRPCMessages();
  SendLoopbackPing();
  PollM4Status();

  delay(1);
}
