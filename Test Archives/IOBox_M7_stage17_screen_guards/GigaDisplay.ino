#include "EROSShared.h"

#if EROS_BUILD_HAS_M7_SIDE

/*
  GigaDisplay.ino

  Arduino Giga Display Shield UI for EROS Mk VI.

  Current screens:
    - Idle screen
    - Manual screen
    - Hitachi screen

  This version uses the EROSBridgeM7.ino command/state interface instead of
  directly poking most IO/control globals from the display layer.

  Main screen:
    - Manual
    - Auto
    - Hitachi
    - Status
    - Save Settings
    - Load Settings

  Hitachi screen:
    - Edits hS On/Off settings through Command_/State_ functions
    - Mode buttons
    - Set Point slider, 25-100%
    - Max Value slider, 25-100%
    - Min Value slider, 25-100%
    - Period slider with Precise/Coarse mode
    - Dimmer Relay enable button
    - Back button returns to previous screen
*/

#include "EROSShared.h"
#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"

// ------------------------------------------------------------
// Giga Display hardware objects
// ------------------------------------------------------------

Arduino_H7_Video Display(800, 480, GigaDisplayShield);
Arduino_GigaDisplayTouch TouchDetector;

// ------------------------------------------------------------
// Screen object pointers
// ------------------------------------------------------------

static lv_obj_t * g_idleScreen = NULL;
static lv_obj_t * g_manualScreen = NULL;
static lv_obj_t * g_autoScreen = NULL;
static lv_obj_t * g_autoSettingsScreen = NULL;
static lv_obj_t * g_hitachiScreen = NULL;
static lv_obj_t * g_hitachiRelayMinScreen = NULL;
static lv_obj_t * g_statusScreen = NULL;

static lv_obj_t * g_previousScreenBeforeHitachi = NULL;
static lv_obj_t * g_previousScreenBeforeRelayMin = NULL;

// ------------------------------------------------------------
// Idle screen widgets
// ------------------------------------------------------------

static lv_obj_t * g_idleStatusLabel = NULL;

// ------------------------------------------------------------
// Status / debug screen widgets
// ------------------------------------------------------------
#define STATUS_DEBUG_ROW_COUNT 22

static lv_obj_t * g_statusDebugValueLabels[STATUS_DEBUG_ROW_COUNT];

// ------------------------------------------------------------
// Manual screen widgets
// ------------------------------------------------------------
static lv_obj_t * g_inputIndicators[InSize];
static lv_obj_t * g_assignableInputIndicators[AssignableInSize];
static lv_obj_t * g_outputIndicators[OutSize];

static lv_obj_t * g_inputValueLabels[InSize];
static lv_obj_t * g_assignableInputValueLabels[AssignableInSize];
static lv_obj_t * g_outputValueLabels[OutSize];

static lv_obj_t * g_manualCurrentOutputLabel = NULL;

// ------------------------------------------------------------
// Auto screen widgets
// ------------------------------------------------------------
static lv_obj_t * g_autoInputIndicators[InSize];
static lv_obj_t * g_autoAssignableInputIndicators[AssignableInSize];
static lv_obj_t * g_autoOutputIndicators[OutSize];

static lv_obj_t * g_autoInputValueLabels[InSize];
static lv_obj_t * g_autoAssignableInputValueLabels[AssignableInSize];
static lv_obj_t * g_autoOutputValueLabels[OutSize];

static lv_obj_t * g_autoStatusLabel = NULL;
static lv_obj_t * g_autoRemainingLabel = NULL;
static lv_obj_t * g_autoCurrentOutputLabel = NULL;

// ------------------------------------------------------------
// Auto Settings screen widgets
// ------------------------------------------------------------

static lv_obj_t * g_autoRunDurationSlider = NULL;
static lv_obj_t * g_autoPauseDurationSlider = NULL;
static lv_obj_t * g_autoPenaltyDurationSlider = NULL;
static lv_obj_t * g_autoOnTimeSlider = NULL;
static lv_obj_t * g_autoOffTimeSlider = NULL;

static lv_obj_t * g_autoRunDurationValueLabel = NULL;
static lv_obj_t * g_autoPauseDurationValueLabel = NULL;
static lv_obj_t * g_autoPenaltyDurationValueLabel = NULL;
static lv_obj_t * g_autoOnTimeValueLabel = NULL;
static lv_obj_t * g_autoOffTimeValueLabel = NULL;

static lv_obj_t * g_autoOutputModeLabels[OutSize];
static lv_obj_t * g_autoOutputInputLabels[OutSize];
static byte g_autoOutputModeUiValue[OutSize];
static int g_autoOutputInputUiIndex[OutSize];
static bool g_autoSettingsUiRefreshing = false;

// ------------------------------------------------------------
// Hitachi screen widgets
// ------------------------------------------------------------

static lv_obj_t * g_hitachiEditorLabel = NULL;
static lv_obj_t * g_hitachiModeLabel = NULL;
static lv_obj_t * g_hitachiCurrentOutputLabel = NULL;
static lv_obj_t * g_hitachiDimmerEnableLabel = NULL;
static lv_obj_t * g_hitachiPeriodModeLabel = NULL;

static lv_obj_t * g_hitachiSetPointSlider = NULL;
static lv_obj_t * g_hitachiMaxSlider = NULL;
static lv_obj_t * g_hitachiMinSlider = NULL;
static lv_obj_t * g_hitachiPeriodSlider = NULL;

static lv_obj_t * g_hitachiSetPointValueLabel = NULL;
static lv_obj_t * g_hitachiMaxValueLabel = NULL;
static lv_obj_t * g_hitachiMinValueLabel = NULL;
static lv_obj_t * g_hitachiPeriodValueLabel = NULL;
static lv_obj_t * g_hitachiRelayMinValueLabel = NULL;



// ------------------------------------------------------------
// Screen build/state flags
// ------------------------------------------------------------

static bool g_displayInitialized = false;

// ------------------------------------------------------------
// Screen lifecycle guards
// ------------------------------------------------------------

enum EROSScreenId {
  SCREEN_NONE,
  SCREEN_IDLE,
  SCREEN_MANUAL,
  SCREEN_AUTO,
  SCREEN_AUTO_SETTINGS,
  SCREEN_HITACHI,
  SCREEN_HITACHI_RELAY_MIN,
  SCREEN_STATUS
};

static EROSScreenId g_currentScreen = SCREEN_NONE;
static EROSScreenId g_buildingScreen = SCREEN_NONE;
static bool g_screenBuilding = false;
static bool g_screenRefreshLocked = false;
static unsigned long g_screenRefreshLockUntilMs = 0;

static bool g_manualScreenBuilt = false;
static bool g_autoScreenBuilt = false;
static bool g_autoSettingsScreenBuilt = false;
static bool g_hitachiScreenBuilt = false;
static bool g_hitachiRelayMinScreenBuilt = false;
static bool g_statusScreenBuilt = false;

static bool g_hitachiEditingOnSettings = true;
static bool g_manualSkipNextAutoRefresh = false;
static bool g_autoSkipNextAutoRefresh = false;
static bool g_hitachiUiRefreshing = false;
static bool g_hitachiSkipNextAutoRefresh = false;
static bool g_hitachiPeriodPreciseMode = true;
static int g_hitachiRelayMinUiValue = 25;
static unsigned long g_lastDisplayedSettingsResultCounter = 0;
static unsigned long g_lastDisplayedTransportCommandFailCounter = 0;


// ------------------------------------------------------------
// Display labels
// ------------------------------------------------------------

static const char * INPUT_NAMES[InSize] = {
  "Start",
  "Stop",
  "Pause"
};

static const char * OUTPUT_NAMES[OutSize] = {
  "Lock 1",
  "Lock 2",
  "AC",
  "Dimmer",
  "Dry 1",
  "Dry 2",
  "Dry 3",
  "Dry 4",
  "Hitachi"
};

// ------------------------------------------------------------
// Auto output mode names
// ------------------------------------------------------------
#define AUTO_OUT_MODE_COUNT 11

static const char * AUTO_OUT_MODE_NAMES[AUTO_OUT_MODE_COUNT] = {
  "Off",
  "On",
  "Input",
  "Invert",
  "Pulse",
  "Random",
  "Pulse Input",
  "Pulse Invert",
  "Random Input",
  "Random Invert",
  "Remote"
};

// ------------------------------------------------------------
// Manual control button mapping
// ------------------------------------------------------------

#define MANUAL_BUTTON_COUNT 7

static const char * MANUAL_BUTTON_NAMES[MANUAL_BUTTON_COUNT] = {
  "Lock",
  "AC",
  "Hitachi",
  "Dry 1",
  "Dry 2",
  "Dry 3",
  "Dry 4"
};

static const int MANUAL_BUTTON_OUTPUT_INDEX[MANUAL_BUTTON_COUNT] = {
  OUT_LOCK_1,
  OUT_AC,
  OUT_HITACHI_VIRTUAL,
  OUT_DRY_1,
  OUT_DRY_2,
  OUT_DRY_3,
  OUT_DRY_4
};

// ------------------------------------------------------------
// Hitachi mode button mapping
// ------------------------------------------------------------

#define HITACHI_MODE_COUNT 7

static const char * HITACHI_MODE_NAMES[HITACHI_MODE_COUNT] = {
  "Off",
  "Value",
  "Pulse",
  "Sine",
  "Saw",
  "Triangle",
  "Random"
};

static const int HITACHI_MODE_VALUES[HITACHI_MODE_COUNT] = {
  hitachiOff,
  hitachiValue,
  hitachiPulse,
  hitachiSine,
  hitachiSawTooth,
  hitachiTriangle,
  hitachiRandom
};

// ------------------------------------------------------------
// Forward declarations
// ------------------------------------------------------------

void GigaDisplay_Setup();
void GigaDisplay_Task();

void GigaDisplay_ShowIdleScreen();
void GigaDisplay_ShowManualScreen();
void GigaDisplay_ShowHitachiScreen();
void GigaDisplay_ShowHitachiRelayMinScreen();
void GigaDisplay_ShowAutoScreen();
void GigaDisplay_ShowAutoSettingsScreen();
void GigaDisplay_ShowStatusScreen();

void GigaDisplay_UpdateManualIndicators();

static void GigaDisplay_CreateIdleScreen();
static void GigaDisplay_CreateManualScreen();
static void GigaDisplay_CreateHitachiScreen();
static void GigaDisplay_CreateHitachiRelayMinScreen();
static void GigaDisplay_CreateAutoScreen();
static void GigaDisplay_CreateAutoSettingsScreen();
static void GigaDisplay_CreateStatusScreen();
static void GigaDisplay_DestroyAutoSettingsScreen();
static void GigaDisplay_DestroyHitachiScreen();

