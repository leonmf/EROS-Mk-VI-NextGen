/*
  IOBox_M7_stage3b_handshake.ino

  Stage 3B RPC Call Handshake Test

  Purpose:
    - Start Serial
    - Try to start SerialRPC
    - Print M4 SerialRPC messages
    - Do NOT call RPC unless:
        1. SerialRPC.begin() has returned true
        2. M7 has seen text from M4
        3. A short post-handshake delay has elapsed

  This avoids the Stage 3 failure where RPC.call() blocked after
  SerialRPC.begin() returned false.

  Deliberately not included:
    - No EROS transport
    - No display / LVGL
    - No old M7 worker thread
    - No IO control logic
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

bool SerialRpcStarted = false;
bool SawM4Text = false;
bool CallsEnabled = false;

uint32_t LastSerialRpcBeginAttemptMs = 0;
uint32_t SerialRpcBeginAttemptCounter = 0;

uint32_t FirstM4TextMs = 0;
uint32_t LastM7HeartbeatMs = 0;
uint32_t M7HeartbeatCounter = 0;

uint32_t LastCallMs = 0;
uint32_t CallCounter = 0;

const uint32_t SERIALRPC_RETRY_INTERVAL_MS = 1000;
const uint32_t POST_M4_TEXT_DELAY_MS = 3000;
const uint32_t CALL_INTERVAL_MS = 3000;

void TryStartSerialRPC()
{
  if (SerialRpcStarted)
  {
    return;
  }

  uint32_t nowMs = millis();

  if (nowMs - LastSerialRpcBeginAttemptMs < SERIALRPC_RETRY_INTERVAL_MS)
  {
    return;
  }

  LastSerialRpcBeginAttemptMs = nowMs;
  SerialRpcBeginAttemptCounter++;

  Serial.println(
    String("M7: SerialRPC.begin attempt ") +
    String(SerialRpcBeginAttemptCounter)
  );

  SerialRpcStarted = SerialRPC.begin();

  Serial.println(
    String("M7: SerialRPC.begin() returned ") +
    String(SerialRpcStarted ? "true" : "false")
  );
}

void PrintSerialRPCMessages()
{
  if (!SerialRpcStarted)
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

    if (!SawM4Text)
    {
      SawM4Text = true;
      FirstM4TextMs = millis();
      Serial.println("M7: detected first M4 SerialRPC text");
    }
  }
}

void RunM7Heartbeat()
{
  uint32_t nowMs = millis();

  if (nowMs - LastM7HeartbeatMs >= 2000)
  {
    LastM7HeartbeatMs = nowMs;
    M7HeartbeatCounter++;

    Serial.println(
      String("M7: heartbeat ") +
      String(M7HeartbeatCounter) +
      String(", millis=") +
      String(nowMs) +
      String(", SerialRPC=") +
      String(SerialRpcStarted ? "true" : "false") +
      String(", sawM4=") +
      String(SawM4Text ? "true" : "false")
    );
  }
}

void RunRpcCallTest()
{
  if (!SerialRpcStarted)
  {
    return;
  }

  if (!SawM4Text)
  {
    return;
  }

  uint32_t nowMs = millis();

  if (!CallsEnabled)
  {
    if (nowMs - FirstM4TextMs >= POST_M4_TEXT_DELAY_MS)
    {
      CallsEnabled = true;
      LastCallMs = 0;
      Serial.println("M7: handshake delay complete, beginning RPC.call test");
    }
    else
    {
      return;
    }
  }

  if (nowMs - LastCallMs < CALL_INTERVAL_MS)
  {
    return;
  }

  LastCallMs = nowMs;
  CallCounter++;

  int requestValue = (int)CallCounter;

  Serial.println(
    String("M7: calling M4_Stage3Ping(") +
    String(requestValue) +
    String(")")
  );

  int result = RPC.call("M4_Stage3Ping", requestValue).as<int>();

  Serial.println(
    String("M7: M4_Stage3Ping returned ") +
    String(result) +
    String(", expected ") +
    String(requestValue + 1) +
    String(", ok=") +
    String(result == requestValue + 1 ? "YES" : "NO")
  );
}

void setup()
{
  Serial.begin(115200);

  uint32_t serialStartMs = millis();
  while (!Serial && millis() - serialStartMs < 3000)
  {
    delay(10);
  }

  Serial.println();
  Serial.println("M7: stage 3B handshake RPC-call test starting");
  Serial.println("M7: starts SerialRPC, waits for M4 text, then calls one M4 function");
  Serial.println("M7: no display, no EROS transport, no control loop");
}

void loop()
{
  TryStartSerialRPC();
  PrintSerialRPCMessages();
  RunM7Heartbeat();
  RunRpcCallTest();

  delay(1);
}
