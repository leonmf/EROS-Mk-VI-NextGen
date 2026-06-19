/*
 * EROS MK VI (NextGen)
 * Copyright 2026
 *
 * Pin 0  = Bridge
 * Pin 1  = Bridge
 * Pin 2  = Dimmer Zero Cross
 * Pin 3  = Dimmer Output
 * Pin 4  = Unused
 * Pin 5  = Start Button
 * Pin 6  = Stop Button
 * Pin 7  = Pause Button
 * Pin 8  = Assignable Input 1
 * Pin 9  = Assignable Input 2
 * Pin 10 = Assignable Input 3
 *
 * Pin 39 = Dry Contact Out 4
 * Pin 41 = Dry Contact Out 3
 * Pin 43 = Dry Contact Out 2
 * Pin 45 = Dry Contact Out 1
 * Pin 47 = Lock 2
 * Pin 49 = Lock 1
 * Pin 51 = AC Output
 * Pin 53 = Dimmer Relay
 */

// ------------------------------------------------------------
// Core includes
// ------------------------------------------------------------

#include "EROSShared.h"
#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"
#include <mbed.h>
#include <string.h>

// ------------------------------------------------------------
// Giga Display public functions
// ------------------------------------------------------------

void GigaDisplay_Setup();
void GigaDisplay_Task();
void GigaDisplay_ShowIdleScreen();
void GigaDisplay_ShowManualScreen();
void GigaDisplay_UpdateManualIndicators();
void GigaDisplay_ShowHitachiScreen();

// ------------------------------------------------------------
// EROS State / Command interface prototypes
// ------------------------------------------------------------

// Input / output state
bool State_GetInput(int inputIndex);
bool State_GetAssignableInput(int inputIndex);
bool State_GetAssignedInputForOutput(int outputIndex);
bool State_GetOutput(int outputIndex);
bool State_GetManualOutputRequest(int outputIndex);

// Manual output commands
void Command_SetManualOutput(int outputIndex, bool state);
void Command_ToggleManualOutput(int outputIndex);
void Command_SetLock(bool state);
void Command_ToggleLock();

// Dimmer relay compatibility commands
bool State_GetDimmerEnabledRequest();
void Command_SetDimmerEnabledRequest(bool enabled);
void Command_ToggleDimmerEnabledRequest();

// Mode state / commands
void Command_SetMode(byte mode);
byte State_GetMode();

// Settings state / commands
int State_GetSettingsLastError();
bool Command_RequestSettingsSave();
bool Command_RequestSettingsLoad();

// Status snapshot and command queue processing
bool Command_SubmitToControl(const EROS_Command & command);
void State_RefreshControlStatus();
void State_ProcessPendingCommands();
void State_CopyControlStatus(EROS_ControlStatus & status);
void State_ApplyControlStatus(const EROS_ControlStatus & status);

// Hitachi state / commands
int State_GetHitachiMode(bool onSettings);
void Command_SetHitachiMode(bool onSettings, int mode);

int State_GetHitachiSetPoint(bool onSettings);
void Command_SetHitachiSetPoint(bool onSettings, int value);

int State_GetHitachiMaxValue(bool onSettings);
void Command_SetHitachiMaxValue(bool onSettings, int value);

int State_GetHitachiMinValue(bool onSettings);
void Command_SetHitachiMinValue(bool onSettings, int value);

int State_GetHitachiPeriod(bool onSettings);
void Command_SetHitachiPeriod(bool onSettings, int periodMs);

bool State_GetHitachiPeriodPrecise(bool onSettings);
void Command_SetHitachiPeriodPrecise(bool onSettings, bool precise);
void Command_ToggleHitachiPeriodPrecise(bool onSettings);

void Command_NormalizeHitachiSettings();

int State_GetHitachiCurrentOutput();

int State_GetHitachiMinRelayValue();
void Command_SetHitachiMinRelayValue(int value);
void Command_AdjustHitachiMinRelayValue(int delta);

bool State_GetHitachiVirtualRequest();
bool State_GetHitachiVirtualOutput();
void Command_SetHitachiVirtualRequest(bool enabled);
void Command_ToggleHitachiVirtualRequest();

// Auto mode state / commands
void Command_RequestAutoStart();
void Command_RequestAutoStop();
void Command_RequestAutoPause();

bool State_GetAutoRunning();
bool State_GetAutoPaused();

unsigned int State_GetAutoRemainingTime();
unsigned int State_GetAutoCurrentTime();
unsigned int State_GetAutoRunDuration();

unsigned int State_GetAutoRunDurationSeconds();
unsigned int State_GetAutoRunDurationMinutes();
void Command_SetAutoRunDurationMinutes(unsigned int minutes);

unsigned int State_GetAutoPauseDurationSeconds();
void Command_SetAutoPauseDurationSeconds(unsigned int seconds);

unsigned int State_GetAutoPenaltyDurationSeconds();
void Command_SetAutoPenaltyDurationSeconds(unsigned int seconds);

unsigned int State_GetAutoIoOnTimeMs();
void Command_SetAutoIoOnTimeMs(unsigned int ms);

unsigned int State_GetAutoIoOffTimeMs();
void Command_SetAutoIoOffTimeMs(unsigned int ms);

byte State_GetAutoOutputMode(int outputIndex);
void Command_SetAutoOutputMode(int outputIndex, byte mode);

int State_GetAutoOutputInputIndex(int outputIndex);
void Command_SetAutoOutputInputIndex(int outputIndex, int inputIndex);
void Command_CycleAutoOutputInputIndex(int outputIndex);

void Command_ForceFixedAutoOutputModes();