static void GigaDisplay_UpdateHitachiScreen();
static void GigaDisplay_UpdateHitachiSliderLabelsFromWidgets();
static void GigaDisplay_UpdateHitachiRelayMinScreen();
static void GigaDisplay_UpdateHitachiRelayMinLabelFromUiValue();
static void GigaDisplay_UpdateAutoScreen();
static void GigaDisplay_UpdateAutoSettingsScreen();
static void GigaDisplay_UpdateAutoSettingsSliderLabelsFromWidgets();
static void GigaDisplay_UpdateAutoOutputModeLabel(int outputIndex, byte mode);
static void GigaDisplay_UpdateAutoOutputInputLabel(int outputIndex, int inputIndex);
static void GigaDisplay_UpdateIdleSettingsResult();
static void GigaDisplay_UpdateIdleTransportHealth();
static void GigaDisplay_UpdateStatusScreen();

static void BeginBuildScreen(EROSScreenId screen);
static void EndBuildScreen(EROSScreenId screen);
static void LeaveCurrentScreen();
static bool CanRefreshScreen(EROSScreenId screen);
static void HoldScreenRefresh(unsigned long holdMs);
static void ScreenScroll_Event(lv_event_t * e);
static lv_obj_t * GigaDisplay_CreateStatusDebugRow(int row, const char * labelText);

static void ManualButton_Event(lv_event_t * e);
static void AutoButton_Event(lv_event_t * e);
static void BackButton_Event(lv_event_t * e);
static void OutputToggle_Event(lv_event_t * e);

static void HitachiButton_Event(lv_event_t * e);
static void HitachiBackButton_Event(lv_event_t * e);
static void HitachiEditOnButton_Event(lv_event_t * e);
static void HitachiEditOffButton_Event(lv_event_t * e);
static void HitachiModeButton_Event(lv_event_t * e);
static void HitachiSlider_Event(lv_event_t * e);
static void HitachiPeriodModeButton_Event(lv_event_t * e);
static void HitachiRelayMinButton_Event(lv_event_t * e);
static void HitachiRelayMinMinusButton_Event(lv_event_t * e);
static void HitachiRelayMinPlusButton_Event(lv_event_t * e);
static void HitachiRelayMinMinus5Button_Event(lv_event_t * e);
static void HitachiRelayMinPlus5Button_Event(lv_event_t * e);
static void HitachiRelayMinBackButton_Event(lv_event_t * e);

static void SaveSettingsButton_Event(lv_event_t * e);
static void LoadSettingsButton_Event(lv_event_t * e);
static void StatusButton_Event(lv_event_t * e);
static void StatusBackButton_Event(lv_event_t * e);
static void StatusPingButton_Event(lv_event_t * e);

static void AutoStartButton_Event(lv_event_t * e);
static void AutoStopButton_Event(lv_event_t * e);
static void AutoPauseButton_Event(lv_event_t * e);
static void AutoBackButton_Event(lv_event_t * e);

static void AutoSettingsButton_Event(lv_event_t * e);
static void AutoSettingsBackButton_Event(lv_event_t * e);
static void AutoSettingsSlider_Event(lv_event_t * e);
static void AutoOutputModeButton_Event(lv_event_t * e);
static void AutoOutputInputButton_Event(lv_event_t * e);

// ------------------------------------------------------------
// Utility helpers
// ------------------------------------------------------------
static lv_obj_t * CreateWhiteLabel(
  lv_obj_t * parent,
  const char * text,
  int x,
  int y
);

static lv_obj_t * CreateCenteredWhiteLabel(
  lv_obj_t * parent,
  const char * text,
  lv_align_t align,
  int xOffset,
  int yOffset
);

static lv_obj_t * CreateButton(
  lv_obj_t * parent,
  const char * text,
  int x,
  int y,
  int w,
  int h,
  lv_event_cb_t eventCb,
  void * userData
);

static lv_obj_t * GetButtonLabel(lv_obj_t * button);

static lv_obj_t * CreateAlignedButton(
  lv_obj_t * parent,
  const char * text,
  lv_align_t align,
  int xOffset,
  int yOffset,
  int w,
  int h,
  lv_event_cb_t eventCb,
  void * userData
);

static lv_obj_t * CreateSliderWithValueLabel(
  lv_obj_t * parent,
  const char * labelText,
  int labelX,
  int sliderX,
  int valueX,
  int y,
  int sliderW,
  int minVal,
  int maxVal,
  const char * initialValueText,
  lv_obj_t ** valueLabel,
  lv_event_cb_t eventCb
);

static lv_obj_t * CreateIndicatorBox(
  lv_obj_t * parent,
  int x,
  int y,
  int size
);

static lv_obj_t * CreateScreen(bool scrollable);

static lv_obj_t * CreateBorderBox(
  lv_obj_t * parent,
  int x,
  int y,
  int w,
  int h,
  int borderWidth,
  int radius,
  uint32_t borderColor
);

static lv_obj_t * CreateScreenTitle(
  lv_obj_t * parent,
  const char * text
);

static lv_obj_t * CreateStatusLabel(
  lv_obj_t * parent,
  const char * text,
  int x,
  int y
);

static lv_obj_t * CreateTopRightOutputLabel(lv_obj_t * parent);

static lv_obj_t * CreateWhiteLabel(
  lv_obj_t * parent,
  const char * text,
  int x,
  int y
)
{
  lv_obj_t * label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(label, x, y);

  return label;
}

static lv_obj_t * CreateCenteredWhiteLabel(
  lv_obj_t * parent,
  const char * text,
  lv_align_t align,
  int xOffset,
  int yOffset
)
{
  lv_obj_t * label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(label, align, xOffset, yOffset);

  return label;
}

static lv_obj_t * CreateButton(
  lv_obj_t * parent,
  const char * text,
  int x,
  int y,
  int w,
  int h,
  lv_event_cb_t eventCb,
  void * userData
)
{
  lv_obj_t * btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);

  if (eventCb != NULL) {
    lv_obj_add_event_cb(btn, eventCb, LV_EVENT_CLICKED, userData);
  }

  lv_obj_t * label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(label);

  return btn;
}

static lv_obj_t * GetButtonLabel(lv_obj_t * button)
{
  if (button == NULL) {
    return NULL;
  }

  return lv_obj_get_child(button, 0);
}

static lv_obj_t * CreateAlignedButton(
  lv_obj_t * parent,
  const char * text,
  lv_align_t align,
  int xOffset,
  int yOffset,
  int w,
  int h,
  lv_event_cb_t eventCb,
  void * userData
)
{
  lv_obj_t * btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_align(btn, align, xOffset, yOffset);

  if (eventCb != NULL) {
    lv_obj_add_event_cb(btn, eventCb, LV_EVENT_CLICKED, userData);
  }

  lv_obj_t * label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(label);

  return btn;
}

static lv_obj_t * CreateSliderWithValueLabel(
  lv_obj_t * parent,
  const char * labelText,
  int labelX,
  int sliderX,
  int valueX,
  int y,
  int sliderW,
  int minVal,
  int maxVal,
  const char * initialValueText,
  lv_obj_t ** valueLabel,
  lv_event_cb_t eventCb
)
{
  CreateWhiteLabel(parent, labelText, labelX, y);

  lv_obj_t * slider = lv_slider_create(parent);
  lv_slider_set_range(slider, minVal, maxVal);
  lv_obj_set_size(slider, sliderW, 20);
  lv_obj_set_pos(slider, sliderX, y + 5);

  if (eventCb != NULL) {
    lv_obj_add_event_cb(slider, eventCb, LV_EVENT_VALUE_CHANGED, NULL);
  }

  *valueLabel = CreateWhiteLabel(parent, initialValueText, valueX, y);

  return slider;
}

