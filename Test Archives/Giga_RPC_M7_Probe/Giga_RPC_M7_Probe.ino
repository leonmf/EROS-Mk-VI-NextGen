/*
  Giga R1 M7 EROS-Style Output Mode Proof

  Role:
    - Runs on M7
    - Initializes Serial / SerialRPC
    - Prints M4 status messages to USB Serial
    - Cycles M4 through OFF, ON, PULSE, RANDOM
    - Does NOT do output timing itself

  Upload this sketch to the M7 core second.
*/

#include "Arduino.h"
#include "RPC.h"
#include "SerialRPC.h"

// EROS-style mode numbers
const int MODE_OFF    = 0;
const int MODE_ON     = 1;
const int MODE_PULSE  = 4;
const int MODE_RANDOM = 5;

struct ModeCommand {
  int mode;
  int onTimeMs;
  int offTimeMs;
  int holdTimeMs;
  const char* name;
};

ModeCommand testSequence[] = {
  { MODE_OFF,    500, 500, 5000,  "OFF"    },
  { MODE_ON,     500, 500, 5000,  "ON"     },
  { MODE_PULSE,  300, 700, 10000, "PULSE"  },
  { MODE_RANDOM, 300, 700, 10000, "RANDOM" }
};

const int testSequenceCount = sizeof(testSequence) / sizeof(testSequence[0]);

int sequenceIndex = 0;

uint32_t lastModeChangeMs = 0;
uint32_t lastPingMs = 0;

String currentCPU() {
  if (RPC.cpu_id() == CM7_CPUID) {
    return "M7";
  } else {
    return "M4";
  }
}

void printSerialRPCMessages() {
  String buffer = "";

  while (SerialRPC.available()) {
    buffer += (char)SerialRPC.read();
  }

  if (buffer.length() > 0) {
    Serial.print(buffer);
  }
}

void sendCurrentCommand() {
  ModeCommand cmd = testSequence[sequenceIndex];

  Serial.println();
  Serial.println(
    "M7: sending mode command: " +
    String(cmd.name) +
    ", on = " +
    String(cmd.onTimeMs) +
    " ms, off = " +
    String(cmd.offTimeMs) +
    " ms"
  );

  int acceptedMode = RPC.call(
    "m4SetOutputConfig",
    cmd.mode,
    cmd.onTimeMs,
    cmd.offTimeMs
  ).as<int>();

  Serial.println(
    "M7: M4 accepted mode number = " +
    String(acceptedMode)
  );

  lastModeChangeMs = millis();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);

  // Wait briefly for Serial, but do not block forever.
  uint32_t serialStartMs = millis();

  while (!Serial && millis() - serialStartMs < 3000) {
    delay(10);
  }

  Serial.println();
  Serial.println("M7: starting EROS-style mode commander");

  if (!SerialRPC.begin()) {
    Serial.println("M7: SerialRPC initialization failed");
  } else {
    Serial.println("M7: SerialRPC initialization complete");
  }

  // Give the M4 time to start and bind RPC functions.
  delay(2000);

  Serial.println("M7: setup complete");

  sendCurrentCommand();
}

void loop() {
  uint32_t nowMs = millis();

  // Keep draining M4 status text.
  printSerialRPCMessages();

  ModeCommand currentCommand = testSequence[sequenceIndex];

  if (nowMs - lastModeChangeMs >= (uint32_t)currentCommand.holdTimeMs) {
    sequenceIndex++;

    if (sequenceIndex >= testSequenceCount) {
      sequenceIndex = 0;
    }

    sendCurrentCommand();
  }

  // Periodic ping to prove call/return still works while mode engine is running.
  if (nowMs - lastPingMs >= 7000) {
    lastPingMs = nowMs;

    int value = random(100);

    Serial.println(
      "M7: calling m4Ping(" +
      String(value) +
      ")"
    );

    int result = RPC.call("m4Ping", value).as<int>();

    Serial.println(
      "M7: m4Ping result = " +
      String(result)
    );
  }

  // Fast blink on M7 so you know M7 is alive independently.
  static uint32_t lastM7LedMs = 0;
  static bool m7LedState = false;

  if (nowMs - lastM7LedMs >= 100) {
    lastM7LedMs = nowMs;
    m7LedState = !m7LedState;
    digitalWrite(LED_BUILTIN, m7LedState ? HIGH : LOW);
  }

  delay(1);
}