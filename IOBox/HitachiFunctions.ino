/*
  HitachiFunctions.ino

  Hitachi / Dimmer AC output functions.

  Notes:
    - hS.sine has 255 elements, so valid indexes are 0 through 254.
    - Period is protected against zero to avoid divide-by-zero.
    - WriteHitachiOutput() uses the working GigaDimmer 0-100% behavior.
    - Dimmer Relay relay output follows Hitachi output automatically:
        relay ON  when hS.currentOutput >= minRelay
        relay OFF when hS.currentOutput <  minRelay
    OUT_HITACHI_VIRTUAL is the auto-mode logical command that selects Hitachi ON or OFF settings.
    OUT_DIMMER_ENABLE is the physical relay output that supplies the dimmer circuit.
*/

void UpdateHitachiRelayOutput(int currentPercent, int minRelay)
{
  currentPercent = constrain(currentPercent, 0, 100);
  minRelay = constrain(minRelay, 0, 100);

  OutValues[OUT_DIMMER_ENABLE] = (currentPercent > 0 && currentPercent >= minRelay);
}


void Hitachi(int mode, int setPoint, int period, int maxVal, int minVal, int minRelay)
{
  static unsigned long lastTriggerTime = 0;
  static bool high = false;

  float x;
  int currentVal = 0;
  int stageVal = 0;
  int sineIndex = 0;
  bool trigger = false;

  minRelay = constrain(minRelay, 0, 100);

  setPoint = constrain(setPoint, 0, 100);
  maxVal = constrain(maxVal, 0, 100);
  minVal = constrain(minVal, 0, 100);

  // For all active dimmer modes, never allow commanded dimmer percent
  // below the relay-safe minimum.
  if (mode != hitachiOff) {
    setPoint = constrain(setPoint, minRelay, 100);
    maxVal = constrain(maxVal, minRelay, 100);
    minVal = constrain(minVal, minRelay, 100);
  }

  if (maxVal < minVal) {
    int temp = maxVal;
    maxVal = minVal;
    minVal = temp;
  }

  // Prevent divide-by-zero and broken time math.
  if (period <= 0) {
    period = 1;
  }

  unsigned long ct = millis();
  unsigned long dt = ct - lastTriggerTime;

  if (dt >= (unsigned long)period) {
    lastTriggerTime = ct;
    trigger = true;
    dt = 0;
  }
  else {
    trigger = false;
  }

  x = (float)((unsigned long)period - dt) / (float)period;
  x = constrain(x, 0.0, 1.0);

  switch (mode)
  {
    case hitachiOff:
      currentVal = 0;
      hS.currentOutput = 0;
      WriteHitachiOutput(currentVal);
      OutValues[OUT_DIMMER_ENABLE] = false;
      break;

    case hitachiValue:
      stageVal = constrain(setPoint, minRelay, 100);

      currentVal = map(stageVal, 0, 100, 0, 255);
      hS.currentOutput = stageVal;

      WriteHitachiOutput(currentVal);
      UpdateHitachiRelayOutput(hS.currentOutput, minRelay);
      break;

    case hitachiPulse:
      if (trigger == true) {
        if (high == true) {
          high = false;
          stageVal = minVal;
        }
        else {
          high = true;
          stageVal = maxVal;
        }

        stageVal = constrain(stageVal, minRelay, 100);

        currentVal = map(stageVal, 0, 100, 0, 255);
        hS.currentOutput = stageVal;

        WriteHitachiOutput(currentVal);
      }

      UpdateHitachiRelayOutput(hS.currentOutput, minRelay);
      break;

    case hitachiSine:
      // hS.sine has 255 elements: indexes 0-254.
      sineIndex = constrain((int)(x * 254.0), 0, 254);

      stageVal = map(hS.sine[sineIndex], 0, 255, minVal, maxVal);
      stageVal = constrain(stageVal, minRelay, 100);

      currentVal = map(stageVal, 0, 100, 0, 255);
      hS.currentOutput = stageVal;

      WriteHitachiOutput(currentVal);
      UpdateHitachiRelayOutput(hS.currentOutput, minRelay);
      break;

    case hitachiSawTooth:
      stageVal = map((int)(255.0 - (x * 255.0)), 0, 255, minVal, maxVal);
      stageVal = constrain(stageVal, minRelay, 100);

      currentVal = map(stageVal, 0, 100, 0, 255);
      hS.currentOutput = stageVal;

      WriteHitachiOutput(currentVal);
      UpdateHitachiRelayOutput(hS.currentOutput, minRelay);
      break;

    case hitachiTriangle:
      if (x < 0.5) {
        stageVal = map((int)(x * 512.0), 0, 256, minVal, maxVal);
      }
      else {
        stageVal = map((int)(512.0 - (x * 512.0)), 0, 256, minVal, maxVal);
      }

      stageVal = constrain(stageVal, minRelay, 100);

      currentVal = map(stageVal, 0, 100, 0, 255);
      hS.currentOutput = stageVal;

      WriteHitachiOutput(currentVal);
      UpdateHitachiRelayOutput(hS.currentOutput, minRelay);
      break;

    case hitachiRandom:
      if (trigger) {
        stageVal = random(minVal, maxVal + 1);
        stageVal = constrain(stageVal, minRelay, 100);

        currentVal = map(stageVal, 0, 100, 0, 255);
        hS.currentOutput = stageVal;

        WriteHitachiOutput(currentVal);
      }

      UpdateHitachiRelayOutput(hS.currentOutput, minRelay);
      break;

    default:
      currentVal = 0;
      hS.currentOutput = 0;
      WriteHitachiOutput(currentVal);
      OutValues[OUT_DIMMER_ENABLE] = false;
      return;
  }
}


void WriteHitachiOutput(int pwmValue)
{
  pwmValue = constrain(pwmValue, 0, 255);

  int userPercent = map(pwmValue, 0, 255, 0, 100);
  userPercent = constrain(userPercent, 0, 100);

  GigaDimmer_setPower((uint8_t)userPercent);
  GigaDimmer_enable(userPercent > 0);
}