static lv_obj_t * CreateIndicatorBox(
  lv_obj_t * parent,
  int x,
  int y,
  int size
)
{
  lv_obj_t * indicator = lv_obj_create(parent);
  lv_obj_set_size(indicator, size, size);
  lv_obj_set_pos(indicator, x, y);
  lv_obj_set_style_radius(indicator, 2, LV_PART_MAIN);
  lv_obj_set_style_border_width(indicator, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(indicator, lv_color_hex(0xA0A0A0), LV_PART_MAIN);
  lv_obj_clear_flag(indicator, LV_OBJ_FLAG_SCROLLABLE);

  return indicator;
}

static lv_obj_t * CreateScreen(bool scrollable)
{
  lv_obj_t * screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);

  if (scrollable) {
    lv_obj_set_scroll_dir(screen, LV_DIR_VER);
    lv_obj_add_event_cb(screen, ScreenScroll_Event, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(screen, ScreenScroll_Event, LV_EVENT_SCROLL_END, NULL);
  }
  else {
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  }

  return screen;
}

static lv_obj_t * CreateBorderBox(
  lv_obj_t * parent,
  int x,
  int y,
  int w,
  int h,
  int borderWidth,
  int radius,
  uint32_t borderColor
)
{
  lv_obj_t * border = lv_obj_create(parent);
  lv_obj_set_size(border, w, h);
  lv_obj_set_pos(border, x, y);
  lv_obj_set_style_bg_opa(border, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(border, borderWidth, LV_PART_MAIN);
  lv_obj_set_style_border_color(border, lv_color_hex(borderColor), LV_PART_MAIN);
  lv_obj_set_style_radius(border, radius, LV_PART_MAIN);
  lv_obj_clear_flag(border, LV_OBJ_FLAG_SCROLLABLE);

  return border;
}

static lv_obj_t * CreateScreenTitle(
  lv_obj_t * parent,
  const char * text
)
{
  return CreateCenteredWhiteLabel(parent, text, LV_ALIGN_TOP_MID, 0, 12);
}

static lv_obj_t * CreateStatusLabel(
  lv_obj_t * parent,
  const char * text,
  int x,
  int y
)
{
  return CreateWhiteLabel(parent, text, x, y);
}

static lv_obj_t * CreateTopRightOutputLabel(lv_obj_t * parent)
{
  return CreateCenteredWhiteLabel(parent, "Output: 0%", LV_ALIGN_TOP_RIGHT, -30, 14);
}

static void SetIndicatorState(lv_obj_t * indicator, bool state)
{
  if (indicator == NULL) {
    return;
  }

  if (state) {
    lv_obj_set_style_bg_color(indicator, lv_color_hex(0x00C853), LV_PART_MAIN);
  } else {
    lv_obj_set_style_bg_color(indicator, lv_color_hex(0x303030), LV_PART_MAIN);
  }
}

static void SetValueLabel(lv_obj_t * label, bool state)
{
  if (label == NULL) {
    return;
  }

  lv_label_set_text(label, state ? "ON" : "OFF");
}

static void SetIdleStatusText(const char * text)
{
  if (g_idleStatusLabel != NULL) {
    lv_label_set_text(g_idleStatusLabel, text);
  }
}

static void SetIdleStatusError(const char * prefix)
{
  if (g_idleStatusLabel == NULL) {
    return;
  }

  static char buffer[48];
  snprintf(buffer, sizeof(buffer), "%s: %d", prefix, State_GetSettingsLastError());
  lv_label_set_text(g_idleStatusLabel, buffer);
}

static void GigaDisplay_UpdateIdleSettingsResult()
{
  if (!CanRefreshScreen(SCREEN_IDLE)) {
    return;
  }

  unsigned long resultCounter = State_GetSettingsResultCounter();

  if (resultCounter == g_lastDisplayedSettingsResultCounter)
  {
    return;
  }

  g_lastDisplayedSettingsResultCounter = resultCounter;

  byte action = State_GetSettingsLastAction();
  bool ok = State_GetSettingsLastOk();

  if (action == EROS_SETTINGS_ACTION_SAVE)
  {
    if (ok) {
      SetIdleStatusText("Settings saved");
    }
    else {
      SetIdleStatusError("Save failed");
    }
  }
  else if (action == EROS_SETTINGS_ACTION_LOAD)
  {
    if (ok) {
      SetIdleStatusText("Settings loaded");
    }
    else {
      SetIdleStatusError("Load failed");
    }
  }
}

static void GigaDisplay_UpdateIdleTransportHealth()
{
  if (!CanRefreshScreen(SCREEN_IDLE)) {
    return;
  }

  unsigned long failCounter = State_GetTransportCommandSendFailedCounter();

  if (failCounter == g_lastDisplayedTransportCommandFailCounter)
  {
    return;
  }

  g_lastDisplayedTransportCommandFailCounter = failCounter;
  SetIdleStatusText("Command send failed");
}

// ------------------------------------------------------------
// Screen lifecycle guard helpers
// ------------------------------------------------------------

static void BeginBuildScreen(EROSScreenId screen)
{
  g_screenBuilding = true;
  g_buildingScreen = screen;
  g_currentScreen = SCREEN_NONE;
  g_screenRefreshLocked = true;
  g_screenRefreshLockUntilMs = millis() + 100UL;
}

static void EndBuildScreen(EROSScreenId screen)
{
  g_currentScreen = screen;
  g_buildingScreen = SCREEN_NONE;
  g_screenBuilding = false;
  g_screenRefreshLocked = false;
  g_screenRefreshLockUntilMs = 0;
}

static void LeaveCurrentScreen()
{
  g_currentScreen = SCREEN_NONE;
  g_buildingScreen = SCREEN_NONE;
  g_screenBuilding = false;
  g_screenRefreshLocked = true;
  g_screenRefreshLockUntilMs = millis() + 100UL;
}

static void HoldScreenRefresh(unsigned long holdMs)
{
  g_screenRefreshLocked = true;
  g_screenRefreshLockUntilMs = millis() + holdMs;
}

static bool CanRefreshScreen(EROSScreenId screen)
{
  if (g_screenBuilding) {
    return false;
  }

  if (g_currentScreen != screen) {
    return false;
  }

  if (g_screenRefreshLocked) {
    if (millis() < g_screenRefreshLockUntilMs) {
      return false;
    }

    g_screenRefreshLocked = false;
    g_screenRefreshLockUntilMs = 0;
  }

  return true;
}

static void ScreenScroll_Event(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_SCROLL_BEGIN) {
    g_screenRefreshLocked = true;
    g_screenRefreshLockUntilMs = millis() + 250UL;
  }
  else if (code == LV_EVENT_SCROLL_END) {
    HoldScreenRefresh(150UL);
  }
}

// ------------------------------------------------------------
// Public display setup / task functions
// ------------------------------------------------------------


void GigaDisplay_Setup()
{
  if (g_displayInitialized) {
    return;
  }

  Serial.println("DISPLAY 1: Display.begin");
  Display.begin();

  Serial.println("DISPLAY 2: TouchDetector.begin");
  TouchDetector.begin();

  Serial.println("DISPLAY 3: Create idle screen only");
  GigaDisplay_CreateIdleScreen();

  g_displayInitialized = true;

  Serial.println("DISPLAY 4: Load idle screen");
  g_currentScreen = SCREEN_IDLE;
  g_buildingScreen = SCREEN_NONE;
  g_screenBuilding = false;
  g_screenRefreshLocked = false;
  lv_scr_load(g_idleScreen);

  Serial.println("DISPLAY 5: Display setup complete");
}


void GigaDisplay_Task()
{
  //Serial.println("GigaDisplay");
  if (!g_displayInitialized) {
    return;
  }

  //Serial.println("Initialized");
  lv_timer_handler();

  //Serial.println("Timer Handler");

  if (lv_scr_act() == g_idleScreen && CanRefreshScreen(SCREEN_IDLE)) {
    GigaDisplay_UpdateIdleSettingsResult();
    GigaDisplay_UpdateIdleTransportHealth();
  }

  if (lv_scr_act() == g_manualScreen && CanRefreshScreen(SCREEN_MANUAL)) {
    // Manual indicators are authoritative M4-reported status only.
    // Do not preserve optimistic M7-side button states.
    g_manualSkipNextAutoRefresh = false;
    GigaDisplay_UpdateManualIndicators();
    //Serial.println("Manual Screen");
  }
  else if (lv_scr_act() == g_autoScreen && CanRefreshScreen(SCREEN_AUTO)) {
    if (g_autoSkipNextAutoRefresh) {
      // An Auto button callback just queued a command.
      // Skip this one automatic refresh so the UI does not redraw from the
      // previous status snapshot before loop() drains the command queue.
      g_autoSkipNextAutoRefresh = false;
    }
    else {
      GigaDisplay_UpdateAutoScreen();
    }
    //Serial.println("Auto Screen");
  }
  else if (lv_scr_act() == g_statusScreen && CanRefreshScreen(SCREEN_STATUS)) {
    GigaDisplay_UpdateStatusScreen();
  }
  else if (lv_scr_act() == g_hitachiScreen && CanRefreshScreen(SCREEN_HITACHI)) {
    if (g_hitachiSkipNextAutoRefresh) {
      // A Hitachi slider callback just queued a command.
      // Skip this one automatic refresh so the UI does not redraw from the
      // previous status snapshot before loop() drains the command queue.
      g_hitachiSkipNextAutoRefresh = false;
    }
    else {
      GigaDisplay_UpdateHitachiScreen();
    }
    //Serial.println("Hitachi Screen");
  }

}

static void GigaDisplay_DestroyAutoSettingsScreen()
{
  if (g_autoSettingsScreen != NULL) {
    lv_obj_del_async(g_autoSettingsScreen);
  }

  g_autoSettingsScreen = NULL;
  g_autoSettingsScreenBuilt = false;

  g_autoRunDurationSlider = NULL;
  g_autoPauseDurationSlider = NULL;
  g_autoPenaltyDurationSlider = NULL;
  g_autoOnTimeSlider = NULL;
  g_autoOffTimeSlider = NULL;

  g_autoRunDurationValueLabel = NULL;
  g_autoPauseDurationValueLabel = NULL;
  g_autoPenaltyDurationValueLabel = NULL;
  g_autoOnTimeValueLabel = NULL;
  g_autoOffTimeValueLabel = NULL;

  for (int i = 0; i < OutSize; i++) {
    g_autoOutputModeLabels[i] = NULL;
    g_autoOutputInputLabels[i] = NULL;
  }
}

static void GigaDisplay_DestroyHitachiScreen()
{
  if (g_hitachiScreen != NULL) {
    lv_obj_del_async(g_hitachiScreen);
  }

  g_hitachiScreen = NULL;
  g_hitachiScreenBuilt = false;

  g_hitachiEditorLabel = NULL;
  g_hitachiModeLabel = NULL;
  g_hitachiCurrentOutputLabel = NULL;
  g_hitachiDimmerEnableLabel = NULL;
  g_hitachiPeriodModeLabel = NULL;

  g_hitachiSetPointSlider = NULL;
  g_hitachiMaxSlider = NULL;
  g_hitachiMinSlider = NULL;
  g_hitachiPeriodSlider = NULL;

  g_hitachiSetPointValueLabel = NULL;
  g_hitachiMaxValueLabel = NULL;
  g_hitachiMinValueLabel = NULL;
  g_hitachiPeriodValueLabel = NULL;

  g_previousScreenBeforeHitachi = NULL;
}


// ------------------------------------------------------------
// Idle screen
// ------------------------------------------------------------

static void GigaDisplay_CreateIdleScreen()
{
  g_idleScreen = CreateScreen(false);

  CreateBorderBox(g_idleScreen, 10, 10, Display.width() - 20, Display.height() - 20, 3, 8, 0xFFFFFF);

  CreateCenteredWhiteLabel(g_idleScreen, "EROS Mk VI", LV_ALIGN_TOP_MID, 0, 28);

  const int mainButtonW = 210;
  const int mainButtonH = 95;
  const int mainY = 145;

  const int manualX = 75;
  const int autoX = 295;
  const int hitachiX = 515;

  CreateButton(g_idleScreen, "Manual", manualX, mainY, mainButtonW, mainButtonH, ManualButton_Event, NULL);
  CreateButton(g_idleScreen, "Auto", autoX, mainY, mainButtonW, mainButtonH, AutoButton_Event, NULL);
  CreateButton(g_idleScreen, "Hitachi", hitachiX, mainY, mainButtonW, mainButtonH, HitachiButton_Event, NULL);

  CreateButton(g_idleScreen, "Status", 310, 260, 180, 55, StatusButton_Event, NULL);

  const int settingsButtonW = 220;
  const int settingsButtonH = 65;
  const int settingsY = 335;

  CreateButton(g_idleScreen, "Save Settings", 160, settingsY, settingsButtonW, settingsButtonH, SaveSettingsButton_Event, NULL);
  CreateButton(g_idleScreen, "Load Settings", 420, settingsY, settingsButtonW, settingsButtonH, LoadSettingsButton_Event, NULL);

  g_idleStatusLabel = CreateCenteredWhiteLabel(g_idleScreen, "Settings ready", LV_ALIGN_BOTTOM_MID, 0, -32);
}

void GigaDisplay_ShowIdleScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  LeaveCurrentScreen();
  BeginBuildScreen(SCREEN_IDLE);

  if (g_idleScreen == NULL) {
    GigaDisplay_CreateIdleScreen();
  }

  EndBuildScreen(SCREEN_IDLE);
  lv_scr_load(g_idleScreen);
}

// ------------------------------------------------------------
// Status / debug screen
// ------------------------------------------------------------

