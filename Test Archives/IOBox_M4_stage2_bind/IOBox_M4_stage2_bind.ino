/*
  IOBox_M4 Stage 2 - RPC Bind Only

  Purpose:
    - Prove M7 can wake M4
    - Prove M4 can initialize SerialRPC
    - Prove M4 can bind a simple RPC function
    - M7 does NOT call the function in this stage

  Upload to M4 core first.
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

uint32_t LastHeartbeatMs = 0;
uint32_t HeartbeatCounter = 0;

int M4_Stage2Ping(int value)
{
  SerialRPC.println("M4: Stage2Ping called with " + String(value));
  return value + 1;
}

void setup()
{
  SerialRPC.begin();

  SerialRPC.println("M4: stage 2 bind-only setup starting");

  RPC.bind("M4_Stage2Ping", M4_Stage2Ping);

  SerialRPC.println("M4: stage 2 RPC.bind complete");
  SerialRPC.println("M4: stage 2 setup complete");
}

void loop()
{
  uint32_t nowMs = millis();

  if (nowMs - LastHeartbeatMs >= 2000)
  {
    LastHeartbeatMs = nowMs;
    HeartbeatCounter++;

    SerialRPC.println(
      "M4: stage 2 heartbeat " +
      String(HeartbeatCounter) +
      ", millis=" +
      String(nowMs)
    );
  }

  delay(1);
}