// ------------------------------------------------------------
// Settings implementation prototypes
// These are implementation-level functions. Display/UI code should use
// Command_RequestSettingsSave(), Command_RequestSettingsLoad(), and
// State_GetSettingsLastError() instead of calling these directly.
// ------------------------------------------------------------

bool Settings_SaveAll();
bool Settings_LoadAll();
bool Settings_LoadAllOrDefaults();
void Settings_LoadDefaults();

bool Settings_Begin();
int Settings_GetLastError();

// ------------------------------------------------------------
// Dedicated system input pins
// These are used for Start / Stop / Pause only.
// ------------------------------------------------------------

#define PIN_IN_START  5
#define PIN_IN_STOP   6
#define PIN_IN_PAUSE  7

const int DigitalInputs[InSize] = {
  PIN_IN_START,  // IN_START
  PIN_IN_STOP,   // IN_STOP
  PIN_IN_PAUSE   // IN_PAUSE
};

// ------------------------------------------------------------
// Assignable input pins
// These are selectable per output.
// ------------------------------------------------------------

#define PIN_ASSIGN_IN_1  8
#define PIN_ASSIGN_IN_2  9
#define PIN_ASSIGN_IN_3  10

const int AssignableInputPins[AssignableInSize] = {
  PIN_ASSIGN_IN_1,
  PIN_ASSIGN_IN_2,
  PIN_ASSIGN_IN_3
};

// ------------------------------------------------------------
// Physical relay output pins
// These must match OUT_LOCK_1 through OUT_DRY_4.
// OUT_HITACHI_VIRTUAL has no physical relay pin.
// ------------------------------------------------------------

const int OutRly[PhysicalOutSize] = {
  49, // OUT_LOCK_1
  47, // OUT_LOCK_2
  51, // OUT_AC
  53, // OUT_DIMMER_ENABLE
  45, // OUT_DRY_1
  43, // OUT_DRY_2
  41, // OUT_DRY_3
  39  // OUT_DRY_4
};

// ------------------------------------------------------------
// Global IO state
// ------------------------------------------------------------

boolean InValues[InSize];
boolean OutValues[OutSize];

// ------------------------------------------------------------
// Mode management
// ------------------------------------------------------------

const byte ModeMax = 1;

struct ModeStruct
{
  byte Current;
  byte Last;
}
Mode;

// ------------------------------------------------------------
// Time / auto-run state
// ------------------------------------------------------------

struct TimeVariables
{
  boolean bRunning;
  boolean bPaused;
  unsigned int iCurrentTime;
  unsigned int iRunDuration;
  unsigned int iPenaltyDuration;
  unsigned int iPauseDuration;
  unsigned int iPauseTime;
  unsigned int iWorkingRunDuration;
  unsigned int iRemainingTime;

  byte StartButton;
  byte PauseButton;
  byte StopButton;

  boolean bFirstScan;
}
TimeVar;

// ------------------------------------------------------------
// Software command switches
// ------------------------------------------------------------

struct SoftwareSwitches
{
  boolean Start;
  boolean Stop;
  boolean Pause;
}
SoftSwitches;

// ------------------------------------------------------------
// Manual mode state
// ------------------------------------------------------------

struct manual_mode
{
  byte out[OutSize];
}
Manual;

// ------------------------------------------------------------
// Mode behavior tables
// These are indexed by Mode.Current.
// ------------------------------------------------------------

boolean UseTimingFunctions[] = {
  false,
  true
};

// ------------------------------------------------------------
// Hitachi settings/state
// ------------------------------------------------------------

struct hitachi_Settings
{
  int modeOff;
  int modeOn;

  int setPointOff;
  int setPointOn;

  int periodOff;
  int periodOn;

  bool periodPreciseOff;
  bool periodPreciseOn;

  int maxValueOff;
  int maxValueOn;

  int minValueOff;
  int minValueOn;

  int minRelayValue;

  int sine[255];
  int currentOutput;
}
hS;

// ------------------------------------------------------------
// EROS Flex settings/state
// ------------------------------------------------------------

struct EROSFlex_Settings
{
  int OutInIdx[OutSize];
  byte OutMode[OutSize];

  unsigned int OnTime;
  unsigned int OffTime;

  int NextToggleTime[OutSize];
}
EROSFlexSettings;

// ------------------------------------------------------------
// Control mode function prototypes
// ------------------------------------------------------------

void Init_Idle(void);
void Init_EROSFlex(void);

void Execute_Idle(void);
void Execute_EROSFlex(void);

void ID_Idle(void);
void ID_EROSFlex(void);

void Control_Setup(void);
void Control_Task(void);
void CheckMode(void);
void RunMode(void);

// ------------------------------------------------------------
// Control mode function tables
// ------------------------------------------------------------

typedef void (* FunctionPointer)();

FunctionPointer InitFunctions[] = {
  Init_Idle,
  Init_EROSFlex
};

FunctionPointer ExecFunctions[] = {
  Execute_Idle,
  Execute_EROSFlex
};

FunctionPointer IdleOps[] = {
  ID_Idle,
  ID_EROSFlex
};

// ------------------------------------------------------------
// Arduino setup / loop
// ------------------------------------------------------------

void setup()
{
  Serial.begin(57600);

  // Set up embedded control functions.
  Control_Setup();

  // Initialize the Giga Display UI.
  GigaDisplay_Setup();
}

void loop()
{
  // Update the Giga Display UI.
  // UI actions enqueue commands through Command_Send().
  GigaDisplay_Task();

  // Process any pending UI/control commands.
  // This is the current single-core command drain point and the future
  // M4-side command drain point.
  State_ProcessPendingCommands();

  // Handle embedded control functions.
  Control_Task();
}