static lv_obj_t * GigaDisplay_CreateStatusDebugRow(int row, const char * labelText)
{
  // Two-column layout keeps the debug/status values above the bottom buttons.
  // This avoids overlap now that the status screen includes performance metrics.
  const int rowsPerColumn = 11;
  const int startY = 58;
  const int rowH = 28;

  int column = row / rowsPerColumn;
  int rowInColumn = row % rowsPerColumn;

  const int leftLabelX = 30;
  const int leftValueX = 260;
  const int rightLabelX = 405;
  const int rightValueX = 690;

  int labelX = (column == 0) ? leftLabelX : rightLabelX;
  int valueX = (column == 0) ? leftValueX : rightValueX;
  int y = startY + (rowInColumn * rowH);

  lv_obj_t * nameLabel = CreateWhiteLabel(g_statusScreen, labelText, labelX, y);
  lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(nameLabel, (column == 0) ? 220 : 270);

  lv_obj_t * valueLabel = CreateWhiteLabel(g_statusScreen, "-", valueX, y);
  lv_label_set_long_mode(valueLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(valueLabel, (column == 0) ? 120 : 90);

  return valueLabel;
}

static void GigaDisplay_CreateStatusScreen()
{
  g_statusScreen = CreateScreen(false);

  CreateScreenTitle(g_statusScreen, "Status / Transport Debug");
  CreateAlignedButton(g_statusScreen, "Ping", LV_ALIGN_BOTTOM_LEFT, 30, -25, 130, 50, StatusPingButton_Event, NULL);
  CreateAlignedButton(g_statusScreen, "Back", LV_ALIGN_BOTTOM_RIGHT, -30, -25, 130, 50, StatusBackButton_Event, NULL);

  g_statusDebugValueLabels[0] = GigaDisplay_CreateStatusDebugRow(0, "Build mode");
  g_statusDebugValueLabels[1] = GigaDisplay_CreateStatusDebugRow(1, "Status packets");
  g_statusDebugValueLabels[2] = GigaDisplay_CreateStatusDebugRow(2, "Status age ms");
  g_statusDebugValueLabels[3] = GigaDisplay_CreateStatusDebugRow(3, "Status fresh");
  g_statusDebugValueLabels[4] = GigaDisplay_CreateStatusDebugRow(4, "M4 publish millis");
  g_statusDebugValueLabels[5] = GigaDisplay_CreateStatusDebugRow(5, "M7 cmd attempts");
  g_statusDebugValueLabels[6] = GigaDisplay_CreateStatusDebugRow(6, "M7 cmd accepted");
  g_statusDebugValueLabels[7] = GigaDisplay_CreateStatusDebugRow(7, "M7 cmd failed");
  g_statusDebugValueLabels[8] = GigaDisplay_CreateStatusDebugRow(8, "M4 q accepted");
  g_statusDebugValueLabels[9] = GigaDisplay_CreateStatusDebugRow(9, "M4 q rejected");
  g_statusDebugValueLabels[10] = GigaDisplay_CreateStatusDebugRow(10, "M4 queue depth");
  g_statusDebugValueLabels[11] = GigaDisplay_CreateStatusDebugRow(11, "M4 q capacity");
  g_statusDebugValueLabels[12] = GigaDisplay_CreateStatusDebugRow(12, "Settings results");
  g_statusDebugValueLabels[13] = GigaDisplay_CreateStatusDebugRow(13, "Loopback sent");
  g_statusDebugValueLabels[14] = GigaDisplay_CreateStatusDebugRow(14, "Loopback request");
  g_statusDebugValueLabels[15] = GigaDisplay_CreateStatusDebugRow(15, "Loopback echo/cnt");
  g_statusDebugValueLabels[16] = GigaDisplay_CreateStatusDebugRow(16, "Loopback ok/age");
  g_statusDebugValueLabels[17] = GigaDisplay_CreateStatusDebugRow(17, "M4 avg loop ms");
  g_statusDebugValueLabels[18] = GigaDisplay_CreateStatusDebugRow(18, "M4 avg exec ms");
  g_statusDebugValueLabels[19] = GigaDisplay_CreateStatusDebugRow(19, "M4 loop count");
  g_statusDebugValueLabels[20] = GigaDisplay_CreateStatusDebugRow(20, "M7 poll avg ms");
  g_statusDebugValueLabels[21] = GigaDisplay_CreateStatusDebugRow(21, "M7 poll count");

  g_statusScreenBuilt = true;
}

void GigaDisplay_ShowStatusScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  LeaveCurrentScreen();
  BeginBuildScreen(SCREEN_STATUS);

  if (!g_statusScreenBuilt) {
    GigaDisplay_CreateStatusScreen();
  }

  EndBuildScreen(SCREEN_STATUS);
  GigaDisplay_UpdateStatusScreen();
  lv_scr_load(g_statusScreen);
}

static void GigaDisplay_SetStatusDebugValue(int row, const char * valueText)
{
  if (row < 0 || row >= STATUS_DEBUG_ROW_COUNT) {
    return;
  }

  if (g_statusDebugValueLabels[row] != NULL) {
    lv_label_set_text(g_statusDebugValueLabels[row], valueText);
  }
}

static void GigaDisplay_UpdateStatusScreen()
{
  if (!g_statusScreenBuilt || !CanRefreshScreen(SCREEN_STATUS)) {
    return;
  }

  char buffer[40];

  GigaDisplay_SetStatusDebugValue(0, EROS_BUILD_MODE_NAME);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportStatusCounter());
  GigaDisplay_SetStatusDebugValue(1, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportStatusAgeMs());
  GigaDisplay_SetStatusDebugValue(2, buffer);

  GigaDisplay_SetStatusDebugValue(3, State_IsTransportStatusFresh(1000) ? "YES" : "NO");

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportStatusPublishMillis());
  GigaDisplay_SetStatusDebugValue(4, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportCommandSendAttemptCounter());
  GigaDisplay_SetStatusDebugValue(5, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportCommandSendAcceptedCounter());
  GigaDisplay_SetStatusDebugValue(6, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportCommandSendFailedCounter());
  GigaDisplay_SetStatusDebugValue(7, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportCommandAcceptedCounter());
  GigaDisplay_SetStatusDebugValue(8, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportCommandRejectedCounter());
  GigaDisplay_SetStatusDebugValue(9, buffer);

  snprintf(buffer, sizeof(buffer), "%u", State_GetTransportCommandQueueDepth());
  GigaDisplay_SetStatusDebugValue(10, buffer);

  snprintf(buffer, sizeof(buffer), "%u", State_GetTransportCommandQueueCapacity());
  GigaDisplay_SetStatusDebugValue(11, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetSettingsResultCounter());
  GigaDisplay_SetStatusDebugValue(12, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportLoopbackRequestCounter());
  GigaDisplay_SetStatusDebugValue(13, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetTransportLoopbackRequestId());
  GigaDisplay_SetStatusDebugValue(14, buffer);

  snprintf(buffer, sizeof(buffer), "%lu / %lu",
           State_GetTransportLoopbackEchoId(),
           State_GetTransportLoopbackEchoCounter());
  GigaDisplay_SetStatusDebugValue(15, buffer);

  snprintf(buffer, sizeof(buffer), "%s / %lu",
           State_GetTransportLoopbackOk() ? "OK" : "WAIT",
           State_GetTransportLoopbackEchoAgeMs());
  GigaDisplay_SetStatusDebugValue(16, buffer);

  snprintf(buffer, sizeof(buffer), "%lu.%03lu",
           ((unsigned long)State_GetM4AvgLoopPeriodUs()) / 1000UL,
           ((unsigned long)State_GetM4AvgLoopPeriodUs()) % 1000UL);
  GigaDisplay_SetStatusDebugValue(17, buffer);

  snprintf(buffer, sizeof(buffer), "%lu.%03lu",
           ((unsigned long)State_GetM4AvgLoopExecUs()) / 1000UL,
           ((unsigned long)State_GetM4AvgLoopExecUs()) % 1000UL);
  GigaDisplay_SetStatusDebugValue(18, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetM4LoopCounter());
  GigaDisplay_SetStatusDebugValue(19, buffer);

  snprintf(buffer, sizeof(buffer), "%lu.%02lu",
           ((unsigned long)State_GetM7StatusPollAvgMsX100()) / 100UL,
           ((unsigned long)State_GetM7StatusPollAvgMsX100()) % 100UL);
  GigaDisplay_SetStatusDebugValue(20, buffer);

  snprintf(buffer, sizeof(buffer), "%lu", State_GetM7StatusPollCounter());
  GigaDisplay_SetStatusDebugValue(21, buffer);
}

// ------------------------------------------------------------
// Manual screen
// ------------------------------------------------------------
static void GigaDisplay_CreateManualScreen()
{
  g_manualScreen = CreateScreen(false);

  // ------------------------------------------------------------
  // Current Hitachi output label
  // ------------------------------------------------------------
  g_manualCurrentOutputLabel = CreateTopRightOutputLabel(g_manualScreen);

  // ------------------------------------------------------------
  // Dedicated inputs
  // ------------------------------------------------------------
  CreateWhiteLabel(g_manualScreen, "Inputs", 40, 45);

  for (int i = 0; i < InSize; i++) {
    int y = 75 + (i * 42);

    CreateWhiteLabel(g_manualScreen, INPUT_NAMES[i], 40, y + 7);

    g_inputIndicators[i] = CreateIndicatorBox(g_manualScreen, 150, y, 28);

    g_inputValueLabels[i] = CreateWhiteLabel(g_manualScreen, "OFF", 190, y + 7);
  }

  // ------------------------------------------------------------
  // Assignable inputs
  // ------------------------------------------------------------
  CreateWhiteLabel(g_manualScreen, "Assignable Inputs", 40, 220);

  for (int i = 0; i < AssignableInSize; i++) {
    int y = 250 + (i * 38);

    CreateWhiteLabel(g_manualScreen, AssignableInputLabels[i], 40, y + 7);

    g_assignableInputIndicators[i] = CreateIndicatorBox(g_manualScreen, 150, y, 28);

    g_assignableInputValueLabels[i] = CreateWhiteLabel(g_manualScreen, "OFF", 190, y + 7);
  }

  // ------------------------------------------------------------
  // Physical output indicators
  // ------------------------------------------------------------
  CreateWhiteLabel(g_manualScreen, "Outputs", 330, 45);

  for (int i = 0; i < PhysicalOutSize; i++) {
    int col = i / 4;
    int row = i % 4;

    int x = 330 + (col * 220);
    int y = 75 + (row * 42);

    CreateWhiteLabel(g_manualScreen, OUTPUT_NAMES[i], x, y + 7);

    g_outputIndicators[i] = CreateIndicatorBox(g_manualScreen, x + 120, y, 28);

    g_outputValueLabels[i] = CreateWhiteLabel(g_manualScreen, "OFF", x + 158, y + 7);
  }

  // ------------------------------------------------------------
  // Manual control buttons
  // ------------------------------------------------------------
  CreateWhiteLabel(g_manualScreen, "Manual Controls", 300, 250);

  for (int i = 0; i < MANUAL_BUTTON_COUNT; i++) {
    int col = i % 4;
    int row = i / 4;

    int x = 300 + (col * 120);
    int y = 280 + (row * 58);

    CreateButton(g_manualScreen, MANUAL_BUTTON_NAMES[i], x, y, 110, 44, OutputToggle_Event, (void *)(uintptr_t)i);
  }

  // ------------------------------------------------------------
  // Navigation buttons
  // ------------------------------------------------------------
  CreateButton(g_manualScreen, "Hitachi", 535, 415, 120, 44, HitachiButton_Event, NULL);
  CreateButton(g_manualScreen, "Back", 665, 415, 120, 44, BackButton_Event, NULL);

  g_manualScreenBuilt = true;
}


