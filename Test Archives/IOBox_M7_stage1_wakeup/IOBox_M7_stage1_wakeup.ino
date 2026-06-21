/*
  IOBox_M7_stage1_wakeup.ino

  Bare dual-core wake-up test for Arduino Giga R1 M7 core.

  Purpose:
    - Start SerialRPC from M7, which should start/cooperate with the M4 core.
    - Drain M4 SerialRPC text and print it to USB Serial.

  Intentionally NOT included:
    - No display/LVGL
    - No EROSTransport
    - No RPC.call()
    - No M4 command/status polling
    - No pin setup

  Upload this to the M7 core second.
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

uint32_t LastM7HeartbeatMs = 0;
uint32_t M7HeartbeatCounter = 0;

String CurrentCPU()
{
  if (RPC.cpu_id() == CM7_CPUID)
  {
    return "M7";
  }

  return "M4";
}

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

  // Wait briefly for USB Serial, but do not block forever.
  uint32_t serialStartMs = millis();

  while (!Serial && millis() - serialStartMs < 3000)
  {
    delay(10);
  }

  Serial.println();
  Serial.println("M7: stage 1 wake-up test starting");
  Serial.println("M7: this sketch only starts SerialRPC and prints M4 messages");
  Serial.println("M7: no RPC.call(), no display, no transport, no control loop");

  if (!SerialRPC.begin())
  {
    Serial.println("M7: SerialRPC.begin() returned false");
  }
  else
  {
    Serial.println("M7: SerialRPC.begin() returned true");
  }

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
