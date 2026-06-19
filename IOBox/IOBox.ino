
/*
 * EROS MK VI (NextGen)
 * Monkey!
 * Copyright 2026
 * 
 * Pin 0 = Bridge
 * Pin 1 = Bridge
 * Pin 2 = Dimmer Zero Cross
 * Pin 3 = Dimmer Output
 * Pin 4 = Unused
 * Pin 5 = Start Button
 * Pin 6 = Stop Button
 * Pin 7 = Pause Button
 * Pin 8 = Assignable Input 1
 * Pin 9 = Assignable Input 2
 * Pin 10 = Assignable Input 3
 

 * ...
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
// Legacy LCD color compatibility
// These are ignored by the new LVGL display stubs, but old code
// still passes them into displayLcdLine().
// ------------------------------------------------------------

#define BLACK       0x0000
#define WHITE       0xFFFF
#define RED         0xF800
#define GREEN       0x07E0
#define BLUE        0x001F
#define YELLOW      0xFFE0
#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define BACKCOLOR   BLACK
#define BACKCOLOR2  BLUE

#define InSize 3

// PhysicalOutSize is the number of real relay outputs in OutRly[].
#define PhysicalOutSize 8

// OutSize is the number of logical outputs.
// This includes the 8 physical outputs plus the virtual Hitachi output.
#define OutSize 9



//#define LEDPin 13

// ------------------------------------------------------------
// Output indexes
// ------------------------------------------------------------
#define OUT_LOCK_1              0
#define OUT_LOCK_2              1
#define OUT_AC                  2
#define OUT_DIMMER_ENABLE       3
#define OUT_DRY_1               4
#define OUT_DRY_2               5
#define OUT_DRY_3               6
#define OUT_DRY_4               7

#define OUT_HITACHI_VIRTUAL     8

//#define DIMMER_USABLE_MIN_PERCENT 15
//#define DIMMER_USABLE_MAX_PERCENT 70

#define BridgeMode 0
#define EROSFlexMode 1

//Dimmer setup functions
#define DIMMER_ZC_PIN    2
#define DIMMER_GATE_PIN  3

#define hitachiOff 0
#define hitachiValue 1
#define hitachiPulse 2
#define hitachiSine 3
#define hitachiSawTooth 4
#define hitachiTriangle 5
#define hitachiRandom 6




#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"
#include <mbed.h>
#include <string.h>

// ------------------------------------------------------------
// Giga Display function prototypes - Pre-defined to fix compile issues.
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

bool State_GetInput(int inputIndex);
bool State_GetAssignableInput(int inputIndex);
bool State_GetAssignedInputForOutput(int outputIndex);
bool State_GetOutput(int outputIndex);
bool State_GetManualOutputRequest(int outputIndex);
void Command_SetManualOutput(int outputIndex, bool state);
void Command_ToggleManualOutput(int outputIndex);
void Command_SetLock(bool state);
void Command_ToggleLock();
bool State_GetDimmerEnabledRequest();
void Command_SetDimmerEnabledRequest(bool enabled);
void Command_ToggleDimmerEnabledRequest();
void Command_SetMode(byte mode);
byte State_GetMode();
int State_GetSettingsLastError();
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
void Command_NormalizeHitachiSettings();
int State_GetHitachiCurrentOutput();
bool State_GetHitachiVirtualRequest();
bool State_GetHitachiVirtualOutput();
void Command_SetHitachiVirtualRequest(bool enabled);
void Command_ToggleHitachiVirtualRequest();
bool State_GetHitachiPeriodPrecise(bool onSettings);
void Command_SetHitachiPeriodPrecise(bool onSettings, bool precise);
void Command_ToggleHitachiPeriodPrecise(bool onSettings);

void State_RefreshControlStatus();
void State_ProcessPendingCommands();

//Settings save load prototypes
bool Settings_SaveAll();
bool Settings_LoadAll();
bool Settings_LoadAllOrDefaults();
void Settings_LoadDefaults();
bool Command_RequestSettingsSave();
bool Command_RequestSettingsLoad();

bool Settings_Begin();
int Settings_GetLastError();

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

//End Function Prototypes

// ------------------------------------------------------------
// ------------------------------------------------------------
// IO Definitions
// ------------------------------------------------------------
// ------------------------------------------------------------

// ------------------------------------------------------------
// Dedicated system input indexes
// ------------------------------------------------------------
#define IN_START  0
#define IN_STOP   1
#define IN_PAUSE  2

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
// Assignable input indexes
// These are used by EROSFlexSettings.OutInIdx[]
// ------------------------------------------------------------

#define ASSIGN_IN_1  0
#define ASSIGN_IN_2  1
#define ASSIGN_IN_3  2
#define AssignableInSize 3

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

const char* AssignableInputLabels[AssignableInSize] = {
  "Input 1",
  "Input 2",
  "Input 3"
};

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

//Include Input jacks + start
boolean InValues[InSize];
//Out 1, out 2, AC relay, PWM Relay
boolean OutValues[OutSize];

byte saveEEPROM = 0;
byte loadEEPROM = 0;
byte pairBT = 0;
byte TimeDisplayModulus = 0;
byte ReadSettings;
boolean InitIODisplay;
//const int NumModes = 3;
const byte ModeMax = 1;
byte initialized = 0;
//Function definitions to allow calling mode functions by reference.  This will
//allow me to avoid writing the same code over and over again.

//Mode management variables
struct ModeStruct{
  byte Current;
  byte Last;
  //byte Max;
} 
Mode;

//Time Declarations
struct TimeVariables
{
  boolean bRunning;
  boolean bPaused;
  boolean bForceClear;
  boolean bRefreshDisplay;
  unsigned int iCurrentTime;
  unsigned int iCurrentTimeAccum;
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

struct SoftwareSwitches
{
  boolean Start;
  boolean Stop;
  boolean Pause;
}
SoftSwitches;


struct manual_mode
{
  byte out[OutSize];
}
Manual;


//Mode Specific Variables
boolean UseTimingFunctions[] = {
  false,true};
boolean DisplayTime[] = {
  false, true};



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

struct EROSFlex_Settings
{
  int OutInIdx[OutSize];
  byte OutMode[OutSize];
  unsigned int OnTime;
  unsigned int OffTime;
  int NextToggleTime[OutSize];
} 
EROSFlexSettings;
 


//Function Prototypes
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

typedef void (* FunctionPointer) (); //Typedef Setup function pointers
FunctionPointer InitFunctions[] = {
  Init_Idle,
  Init_EROSFlex};
 
FunctionPointer ExecFunctions[] = {
  Execute_Idle,
  Execute_EROSFlex};
  
FunctionPointer IdleOps[] = {
  ID_Idle,
  ID_EROSFlex};

void setup() {
  // Open debug serial connection
  Serial.begin(57600);

  //Setup embedded control functions.
  Control_Setup();

  // Initialize the Giga Display UI
  GigaDisplay_Setup();
}


void loop() {
  // Update the Giga Display UI.
  // UI actions enqueue commands through Command_Send().
  GigaDisplay_Task();

  // Process any pending UI/control commands.
  // For now this still runs on the same core, but this simulates the future
  // M4-side command drain point.
  State_ProcessPendingCommands();

  // Handle embedded control functions.
  Control_Task();
}











