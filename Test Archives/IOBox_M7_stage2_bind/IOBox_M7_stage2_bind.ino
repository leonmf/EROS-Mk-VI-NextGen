/*
  IOBox_M7 Stage 2 - Drain SerialRPC Only

  Purpose:
    - Start SerialRPC, which wakes M4
    - Drain M4 SerialRPC messages to USB Serial
    - Do NOT call any M4 RPC function yet

  Upload to M7 core second.
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

uint32_t LastM7HeartbeatMs = 0;
uint32_t M7HeartbeatCounter = 0;

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

void setup()
{
  Serial.begin(115200);

  uint32_t serialStartMs = millis();
  while (!Serial && millis() - serialStartMs < 3000)
  {
    delay(10);
  }

  Serial.println();
  Serial.println("M7: stage 2 bind-only test starting");
  Serial.println("M7: this sketch starts SerialRPC and prints M4 messages");
  Serial.println("M7: M4 binds one RPC function, but M7 does not call it yet");

  bool serialRpcOk = SerialRPC.begin();

  Serial.println(
    "M7: SerialRPC.begin() returned " +
    String(serialRpcOk ? "true" : "false")
  );

  Serial.println("M7: setup complete, waiting for M4 heartbeat");
}

void loop()
{
  PrintSerialRPCMessages();

  uint32_t nowMs = millis();

  if (nowMs - LastM7HeartbeatMs >= 2000)
  {
    LastM7HeartbeatMs = nowMs;
    M7HeartbeatCounter++;

    Serial.println(
      "M7: heartbeat " +
      String(M7HeartbeatCounter) +
      ", millis=" +
      String(nowMs)
    );
  }

  delay(1);
}
