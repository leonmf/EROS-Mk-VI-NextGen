// ------------------------------------------------------------
// coreMiscSupportFun.ino
//
// Digital inputs use INPUT_PULLUP.
// Therefore:
//   Unpressed = HIGH
//   Pressed   = LOW
// ------------------------------------------------------------

bool IsInputPressed(int buttonidx)
{
  if (buttonidx < 0 || buttonidx >= InSize) {
    return false;
  }

  return digitalRead(DigitalInputs[buttonidx]) == LOW;
}

void ReadInputs(int numInputs)
{
  for (int i = 0; i < numInputs; i++)
  {
    if (i < InSize) {
      InValues[i] = IsInputPressed(i);
    }
  }
}

void SetOutputs()
{
  // Relay outputs are not inverted.
  // Only write physical relay outputs.
  // Logical-only outputs, such as OUT_HITACHI_VIRTUAL, do not have pins.
  for (int i = 0; i < PhysicalOutSize; i++)
  {
    digitalWrite(OutRly[i], OutValues[i]);
  }
}

void ResetIO()
{
  for (int i = 0; i < OutSize; i++)
  {
    OutValues[i] = LOW;
  }
}

// Blocking button check.
// Returns true once the button has been pressed and released.
boolean CheckButton(int buttonidx)
{
  boolean retval = false;

  if (IsInputPressed(buttonidx))
  {
    while (IsInputPressed(buttonidx))
    {
      retval = true;
      delay(10); // debounce / release wait
    }
  }

  return retval;
}

// Non-blocking-ish button check.
// Returns true if button is currently pressed after debounce.
// Note: this can return true repeatedly while held.
boolean CheckButtonNoWait(int buttonidx)
{
  if (!IsInputPressed(buttonidx))
  {
    return false;
  }

  delay(20); // debounce

  return IsInputPressed(buttonidx);
}
