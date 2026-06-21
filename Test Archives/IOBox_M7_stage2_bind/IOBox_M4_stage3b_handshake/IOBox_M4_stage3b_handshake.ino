/*
  IOBox_M4_stage3b_handshake.ino

  Stage 3B RPC Call Handshake Test

  Purpose:
    - M4 starts SerialRPC
    - M4 binds one simple RPC function
    - M4 sends setup/heartbeat text
    - M7 should wait until it sees M4 text before making RPC.call()

  Deliberately not included:
    - No EROS transport
    - No Control_Setup()
    - No Control_Task()
    - No display
    - No IO pin setup
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

uint32_t LastHeartbeatMs = 0;
uint32_t HeartbeatCounter = 0;

int M4_Stage3Ping(int value)
{
  SerialRPC.println(
    String("M4: M4_Stage3Ping received ") +
    String(value)
  );

  return value + 1;
}

void setup()
{
  SerialRPC.begin();

  SerialRPC.println("M4: stage 3B setup starting");

  RPC.bind("M4_Stage3Ping", M4_Stage3Ping);

  SerialRPC.println("M4: stage 3B RPC.bind complete");
  SerialRPC.println("M4: stage 3B setup complete");
}

void loop()
{
  uint32_t nowMs = millis();

  if (nowMs - LastHeartbeatMs >= 2000)
  {
    LastHeartbeatMs = nowMs;
    HeartbeatCounter++;

    SerialRPC.println(
      String("M4: stage 3B heartbeat ") +
      String(HeartbeatCounter) +
      String(", millis=") +
      String(nowMs)
    );
  }

  delay(1);
}