void GigaDisplay_ShowManualScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  LeaveCurrentScreen();
  BeginBuildScreen(SCREEN_MANUAL);

  if (!g_manualScreenBuilt) {
    GigaDisplay_CreateManualScreen();
  }

  EndBuildScreen(SCREEN_MANUAL);
  GigaDisplay_UpdateManualIndicators();
  lv_scr_load(g_manualScreen);
}

void GigaDisplay_UpdateManualIndicators()
{
  if (!g_manualScreenBuilt || !CanRefreshScreen(SCREEN_MANUAL)) {
    return;
  }

  for (int i = 0; i < InSize; i++) {
    bool state = State_GetInput(i);
    SetIndicatorState(g_inputIndicators[i], state);
    SetValueLabel(g_inputValueLabels[i], state);
  }

  for (int i = 0; i < AssignableInSize; i++) {
    bool state = State_GetAssignableInput(i);
    SetIndicatorState(g_assignableInputIndicators[i], state);
    SetValueLabel(g_assignableInputValueLabels[i], state);
  }

  for (int i = 0; i < PhysicalOutSize; i++) {
    bool state = State_GetOutput(i);
    SetIndicatorState(g_outputIndicators[i], state);
    SetValueLabel(g_outputValueLabels[i], state);
  }

  if (g_manualCurrentOutputLabel != NULL) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Output: %d%%", State_GetHitachiCurrentOutput());
    lv_label_set_text(g_manualCurrentOutputLabel, buffer);
  }
}


// ------------------------------------------------------------
// Auto screen
// ------------------------------------------------------------

static void FormatTimeMMSS(char * buffer, size_t bufferSize, unsigned int totalSeconds)
{
  unsigned int minutes = totalSeconds / 60;
  unsigned int seconds = totalSeconds % 60;

  snprintf(buffer, bufferSize, "%u:%02u", minutes, seconds);
}

static void AutoSettings_FormatMs(char * buffer, size_t bufferSize, unsigned int ms)
{
  if (ms < 1000) {
    snprintf(buffer, bufferSize, "%u ms", ms);
  }
  else {
    float seconds = ms / 1000.0;
    snprintf(buffer, bufferSize, "%.1f s", seconds);
  }
}

static void GigaDisplay_CreateAutoScreen()
{
  g_autoScreen = CreateScreen(false);

  // ------------------------------------------------------------
  // Top status labels
  // ------------------------------------------------------------
  CreateScreenTitle(g_autoScreen, "Auto");

  g_autoStatusLabel = CreateStatusLabel(g_autoScreen, "Status: Stopped", 40, 55);
  g_autoRemainingLabel = CreateStatusLabel(g_autoScreen, "Remaining: 0:00", 300, 55);
  g_autoCurrentOutputLabel = CreateStatusLabel(g_autoScreen, "Hitachi Output: 0%", 560, 55);

  // ------------------------------------------------------------
  // Auto control buttons
  // ------------------------------------------------------------
  const int buttonY = 100;
  const int buttonW = 150;
  const int buttonH = 60;

  CreateButton(g_autoScreen, "Start", 40, buttonY, buttonW, buttonH, AutoStartButton_Event, NULL);
  CreateButton(g_autoScreen, "Stop", 220, buttonY, buttonW, buttonH, AutoStopButton_Event, NULL);
  CreateButton(g_autoScreen, "Pause", 400, buttonY, buttonW, buttonH, AutoPauseButton_Event, NULL);
  CreateButton(g_autoScreen, "Hitachi", 580, buttonY, buttonW, buttonH, HitachiButton_Event, NULL);

  // ------------------------------------------------------------
  // Input indicators
  // ------------------------------------------------------------
  CreateWhiteLabel(g_autoScreen, "Inputs", 40, 195);

  for (int i = 0; i < InSize; i++) {
    int y = 220 + (i * 42);

    CreateWhiteLabel(g_autoScreen, INPUT_NAMES[i], 40, y + 8);

    g_autoInputIndicators[i] = CreateIndicatorBox(g_autoScreen, 150, y, 30);

    g_autoInputValueLabels[i] = CreateWhiteLabel(g_autoScreen, "OFF", 190, y + 8);
  }

  // ------------------------------------------------------------
  // Assignable input indicators
  // ------------------------------------------------------------
  CreateWhiteLabel(g_autoScreen, "Assignable", 40, 350);

  for (int i = 0; i < AssignableInSize; i++) {
    int y = 375 + (i * 32);

    CreateWhiteLabel(g_autoScreen, AssignableInputLabels[i], 40, y + 6);

    g_autoAssignableInputIndicators[i] = CreateIndicatorBox(g_autoScreen, 150, y, 24);

    g_autoAssignableInputValueLabels[i] = CreateWhiteLabel(g_autoScreen, "OFF", 180, y + 6);
  }

  // ------------------------------------------------------------
  // Output indicators
  // ------------------------------------------------------------
  CreateWhiteLabel(g_autoScreen, "Outputs", 330, 195);

  for (int i = 0; i < OutSize; i++) {
    int col = i / 3;
    int row = i % 3;

    int x = 300 + (col * 165);
    int y = 220 + (row * 50);

    CreateWhiteLabel(g_autoScreen, OUTPUT_NAMES[i], x, y + 8);

    g_autoOutputIndicators[i] = CreateIndicatorBox(g_autoScreen, x + 90, y, 28);

    g_autoOutputValueLabels[i] = CreateWhiteLabel(g_autoScreen, "OFF", x + 122, y + 8);
  }

  // ------------------------------------------------------------
  // Navigation buttons
  // ------------------------------------------------------------
  CreateButton(g_autoScreen, "Settings", 535, 415, 120, 44, AutoSettingsButton_Event, NULL);
  CreateButton(g_autoScreen, "Back", 665, 415, 120, 44, AutoBackButton_Event, NULL);

  g_autoScreenBuilt = true;
}

void GigaDisplay_ShowAutoScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  LeaveCurrentScreen();
  BeginBuildScreen(SCREEN_AUTO);

  if (!g_autoScreenBuilt) {
    GigaDisplay_CreateAutoScreen();
  }

  EndBuildScreen(SCREEN_AUTO);
  GigaDisplay_UpdateAutoScreen();
  lv_scr_load(g_autoScreen);
}

static void GigaDisplay_UpdateAutoScreen()
{
  if (!g_autoScreenBuilt || !CanRefreshScreen(SCREEN_AUTO)) {
    return;
  }

  if (State_GetAutoRunning()) {
    if (State_GetAutoPaused()) {
      lv_label_set_text(g_autoStatusLabel, "Status: Paused");
    }
    else {
      lv_label_set_text(g_autoStatusLabel, "Status: Running");
    }
  }
  else {
    lv_label_set_text(g_autoStatusLabel, "Status: Stopped");
  }

  char buffer[40];

  FormatTimeMMSS(buffer, sizeof(buffer), State_GetAutoRemainingTime());

  char timeBuffer[64];
  snprintf(timeBuffer, sizeof(timeBuffer), "Remaining: %s", buffer);
  lv_label_set_text(g_autoRemainingLabel, timeBuffer);

  snprintf(timeBuffer, sizeof(timeBuffer), "Hitachi Output: %d%%", State_GetHitachiCurrentOutput());
  lv_label_set_text(g_autoCurrentOutputLabel, timeBuffer);

  for (int i = 0; i < InSize; i++) {
    bool state = State_GetInput(i);
    SetIndicatorState(g_autoInputIndicators[i], state);
    SetValueLabel(g_autoInputValueLabels[i], state);
  }

  for (int i = 0; i < AssignableInSize; i++) {
    bool state = State_GetAssignableInput(i);
    SetIndicatorState(g_autoAssignableInputIndicators[i], state);
    SetValueLabel(g_autoAssignableInputValueLabels[i], state);
  }

  for (int i = 0; i < OutSize; i++) {
    bool state = State_GetOutput(i);
    SetIndicatorState(g_autoOutputIndicators[i], state);
    SetValueLabel(g_autoOutputValueLabels[i], state);
  }
}

// ------------------------------------------------------------
//Auto Settings Helper Functions
// ------------------------------------------------------------

static int AutoSettings_MsSliderToValue(int sliderValue)
{
  sliderValue = constrain(sliderValue, 0, 100);

  if (sliderValue <= 20) {
    return map(sliderValue, 0, 20, 0, 1000);
  }
  else if (sliderValue <= 60) {
    return map(sliderValue, 21, 60, 1000, 30000);
  }
  else {
    return map(sliderValue, 61, 100, 30000, 300000);
  }
}

static int AutoSettings_MsValueToSlider(int ms)
{
  ms = constrain(ms, 0, 300000);

  if (ms <= 1000) {
    return map(ms, 0, 1000, 0, 20);
  }
  else if (ms <= 30000) {
    return map(ms, 1000, 30000, 21, 60);
  }
  else {
    return map(ms, 30000, 300000, 61, 100);
  }
}

// ------------------------------------------------------------
// Auto Settings screen
// ------------------------------------------------------------
static void CreateAutoSettingsSlider(
  lv_obj_t * parent,
  const char * labelText,
  int y,
  int minVal,
  int maxVal,
  lv_obj_t ** slider,
  lv_obj_t ** valueLabel
)
{
  *slider = CreateSliderWithValueLabel(parent, labelText, 35, 190, 690, y, 480, minVal, maxVal, "", valueLabel, AutoSettingsSlider_Event);
}

