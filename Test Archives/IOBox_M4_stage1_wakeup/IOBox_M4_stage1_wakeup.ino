/*
  IOBox_M4_stage1_wakeup.ino

  Bare dual-core wake-up test for Arduino Giga R1 M4 core.

  Purpose:
    - Prove that the M7 can start the M4 without crashing the board.
    - Prove that M4 can initialize SerialRPC and send heartbeat text.

  Intentionally NOT included:
    - No IOBox control globals
    - No Control_Setup()
    - No Control_Task()
    - No EROSTransport_Setup()
    - No RPC.bind()
    - No RPC.call()
    - No pin setup
    - No display/LVGL

  Upload this to the M4 core first.
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

uint32_t LastHeartbeatMs = 0;
uint32_t HeartbeatCounter = 0;

String CurrentCPU()
{
  if (RPC.cpu_id() == CM7_CPUID)
  {
    return "M7";
  }

  return "M4";
}

void setup()
{
  // This is the only runtime subsystem being tested on M4.
  SerialRPC.begin();

  SerialRPC.println(CurrentCPU() + ": stage 1 wake-up setup complete");
}

void loop()
{
  uint32_t nowMs = millis();

  if (nowMs - LastHeartbeatMs >= 1000)
  {
    LastHeartbeatMs = nowMs;
    HeartbeatCounter++;

    SerialRPC.println(
      CurrentCPU() +
      ": stage 1 heartbeat " +
      String(HeartbeatCounter) +
      ", millis=" +
      String(nowMs)
    );
  }

  delay(1);
}
