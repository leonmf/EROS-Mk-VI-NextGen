/*
  EROSControl.ino
  Control-side scan wrapper for EROS Mk VI.
  This file collects the logic that will eventually move toward
  the real-time/control core.
*/

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