/*
  Giga R1 M4 EROS-Style Output Mode Proof

  Role:
    - Runs on M4
    - Owns output timing
    - Supports OFF, ON, PULSE, RANDOM
    - Receives mode/timing commands from M7 using RPC
    - Reports status to M7 using SerialRPC

  Upload this sketch to the M4 core first.
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

// Start with LED_BUILTIN.
// After this works, change to 52 or another real output pin.
const int TEST_OUTPUT_PIN = 47;

// EROS-style mode numbers
const int MODE_OFF    = 0;
const int MODE_ON     = 1;
const int MODE_PULSE  = 4;
const int MODE_RANDOM = 5;

volatile int gMode = MODE_OFF;
volatile uint32_t gOnTimeMs = 500;
volatile uint32_t gOffTimeMs = 500;

bool outputState = false;

uint32_t lastToggleMs = 0;
uint32_t nextToggleDelayMs = 500;

uint32_t lastHeartbeatMs = 0;

String modeName(int mode) {
  switch (mode) {
    case MODE_OFF:
      return "OFF";

    case MODE_ON:
      return "ON";

    case MODE_PULSE:
      return "PULSE";

    case MODE_RANDOM:
      return "RANDOM";

    default:
      return "UNKNOWN";
  }
}

String currentCPU() {
  if (RPC.cpu_id() == CM7_CPUID) {
    return "M7";
  } else {
    return "M4";
  }
}

uint32_t clampTime(int valueMs) {
  if (valueMs < 25) {
    return 25;
  }

  if (valueMs > 60000) {
    return 60000;
  }

  return (uint32_t)valueMs;
}

uint32_t calculateNextDelay(bool currentOutputState, bool randomEnabled) {
  uint32_t baseDelayMs;

  if (currentOutputState) {
    baseDelayMs = gOnTimeMs;
  } else {
    baseDelayMs = gOffTimeMs;
  }

  if (!randomEnabled) {
    return baseDelayMs;
  }

  // Arduino random(min, max) returns min through max - 1.
  // This gives 1 through baseDelayMs, inclusive.
  return (uint32_t)random(1, baseDelayMs + 1);
}

void forceOutput(bool newState) {
  outputState = newState;
  digitalWrite(TEST_OUTPUT_PIN, outputState ? HIGH : LOW);
}

void resetTiming() {
  lastToggleMs = millis();
  nextToggleDelayMs = calculateNextDelay(outputState, gMode == MODE_RANDOM);
}

/*
  RPC command from M7.

  Args:
    mode:
      0 = OFF
      1 = ON
      4 = PULSE
      5 = RANDOM

    onTimeMs:
      ON duration for PULSE/RANDOM

    offTimeMs:
      OFF duration for PULSE/RANDOM

  Return:
    Accepted mode number
*/
int m4SetOutputConfig(int mode, int onTimeMs, int offTimeMs) {
  if (
    mode != MODE_OFF &&
    mode != MODE_ON &&
    mode != MODE_PULSE &&
    mode != MODE_RANDOM
  ) {
    SerialRPC.println(
      currentCPU() +
      ": rejected unknown mode " +
      String(mode)
    );

    return gMode;
  }

  gMode = mode;
  gOnTimeMs = clampTime(onTimeMs);
  gOffTimeMs = clampTime(offTimeMs);

  if (gMode == MODE_OFF) {
    forceOutput(false);
  } else if (gMode == MODE_ON) {
    forceOutput(true);
  } else {
    // For pulse/random modes, restart from OFF so the transition is obvious.
    forceOutput(false);
  }

  resetTiming();

  SerialRPC.println(
    currentCPU() +
    ": config accepted, mode = " +
    modeName(gMode) +
    ", on = " +
    String(gOnTimeMs) +
    " ms, off = " +
    String(gOffTimeMs) +
    " ms"
  );

  return gMode;
}

/*
  Simple ping for proving RPC call/return still works.
*/
int m4Ping(int value) {
  SerialRPC.println(
    currentCPU() +
    ": ping received " +
    String(value)
  );

  return value + 1;
}

void runOutputEngine() {
  uint32_t nowMs = millis();

  switch (gMode) {
    case MODE_OFF:
      if (outputState) {
        forceOutput(false);
      }
      break;

    case MODE_ON:
      if (!outputState) {
        forceOutput(true);
      }
      break;

    case MODE_PULSE:
    case MODE_RANDOM:
      if (nowMs - lastToggleMs >= nextToggleDelayMs) {
        lastToggleMs = nowMs;

        forceOutput(!outputState);

        nextToggleDelayMs = calculateNextDelay(outputState, gMode == MODE_RANDOM);

        SerialRPC.println(
          currentCPU() +
          ": toggled output " +
          String(outputState ? "ON" : "OFF") +
          ", next delay = " +
          String(nextToggleDelayMs) +
          " ms"
        );
      }
      break;

    default:
      forceOutput(false);
      break;
  }
}

void sendHeartbeat() {
  uint32_t nowMs = millis();

  if (nowMs - lastHeartbeatMs >= 1000) {
    lastHeartbeatMs = nowMs;

    SerialRPC.println(
      currentCPU() +
      ": heartbeat, mode = " +
      modeName(gMode) +
      ", output = " +
      String(outputState ? "ON" : "OFF") +
      ", on = " +
      String(gOnTimeMs) +
      " ms, off = " +
      String(gOffTimeMs) +
      " ms, next delay = " +
      String(nextToggleDelayMs) +
      " ms"
    );
  }
}

void setup() {
  pinMode(TEST_OUTPUT_PIN, OUTPUT);
  forceOutput(false);

  randomSeed(analogRead(A0) + millis());

  SerialRPC.begin();

  RPC.bind("m4SetOutputConfig", m4SetOutputConfig);
  RPC.bind("m4Ping", m4Ping);

  SerialRPC.println(currentCPU() + ": M4 output engine setup complete");
}

void loop() {
  runOutputEngine();
  sendHeartbeat();

  delay(1);
}