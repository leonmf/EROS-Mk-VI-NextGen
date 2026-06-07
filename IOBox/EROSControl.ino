/*
  EROSControl.ino
  Control-side scan wrapper for EROS Mk VI.
  This file collects the logic that will eventually move toward
  the real-time/control core.
*/

void Control_Setup(void)
{
  // Setup the Hitachi AC Dimmer
  GigaDimmer_Setup();

  // Build sine lookup table
  float x;
  float y;

  for (int i = 0; i < 255; i++)
  {
    x = (float)i;
    y = sin((x / 255) * 2 * PI);
    hS.sine[i] = int(y * 128) + 128;
  }

  // Setup physical outputs
  for (int i = 0; i < PhysicalOutSize; i++)
  {
    pinMode(OutRly[i], OUTPUT);
  }

  // Initialize logical output states
  for (int i = 0; i < OutSize; i++)
  {
    OutValues[i] = LOW;
  }

  // Setup dedicated inputs
  for (int i = 0; i < InSize; i++)
  {
    pinMode(DigitalInputs[i], INPUT_PULLUP);
  }

  // Setup assignable inputs
  for (int i = 0; i < AssignableInSize; i++)
  {
    pinMode(AssignableInputPins[i], INPUT_PULLUP);
  }

  // Outputs initialize ON. Turn them off before they can actually initialize.
  SetOutputs();

  // Always go to idle mode on startup
  Mode.Current = 0;
  Mode.Last = -1;

  // Default Hitachi Settings
  hS.minRelayValue = 10;
  hS.periodOff = 1000;
  hS.minValueOff = 10;
  hS.maxValueOff = 100;
  hS.modeOff = 0;
  hS.setPointOff = 0;

  hS.periodOn = 1000;
  hS.minValueOn = 10;
  hS.maxValueOn = 100;
  hS.modeOn = 1;
  hS.setPointOn = 100;
  UseIODisplay = true;
  resetTimeDisplay = true;

  // Initialize flash storage
  Settings_Begin();

  // Load settings from flash memory
  Settings_LoadAllOrDefaults();
  Command_ForceFixedAutoOutputModes();
}

void Control_Task(void)
{
  
  UpdateRunTimer();

  ReadInputs(InSize);

  CheckMode();

  RunMode();
}

void CheckMode()
{
  if ((Mode.Current > ModeMax || Mode.Current < 0))
  {
    //Reset mode to zero if we try to set something outside of allowed variables
    Mode.Current = 0;    
  }

    if (Mode.Current != Mode.Last)
  {
    if(Mode.Last == EROSFlexMode)
    {
      Detach_Timer();
    }

    resetTimeDisplay = true;
    
    Mode.Last = Mode.Current;
    InitFunctions[Mode.Current]();
    
    TimeVar.bRunning = false;
    TimeVar.bPaused = false;
    //Cause the IO display to be regenerated
    InitIODisplay = true;
  }  
}


void RunMode()
{   
  
  if (TimeVar.bRunning == true || UseTimingFunctions[Mode.Current] == false)
  {
    if (TimeVar.PauseButton>=0 && UseTimingFunctions[Mode.Current] == true)
    {
      CheckPauseButton(TimeVar.PauseButton);
    }

    if (TimeVar.StopButton>=0 && UseTimingFunctions[Mode.Current] == true)
    {
      CheckStopButton(TimeVar.StopButton);
    }
    
    if(UseTimingFunctions[Mode.Current] == true && TimeVar.bRunning == false)
    {
      ResetIO(); 
    }
  
    ExecFunctions[Mode.Current]();
  }
  else
  {

    IdleOps[Mode.Current]();
    CheckRunButton(TimeVar.StartButton);
  }
  
  if (DisplayDigital[Mode.Current])
  {
    DisplayIO(5, InitIODisplay);
  }
  InitIODisplay = false; 

  SetOutputs();
}