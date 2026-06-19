#include "EROSShared.h"

#if EROS_BUILD_HAS_M4_SIDE



void Init_EROSFlex()
{

  //Console.println("Setup RunTimer");
  Setup_RunTimer(1000);

  TimeVar.bFirstScan = true;
  
  for (int i=0; i<OutSize; i++)
  {
    EROSFlexSettings.NextToggleTime[i]=0;
  }
}

//Idle Operations
void ID_EROSFlex()
{
  ResetIO();
    for (int i=0; i<OutSize; i++)
  {
    //Initialize the pulse/random functions first toggle to be
    //at the defined off time. this forces the system to start the
    //random functions after an initial delay equal to Off Time.
    //This both prevents immediate stimulation on start of the doll box
    //but it also fixes the bug where random timing might not start
    //for minutes after the program was started.
    EROSFlexSettings.NextToggleTime[i]=EROSFlexSettings.OffTime;
  }

}

void Execute_EROSFlex()
{
  if (TimeVar.bFirstScan == true) {
    TimeVar.iCurrentTime = 0;
    TimeVar.bFirstScan = false;
  }
  
  if (TimeVar.bPaused == true)
  {
    ResetIO();
    //Keep locks engaged.
    OutValues[OUT_LOCK_1] = true;
    OutValues[OUT_LOCK_2] = true;
  }
  else
  {
    for (int i = 0; i < OutSize; i++)
    {
      // OUT_DIMMER_ENABLE is automatic.
      // It is controlled by Hitachi() based on actual dimmer output.
      if (i == OUT_DIMMER_ENABLE) {
        OutValues[i] = false;
        continue;
      }

      // Handle normal logical/physical outputs.
      switch(EROSFlexSettings.OutMode[i])
      {
      case 0:
        //Always Off
          OutValues[i]=false;
        break;
      case 1:
        //Always On
          OutValues[i]=true;
        break;
      case 2:
        //Normal
        //OutValues[i] = InValues[EROSFlexSettings.OutInIdx[i]];   
        OutValues[i] = Control_GetAssignedInputForOutput(i);   
        break;
      case 3:
        //Invert
        //OutValues[i] = !InValues[EROSFlexSettings.OutInIdx[i]];
        OutValues[i] = !Control_GetAssignedInputForOutput(i);
        break;
      case 4:
        //Pulse
        OutValues[i] = ToggleFlex(i, OutValues[i], false);
        break;
      case 5:
        //Random
        OutValues[i] = ToggleFlex(i, OutValues[i], true);
        break;
      case 6:
        //Pulse Input
        //if (InValues[EROSFlexSettings.OutInIdx[i]] == true)
        if (Control_GetAssignedInputForOutput(i) == true)
        {
          OutValues[i] = ToggleFlex(i, OutValues[i], false);
        }
        else
        {
          OutValues[i] = false;
        }
        break;
      case 7:
        //Pulse Invert
        //if (InValues[EROSFlexSettings.OutInIdx[i]] == false)
        if (Control_GetAssignedInputForOutput(i) == false)
        {
          OutValues[i] = ToggleFlex(i, OutValues[i], false);
        }
        else
        {
          OutValues[i] = false;
        }
        break;
      case 8:
        //Random Input
        //if (InValues[EROSFlexSettings.OutInIdx[i]] == true)
        if (Control_GetAssignedInputForOutput(i) == true)
        {
          OutValues[i] = ToggleFlex(i, OutValues[i], true);
        }
        else
        {
          OutValues[i] = false;
        }
        break;
      case 9:
        //Random Input Invert
        //if (InValues[EROSFlexSettings.OutInIdx[i]] == false)
        if (Control_GetAssignedInputForOutput(i) == false)
        {
          OutValues[i] = ToggleFlex(i, OutValues[i], true);
        }
        else
        {
          OutValues[i] = false;
        }
        break;
      case 10:
        //Remote Control
        OutValues[i] = Manual.out[i];
        break;
      default:
        //value = 0;
        break;
      } 
    }

    //Control the Hitachi output based on the "output state" of the 
    //virtual output for the Hitachi. I realize that I'm dual dutying the "OutValues" since
    //the Hitachi function actually overrides OutValues to set the 4th relay for Hitachi control.
    if (OutValues[OUT_HITACHI_VIRTUAL]) {
      Hitachi(hS.modeOn, hS.setPointOn, hS.periodOn, hS.maxValueOn, hS.minValueOn, hS.minRelayValue);
    } else {
      Hitachi(hS.modeOff, hS.setPointOff, hS.periodOff, hS.maxValueOff, hS.minValueOff, hS.minRelayValue);
    }

  }
}



boolean ToggleFlex(int idx, boolean bValue, boolean bRandom)
{
  boolean bReturn = bValue;

  // TimeVar.iCurrentTime is in seconds.
  // EROSFlexSettings.OnTime / OffTime are in milliseconds.
  // Use millis() for pulse/random timing.
  unsigned long currentMs = millis();

  if (TimeVar.bRunning == false) {
    EROSFlexSettings.NextToggleTime[idx] = currentMs + EROSFlexSettings.OffTime;
    return false;
  }

  if (currentMs >= (unsigned long)EROSFlexSettings.NextToggleTime[idx])
  {
    unsigned int addTimeMs = 0;

    if (bValue == true)
    {
      addTimeMs = EROSFlexSettings.OffTime;
    }
    else
    {
      addTimeMs = EROSFlexSettings.OnTime;
    }

    if (bRandom == true)
    {
      if (addTimeMs > 1) {
        addTimeMs = random(100, addTimeMs + 1);
      }
      else {
        addTimeMs = 1;
      }
    }
    bReturn = !bValue;
    EROSFlexSettings.NextToggleTime[idx] = currentMs + addTimeMs;
  }
  return bReturn;
}

#endif  // EROS_BUILD_HAS_M4_SIDE
