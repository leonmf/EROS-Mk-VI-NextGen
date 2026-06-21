/*
  IOBox_M4.ino - Stage 3 RPC Call Test

  Purpose:
    - Prove that M7 can wake M4
    - Prove SerialRPC heartbeat still works
    - Prove M4 can bind one simple RPC function
    - Prove M7 can call that function and receive a return value

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

  SerialRPC.println("M4: stage 3 RPC-call test setup starting");

  RPC.bind("M4_Stage3Ping", M4_Stage3Ping);

  SerialRPC.println("M4: stage 3 RPC.bind complete");
  SerialRPC.println("M4: stage 3 setup complete");
}

void loop()
{
  uint32_t nowMs = millis();

  if (nowMs - LastHeartbeatMs >= 2000)
  {
    LastHeartbeatMs = nowMs;
    HeartbeatCounter++;

    SerialRPC.println(
      String("M4: stage 3 heartbeat ") +
      String(HeartbeatCounter) +
      String(", millis=") +
      String(nowMs)
    );
  }

  delay(1);
}
