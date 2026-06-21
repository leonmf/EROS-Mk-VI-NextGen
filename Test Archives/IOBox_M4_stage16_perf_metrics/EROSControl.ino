#include "EROSShared.h"

#if EROS_BUILD_HAS_M4_SIDE

/*
  EROSControl.ino
  Control-side scan wrapper for EROS Mk VI.
  This file collects the logic that will eventually move toward
  the real-time/control core.
*/

void Control_Setup(void)
{
  SerialRPC.println("M4_DIAG: Control_Setup enter");

  // Setup the Hitachi AC Dimmer
  SerialRPC.println("M4_DIAG: before GigaDimmer_Setup");
  GigaDimmer_Setup();
  SerialRPC.println("M4_DIAG: after GigaDimmer_Setup");

  // Build sine lookup table
  SerialRPC.println("M4_DIAG: before sine table");
  float x;
  float y;

  for (int i = 0; i < 255; i++)
  {
    x = (float)i;
    y = sin((x / 255) * 2 * PI);
    hS.sine[i] = int(y * 128) + 128;
  }

  SerialRPC.println("M4_DIAG: after sine table");

  // Setup physical outputs
  SerialRPC.println("M4_DIAG: before physical output pinMode");
  for (int i = 0; i < PhysicalOutSize; i++)
  {
    pinMode(OutRly[i], OUTPUT);
  }

  SerialRPC.println("M4_DIAG: after physical output pinMode");

  // Initialize logical output states
  SerialRPC.println("M4_DIAG: before logical output init");
  for (int i = 0; i < OutSize; i++)
  {
    OutValues[i] = LOW;
  }

  SerialRPC.println("M4_DIAG: after logical output init");

  // Setup dedicated inputs
  SerialRPC.println("M4_DIAG: before dedicated input pinMode");
  for (int i = 0; i < InSize; i++)
  {
    pinMode(DigitalInputs[i], INPUT_PULLUP);
  }

  SerialRPC.println("M4_DIAG: after dedicated input pinMode");

  // Setup assignable inputs
  SerialRPC.println("M4_DIAG: before assignable input pinMode");
  for (int i = 0; i < AssignableInSize; i++)
  {
    pinMode(AssignableInputPins[i], INPUT_PULLUP);
  }

  SerialRPC.println("M4_DIAG: after assignable input pinMode");

  // Outputs initialize ON. Turn them off before they can actually initialize.
  SerialRPC.println("M4_DIAG: before SetOutputs");
  SetOutputs();
  SerialRPC.println("M4_DIAG: after SetOutputs");

  // Always go to idle mode on startup
  SerialRPC.println("M4_DIAG: before mode init");
  Mode.Current = 0;
  Mode.Last = -1;


  SerialRPC.println("M4_DIAG: after mode init");

  // Split-core baseline:
  // M4 does not own persistent settings storage. M7 owns save/load.
  // M4 loads runtime defaults, then accepts live settings/commands from M7.
  SerialRPC.println("M4_DIAG: M4 persistent settings storage removed; loading runtime defaults");
  SerialRPC.println("M4_DIAG: before Settings_LoadDefaults");
  Settings_LoadDefaults();
  SerialRPC.println("M4_DIAG: after Settings_LoadDefaults");

  // Normalize loaded/default Hitachi values before any mode uses them.
  // Do not depend on the Hitachi UI screen to do this.
  SerialRPC.println("M4_DIAG: before Command_NormalizeHitachiSettings");
  Command_NormalizeHitachiSettings();
  SerialRPC.println("M4_DIAG: after Command_NormalizeHitachiSettings");

  SerialRPC.println("M4_DIAG: before Command_ForceFixedAutoOutputModes");
  Command_ForceFixedAutoOutputModes();
  SerialRPC.println("M4_DIAG: after Command_ForceFixedAutoOutputModes");

  // Seed the UI/control status snapshot before the first display update.
  SerialRPC.println("M4_DIAG: before State_RefreshControlStatus");
  State_RefreshControlStatus();
  SerialRPC.println("M4_DIAG: after State_RefreshControlStatus");
  SerialRPC.println("M4_DIAG: Control_Setup exit");
}

void Control_Task(void)
{
  UpdateRunTimer();

  ReadInputs(InSize);

  CheckMode();

  RunMode();

  State_RefreshControlStatus();
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


    
    Mode.Last = Mode.Current;
    InitFunctions[Mode.Current]();
    
    TimeVar.bRunning = false;
    TimeVar.bPaused = false;
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
  

  SetOutputs();
}

#endif  // EROS_BUILD_HAS_M4_SIDE
