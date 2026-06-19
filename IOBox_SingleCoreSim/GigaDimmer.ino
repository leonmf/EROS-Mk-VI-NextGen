#include "EROSShared.h"

#if EROS_BUILD_HAS_M4_SIDE

/*
  GigaDimmer.ino

  RobotDyn-style AC dimmer driver for Arduino Giga R1.

  Non-blocking / hardware-timer scheduled version.

  Pins:
    Zero cross input: 2
    Triac gate output: 3

  Notes:
    - Smaller delay = more power.
    - 0% disables triac firing.
    - Relay enable for Dimmer is handled elsewhere through OutValues[]
      and SetOutputs(), because pin 53 is part of the normal output array.
*/

#include "EROSShared.h"
#include <mbed.h>

// ------------------------------------------------------------
// Timing constants
// ------------------------------------------------------------

// 60 Hz AC half-cycle is about 8333 us.
// These values match the stable range we found experimentally.
//
// If you later want more aggressive maximum power, lower FASTEST.
// If the motor gets weird near the top, raise FASTEST.
#define DIMMER_FASTEST_DELAY_US       2400
#define DIMMER_SLOWEST_DELAY_US       7800
#define DIMMER_TRIAC_PULSE_US         500

// Optional guard: ignore impossible extra zero-cross events too close together.
// 60 Hz half-cycle is ~8333 us, so 3000 us is safely below that.
#define DIMMER_MIN_ZC_INTERVAL_US     3000

// ------------------------------------------------------------
// Runtime state
// ------------------------------------------------------------

static volatile uint16_t g_dimmerFireDelayUs = DIMMER_SLOWEST_DELAY_US;
static volatile uint8_t g_dimmerPowerPercent = 0;
static volatile bool g_dimmerEnabled = false;

static volatile unsigned long g_zeroCrossCount = 0;
static volatile unsigned long g_fireCount = 0;
static volatile unsigned long g_gateOffCount = 0;

static volatile unsigned long g_lastZeroCrossMicros = 0;

// Hardware timer callbacks
static mbed::Timeout g_gateFireTimer;
static mbed::Timeout g_gateOffTimer;

// ------------------------------------------------------------
// Timer callback: turn gate off
// ------------------------------------------------------------

static void GigaDimmer_gateOffCallback()
{
  digitalWrite(DIMMER_GATE_PIN, LOW);
  g_gateOffCount++;
}

// ------------------------------------------------------------
// Timer callback: fire triac gate pulse
// ------------------------------------------------------------

static void GigaDimmer_gateFireCallback()
{
  if (!g_dimmerEnabled || g_dimmerPowerPercent == 0) {
    digitalWrite(DIMMER_GATE_PIN, LOW);
    return;
  }

  digitalWrite(DIMMER_GATE_PIN, HIGH);
  g_fireCount++;

  // Schedule gate off after short pulse.
  g_gateOffTimer.detach();
  g_gateOffTimer.attach_us(
    mbed::callback(GigaDimmer_gateOffCallback),
    DIMMER_TRIAC_PULSE_US
  );
}

// ------------------------------------------------------------
// Zero-cross interrupt
// ------------------------------------------------------------

void GigaDimmer_zeroCrossISR()
{
  unsigned long nowUs = micros();
  unsigned long dtUs = nowUs - g_lastZeroCrossMicros;

  // Ignore noise / double triggers that are impossibly close together.
  if (dtUs < DIMMER_MIN_ZC_INTERVAL_US) {
    return;
  }

  g_lastZeroCrossMicros = nowUs;
  g_zeroCrossCount++;

  if (!g_dimmerEnabled || g_dimmerPowerPercent == 0) {
    digitalWrite(DIMMER_GATE_PIN, LOW);
    g_gateFireTimer.detach();
    g_gateOffTimer.detach();
    return;
  }

  uint16_t localDelay = g_dimmerFireDelayUs;

  // Cancel any pending gate pulse from the previous half-cycle.
  g_gateFireTimer.detach();
  g_gateOffTimer.detach();
  digitalWrite(DIMMER_GATE_PIN, LOW);

  // Schedule gate pulse after phase delay.
  g_gateFireTimer.attach_us(
    mbed::callback(GigaDimmer_gateFireCallback),
    localDelay
  );
}

// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------

void GigaDimmer_Setup()
{
  pinMode(DIMMER_GATE_PIN, OUTPUT);
  digitalWrite(DIMMER_GATE_PIN, LOW);

  pinMode(DIMMER_ZC_PIN, INPUT);

  g_dimmerPowerPercent = 0;
  g_dimmerFireDelayUs = DIMMER_SLOWEST_DELAY_US;
  g_dimmerEnabled = false;

  g_zeroCrossCount = 0;
  g_fireCount = 0;
  g_gateOffCount = 0;
  g_lastZeroCrossMicros = 0;

  g_gateFireTimer.detach();
  g_gateOffTimer.detach();

  attachInterrupt(
    digitalPinToInterrupt(DIMMER_ZC_PIN),
    GigaDimmer_zeroCrossISR,
    RISING
  );
}

// ------------------------------------------------------------
// Public control functions
// ------------------------------------------------------------

void GigaDimmer_setPower(uint8_t percent)
{
  percent = constrain(percent, 0, 100);

  uint16_t newDelay;

  if (percent == 0) {
    newDelay = DIMMER_SLOWEST_DELAY_US;
  }
  else {
    newDelay = map(
      percent,
      1,
      100,
      DIMMER_SLOWEST_DELAY_US,
      DIMMER_FASTEST_DELAY_US
    );
  }

  noInterrupts();
  g_dimmerPowerPercent = percent;
  g_dimmerFireDelayUs = newDelay;
  interrupts();

  if (percent == 0) {
    digitalWrite(DIMMER_GATE_PIN, LOW);
    g_gateFireTimer.detach();
    g_gateOffTimer.detach();
  }
}

void GigaDimmer_enable(bool enabled)
{
  noInterrupts();
  g_dimmerEnabled = enabled;
  interrupts();

  if (!enabled) {
    digitalWrite(DIMMER_GATE_PIN, LOW);
    g_gateFireTimer.detach();
    g_gateOffTimer.detach();
  }
}

#endif  // EROS_BUILD_HAS_M4_SIDE
