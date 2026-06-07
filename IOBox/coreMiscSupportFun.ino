
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

// ReadInputs

void ReadInputs(int NumInputs)
{
  for (int i = 0; i < NumInputs; i++)
  {
    if (i < InSize) {
      InValues[i] = IsInputPressed(i);
    }
  }
}


//Set physical relay Outputs
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

//Reset IO
void ResetIO()
{
  for (int i=0; i<OutSize; i++)
  {
    OutValues[i] = LOW;
  }
}



/*
String BooltoString(boolean Value)
{


  if (Value == HIGH)
  {
    return "1";
  }
  else
  {
    return "0";
  }
}
*/

char BooltoChar(boolean Value)
{


  if (Value == HIGH)
  {
    return '1';
  }
  else
  {
    return '0';
  }
}

/*
boolean InvertBit(boolean Value)
{
  if (Value == true)
  {
    return false;
  }
  else
  {
    return true; 
  }

}
*/


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
  boolean retval = false;
  if (IsInputPressed(buttonidx))
  {
    delay(20); // debounce
    if (IsInputPressed(buttonidx))
    {
      retval = true;
    }
  }
  return retval;
}

// Approximate long-press check.
byte PressCount;
boolean CheckWifiReset(int buttonidx)
{
  if (IsInputPressed(buttonidx))
  {
    PressCount = PressCount + 1;
    if (PressCount >= 20)
    {
      return true;
    }
  }
  else
  {
    PressCount = 0;
  }
  return false;
}


/*
void wifi_reset() {
  // call the wifi reset script
  Process p;
  p.runShellCommand("wifi-reset-and-reboot");
}
*/