static void GigaDisplay_CreateAutoSettingsScreen()
{
  g_autoSettingsScreen = CreateScreen(true);
  // Stage 17: scrolling is enabled, but screen refresh is guarded during
  // scroll gestures/build/destroy transitions so LVGL widgets are not updated
  // while the scrollable object tree is moving.

  CreateScreenTitle(g_autoSettingsScreen, "Auto Settings");

  CreateButton(g_autoSettingsScreen, "Back", 650, 20, 120, 45, AutoSettingsBackButton_Event, NULL);

  CreateAutoSettingsSlider(g_autoSettingsScreen, "Run Duration", 85, 1, 300,
                           &g_autoRunDurationSlider, &g_autoRunDurationValueLabel);

  CreateAutoSettingsSlider(g_autoSettingsScreen, "Pause Duration", 145, 0, 300,
                           &g_autoPauseDurationSlider, &g_autoPauseDurationValueLabel);

  CreateAutoSettingsSlider(g_autoSettingsScreen, "Penalty Duration", 205, 0, 300,
                           &g_autoPenaltyDurationSlider, &g_autoPenaltyDurationValueLabel);

  CreateAutoSettingsSlider(g_autoSettingsScreen, "IO On Time", 265, 0, 100,
                           &g_autoOnTimeSlider, &g_autoOnTimeValueLabel);

  CreateAutoSettingsSlider(g_autoSettingsScreen, "IO Off Time", 325, 0, 100,
                           &g_autoOffTimeSlider, &g_autoOffTimeValueLabel);

  CreateWhiteLabel(g_autoSettingsScreen, "Output Modes", 35, 395);

  int y = 440;

  for (int outputIndex = 0; outputIndex < OutSize; outputIndex++) {
    if (outputIndex == OUT_LOCK_1 || 
      outputIndex == OUT_LOCK_2 || 
      outputIndex == OUT_DIMMER_ENABLE) {
        continue;
      }

    CreateWhiteLabel(g_autoSettingsScreen, OUTPUT_NAMES[outputIndex], 50, y + 12);

    lv_obj_t * modeBtn = CreateButton(g_autoSettingsScreen, "Off", 250, y, 260, 44, AutoOutputModeButton_Event, (void *)(uintptr_t)outputIndex);
    g_autoOutputModeLabels[outputIndex] = GetButtonLabel(modeBtn);
    g_autoOutputModeUiValue[outputIndex] = 0;

    lv_obj_t * inputBtn = CreateButton(g_autoSettingsScreen, "Input 1", 530, y, 140, 44, AutoOutputInputButton_Event, (void *)(uintptr_t)outputIndex);
    g_autoOutputInputLabels[outputIndex] = GetButtonLabel(inputBtn);
    g_autoOutputInputUiIndex[outputIndex] = 0;

    y += 58;

  }

  g_autoSettingsScreenBuilt = true;
}

void GigaDisplay_ShowAutoSettingsScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  LeaveCurrentScreen();
  BeginBuildScreen(SCREEN_AUTO_SETTINGS);

  if (!g_autoSettingsScreenBuilt) {
    GigaDisplay_CreateAutoSettingsScreen();
  }

  EndBuildScreen(SCREEN_AUTO_SETTINGS);
  GigaDisplay_UpdateAutoSettingsScreen();
  lv_scr_load(g_autoSettingsScreen);
}

static void GigaDisplay_UpdateAutoSettingsScreen()
{
  if (!g_autoSettingsScreenBuilt || !CanRefreshScreen(SCREEN_AUTO_SETTINGS)) {
    return;
  }

  g_autoSettingsUiRefreshing = true;

  char buffer[40];

  unsigned int runMinutes = State_GetAutoRunDurationMinutes();
  unsigned int pauseSeconds = State_GetAutoPauseDurationSeconds();
  unsigned int penaltySeconds = State_GetAutoPenaltyDurationSeconds();
  unsigned int onMs = State_GetAutoIoOnTimeMs();
  unsigned int offMs = State_GetAutoIoOffTimeMs();

  lv_slider_set_value(g_autoRunDurationSlider, runMinutes, LV_ANIM_OFF);
  lv_slider_set_value(g_autoPauseDurationSlider, pauseSeconds, LV_ANIM_OFF);
  lv_slider_set_value(g_autoPenaltyDurationSlider, penaltySeconds, LV_ANIM_OFF);
  lv_slider_set_value(g_autoOnTimeSlider, AutoSettings_MsValueToSlider(onMs), LV_ANIM_OFF);
  lv_slider_set_value(g_autoOffTimeSlider, AutoSettings_MsValueToSlider(offMs), LV_ANIM_OFF);

  snprintf(buffer, sizeof(buffer), "%u min", runMinutes);
  lv_label_set_text(g_autoRunDurationValueLabel, buffer);

  snprintf(buffer, sizeof(buffer), "%u s", pauseSeconds);
  lv_label_set_text(g_autoPauseDurationValueLabel, buffer);

  snprintf(buffer, sizeof(buffer), "%u s", penaltySeconds);
  lv_label_set_text(g_autoPenaltyDurationValueLabel, buffer);

  AutoSettings_FormatMs(buffer, sizeof(buffer), onMs);
  lv_label_set_text(g_autoOnTimeValueLabel, buffer);

  AutoSettings_FormatMs(buffer, sizeof(buffer), offMs);
  lv_label_set_text(g_autoOffTimeValueLabel, buffer);

  for (int outputIndex = 0; outputIndex < OutSize; outputIndex++) {
    if (outputIndex == OUT_LOCK_1 || 
      outputIndex == OUT_LOCK_2 || 
      outputIndex == OUT_DIMMER_ENABLE) {
      continue;
    }

    byte mode = State_GetAutoOutputMode(outputIndex);
    mode = constrain(mode, 0, AUTO_OUT_MODE_COUNT - 1);
    GigaDisplay_UpdateAutoOutputModeLabel(outputIndex, mode);

    int inputIndex = State_GetAutoOutputInputIndex(outputIndex);
    inputIndex = constrain(inputIndex, 0, AssignableInSize - 1);
    GigaDisplay_UpdateAutoOutputInputLabel(outputIndex, inputIndex);
  }

  g_autoSettingsUiRefreshing = false;
}

static void GigaDisplay_UpdateAutoSettingsSliderLabelsFromWidgets()
{
  char buffer[40];

  int runMinutes = lv_slider_get_value(g_autoRunDurationSlider);
  int pauseSeconds = lv_slider_get_value(g_autoPauseDurationSlider);
  int penaltySeconds = lv_slider_get_value(g_autoPenaltyDurationSlider);
  int onMs = AutoSettings_MsSliderToValue(lv_slider_get_value(g_autoOnTimeSlider));
  int offMs = AutoSettings_MsSliderToValue(lv_slider_get_value(g_autoOffTimeSlider));

  snprintf(buffer, sizeof(buffer), "%d min", runMinutes);
  lv_label_set_text(g_autoRunDurationValueLabel, buffer);

  snprintf(buffer, sizeof(buffer), "%d s", pauseSeconds);
  lv_label_set_text(g_autoPauseDurationValueLabel, buffer);

  snprintf(buffer, sizeof(buffer), "%d s", penaltySeconds);
  lv_label_set_text(g_autoPenaltyDurationValueLabel, buffer);

  AutoSettings_FormatMs(buffer, sizeof(buffer), onMs);
  lv_label_set_text(g_autoOnTimeValueLabel, buffer);

  AutoSettings_FormatMs(buffer, sizeof(buffer), offMs);
  lv_label_set_text(g_autoOffTimeValueLabel, buffer);
}

static void GigaDisplay_UpdateAutoOutputModeLabel(int outputIndex, byte mode)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  mode = constrain(mode, 0, AUTO_OUT_MODE_COUNT - 1);
  g_autoOutputModeUiValue[outputIndex] = mode;

  if (g_autoOutputModeLabels[outputIndex] != NULL) {
    lv_label_set_text(g_autoOutputModeLabels[outputIndex], AUTO_OUT_MODE_NAMES[mode]);
  }
}

static void GigaDisplay_UpdateAutoOutputInputLabel(int outputIndex, int inputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  inputIndex = constrain(inputIndex, 0, AssignableInSize - 1);
  g_autoOutputInputUiIndex[outputIndex] = inputIndex;

  if (g_autoOutputInputLabels[outputIndex] != NULL) {
    lv_label_set_text(g_autoOutputInputLabels[outputIndex], AssignableInputLabels[inputIndex]);
  }
}

// ------------------------------------------------------------
// Hitachi helper functions
// ------------------------------------------------------------

static const char * Hitachi_GetModeName(int mode)
{
  for (int i = 0; i < HITACHI_MODE_COUNT; i++) {
    if (HITACHI_MODE_VALUES[i] == mode) {
      return HITACHI_MODE_NAMES[i];
    }
  }

  return "Unknown";
}

static int Hitachi_PeriodSliderToMs(int sliderValue)
{
  sliderValue = constrain(sliderValue, 0, 100);

  if (g_hitachiPeriodPreciseMode) {
    return map(sliderValue, 0, 100, 100, 2000);
  }

  if (sliderValue <= 20) {
    return map(sliderValue, 0, 20, 100, 1000);
  }
  else if (sliderValue <= 60) {
    return map(sliderValue, 21, 60, 1000, 30000);
  }
  else {
    return map(sliderValue, 61, 100, 30000, 300000);
  }
}

static int Hitachi_PeriodMsToSlider(int periodMs)
{
  periodMs = constrain(periodMs, 100, 300000);

  if (g_hitachiPeriodPreciseMode) {
    periodMs = constrain(periodMs, 100, 2000);
    return map(periodMs, 100, 2000, 0, 100);
  }

  if (periodMs <= 1000) {
    return map(periodMs, 100, 1000, 0, 20);
  }
  else if (periodMs <= 30000) {
    return map(periodMs, 1000, 30000, 21, 60);
  }
  else {
    return map(periodMs, 30000, 300000, 61, 100);
  }
}

static void Hitachi_FormatPeriod(char * buffer, size_t bufferSize, int periodMs)
{
  if (periodMs < 1000) {
    snprintf(buffer, bufferSize, "%d ms", periodMs);
  }
  else {
    float seconds = periodMs / 1000.0;
    snprintf(buffer, bufferSize, "%.1f s", seconds);
  }
}

// ------------------------------------------------------------
// Hitachi screen
// ------------------------------------------------------------

static void GigaDisplay_CreateHitachiScreen()
{
  g_hitachiScreen = CreateScreen(false);

  CreateScreenTitle(g_hitachiScreen, "Hitachi");

  g_hitachiCurrentOutputLabel = CreateTopRightOutputLabel(g_hitachiScreen);

  CreateButton(g_hitachiScreen, "Edit ON", 30, 55, 110, 42, HitachiEditOnButton_Event, NULL);
  CreateButton(g_hitachiScreen, "Edit OFF", 155, 55, 110, 42, HitachiEditOffButton_Event, NULL);

  g_hitachiEditorLabel = CreateWhiteLabel(g_hitachiScreen, "Editing: ON settings", 290, 67);
  g_hitachiModeLabel = CreateWhiteLabel(g_hitachiScreen, "Mode: Value", 520, 67);

  for (int i = 0; i < HITACHI_MODE_COUNT; i++) {
    CreateButton(g_hitachiScreen, HITACHI_MODE_NAMES[i], 30 + (i * 108), 112, 102, 42, HitachiModeButton_Event, (void *)(uintptr_t)i);
  }

  const int labelX = 35;
  const int sliderX = 160;
  const int valueX = 690;
  const int sliderW = 500;

  g_hitachiSetPointSlider = CreateSliderWithValueLabel(
    g_hitachiScreen, "Set Point", labelX, sliderX, valueX, 185, sliderW, State_GetHitachiMinRelayValue(),
    100, "25%", &g_hitachiSetPointValueLabel, HitachiSlider_Event);

  g_hitachiMaxSlider = CreateSliderWithValueLabel(
    g_hitachiScreen, "Max Value", labelX, sliderX, valueX, 245, sliderW, State_GetHitachiMinRelayValue(), 
    100, "100%", &g_hitachiMaxValueLabel, HitachiSlider_Event);  

  g_hitachiMinSlider = CreateSliderWithValueLabel(
    g_hitachiScreen, "Min Value", labelX, sliderX, valueX, 305, sliderW, State_GetHitachiMinRelayValue(), 
    100, "25%", &g_hitachiMinValueLabel, HitachiSlider_Event);

  g_hitachiPeriodSlider = CreateSliderWithValueLabel(
    g_hitachiScreen, "Period", labelX, sliderX, valueX, 365, sliderW, 0, 100, "0.1 s", 
    &g_hitachiPeriodValueLabel, HitachiSlider_Event);

  lv_obj_t * periodModeBtn = CreateButton(g_hitachiScreen, "Period: Precise", 35, 415, 170, 42, HitachiPeriodModeButton_Event, NULL);
  g_hitachiPeriodModeLabel = GetButtonLabel(periodModeBtn);

  g_hitachiDimmerEnableLabel = CreateCenteredWhiteLabel(g_hitachiScreen, "Dimmer: OFF", LV_ALIGN_BOTTOM_MID, -115, -38);

  CreateAlignedButton(g_hitachiScreen, "Relay Min", LV_ALIGN_BOTTOM_RIGHT, -175, -25, 130, 50, HitachiRelayMinButton_Event, NULL);
  CreateAlignedButton(g_hitachiScreen, "Back", LV_ALIGN_BOTTOM_RIGHT, -30, -25, 130, 50, HitachiBackButton_Event, NULL);

  g_hitachiScreenBuilt = true;
}

