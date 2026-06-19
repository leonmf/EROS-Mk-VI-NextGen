/*
  coreTimeFunctions.ino

  Arduino Giga-compatible replacement for old TimerOne-based timing.
  Uses a millis() catch-up scheduler instead of TimerOne.
*/

static bool g_runTimerAttached = false;
static unsigned long g_runTimerPeriodMs = 1000;
static unsigned long g_runTimerLastMs = 0;

void Setup_RunTimer(long mSec)
{
  if (mSec <= 0) {
    mSec = 1000;
  }

  g_runTimerPeriodMs = (unsigned long)mSec;
  g_runTimerLastMs = millis();
  g_runTimerAttached = true;
}

void Detach_Timer()
{
  g_runTimerAttached = false;
}

void UpdateRunTimer()
{
  if (!g_runTimerAttached) {
    return;
  }

  unsigned long now = millis();

  while ((unsigned long)(now - g_runTimerLastMs) >= g_runTimerPeriodMs) {
    g_runTimerLastMs += g_runTimerPeriodMs;
    Time_Interrupt();
  }
}

/*********************************Timer Interrupt****************************/

void Time_Interrupt()
{
  TimeVar.iCurrentTime++;	

  if (TimeVar.bRunning == true && TimeVar.bPaused == false)
  {
    TimeVar.iPauseTime = 0;	

    if (TimeVar.iCurrentTime >= TimeVar.iWorkingRunDuration || TimeVar.iCurrentTime == 65535)
    {
      TimeVar.bRunning = false;
    }
  }
  else if (TimeVar.bRunning == true && TimeVar.bPaused == true)
  {
    if (TimeVar.iPauseTime == 0)
    {
      int iLastRunDuration = TimeVar.iWorkingRunDuration;
      TimeVar.iWorkingRunDuration += TimeVar.iPenaltyDuration;

      // Capture rollover
      if (TimeVar.iWorkingRunDuration < iLastRunDuration) {
        TimeVar.iWorkingRunDuration = 65535;
      }
    }

    TimeVar.iPauseTime++;

    if (TimeVar.iPauseTime >= TimeVar.iPauseDuration)
    {
      TimeVar.iPauseTime = 0;
      TimeVar.bPaused = false;
    }
  }
  else
  {
    TimeVar.iCurrentTime = 0;
    TimeVar.iPauseTime = 0;
    TimeVar.bPaused = false;
    TimeVar.iRemainingTime = 0;
    TimeVar.iWorkingRunDuration = TimeVar.iRunDuration;
  }

  if (TimeVar.bRunning == true)
  {
    TimeVar.iRemainingTime = TimeVar.iWorkingRunDuration - TimeVar.iCurrentTime;
  }
}

bool ButtonInRange(int input)
{
  return (input >= 0 && input < InSize);
}

void CheckPauseButton(int PauseInput)
{
  if (ButtonInRange(PauseInput) && CheckButtonNoWait(PauseInput))
  {
    TimeVar.bPaused = true;  
  }
    
  if (SoftSwitches.Pause == 1)
  {
    TimeVar.bPaused = true;
    SoftSwitches.Pause = 0;
  }
}

void CheckRunButton(int RunInput)
{
  if (ButtonInRange(RunInput) && CheckButton(RunInput))
  {
    TimeVar.iCurrentTime = 0;
    TimeVar.bRunning = true; 
    SoftSwitches.Stop = 0;
  }
  
  if (SoftSwitches.Start == 1)
  {
    TimeVar.iCurrentTime = 0;
    TimeVar.bRunning = true;
    SoftSwitches.Start = 0;
    SoftSwitches.Stop = 0;
  }
}

void CheckStopButton(int StopInput)
{
  if (ButtonInRange(StopInput) && CheckButtonNoWait(StopInput))
  {
    TimeVar.bRunning = false;
    TimeVar.bPaused = false; 
  }

  if (SoftSwitches.Stop == 1)
  {
    TimeVar.bRunning = false;
    TimeVar.bPaused = false;
    SoftSwitches.Stop = 0;
  }
}