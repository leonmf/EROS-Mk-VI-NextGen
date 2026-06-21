#include "EROSShared.h"

#if EROS_BUILD_HAS_M4_SIDE

// apply the calibration to the sensor reading
//  sensorValue = map(sensorValue, sensorMin, sensorMax, 0, 255);




void Init_Idle()
{
  // Control-side mode init only.
  // Screen navigation is owned by the M7 display side.
}

//Mode Display
void ID_Idle()
{
  
  //displayLcdLine(2,3,"Bridge", GREEN);
}



void Execute_Idle()
{
  // Manual mode controls physical outputs, except the Dimmer relay.
  // OUT_DIMMER_ENABLE is automatic and is controlled only by Hitachi().

  for (int i = 0; i < PhysicalOutSize; i++)
  {
    if (i == OUT_DIMMER_ENABLE) {
      continue;
    }
    OutValues[i] = Manual.out[i];
  }

  // Manual mode also controls the virtual Hitachi command.
  OutValues[OUT_HITACHI_VIRTUAL] = Manual.out[OUT_HITACHI_VIRTUAL];

  // The virtual Hitachi command enables/disables the dimmer system.
  if (OutValues[OUT_HITACHI_VIRTUAL] == true) {
    Hitachi(hS.modeOn, hS.setPointOn, hS.periodOn, hS.maxValueOn, hS.minValueOn, hS.minRelayValue);
  }
  else {
    // Disabled means force the dimmer output and dimmer relay off.
    Hitachi(hitachiOff, hS.setPointOff, hS.periodOff, hS.maxValueOff, hS.minValueOff, hS.minRelayValue);
  }
}

#endif  // EROS_BUILD_HAS_M4_SIDE
