/*
  IOBox_M4.ino

  EROS Mk VI M4-side sketch.

  Owns:
    - Physical IO
    - Control logic
    - Hitachi dimmer timing/control
    - Settings storage
    - M4-side command execution
    - Future M4 transport endpoint
*/

#include "EROSShared.h"

#include <mbed.h>
#include <string.h>

// ------------------------------------------------------------
// M4-facing bridge/control prototypes
// ------------------------------------------------------------

bool EROSM4_ReceiveCommandFromTransport(const EROS_Command & command);
void State_RefreshControlStatus();
void State_ProcessPendingCommands();
void EROSTransport_Setup();
void State_CopyControlStatus(EROS_ControlStatus & status);
bool Control_GetAssignedInputForOutput(int outputIndex);

void Command_NormalizeHitachiSettings();
void Command_ForceFixedAutoOutputModes();

// Settings implementation prototypes
bool Settings_SaveAll();
bool Settings_LoadAll();
bool Settings_LoadAllOrDefaults();
void Settings_LoadDefaults();
bool Settings_Begin();
int Settings_GetLastError();

// Control mode function prototypes
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
// M4-side IO pins, state, and control tables
// ------------------------------------------------------------

// Dedicated system input pins
// These are used for Start / Stop / Pause only.
#define PIN_IN_START  5
#define PIN_IN_STOP   6
#define PIN_IN_PAUSE  7

const int DigitalInputs[InSize] = {
  PIN_IN_START,  // IN_START
  PIN_IN_STOP,   // IN_STOP
  PIN_IN_PAUSE   // IN_PAUSE
};

// Assignable input pins
// These are selectable per output.
#define PIN_ASSIGN_IN_1  8
#define PIN_ASSIGN_IN_2  9
#define PIN_ASSIGN_IN_3  10

const int AssignableInputPins[AssignableInSize] = {
  PIN_ASSIGN_IN_1,
  PIN_ASSIGN_IN_2,
  PIN_ASSIGN_IN_3
};

// Physical relay output pins
// These must match OUT_LOCK_1 through OUT_DRY_4.
// OUT_HITACHI_VIRTUAL has no physical relay pin.
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

// Global IO state
boolean InValues[InSize];
boolean OutValues[OutSize];

// Mode management
const byte ModeMax = 1;

struct ModeStruct
{
  byte Current;
  byte Last;
}
Mode;

// Time / auto-run state
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

// Software command switches
struct SoftwareSwitches
{
  boolean Start;
  boolean Stop;
  boolean Pause;
}
SoftSwitches;

// Manual mode state
struct manual_mode
{
  byte out[OutSize];
}
Manual;

// Mode behavior tables
// These are indexed by Mode.Current.
boolean UseTimingFunctions[] = {
  false,
  true
};

// Hitachi settings/state
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

// EROS Flex settings/state
struct EROSFlex_Settings
{
  int OutInIdx[OutSize];
  byte OutMode[OutSize];

  unsigned int OnTime;
  unsigned int OffTime;

  int NextToggleTime[OutSize];
}
EROSFlexSettings;

// Control mode function tables
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
  EROSTransport_Setup();
  Control_Setup();
}

void loop()
{
  State_ProcessPendingCommands();
  Control_Task();
}