void GigaDisplay_ShowHitachiScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  g_previousScreenBeforeHitachi = lv_scr_act();

  LeaveCurrentScreen();
  BeginBuildScreen(SCREEN_HITACHI);

  if (!g_hitachiScreenBuilt) {
    GigaDisplay_CreateHitachiScreen();
  }

  EndBuildScreen(SCREEN_HITACHI);
  GigaDisplay_UpdateHitachiScreen();
  lv_scr_load(g_hitachiScreen);
}

static void GigaDisplay_UpdateHitachiScreen()
{
  if (!g_hitachiScreenBuilt || g_hitachiUiRefreshing || !CanRefreshScreen(SCREEN_HITACHI)) {
    return;
  }

  g_hitachiUiRefreshing = true;

  int minRelayValue = State_GetHitachiMinRelayValue();

  int setPoint = constrain(State_GetHitachiSetPoint(g_hitachiEditingOnSettings), minRelayValue, 100);
  int maxValue = constrain(State_GetHitachiMaxValue(g_hitachiEditingOnSettings), minRelayValue, 100);
  int minValue = constrain(State_GetHitachiMinValue(g_hitachiEditingOnSettings), minRelayValue, 100);

  lv_slider_set_range(g_hitachiSetPointSlider, minRelayValue, 100);
  lv_slider_set_range(g_hitachiMaxSlider, minRelayValue, 100);
  lv_slider_set_range(g_hitachiMinSlider, minRelayValue, 100);

  // Display refresh is read-only. Do not send Hitachi commands here.
  // M7 commands, M4 decides, M7 displays M4-reported truth.
  g_hitachiPeriodPreciseMode =
    State_GetHitachiPeriodPrecise(g_hitachiEditingOnSettings);

  int periodMs = State_GetHitachiPeriod(g_hitachiEditingOnSettings);
  periodMs = constrain(periodMs, 100, 300000);

  int mode = State_GetHitachiMode(g_hitachiEditingOnSettings);

  lv_label_set_text(
    g_hitachiEditorLabel,
    g_hitachiEditingOnSettings ? "Editing: ON settings" : "Editing: OFF settings"
  );

  char buffer[32];

  snprintf(buffer, sizeof(buffer), "Mode: %s", Hitachi_GetModeName(mode));
  lv_label_set_text(g_hitachiModeLabel, buffer);

  snprintf(buffer, sizeof(buffer), "Output: %d%%", State_GetHitachiCurrentOutput());
  lv_label_set_text(g_hitachiCurrentOutputLabel, buffer);

  lv_label_set_text(
    g_hitachiDimmerEnableLabel,
    State_GetDimmerEnabledRequest() ? "Dimmer: ON" : "Dimmer: OFF"
  );

  lv_label_set_text(
    g_hitachiPeriodModeLabel,
    g_hitachiPeriodPreciseMode ? "Period: Precise" : "Period: Coarse"
  );

  lv_slider_set_value(g_hitachiSetPointSlider, setPoint, LV_ANIM_OFF);
  lv_slider_set_value(g_hitachiMaxSlider, maxValue, LV_ANIM_OFF);
  lv_slider_set_value(g_hitachiMinSlider, minValue, LV_ANIM_OFF);
  lv_slider_set_value(g_hitachiPeriodSlider, Hitachi_PeriodMsToSlider(periodMs), LV_ANIM_OFF);

  snprintf(buffer, sizeof(buffer), "%d%%", setPoint);
  lv_label_set_text(g_hitachiSetPointValueLabel, buffer);

  snprintf(buffer, sizeof(buffer), "%d%%", maxValue);
  lv_label_set_text(g_hitachiMaxValueLabel, buffer);

  snprintf(buffer, sizeof(buffer), "%d%%", minValue);
  lv_label_set_text(g_hitachiMinValueLabel, buffer);

  Hitachi_FormatPeriod(buffer, sizeof(buffer), periodMs);
  lv_label_set_text(g_hitachiPeriodValueLabel, buffer);

  g_hitachiUiRefreshing = false;
}

static void GigaDisplay_UpdateHitachiSliderLabelsFromWidgets()
{
  if (!g_hitachiScreenBuilt) {
    return;
  }

  char buffer[32];

  if (g_hitachiSetPointSlider != NULL && g_hitachiSetPointValueLabel != NULL) {
    snprintf(buffer, sizeof(buffer), "%d%%", lv_slider_get_value(g_hitachiSetPointSlider));
    lv_label_set_text(g_hitachiSetPointValueLabel, buffer);
  }

  if (g_hitachiMaxSlider != NULL && g_hitachiMaxValueLabel != NULL) {
    snprintf(buffer, sizeof(buffer), "%d%%", lv_slider_get_value(g_hitachiMaxSlider));
    lv_label_set_text(g_hitachiMaxValueLabel, buffer);
  }

  if (g_hitachiMinSlider != NULL && g_hitachiMinValueLabel != NULL) {
    snprintf(buffer, sizeof(buffer), "%d%%", lv_slider_get_value(g_hitachiMinSlider));
    lv_label_set_text(g_hitachiMinValueLabel, buffer);
  }

  if (g_hitachiPeriodSlider != NULL && g_hitachiPeriodValueLabel != NULL) {
    int periodMs = Hitachi_PeriodSliderToMs(lv_slider_get_value(g_hitachiPeriodSlider));
    Hitachi_FormatPeriod(buffer, sizeof(buffer), periodMs);
    lv_label_set_text(g_hitachiPeriodValueLabel, buffer);
  }
}

// ------------------------------------------------------------
// Hitachi Min Relay screen
// ------------------------------------------------------------
static void GigaDisplay_CreateHitachiRelayMinScreen()
{
  g_hitachiRelayMinScreen = CreateScreen(false);

  CreateCenteredWhiteLabel(g_hitachiRelayMinScreen, "Dimmer Relay Minimum", LV_ALIGN_TOP_MID, 0, 30);
  CreateCenteredWhiteLabel(g_hitachiRelayMinScreen, "Relay turns ON when Hitachi output is at or above this value.", LV_ALIGN_TOP_MID, 0, 80);

  g_hitachiRelayMinValueLabel = CreateCenteredWhiteLabel(g_hitachiRelayMinScreen, "Min: 25%", LV_ALIGN_TOP_MID, 0, 145);

  const int buttonW = 120;
  const int buttonH = 60;
  const int y1 = 210;

  CreateButton(g_hitachiRelayMinScreen, "-5", 180, y1, buttonW, buttonH, HitachiRelayMinMinus5Button_Event, NULL);
  CreateButton(g_hitachiRelayMinScreen, "-1", 320, y1, buttonW, buttonH, HitachiRelayMinMinusButton_Event, NULL);
  CreateButton(g_hitachiRelayMinScreen, "+1", 460, y1, buttonW, buttonH, HitachiRelayMinPlusButton_Event, NULL);
  CreateButton(g_hitachiRelayMinScreen, "+5", 600, y1, buttonW, buttonH, HitachiRelayMinPlus5Button_Event, NULL);

  CreateAlignedButton(g_hitachiRelayMinScreen, "Back", LV_ALIGN_BOTTOM_RIGHT, -30, -25, 130, 50, HitachiRelayMinBackButton_Event, NULL);

  g_hitachiRelayMinScreenBuilt = true;
}

void GigaDisplay_ShowHitachiRelayMinScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  g_previousScreenBeforeRelayMin = lv_scr_act();

  LeaveCurrentScreen();
  BeginBuildScreen(SCREEN_HITACHI_RELAY_MIN);

  if (!g_hitachiRelayMinScreenBuilt) {
    GigaDisplay_CreateHitachiRelayMinScreen();
  }

  EndBuildScreen(SCREEN_HITACHI_RELAY_MIN);
  GigaDisplay_UpdateHitachiRelayMinScreen();
  lv_scr_load(g_hitachiRelayMinScreen);
}

static void GigaDisplay_UpdateHitachiRelayMinScreen()
{
  if (!g_hitachiRelayMinScreenBuilt || !CanRefreshScreen(SCREEN_HITACHI_RELAY_MIN)) {
    return;
  }

  g_hitachiRelayMinUiValue = constrain(State_GetHitachiMinRelayValue(), 1, 100);
  GigaDisplay_UpdateHitachiRelayMinLabelFromUiValue();
}

static void GigaDisplay_UpdateHitachiRelayMinLabelFromUiValue()
{
  if (g_hitachiRelayMinValueLabel == NULL) {
    return;
  }

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "Min: %d%%", g_hitachiRelayMinUiValue);
  lv_label_set_text(g_hitachiRelayMinValueLabel, buffer);
}

// ------------------------------------------------------------
// Button event handlers
// ------------------------------------------------------------

static void StatusButton_Event(lv_event_t * e)
{
  GigaDisplay_ShowStatusScreen();
}

static void StatusBackButton_Event(lv_event_t * e)
{
  GigaDisplay_ShowIdleScreen();
}

static void StatusPingButton_Event(lv_event_t * e)
{
  Command_RequestTransportLoopbackPing();
  GigaDisplay_UpdateStatusScreen();
}

static void ManualButton_Event(lv_event_t * e)
{
  Command_SetMode(BridgeMode);
  GigaDisplay_ShowManualScreen();
}

static void AutoButton_Event(lv_event_t * e)
{
  Command_SetMode(EROSFlexMode);
  GigaDisplay_ShowAutoScreen();
}

