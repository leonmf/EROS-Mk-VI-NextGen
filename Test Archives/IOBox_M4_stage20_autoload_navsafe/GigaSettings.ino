/*
  SettingsDefaults.ino

  M4-side runtime defaults only.

  Split-core architecture note:
    - M4 owns live control state and physical I/O.
    - M7 owns persistent settings save/load.
    - M4 must not mount QSPI/FAT or perform file storage operations.
*/

#include "EROSShared.h"

#if EROS_BUILD_HAS_M4_SIDE

void Settings_LoadDefaults()
{
  hS.modeOff = hitachiOff;
  hS.modeOn = hitachiValue;

  hS.setPointOff = 25;
  hS.setPointOn = 50;

  hS.periodOff = 1000;
  hS.periodOn = 1000;

  hS.periodPreciseOff = true;
  hS.periodPreciseOn = true;

  hS.maxValueOff = 100;
  hS.maxValueOn = 100;

  hS.minValueOff = 25;
  hS.minValueOn = 25;

  hS.minRelayValue = 30;
  hS.currentOutput = 0;




  TimeVar.iRunDuration = 1800;      // 30 minutes
  TimeVar.iPauseDuration = 60;     // 60 seconds
  TimeVar.iPenaltyDuration = 120;  //120 Seconds

  EROSFlexSettings.OnTime = 1000;    // milliseconds
  EROSFlexSettings.OffTime = 1000;   // milliseconds

  for (int i = 0; i < OutSize; i++) {
    EROSFlexSettings.OutInIdx[i] = 0;
    EROSFlexSettings.OutMode[i] = 0;
  }

  EROSFlexSettings.OutMode[OUT_LOCK_1] = 1;
  EROSFlexSettings.OutMode[OUT_LOCK_2] = 1;


  TimeVar.StartButton = IN_START;
  TimeVar.StopButton  = IN_STOP;
  TimeVar.PauseButton = IN_PAUSE;

}

#endif  // EROS_BUILD_HAS_M4_SIDE
