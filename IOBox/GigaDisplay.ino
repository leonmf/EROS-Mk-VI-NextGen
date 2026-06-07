/*
  GigaDisplay.ino

  Arduino Giga Display Shield UI for EROS Mk VI.

  Current screens:
    - Idle screen
    - Manual screen
    - Hitachi screen

  This version uses the EROSState.ino command/state interface instead of
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

#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"

// ------------------------------------------------------------
// Giga Display hardware objects
// ------------------------------------------------------------

Arduino_H7_Video Display(800, 480, GigaDisplayShield);
Arduino_GigaDisplayTouch TouchDetector;

// ------------------------------------------------------------
// Output indexes
// These must match the logical output order in IOBox.ino
// ------------------------------------------------------------

#ifndef OUT_LOCK_1
#define OUT_LOCK_1              0
#endif

#ifndef OUT_LOCK_2
#define OUT_LOCK_2              1
#endif

#ifndef OUT_AC
#define OUT_AC                  2
#endif

#ifndef OUT_DIMMER_ENABLE
#define OUT_DIMMER_ENABLE  3
#endif

#ifndef OUT_DRY_1
#define OUT_DRY_1               4
#endif

#ifndef OUT_DRY_2
#define OUT_DRY_2               5
#endif

#ifndef OUT_DRY_3
#define OUT_DRY_3               6
#endif

#ifndef OUT_DRY_4
#define OUT_DRY_4               7
#endif

#ifndef OUT_HITACHI_VIRTUAL
#define OUT_HITACHI_VIRTUAL     8
#endif

// ------------------------------------------------------------
// Hitachi mode fallback defines
// ------------------------------------------------------------

#ifndef hitachiOff
#define hitachiOff       0
#endif

#ifndef hitachiValue
#define hitachiValue     1
#endif

#ifndef hitachiPulse
#define hitachiPulse     2
#endif

#ifndef hitachiSine
#define hitachiSine      3
#endif

#ifndef hitachiSawTooth
#define hitachiSawTooth  4
#endif

#ifndef hitachiTriangle
#define hitachiTriangle  5
#endif

#ifndef hitachiRandom
#define hitachiRandom    6
#endif

// ------------------------------------------------------------
// Screen object pointers
// ------------------------------------------------------------

static lv_obj_t * g_idleScreen = NULL;
static lv_obj_t * g_manualScreen = NULL;
static lv_obj_t * g_autoScreen = NULL;
static lv_obj_t * g_autoSettingsScreen = NULL;
static lv_obj_t * g_hitachiScreen = NULL;
static lv_obj_t * g_hitachiRelayMinScreen = NULL;

static lv_obj_t * g_previousScreenBeforeHitachi = NULL;
static lv_obj_t * g_previousScreenBeforeRelayMin = NULL;

// ------------------------------------------------------------
// Idle screen widgets
// ------------------------------------------------------------

static lv_obj_t * g_idleStatusLabel = NULL;

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

static bool g_manualScreenBuilt = false;
static bool g_autoScreenBuilt = false;
static bool g_autoSettingsScreenBuilt = false;
static bool g_hitachiScreenBuilt = false;
static bool g_hitachiRelayMinScreenBuilt = false;

static bool g_hitachiEditingOnSettings = true;
static bool g_hitachiUiRefreshing = false;
static bool g_hitachiPeriodPreciseMode = true;


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

void GigaDisplay_UpdateManualIndicators();

static void GigaDisplay_CreateIdleScreen();
static void GigaDisplay_CreateManualScreen();
static void GigaDisplay_CreateHitachiScreen();
static void GigaDisplay_CreateHitachiRelayMinScreen();
static void GigaDisplay_CreateAutoScreen();
static void GigaDisplay_CreateAutoSettingsScreen();
static void GigaDisplay_DestroyAutoSettingsScreen();
static void GigaDisplay_DestroyHitachiScreen();

static void GigaDisplay_UpdateHitachiScreen();
static void GigaDisplay_UpdateHitachiRelayMinScreen();
static void GigaDisplay_UpdateAutoScreen();
static void GigaDisplay_UpdateAutoSettingsScreen();

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
  snprintf(buffer, sizeof(buffer), "%s: %d", prefix, Settings_GetLastError());
  lv_label_set_text(g_idleStatusLabel, buffer);
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

  if (lv_scr_act() == g_manualScreen) {
    GigaDisplay_UpdateManualIndicators();
    //Serial.println("Manual Screen");
  }
  else if (lv_scr_act() == g_autoScreen) {
    GigaDisplay_UpdateAutoScreen();
    //Serial.println("Auto Screen");
  }
  else if (lv_scr_act() == g_hitachiScreen) {
    GigaDisplay_UpdateHitachiScreen();
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
  g_idleScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(g_idleScreen, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_clear_flag(g_idleScreen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * border = lv_obj_create(g_idleScreen);
  lv_obj_set_size(border, Display.width() - 20, Display.height() - 20);
  lv_obj_set_pos(border, 10, 10);
  lv_obj_set_style_bg_opa(border, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(border, 3, LV_PART_MAIN);
  lv_obj_set_style_border_color(border, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_radius(border, 8, LV_PART_MAIN);
  lv_obj_clear_flag(border, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * title = lv_label_create(g_idleScreen);
  lv_label_set_text(title, "EROS Mk VI");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 32);

  const int mainButtonW = 210;
  const int mainButtonH = 95;
  const int mainY = 145;

  const int manualX = 75;
  const int autoX = 295;
  const int hitachiX = 515;

  lv_obj_t * manualBtn = lv_btn_create(g_idleScreen);
  lv_obj_set_size(manualBtn, mainButtonW, mainButtonH);
  lv_obj_set_pos(manualBtn, manualX, mainY);
  lv_obj_add_event_cb(manualBtn, ManualButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * manualLabel = lv_label_create(manualBtn);
  lv_label_set_text(manualLabel, "Manual");
  lv_obj_set_style_text_font(manualLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(manualLabel);

  lv_obj_t * autoBtn = lv_btn_create(g_idleScreen);
  lv_obj_set_size(autoBtn, mainButtonW, mainButtonH);
  lv_obj_set_pos(autoBtn, autoX, mainY);
  lv_obj_add_event_cb(autoBtn, AutoButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * autoLabel = lv_label_create(autoBtn);
  lv_label_set_text(autoLabel, "Auto");
  lv_obj_set_style_text_font(autoLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(autoLabel);

  lv_obj_t * hitachiBtn = lv_btn_create(g_idleScreen);
  lv_obj_set_size(hitachiBtn, mainButtonW, mainButtonH);
  lv_obj_set_pos(hitachiBtn, hitachiX, mainY);
  lv_obj_add_event_cb(hitachiBtn, HitachiButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * hitachiLabel = lv_label_create(hitachiBtn);
  lv_label_set_text(hitachiLabel, "Hitachi");
  lv_obj_set_style_text_font(hitachiLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(hitachiLabel);

  const int settingsButtonW = 220;
  const int settingsButtonH = 65;
  const int settingsY = 335;

  lv_obj_t * saveBtn = lv_btn_create(g_idleScreen);
  lv_obj_set_size(saveBtn, settingsButtonW, settingsButtonH);
  lv_obj_set_pos(saveBtn, 160, settingsY);
  lv_obj_add_event_cb(saveBtn, SaveSettingsButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * saveLabel = lv_label_create(saveBtn);
  lv_label_set_text(saveLabel, "Save Settings");
  lv_obj_set_style_text_font(saveLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(saveLabel);

  lv_obj_t * loadBtn = lv_btn_create(g_idleScreen);
  lv_obj_set_size(loadBtn, settingsButtonW, settingsButtonH);
  lv_obj_set_pos(loadBtn, 420, settingsY);
  lv_obj_add_event_cb(loadBtn, LoadSettingsButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * loadLabel = lv_label_create(loadBtn);
  lv_label_set_text(loadLabel, "Load Settings");
  lv_obj_set_style_text_font(loadLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(loadLabel);

  g_idleStatusLabel = lv_label_create(g_idleScreen);
  lv_label_set_text(g_idleStatusLabel, "Settings ready");
  lv_obj_set_style_text_color(g_idleStatusLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_idleStatusLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(g_idleStatusLabel, LV_ALIGN_BOTTOM_MID, 0, -32);
}

void GigaDisplay_ShowIdleScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  if (g_idleScreen == NULL) {
    GigaDisplay_CreateIdleScreen();
  }
  lv_scr_load(g_idleScreen);
}

// ------------------------------------------------------------
// Manual screen
// ------------------------------------------------------------

static void GigaDisplay_CreateManualScreen()
{
  g_manualScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(g_manualScreen, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_clear_flag(g_manualScreen, LV_OBJ_FLAG_SCROLLABLE);

  // ------------------------------------------------------------
  // Current Hitachi output label
  // ------------------------------------------------------------
  g_manualCurrentOutputLabel = lv_label_create(g_manualScreen);
  lv_label_set_text(g_manualCurrentOutputLabel, "Output: 0%");
  lv_obj_set_style_text_color(g_manualCurrentOutputLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_manualCurrentOutputLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(g_manualCurrentOutputLabel, LV_ALIGN_TOP_RIGHT, -30, 14);

  // ------------------------------------------------------------
  // Dedicated inputs
  // ------------------------------------------------------------
  lv_obj_t * inputHeader = lv_label_create(g_manualScreen);
  lv_label_set_text(inputHeader, "Inputs");
  lv_obj_set_style_text_color(inputHeader, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(inputHeader, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(inputHeader, 40, 45);

  for (int i = 0; i < InSize; i++) {
    int y = 75 + (i * 42);

    lv_obj_t * name = lv_label_create(g_manualScreen);
    lv_label_set_text(name, INPUT_NAMES[i]);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(name, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(name, 40, y + 7);

    g_inputIndicators[i] = lv_obj_create(g_manualScreen);
    lv_obj_set_size(g_inputIndicators[i], 28, 28);
    lv_obj_set_pos(g_inputIndicators[i], 150, y);
    lv_obj_set_style_radius(g_inputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_inputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_inputIndicators[i], lv_color_hex(0xA0A0A0), LV_PART_MAIN);
    lv_obj_clear_flag(g_inputIndicators[i], LV_OBJ_FLAG_SCROLLABLE);

    g_inputValueLabels[i] = lv_label_create(g_manualScreen);
    lv_label_set_text(g_inputValueLabels[i], "OFF");
    lv_obj_set_style_text_color(g_inputValueLabels[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_inputValueLabels[i], LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(g_inputValueLabels[i], 190, y + 7);
  }

  // ------------------------------------------------------------
  // Assignable inputs
  // ------------------------------------------------------------
  lv_obj_t * assignableInputHeader = lv_label_create(g_manualScreen);
  lv_label_set_text(assignableInputHeader, "Assignable Inputs");
  lv_obj_set_style_text_color(assignableInputHeader, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(assignableInputHeader, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(assignableInputHeader, 40, 220);

  for (int i = 0; i < AssignableInSize; i++) {
    int y = 250 + (i * 38);

    lv_obj_t * name = lv_label_create(g_manualScreen);
    lv_label_set_text(name, AssignableInputLabels[i]);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(name, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(name, 40, y + 7);

    g_assignableInputIndicators[i] = lv_obj_create(g_manualScreen);
    lv_obj_set_size(g_assignableInputIndicators[i], 28, 28);
    lv_obj_set_pos(g_assignableInputIndicators[i], 150, y);
    lv_obj_set_style_radius(g_assignableInputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_assignableInputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_assignableInputIndicators[i], lv_color_hex(0xA0A0A0), LV_PART_MAIN);
    lv_obj_clear_flag(g_assignableInputIndicators[i], LV_OBJ_FLAG_SCROLLABLE);

    g_assignableInputValueLabels[i] = lv_label_create(g_manualScreen);
    lv_label_set_text(g_assignableInputValueLabels[i], "OFF");
    lv_obj_set_style_text_color(g_assignableInputValueLabels[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_assignableInputValueLabels[i], LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(g_assignableInputValueLabels[i], 190, y + 7);
  }

  // ------------------------------------------------------------
  // Physical output indicators
  // ------------------------------------------------------------
  lv_obj_t * outputHeader = lv_label_create(g_manualScreen);
  lv_label_set_text(outputHeader, "Outputs");
  lv_obj_set_style_text_color(outputHeader, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(outputHeader, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(outputHeader, 330, 45);

  for (int i = 0; i < PhysicalOutSize; i++) {
    int col = i / 4;
    int row = i % 4;

    int x = 330 + (col * 220);
    int y = 75 + (row * 42);

    lv_obj_t * name = lv_label_create(g_manualScreen);
    lv_label_set_text(name, OUTPUT_NAMES[i]);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(name, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(name, x, y + 7);

    g_outputIndicators[i] = lv_obj_create(g_manualScreen);
    lv_obj_set_size(g_outputIndicators[i], 28, 28);
    lv_obj_set_pos(g_outputIndicators[i], x + 120, y);
    lv_obj_set_style_radius(g_outputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_outputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_outputIndicators[i], lv_color_hex(0xA0A0A0), LV_PART_MAIN);
    lv_obj_clear_flag(g_outputIndicators[i], LV_OBJ_FLAG_SCROLLABLE);

    g_outputValueLabels[i] = lv_label_create(g_manualScreen);
    lv_label_set_text(g_outputValueLabels[i], "OFF");
    lv_obj_set_style_text_color(g_outputValueLabels[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_outputValueLabels[i], LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(g_outputValueLabels[i], x + 158, y + 7);
  }

  // ------------------------------------------------------------
  // Manual control buttons
  // ------------------------------------------------------------
  lv_obj_t * buttonHeader = lv_label_create(g_manualScreen);
  lv_label_set_text(buttonHeader, "Manual Controls");
  lv_obj_set_style_text_color(buttonHeader, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(buttonHeader, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(buttonHeader, 300, 250);

  for (int i = 0; i < MANUAL_BUTTON_COUNT; i++) {
    int col = i % 4;
    int row = i / 4;

    int x = 300 + (col * 120);
    int y = 280 + (row * 58);

    lv_obj_t * toggleBtn = lv_btn_create(g_manualScreen);
    lv_obj_set_size(toggleBtn, 110, 44);
    lv_obj_set_pos(toggleBtn, x, y);
    lv_obj_add_event_cb(toggleBtn, OutputToggle_Event, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

    lv_obj_t * toggleLabel = lv_label_create(toggleBtn);
    lv_label_set_text(toggleLabel, MANUAL_BUTTON_NAMES[i]);
    lv_obj_set_style_text_font(toggleLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_center(toggleLabel);
  }

  // ------------------------------------------------------------
  // Navigation buttons
  // ------------------------------------------------------------
  lv_obj_t * hitachiBtn = lv_btn_create(g_manualScreen);
  lv_obj_set_size(hitachiBtn, 120, 44);
  lv_obj_set_pos(hitachiBtn, 535, 415);
  lv_obj_add_event_cb(hitachiBtn, HitachiButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * hitachiLabel = lv_label_create(hitachiBtn);
  lv_label_set_text(hitachiLabel, "Hitachi");
  lv_obj_set_style_text_font(hitachiLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(hitachiLabel);

  lv_obj_t * backBtn = lv_btn_create(g_manualScreen);
  lv_obj_set_size(backBtn, 120, 44);
  lv_obj_set_pos(backBtn, 665, 415);
  lv_obj_add_event_cb(backBtn, BackButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "Back");
  lv_obj_set_style_text_font(backLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(backLabel);

  g_manualScreenBuilt = true;
}

void GigaDisplay_ShowManualScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  if (!g_manualScreenBuilt) {
    GigaDisplay_CreateManualScreen();
  }

  GigaDisplay_UpdateManualIndicators();
  lv_scr_load(g_manualScreen);
}

void GigaDisplay_UpdateManualIndicators()
{
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
  g_autoScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(g_autoScreen, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_clear_flag(g_autoScreen, LV_OBJ_FLAG_SCROLLABLE);

  // Title
  lv_obj_t * title = lv_label_create(g_autoScreen);
  lv_label_set_text(title, "Auto");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  // Status
  g_autoStatusLabel = lv_label_create(g_autoScreen);
  lv_label_set_text(g_autoStatusLabel, "Status: Stopped");
  lv_obj_set_style_text_color(g_autoStatusLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_autoStatusLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(g_autoStatusLabel, 40, 55);

  g_autoRemainingLabel = lv_label_create(g_autoScreen);
  lv_label_set_text(g_autoRemainingLabel, "Remaining: 0:00");
  lv_obj_set_style_text_color(g_autoRemainingLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_autoRemainingLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(g_autoRemainingLabel, 300, 55);

  g_autoCurrentOutputLabel = lv_label_create(g_autoScreen);
  lv_label_set_text(g_autoCurrentOutputLabel, "Hitachi Output: 0%");
  lv_obj_set_style_text_color(g_autoCurrentOutputLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_autoCurrentOutputLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(g_autoCurrentOutputLabel, 560, 55);

  // ------------------------------------------------------------
  // Auto control buttons
  // ------------------------------------------------------------

  const int buttonY = 100;
  const int buttonW = 150;
  const int buttonH = 60;

  lv_obj_t * startBtn = lv_btn_create(g_autoScreen);
  lv_obj_set_size(startBtn, buttonW, buttonH);
  lv_obj_set_pos(startBtn, 40, buttonY);
  lv_obj_add_event_cb(startBtn, AutoStartButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * startLabel = lv_label_create(startBtn);
  lv_label_set_text(startLabel, "Start");
  lv_obj_set_style_text_font(startLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(startLabel);

  lv_obj_t * stopBtn = lv_btn_create(g_autoScreen);
  lv_obj_set_size(stopBtn, buttonW, buttonH);
  lv_obj_set_pos(stopBtn, 220, buttonY);
  lv_obj_add_event_cb(stopBtn, AutoStopButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * stopLabel = lv_label_create(stopBtn);
  lv_label_set_text(stopLabel, "Stop");
  lv_obj_set_style_text_font(stopLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(stopLabel);

  lv_obj_t * pauseBtn = lv_btn_create(g_autoScreen);
  lv_obj_set_size(pauseBtn, buttonW, buttonH);
  lv_obj_set_pos(pauseBtn, 400, buttonY);
  lv_obj_add_event_cb(pauseBtn, AutoPauseButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * pauseLabel = lv_label_create(pauseBtn);
  lv_label_set_text(pauseLabel, "Pause");
  lv_obj_set_style_text_font(pauseLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(pauseLabel);

  lv_obj_t * hitachiBtn = lv_btn_create(g_autoScreen);
  lv_obj_set_size(hitachiBtn, buttonW, buttonH);
  lv_obj_set_pos(hitachiBtn, 580, buttonY);
  lv_obj_add_event_cb(hitachiBtn, HitachiButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * hitachiLabel = lv_label_create(hitachiBtn);
  lv_label_set_text(hitachiLabel, "Hitachi");
  lv_obj_set_style_text_font(hitachiLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(hitachiLabel);

  // ------------------------------------------------------------
  // Input indicators
  // ------------------------------------------------------------

  lv_obj_t * inputHeader = lv_label_create(g_autoScreen);
  lv_label_set_text(inputHeader, "Inputs");
  lv_obj_set_style_text_color(inputHeader, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(inputHeader, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(inputHeader, 40, 195);

  for (int i = 0; i < InSize; i++) {
    int y = 220 + (i * 42);

    lv_obj_t * name = lv_label_create(g_autoScreen);
    lv_label_set_text(name, INPUT_NAMES[i]);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(name, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(name, 40, y + 8);

    g_autoInputIndicators[i] = lv_obj_create(g_autoScreen);
    lv_obj_set_size(g_autoInputIndicators[i], 30, 30);
    lv_obj_set_pos(g_autoInputIndicators[i], 150, y);
    lv_obj_set_style_radius(g_autoInputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_autoInputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_autoInputIndicators[i], lv_color_hex(0xA0A0A0), LV_PART_MAIN);
    lv_obj_clear_flag(g_autoInputIndicators[i], LV_OBJ_FLAG_SCROLLABLE);

    g_autoInputValueLabels[i] = lv_label_create(g_autoScreen);
    lv_label_set_text(g_autoInputValueLabels[i], "OFF");
    lv_obj_set_style_text_color(g_autoInputValueLabels[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_autoInputValueLabels[i], LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(g_autoInputValueLabels[i], 190, y + 8);
  }

  lv_obj_t * assignableInputHeader = lv_label_create(g_autoScreen);
  lv_label_set_text(assignableInputHeader, "Assignable");
  lv_obj_set_style_text_color(assignableInputHeader, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(assignableInputHeader, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(assignableInputHeader, 40, 350);

  for (int i = 0; i < AssignableInSize; i++) {
    int y = 375 + (i * 32);

    lv_obj_t * name = lv_label_create(g_autoScreen);
    lv_label_set_text(name, AssignableInputLabels[i]);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(name, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(name, 40, y + 6);
    
    g_autoAssignableInputIndicators[i] = lv_obj_create(g_autoScreen);
    lv_obj_set_size(g_autoAssignableInputIndicators[i], 24, 24);
    lv_obj_set_pos(g_autoAssignableInputIndicators[i], 150, y);
    lv_obj_set_style_radius(g_autoAssignableInputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_autoAssignableInputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_autoAssignableInputIndicators[i], lv_color_hex(0xA0A0A0), LV_PART_MAIN);
    lv_obj_clear_flag(g_autoAssignableInputIndicators[i], LV_OBJ_FLAG_SCROLLABLE);
    
    g_autoAssignableInputValueLabels[i] = lv_label_create(g_autoScreen);
    lv_label_set_text(g_autoAssignableInputValueLabels[i], "OFF");
    lv_obj_set_style_text_color(g_autoAssignableInputValueLabels[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_autoAssignableInputValueLabels[i], LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(g_autoAssignableInputValueLabels[i], 180, y + 6);
  }

  // ------------------------------------------------------------
  // Output indicators
  // ------------------------------------------------------------

  lv_obj_t * outputHeader = lv_label_create(g_autoScreen);
  lv_label_set_text(outputHeader, "Outputs");
  lv_obj_set_style_text_color(outputHeader, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(outputHeader, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(outputHeader, 330, 195);

  for (int i = 0; i < OutSize; i++) {
    int col = i / 3;
    int row = i % 3;

    int x = 300 + (col * 165);
    int y = 220 + (row * 50);

    lv_obj_t * name = lv_label_create(g_autoScreen);
    lv_label_set_text(name, OUTPUT_NAMES[i]);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(name, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(name, x, y + 8);

    g_autoOutputIndicators[i] = lv_obj_create(g_autoScreen);
    lv_obj_set_size(g_autoOutputIndicators[i], 28, 28);
    lv_obj_set_pos(g_autoOutputIndicators[i], x + 90, y);
    lv_obj_set_style_radius(g_autoOutputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_autoOutputIndicators[i], 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_autoOutputIndicators[i], lv_color_hex(0xA0A0A0), LV_PART_MAIN);
    lv_obj_clear_flag(g_autoOutputIndicators[i], LV_OBJ_FLAG_SCROLLABLE);

    g_autoOutputValueLabels[i] = lv_label_create(g_autoScreen);
    lv_label_set_text(g_autoOutputValueLabels[i], "OFF");
    lv_obj_set_style_text_color(g_autoOutputValueLabels[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_autoOutputValueLabels[i], LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(g_autoOutputValueLabels[i], x + 122, y + 8);
  }

  // Settings Button
  lv_obj_t * settingsBtn = lv_btn_create(g_autoScreen);
  lv_obj_set_size(settingsBtn, 120, 44);
  lv_obj_set_pos(settingsBtn, 535, 415);
  lv_obj_add_event_cb(settingsBtn, AutoSettingsButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * settingsLabel = lv_label_create(settingsBtn);
  lv_label_set_text(settingsLabel, "Settings");
  lv_obj_set_style_text_font(settingsLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(settingsLabel);

  // Back button
  lv_obj_t * backBtn = lv_btn_create(g_autoScreen);
  lv_obj_set_size(backBtn, 120, 44);
  lv_obj_set_pos(backBtn, 665, 415);
  lv_obj_add_event_cb(backBtn, AutoBackButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "Back");
  lv_obj_set_style_text_font(backLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(backLabel);

  g_autoScreenBuilt = true;
}

void GigaDisplay_ShowAutoScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  if (!g_autoScreenBuilt) {
    GigaDisplay_CreateAutoScreen();
  }

  GigaDisplay_UpdateAutoScreen();
  lv_scr_load(g_autoScreen);
}

static void GigaDisplay_UpdateAutoScreen()
{
  if (!g_autoScreenBuilt) {
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
  lv_obj_t * label = lv_label_create(parent);
  lv_label_set_text(label, labelText);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(label, 35, y);

  *slider = lv_slider_create(parent);
  lv_slider_set_range(*slider, minVal, maxVal);
  lv_obj_set_size(*slider, 480, 20);
  lv_obj_set_pos(*slider, 190, y + 5);
  lv_obj_add_event_cb(*slider, AutoSettingsSlider_Event, LV_EVENT_VALUE_CHANGED, NULL);

  *valueLabel = lv_label_create(parent);
  lv_label_set_text(*valueLabel, "");
  lv_obj_set_style_text_color(*valueLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(*valueLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(*valueLabel, 690, y);
}

static void GigaDisplay_CreateAutoSettingsScreen()
{
  g_autoSettingsScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(g_autoSettingsScreen, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_scroll_dir(g_autoSettingsScreen, LV_DIR_VER);

  lv_obj_t * title = lv_label_create(g_autoSettingsScreen);
  lv_label_set_text(title, "Auto Settings");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  lv_obj_t * backBtn = lv_btn_create(g_autoSettingsScreen);
  lv_obj_set_size(backBtn, 120, 45);
  lv_obj_set_pos(backBtn, 650, 20);
  lv_obj_add_event_cb(backBtn, AutoSettingsBackButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "Back");
  lv_obj_center(backLabel);

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

  lv_obj_t * modeHeader = lv_label_create(g_autoSettingsScreen);
  lv_label_set_text(modeHeader, "Output Modes");
  lv_obj_set_style_text_color(modeHeader, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(modeHeader, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(modeHeader, 35, 395);

  int y = 440;

  for (int outputIndex = 0; outputIndex < OutSize; outputIndex++) {
    if (outputIndex == OUT_LOCK_1 || 
      outputIndex == OUT_LOCK_2 || 
      outputIndex == OUT_DIMMER_ENABLE) {
        continue;
      }

    lv_obj_t * nameLabel = lv_label_create(g_autoSettingsScreen);
    lv_label_set_text(nameLabel, OUTPUT_NAMES[outputIndex]);
    lv_obj_set_style_text_color(nameLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(nameLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(nameLabel, 50, y + 12);

    lv_obj_t * modeBtn = lv_btn_create(g_autoSettingsScreen);
    lv_obj_set_size(modeBtn, 260, 44);
    lv_obj_set_pos(modeBtn, 250, y);
    lv_obj_add_event_cb(modeBtn, AutoOutputModeButton_Event, LV_EVENT_CLICKED, (void *)(uintptr_t)outputIndex);

    g_autoOutputModeLabels[outputIndex] = lv_label_create(modeBtn);
    lv_label_set_text(g_autoOutputModeLabels[outputIndex], "Off");
    lv_obj_center(g_autoOutputModeLabels[outputIndex]);

    lv_obj_t * inputBtn = lv_btn_create(g_autoSettingsScreen);
    lv_obj_set_size(inputBtn, 140, 44);
    lv_obj_set_pos(inputBtn, 530, y);
    lv_obj_add_event_cb(inputBtn, AutoOutputInputButton_Event, LV_EVENT_CLICKED, (void *)(uintptr_t)outputIndex);

    g_autoOutputInputLabels[outputIndex] = lv_label_create(inputBtn);
    lv_label_set_text(g_autoOutputInputLabels[outputIndex], "Input 1");
    lv_obj_center(g_autoOutputInputLabels[outputIndex]);

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

  if (!g_autoSettingsScreenBuilt) {
    GigaDisplay_CreateAutoSettingsScreen();
  }

  GigaDisplay_UpdateAutoSettingsScreen();
  lv_scr_load(g_autoSettingsScreen);
}

static void GigaDisplay_UpdateAutoSettingsScreen()
{
  if (!g_autoSettingsScreenBuilt) {
    return;
  }

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

    if (g_autoOutputModeLabels[outputIndex] != NULL) {
      lv_label_set_text(g_autoOutputModeLabels[outputIndex], AUTO_OUT_MODE_NAMES[mode]);
    }

    int inputIndex = State_GetAutoOutputInputIndex(outputIndex);
    inputIndex = constrain(inputIndex, 0, AssignableInSize - 1);

    if (g_autoOutputInputLabels[outputIndex] != NULL) {
      lv_label_set_text(g_autoOutputInputLabels[outputIndex], AssignableInputLabels[inputIndex]);
    }
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
  g_hitachiScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(g_hitachiScreen, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_clear_flag(g_hitachiScreen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * title = lv_label_create(g_hitachiScreen);
  lv_label_set_text(title, "Hitachi");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  g_hitachiCurrentOutputLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(g_hitachiCurrentOutputLabel, "Output: 0%");
  lv_obj_set_style_text_color(g_hitachiCurrentOutputLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_hitachiCurrentOutputLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(g_hitachiCurrentOutputLabel, LV_ALIGN_TOP_RIGHT, -30, 14);

  lv_obj_t * onBtn = lv_btn_create(g_hitachiScreen);
  lv_obj_set_size(onBtn, 110, 42);
  lv_obj_set_pos(onBtn, 30, 55);
  lv_obj_add_event_cb(onBtn, HitachiEditOnButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * onLabel = lv_label_create(onBtn);
  lv_label_set_text(onLabel, "Edit ON");
  lv_obj_set_style_text_font(onLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(onLabel);

  lv_obj_t * offBtn = lv_btn_create(g_hitachiScreen);
  lv_obj_set_size(offBtn, 110, 42);
  lv_obj_set_pos(offBtn, 155, 55);
  lv_obj_add_event_cb(offBtn, HitachiEditOffButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * offLabel = lv_label_create(offBtn);
  lv_label_set_text(offLabel, "Edit OFF");
  lv_obj_set_style_text_font(offLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(offLabel);

  g_hitachiEditorLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(g_hitachiEditorLabel, "Editing: ON settings");
  lv_obj_set_style_text_color(g_hitachiEditorLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_hitachiEditorLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(g_hitachiEditorLabel, 290, 67);

  g_hitachiModeLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(g_hitachiModeLabel, "Mode: Value");
  lv_obj_set_style_text_color(g_hitachiModeLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_hitachiModeLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(g_hitachiModeLabel, 520, 67);

  for (int i = 0; i < HITACHI_MODE_COUNT; i++) {
    lv_obj_t * modeBtn = lv_btn_create(g_hitachiScreen);
    lv_obj_set_size(modeBtn, 102, 42);
    lv_obj_set_pos(modeBtn, 30 + (i * 108), 112);
    lv_obj_add_event_cb(modeBtn, HitachiModeButton_Event, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

    lv_obj_t * modeLabel = lv_label_create(modeBtn);
    lv_label_set_text(modeLabel, HITACHI_MODE_NAMES[i]);
    lv_obj_set_style_text_font(modeLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_center(modeLabel);
  }

  const int labelX = 35;
  const int sliderX = 160;
  const int valueX = 690;
  const int sliderW = 500;

  lv_obj_t * setPointLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(setPointLabel, "Set Point");
  lv_obj_set_style_text_color(setPointLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(setPointLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(setPointLabel, labelX, 185);

  g_hitachiSetPointSlider = lv_slider_create(g_hitachiScreen);
  lv_slider_set_range(g_hitachiSetPointSlider, State_GetHitachiMinRelayValue(), 100);
  lv_obj_set_size(g_hitachiSetPointSlider, sliderW, 20);
  lv_obj_set_pos(g_hitachiSetPointSlider, sliderX, 190);
  lv_obj_add_event_cb(g_hitachiSetPointSlider, HitachiSlider_Event, LV_EVENT_VALUE_CHANGED, NULL);

  g_hitachiSetPointValueLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(g_hitachiSetPointValueLabel, "25%");
  lv_obj_set_style_text_color(g_hitachiSetPointValueLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_hitachiSetPointValueLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(g_hitachiSetPointValueLabel, valueX, 185);

  lv_obj_t * maxLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(maxLabel, "Max Value");
  lv_obj_set_style_text_color(maxLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(maxLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(maxLabel, labelX, 245);

  g_hitachiMaxSlider = lv_slider_create(g_hitachiScreen);
  lv_slider_set_range(g_hitachiMaxSlider, State_GetHitachiMinRelayValue(), 100);
  lv_obj_set_size(g_hitachiMaxSlider, sliderW, 20);
  lv_obj_set_pos(g_hitachiMaxSlider, sliderX, 250);
  lv_obj_add_event_cb(g_hitachiMaxSlider, HitachiSlider_Event, LV_EVENT_VALUE_CHANGED, NULL);

  g_hitachiMaxValueLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(g_hitachiMaxValueLabel, "100%");
  lv_obj_set_style_text_color(g_hitachiMaxValueLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_hitachiMaxValueLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(g_hitachiMaxValueLabel, valueX, 245);

  lv_obj_t * minLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(minLabel, "Min Value");
  lv_obj_set_style_text_color(minLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(minLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(minLabel, labelX, 305);

  g_hitachiMinSlider = lv_slider_create(g_hitachiScreen);
  lv_slider_set_range(g_hitachiMinSlider, State_GetHitachiMinRelayValue(), 100);
  lv_obj_set_size(g_hitachiMinSlider, sliderW, 20);
  lv_obj_set_pos(g_hitachiMinSlider, sliderX, 310);
  lv_obj_add_event_cb(g_hitachiMinSlider, HitachiSlider_Event, LV_EVENT_VALUE_CHANGED, NULL);

  g_hitachiMinValueLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(g_hitachiMinValueLabel, "25%");
  lv_obj_set_style_text_color(g_hitachiMinValueLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_hitachiMinValueLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(g_hitachiMinValueLabel, valueX, 305);

  lv_obj_t * periodLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(periodLabel, "Period");
  lv_obj_set_style_text_color(periodLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(periodLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(periodLabel, labelX, 365);

  g_hitachiPeriodSlider = lv_slider_create(g_hitachiScreen);
  lv_slider_set_range(g_hitachiPeriodSlider, 0, 100);
  lv_obj_set_size(g_hitachiPeriodSlider, sliderW, 20);
  lv_obj_set_pos(g_hitachiPeriodSlider, sliderX, 370);
  lv_obj_add_event_cb(g_hitachiPeriodSlider, HitachiSlider_Event, LV_EVENT_VALUE_CHANGED, NULL);

  g_hitachiPeriodValueLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(g_hitachiPeriodValueLabel, "0.1 s");
  lv_obj_set_style_text_color(g_hitachiPeriodValueLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_hitachiPeriodValueLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_set_pos(g_hitachiPeriodValueLabel, valueX, 365);

  lv_obj_t * periodModeBtn = lv_btn_create(g_hitachiScreen);
  lv_obj_set_size(periodModeBtn, 170, 42);
  lv_obj_set_pos(periodModeBtn, 35, 415);
  lv_obj_add_event_cb(periodModeBtn, HitachiPeriodModeButton_Event, LV_EVENT_CLICKED, NULL);

  g_hitachiPeriodModeLabel = lv_label_create(periodModeBtn);
  lv_label_set_text(g_hitachiPeriodModeLabel, "Period: Precise");
  lv_obj_set_style_text_font(g_hitachiPeriodModeLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(g_hitachiPeriodModeLabel);

  g_hitachiDimmerEnableLabel = lv_label_create(g_hitachiScreen);
  lv_label_set_text(g_hitachiDimmerEnableLabel, "Dimmer: OFF");
  lv_obj_set_style_text_color(g_hitachiDimmerEnableLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_hitachiDimmerEnableLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(g_hitachiDimmerEnableLabel, LV_ALIGN_BOTTOM_MID, -115, -38);

  lv_obj_t * relayMinBtn = lv_btn_create(g_hitachiScreen);
  lv_obj_set_size(relayMinBtn, 130, 50);
  lv_obj_align(relayMinBtn, LV_ALIGN_BOTTOM_RIGHT, -175, -25);
  lv_obj_add_event_cb(relayMinBtn, HitachiRelayMinButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * relayMinLabel = lv_label_create(relayMinBtn);
  lv_label_set_text(relayMinLabel, "Relay Min");
  lv_obj_set_style_text_font(relayMinLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(relayMinLabel);

  lv_obj_t * backBtn = lv_btn_create(g_hitachiScreen);
  lv_obj_set_size(backBtn, 130, 50);
  lv_obj_align(backBtn, LV_ALIGN_BOTTOM_RIGHT, -30, -25);
  lv_obj_add_event_cb(backBtn, HitachiBackButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "Back");
  lv_obj_set_style_text_font(backLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_center(backLabel);

  g_hitachiScreenBuilt = true;
}

void GigaDisplay_ShowHitachiScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  g_previousScreenBeforeHitachi = lv_scr_act();

  if (!g_hitachiScreenBuilt) {
    GigaDisplay_CreateHitachiScreen();
  }

  GigaDisplay_UpdateHitachiScreen();
  lv_scr_load(g_hitachiScreen);
}

static void GigaDisplay_UpdateHitachiScreen()
{
  if (!g_hitachiScreenBuilt || g_hitachiUiRefreshing) {
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

  Command_SetHitachiSetPoint(g_hitachiEditingOnSettings, setPoint);
  Command_SetHitachiMaxValue(g_hitachiEditingOnSettings, maxValue);
  Command_SetHitachiMinValue(g_hitachiEditingOnSettings, minValue);

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

// ------------------------------------------------------------
// Hitachi Min Relay screen
// ------------------------------------------------------------
static void GigaDisplay_CreateHitachiRelayMinScreen()
{
  g_hitachiRelayMinScreen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(g_hitachiRelayMinScreen, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_clear_flag(g_hitachiRelayMinScreen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t * title = lv_label_create(g_hitachiRelayMinScreen);
  lv_label_set_text(title, "Dimmer Relay Minimum");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

  lv_obj_t * note = lv_label_create(g_hitachiRelayMinScreen);
  lv_label_set_text(note, "Relay turns ON when Hitachi output is at or above this value.");
  lv_obj_set_style_text_color(note, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(note, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 80);

  g_hitachiRelayMinValueLabel = lv_label_create(g_hitachiRelayMinScreen);
  lv_label_set_text(g_hitachiRelayMinValueLabel, "Min: 25%");
  lv_obj_set_style_text_color(g_hitachiRelayMinValueLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_hitachiRelayMinValueLabel, LV_FONT_DEFAULT, LV_PART_MAIN);
  lv_obj_align(g_hitachiRelayMinValueLabel, LV_ALIGN_TOP_MID, 0, 145);

  const int buttonW = 120;
  const int buttonH = 60;
  const int y1 = 210;

  lv_obj_t * minus5Btn = lv_btn_create(g_hitachiRelayMinScreen);
  lv_obj_set_size(minus5Btn, buttonW, buttonH);
  lv_obj_set_pos(minus5Btn, 180, y1);
  lv_obj_add_event_cb(minus5Btn, HitachiRelayMinMinus5Button_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * minus5Label = lv_label_create(minus5Btn);
  lv_label_set_text(minus5Label, "-5");
  lv_obj_center(minus5Label);

  lv_obj_t * minusBtn = lv_btn_create(g_hitachiRelayMinScreen);
  lv_obj_set_size(minusBtn, buttonW, buttonH);
  lv_obj_set_pos(minusBtn, 320, y1);
  lv_obj_add_event_cb(minusBtn, HitachiRelayMinMinusButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * minusLabel = lv_label_create(minusBtn);
  lv_label_set_text(minusLabel, "-1");
  lv_obj_center(minusLabel);

  lv_obj_t * plusBtn = lv_btn_create(g_hitachiRelayMinScreen);
  lv_obj_set_size(plusBtn, buttonW, buttonH);
  lv_obj_set_pos(plusBtn, 460, y1);
  lv_obj_add_event_cb(plusBtn, HitachiRelayMinPlusButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * plusLabel = lv_label_create(plusBtn);
  lv_label_set_text(plusLabel, "+1");
  lv_obj_center(plusLabel);

  lv_obj_t * plus5Btn = lv_btn_create(g_hitachiRelayMinScreen);
  lv_obj_set_size(plus5Btn, buttonW, buttonH);
  lv_obj_set_pos(plus5Btn, 600, y1);
  lv_obj_add_event_cb(plus5Btn, HitachiRelayMinPlus5Button_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * plus5Label = lv_label_create(plus5Btn);
  lv_label_set_text(plus5Label, "+5");
  lv_obj_center(plus5Label);

  lv_obj_t * backBtn = lv_btn_create(g_hitachiRelayMinScreen);
  lv_obj_set_size(backBtn, 130, 50);
  lv_obj_align(backBtn, LV_ALIGN_BOTTOM_RIGHT, -30, -25);
  lv_obj_add_event_cb(backBtn, HitachiRelayMinBackButton_Event, LV_EVENT_CLICKED, NULL);

  lv_obj_t * backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "Back");
  lv_obj_center(backLabel);

  g_hitachiRelayMinScreenBuilt = true;
}

void GigaDisplay_ShowHitachiRelayMinScreen()
{
  if (!g_displayInitialized) {
    GigaDisplay_Setup();
    return;
  }

  g_previousScreenBeforeRelayMin = lv_scr_act();

  if (!g_hitachiRelayMinScreenBuilt) {
    GigaDisplay_CreateHitachiRelayMinScreen();
  }

  GigaDisplay_UpdateHitachiRelayMinScreen();
  lv_scr_load(g_hitachiRelayMinScreen);
}

static void GigaDisplay_UpdateHitachiRelayMinScreen()
{
  if (!g_hitachiRelayMinScreenBuilt) {
    return;
  }

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "Min: %d%%", State_GetHitachiMinRelayValue());
  lv_label_set_text(g_hitachiRelayMinValueLabel, buffer);
}

// ------------------------------------------------------------
// Button event handlers
// ------------------------------------------------------------

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
  GigaDisplay_UpdateAutoScreen();
}

static void AutoStopButton_Event(lv_event_t * e)
{
  Command_RequestAutoStop();
  GigaDisplay_UpdateAutoScreen();
}

static void AutoPauseButton_Event(lv_event_t * e)
{
  Command_RequestAutoPause();
  GigaDisplay_UpdateAutoScreen();
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
  GigaDisplay_UpdateManualIndicators();
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
  lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);

  if (slider == g_autoRunDurationSlider) {
    Command_SetAutoRunDurationMinutes(lv_slider_get_value(slider));
  }
  else if (slider == g_autoPauseDurationSlider) {
    Command_SetAutoPauseDurationSeconds(lv_slider_get_value(slider));
  }
  else if (slider == g_autoPenaltyDurationSlider) {
    Command_SetAutoPenaltyDurationSeconds(lv_slider_get_value(slider));
  }
  else if (slider == g_autoOnTimeSlider) {
    Command_SetAutoIoOnTimeMs(AutoSettings_MsSliderToValue(lv_slider_get_value(slider)));
  }
  else if (slider == g_autoOffTimeSlider) {
    Command_SetAutoIoOffTimeMs(AutoSettings_MsSliderToValue(lv_slider_get_value(slider)));
  }

  GigaDisplay_UpdateAutoSettingsScreen();
}

static void AutoOutputModeButton_Event(lv_event_t * e)
{
  int outputIndex = (int)(uintptr_t)lv_event_get_user_data(e);

  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  byte mode = State_GetAutoOutputMode(outputIndex);
  mode++;

  if (mode >= AUTO_OUT_MODE_COUNT) {
    mode = 0;
  }

  Command_SetAutoOutputMode(outputIndex, mode);

  GigaDisplay_UpdateAutoSettingsScreen();
}

static void AutoOutputInputButton_Event(lv_event_t * e)
{
  int outputIndex = (int)(uintptr_t)lv_event_get_user_data(e);

  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  Command_CycleAutoOutputInputIndex(outputIndex);

  GigaDisplay_UpdateAutoSettingsScreen();
}

static void HitachiButton_Event(lv_event_t * e)
{
  GigaDisplay_ShowHitachiScreen();
}

static void HitachiBackButton_Event(lv_event_t * e)
{
  lv_obj_t * previousScreen = g_previousScreenBeforeHitachi;

  if (previousScreen != NULL) {
    lv_scr_load(previousScreen);
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

  Command_SetHitachiMode(g_hitachiEditingOnSettings, HITACHI_MODE_VALUES[modeIdx]);

  GigaDisplay_UpdateHitachiScreen();
}

static void HitachiSlider_Event(lv_event_t * e)
{
  if (g_hitachiUiRefreshing) {
    return;
  }

  lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);

  if (slider == g_hitachiSetPointSlider) {
    Command_SetHitachiSetPoint(
      g_hitachiEditingOnSettings,
      constrain(lv_slider_get_value(slider), State_GetHitachiMinRelayValue(), 100)
    );
  }
  else if (slider == g_hitachiMaxSlider) {
    Command_SetHitachiMaxValue(
      g_hitachiEditingOnSettings,
      constrain(lv_slider_get_value(slider), State_GetHitachiMinRelayValue(), 100)
    );
  }
  else if (slider == g_hitachiMinSlider) {
    Command_SetHitachiMinValue(
      g_hitachiEditingOnSettings,
      constrain(lv_slider_get_value(slider), State_GetHitachiMinRelayValue(), 100)
    );
  }
  else if (slider == g_hitachiPeriodSlider) {
    int sliderValue = lv_slider_get_value(slider);

    Command_SetHitachiPeriod(
      g_hitachiEditingOnSettings,
      Hitachi_PeriodSliderToMs(sliderValue)
    );
  }

  GigaDisplay_UpdateHitachiScreen();
}

static void HitachiPeriodModeButton_Event(lv_event_t * e)
{
  Command_ToggleHitachiPeriodPrecise(g_hitachiEditingOnSettings);
  GigaDisplay_UpdateHitachiScreen();
}

static void HitachiRelayMinButton_Event(lv_event_t * e)
{
  GigaDisplay_ShowHitachiRelayMinScreen();
}

static void HitachiRelayMinMinusButton_Event(lv_event_t * e)
{
  Command_AdjustHitachiMinRelayValue(-1);
  GigaDisplay_UpdateHitachiRelayMinScreen();
}

static void HitachiRelayMinPlusButton_Event(lv_event_t * e)
{
  Command_AdjustHitachiMinRelayValue(1);
  GigaDisplay_UpdateHitachiRelayMinScreen();
}

static void HitachiRelayMinMinus5Button_Event(lv_event_t * e)
{
  Command_AdjustHitachiMinRelayValue(-5);
  GigaDisplay_UpdateHitachiRelayMinScreen();
}

static void HitachiRelayMinPlus5Button_Event(lv_event_t * e)
{
  Command_AdjustHitachiMinRelayValue(5);
  GigaDisplay_UpdateHitachiRelayMinScreen();
}

static void HitachiRelayMinBackButton_Event(lv_event_t * e)
{
  if (g_previousScreenBeforeRelayMin != NULL) {
    GigaDisplay_UpdateHitachiScreen();
    lv_scr_load(g_previousScreenBeforeRelayMin);
  }
  else {
    GigaDisplay_ShowHitachiScreen();
  }
}

static void SaveSettingsButton_Event(lv_event_t * e)
{
  bool ok = Settings_SaveAll();

  if (ok) {
    SetIdleStatusText("Settings saved");
  }
  else {
    SetIdleStatusError("Save failed");
  }
}

static void LoadSettingsButton_Event(lv_event_t * e)
{
  bool ok = Settings_LoadAll();

  if (ok) {
    Command_NormalizeHitachiSettings();
    State_RefreshControlStatus();
    SetIdleStatusText("Settings loaded");
  }
  else {
    SetIdleStatusError("Load failed");
  }

  GigaDisplay_UpdateManualIndicators();

  if (lv_scr_act() == g_hitachiScreen) {
    GigaDisplay_UpdateHitachiScreen();
  }
}

// ------------------------------------------------------------
//Misc Helper Functions
// ------------------------------------------------------------
static void SetLargeTitleStyle(lv_obj_t * label)
{
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

#if LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(label, &lv_font_montserrat_48, LV_PART_MAIN);
#elif LV_FONT_MONTSERRAT_36
  lv_obj_set_style_text_font(label, &lv_font_montserrat_36, LV_PART_MAIN);
#elif LV_FONT_MONTSERRAT_28
  lv_obj_set_style_text_font(label, &lv_font_montserrat_28, LV_PART_MAIN);
#else
  lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, LV_PART_MAIN);
#endif
}



// ------------------------------------------------------------
// Replacement stubs for old LCD API
// ------------------------------------------------------------

void InitLCD()
{
  GigaDisplay_Setup();
}



void DisplayIO(int DigitalRow, boolean first)
{
  if (g_displayInitialized && lv_scr_act() == g_manualScreen) {
    GigaDisplay_UpdateManualIndicators();
  }
}