static void BackButton_Event(lv_event_t * e)
{
  Command_SetMode(BridgeMode);
  GigaDisplay_ShowIdleScreen();
}

static void AutoStartButton_Event(lv_event_t * e)
{
  Command_SetMode(EROSFlexMode);
  Command_RequestAutoStart();

  if (g_autoStatusLabel != NULL) {
    lv_label_set_text(g_autoStatusLabel, "Status: Start Requested");
  }

  g_autoSkipNextAutoRefresh = true;
}

static void AutoStopButton_Event(lv_event_t * e)
{
  Command_RequestAutoStop();

  if (g_autoStatusLabel != NULL) {
    lv_label_set_text(g_autoStatusLabel, "Status: Stop Requested");
  }

  g_autoSkipNextAutoRefresh = true;
}

static void AutoPauseButton_Event(lv_event_t * e)
{
  Command_RequestAutoPause();

  if (g_autoStatusLabel != NULL) {
    lv_label_set_text(g_autoStatusLabel, "Status: Pause Requested");
  }

  g_autoSkipNextAutoRefresh = true;
}

static void AutoBackButton_Event(lv_event_t * e)
{
  GigaDisplay_ShowIdleScreen();
}

static void OutputToggle_Event(lv_event_t * e)
{
  int buttonIdx = (int)(uintptr_t)lv_event_get_user_data(e);

  if (buttonIdx < 0 || buttonIdx >= MANUAL_BUTTON_COUNT) {
    return;
  }

  int outputIdx = MANUAL_BUTTON_OUTPUT_INDEX[buttonIdx];

  if (outputIdx == OUT_LOCK_1) {
    Command_ToggleLock();
  }
  else if (outputIdx == OUT_HITACHI_VIRTUAL) {
    Command_ToggleHitachiVirtualRequest();
  }
  else {
    Command_ToggleManualOutput(outputIdx);
  }

  // Do not optimistically change indicators here.
  // The displayed state should come only from the next M4 status mask poll,
  // because M4 owns physical inputs, automatic modes, and actual output state.
  g_manualSkipNextAutoRefresh = false;
}

static void AutoSettingsButton_Event(lv_event_t * e)
{
  GigaDisplay_ShowAutoSettingsScreen();
}

static void AutoSettingsBackButton_Event(lv_event_t * e)
{
  GigaDisplay_ShowAutoScreen();
  GigaDisplay_DestroyAutoSettingsScreen();
}

static void AutoSettingsSlider_Event(lv_event_t * e)
{
  if (g_autoSettingsUiRefreshing) {
    return;
  }

  lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
  int sliderValue = lv_slider_get_value(slider);

  if (slider == g_autoRunDurationSlider) {
    Command_SetAutoRunDurationMinutes(sliderValue);
  }
  else if (slider == g_autoPauseDurationSlider) {
    Command_SetAutoPauseDurationSeconds(sliderValue);
  }
  else if (slider == g_autoPenaltyDurationSlider) {
    Command_SetAutoPenaltyDurationSeconds(sliderValue);
  }
  else if (slider == g_autoOnTimeSlider) {
    Command_SetAutoIoOnTimeMs(AutoSettings_MsSliderToValue(sliderValue));
  }
  else if (slider == g_autoOffTimeSlider) {
    Command_SetAutoIoOffTimeMs(AutoSettings_MsSliderToValue(sliderValue));
  }

  // Update visible text from the widget value immediately.
  // Do not refresh the full screen here, because commands are deferred and
  // the status snapshot still contains the previous setting until loop()
  // drains the command queue.
  GigaDisplay_UpdateAutoSettingsSliderLabelsFromWidgets();
}

static void AutoOutputModeButton_Event(lv_event_t * e)
{
  int outputIndex = (int)(uintptr_t)lv_event_get_user_data(e);

  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  byte mode = g_autoOutputModeUiValue[outputIndex];
  mode++;

  if (mode >= AUTO_OUT_MODE_COUNT) {
    mode = 0;
  }

  Command_SetAutoOutputMode(outputIndex, mode);
  GigaDisplay_UpdateAutoOutputModeLabel(outputIndex, mode);
}

static void AutoOutputInputButton_Event(lv_event_t * e)
{
  int outputIndex = (int)(uintptr_t)lv_event_get_user_data(e);

  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  int inputIndex = g_autoOutputInputUiIndex[outputIndex] + 1;

  if (inputIndex >= AssignableInSize) {
    inputIndex = 0;
  }

  Command_SetAutoOutputInputIndex(outputIndex, inputIndex);
  GigaDisplay_UpdateAutoOutputInputLabel(outputIndex, inputIndex);
}

static void HitachiButton_Event(lv_event_t * e)
{
  GigaDisplay_ShowHitachiScreen();
}

static void HitachiBackButton_Event(lv_event_t * e)
{
  lv_obj_t * previousScreen = g_previousScreenBeforeHitachi;

  if (previousScreen == g_manualScreen) {
    GigaDisplay_ShowManualScreen();
  }
  else if (previousScreen == g_autoScreen) {
    GigaDisplay_ShowAutoScreen();
  }
  else if (previousScreen == g_statusScreen) {
    GigaDisplay_ShowStatusScreen();
  }
  else {
    GigaDisplay_ShowIdleScreen();
  }

  GigaDisplay_DestroyHitachiScreen();
}

static void HitachiEditOnButton_Event(lv_event_t * e)
{
  g_hitachiEditingOnSettings = true;
  GigaDisplay_UpdateHitachiScreen();
}

static void HitachiEditOffButton_Event(lv_event_t * e)
{
  g_hitachiEditingOnSettings = false;
  GigaDisplay_UpdateHitachiScreen();
}

static void HitachiModeButton_Event(lv_event_t * e)
{
  int modeIdx = (int)(uintptr_t)lv_event_get_user_data(e);

  if (modeIdx < 0 || modeIdx >= HITACHI_MODE_COUNT) {
    return;
  }

  int mode = HITACHI_MODE_VALUES[modeIdx];
  Command_SetHitachiMode(g_hitachiEditingOnSettings, mode);

  // Do not optimistically redraw the mode label. It will update from M4 status.
  g_hitachiSkipNextAutoRefresh = true;
}

static void HitachiSlider_Event(lv_event_t * e)
{
  if (g_hitachiUiRefreshing) {
    return;
  }

  lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
  int minRelayValue = State_GetHitachiMinRelayValue();
  int sliderValue = lv_slider_get_value(slider);

  if (slider == g_hitachiSetPointSlider) {
    sliderValue = constrain(sliderValue, minRelayValue, 100);
    lv_slider_set_value(g_hitachiSetPointSlider, sliderValue, LV_ANIM_OFF);
    Command_SetHitachiSetPoint(g_hitachiEditingOnSettings, sliderValue);
  }
  else if (slider == g_hitachiMaxSlider) {
    sliderValue = constrain(sliderValue, minRelayValue, 100);
    lv_slider_set_value(g_hitachiMaxSlider, sliderValue, LV_ANIM_OFF);
    Command_SetHitachiMaxValue(g_hitachiEditingOnSettings, sliderValue);
  }
  else if (slider == g_hitachiMinSlider) {
    sliderValue = constrain(sliderValue, minRelayValue, 100);
    lv_slider_set_value(g_hitachiMinSlider, sliderValue, LV_ANIM_OFF);
    Command_SetHitachiMinValue(g_hitachiEditingOnSettings, sliderValue);
  }
  else if (slider == g_hitachiPeriodSlider) {
    Command_SetHitachiPeriod(
      g_hitachiEditingOnSettings,
      Hitachi_PeriodSliderToMs(sliderValue)
    );
  }

  // Update the visible slider text from the event/widget values immediately.
  // Do not call GigaDisplay_UpdateHitachiScreen() here, because when commands
  // are deferred it would read the previous status snapshot and snap the
  // slider back before loop() has processed the command queue.
  GigaDisplay_UpdateHitachiSliderLabelsFromWidgets();
  g_hitachiSkipNextAutoRefresh = true;
}

static void HitachiPeriodModeButton_Event(lv_event_t * e)
{
  bool requestedPrecise = !State_GetHitachiPeriodPrecise(g_hitachiEditingOnSettings);
  Command_SetHitachiPeriodPrecise(g_hitachiEditingOnSettings, requestedPrecise);

  // Do not optimistically redraw the label. It will update from M4 status.
  g_hitachiSkipNextAutoRefresh = true;
}

static void HitachiRelayMinButton_Event(lv_event_t * e)
{
  GigaDisplay_ShowHitachiRelayMinScreen();
}

static void HitachiRelayMinMinusButton_Event(lv_event_t * e)
{
  g_hitachiRelayMinUiValue = constrain(g_hitachiRelayMinUiValue - 1, 1, 100);
  Command_SetHitachiMinRelayValue(g_hitachiRelayMinUiValue);
  GigaDisplay_UpdateHitachiRelayMinLabelFromUiValue();
}

static void HitachiRelayMinPlusButton_Event(lv_event_t * e)
{
  g_hitachiRelayMinUiValue = constrain(g_hitachiRelayMinUiValue + 1, 1, 100);
  Command_SetHitachiMinRelayValue(g_hitachiRelayMinUiValue);
  GigaDisplay_UpdateHitachiRelayMinLabelFromUiValue();
}

static void HitachiRelayMinMinus5Button_Event(lv_event_t * e)
{
  g_hitachiRelayMinUiValue = constrain(g_hitachiRelayMinUiValue - 5, 1, 100);
  Command_SetHitachiMinRelayValue(g_hitachiRelayMinUiValue);
  GigaDisplay_UpdateHitachiRelayMinLabelFromUiValue();
}

static void HitachiRelayMinPlus5Button_Event(lv_event_t * e)
{
  g_hitachiRelayMinUiValue = constrain(g_hitachiRelayMinUiValue + 5, 1, 100);
  Command_SetHitachiMinRelayValue(g_hitachiRelayMinUiValue);
  GigaDisplay_UpdateHitachiRelayMinLabelFromUiValue();
}

static void HitachiRelayMinBackButton_Event(lv_event_t * e)
{
  if (g_hitachiScreenBuilt && g_hitachiScreen != NULL) {
    LeaveCurrentScreen();
    BeginBuildScreen(SCREEN_HITACHI);
    EndBuildScreen(SCREEN_HITACHI);
    GigaDisplay_UpdateHitachiScreen();
    lv_scr_load(g_hitachiScreen);
  }
  else {
    GigaDisplay_ShowHitachiScreen();
  }
}

static void SaveSettingsButton_Event(lv_event_t * e)
{
  Command_RequestSettingsSave();
  SetIdleStatusText("M7 save not implemented yet");
}

static void LoadSettingsButton_Event(lv_event_t * e)
{
  Command_RequestSettingsLoad();
  SetIdleStatusText("M7 load not implemented yet");
}

// ------------------------------------------------------------
//Misc Helper Functions
// ------------------------------------------------------------

#endif  // EROS_BUILD_HAS_M7_SIDE
