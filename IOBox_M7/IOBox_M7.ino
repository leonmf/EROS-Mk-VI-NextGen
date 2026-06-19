/*
  IOBox_M7.ino

  EROS Mk VI M7-side sketch.

  Owns:
    - Giga Display / LVGL UI
    - Operator command creation
    - M7-side status snapshot
    - Future M7 transport endpoint
*/

#include "EROSShared.h"

#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"

void GigaDisplay_Setup();
void GigaDisplay_Task();
void EROSTransport_Setup();

void setup()
{
  EROSTransport_Setup();
  GigaDisplay_Setup();
}

void loop()
{
  GigaDisplay_Task();
}