/*
  IOBox_M7.ino - Stage 3 RPC Call Test

  Purpose:
    - Start Serial / SerialRPC
    - Drain and print M4 SerialRPC messages
    - After a startup delay, call one simple M4 RPC function
    - Confirm the return value

  Deliberately not included:
    - No EROS transport
    - No display / LVGL
    - No old M7 worker thread
    - No IO control logic
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

uint32_t LastM7HeartbeatMs = 0;
uint32_t M7HeartbeatCounter = 0;

uint32_t LastCallMs = 0;
uint32_t CallCounter = 0;
bool CallsEnabled = false;

const uint32_t STARTUP_DELAY_MS = 8000;
const uint32_t CALL_INTERVAL_MS = 3000;

void PrintSerialRPCMessages()
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
      String(nowMs)
    );
  }
}

void RunRpcCallTest()
{
  uint32_t nowMs = millis();

  if (!CallsEnabled)
  {
    if (nowMs >= STARTUP_DELAY_MS)
    {
      CallsEnabled = true;
      LastCallMs = 0;
      Serial.println("M7: startup delay complete, beginning RPC.call test");
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
  Serial.println("M7: stage 3 RPC-call test starting");
  Serial.println("M7: starts SerialRPC, prints M4 messages, then calls one M4 function");
  Serial.println("M7: no display, no EROS transport, no control loop");

  bool rpcStarted = SerialRPC.begin();

  Serial.println(
    String("M7: SerialRPC.begin() returned ") +
    String(rpcStarted ? "true" : "false")
  );

  Serial.println("M7: setup complete, waiting before first RPC.call");
}

void loop()
{
  PrintSerialRPCMessages();
  RunM7Heartbeat();
  RunRpcCallTest();

  delay(1);
}
