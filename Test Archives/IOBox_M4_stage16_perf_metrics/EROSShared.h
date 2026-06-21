#ifndef EROS_SHARED_H
#define EROS_SHARED_H

#include <Arduino.h>

// ------------------------------------------------------------
// Build mode selection
//
// Default mode is the current single-core simulation. Future M7-only and
// M4-only builds should set exactly one of these flags to 1.
// ------------------------------------------------------------

#if !defined(EROS_BUILD_SINGLE_CORE_SIM) && !defined(EROS_BUILD_M7_CORE) && !defined(EROS_BUILD_M4_CORE)
#define EROS_BUILD_SINGLE_CORE_SIM 0
#define EROS_BUILD_M7_CORE        0
#define EROS_BUILD_M4_CORE        1
#endif

#ifndef EROS_BUILD_SINGLE_CORE_SIM
#define EROS_BUILD_SINGLE_CORE_SIM 0
#endif

#ifndef EROS_BUILD_M7_CORE
#define EROS_BUILD_M7_CORE 0
#endif

#ifndef EROS_BUILD_M4_CORE
#define EROS_BUILD_M4_CORE 0
#endif

#define EROS_BUILD_MODE_COUNT (EROS_BUILD_SINGLE_CORE_SIM + EROS_BUILD_M7_CORE + EROS_BUILD_M4_CORE)

#if EROS_BUILD_MODE_COUNT != 1
#error "Select exactly one EROS build mode: EROS_BUILD_SINGLE_CORE_SIM, EROS_BUILD_M7_CORE, or EROS_BUILD_M4_CORE."
#endif

#if EROS_BUILD_SINGLE_CORE_SIM
#define EROS_BUILD_MODE_NAME "Single Core Sim"
#define EROS_BUILD_HAS_M7_SIDE 1
#define EROS_BUILD_HAS_M4_SIDE 1
#define EROS_BUILD_USES_IN_PROCESS_TRANSPORT 1
#elif EROS_BUILD_M7_CORE
#define EROS_BUILD_MODE_NAME "M7 UI Core"
#define EROS_BUILD_HAS_M7_SIDE 1
#define EROS_BUILD_HAS_M4_SIDE 0
#define EROS_BUILD_USES_IN_PROCESS_TRANSPORT 0
#elif EROS_BUILD_M4_CORE
#define EROS_BUILD_MODE_NAME "M4 Control Core"
#define EROS_BUILD_HAS_M7_SIDE 0
#define EROS_BUILD_HAS_M4_SIDE 1
#define EROS_BUILD_USES_IN_PROCESS_TRANSPORT 0
#endif

// ------------------------------------------------------------
// Shared sizing constants
// ------------------------------------------------------------

#define InSize                  3
#define AssignableInSize        3

// PhysicalOutSize is the number of real relay outputs in OutRly[].
#define PhysicalOutSize         8

// OutSize is the number of logical outputs.
// This includes the 8 physical outputs plus the virtual Hitachi output.
#define OutSize                 9

// ------------------------------------------------------------
// Dedicated system input indexes
// ------------------------------------------------------------

#define IN_START                0
#define IN_STOP                 1
#define IN_PAUSE                2

// ------------------------------------------------------------
// Assignable input indexes
// ------------------------------------------------------------

#define ASSIGN_IN_1             0
#define ASSIGN_IN_2             1
#define ASSIGN_IN_3             2

// ------------------------------------------------------------
// Shared assignable input display labels
// ------------------------------------------------------------

static const char* const AssignableInputLabels[AssignableInSize] = {
  "Input 1",
  "Input 2",
  "Input 3"
};

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

// ------------------------------------------------------------
// Mode indexes
// ------------------------------------------------------------

#define BridgeMode              0
#define EROSFlexMode            1

// ------------------------------------------------------------
// Dimmer pins
// ------------------------------------------------------------

#define DIMMER_ZC_PIN           2
#define DIMMER_GATE_PIN         3

// ------------------------------------------------------------
// Hitachi mode values
// ------------------------------------------------------------

#define hitachiOff              0
#define hitachiValue            1
#define hitachiPulse            2
#define hitachiSine             3
#define hitachiSawTooth         4
#define hitachiTriangle         5
#define hitachiRandom           6


// ------------------------------------------------------------
// Settings result values
// ------------------------------------------------------------

#define EROS_SETTINGS_ACTION_NONE      0
#define EROS_SETTINGS_ACTION_SAVE      1
#define EROS_SETTINGS_ACTION_LOAD      2

// ------------------------------------------------------------
// Command packet definitions
//
// These are shared because they are the future M7-to-M4 command packet.
// ------------------------------------------------------------

enum EROS_CommandType
{
  EROS_CMD_NONE = 0,

  EROS_CMD_SET_MODE,

  EROS_CMD_SET_MANUAL_OUTPUT,
  EROS_CMD_TOGGLE_MANUAL_OUTPUT,

  EROS_CMD_SET_LOCK,
  EROS_CMD_TOGGLE_LOCK,

  EROS_CMD_SET_HITACHI_MODE,
  EROS_CMD_SET_HITACHI_SETPOINT,
  EROS_CMD_SET_HITACHI_MIN_VALUE,
  EROS_CMD_SET_HITACHI_MAX_VALUE,
  EROS_CMD_SET_HITACHI_PERIOD,
  EROS_CMD_SET_HITACHI_PERIOD_PRECISE,
  EROS_CMD_TOGGLE_HITACHI_PERIOD_PRECISE,
  EROS_CMD_SET_HITACHI_MIN_RELAY_VALUE,

  EROS_CMD_AUTO_START,
  EROS_CMD_AUTO_STOP,
  EROS_CMD_AUTO_PAUSE,

  EROS_CMD_SET_AUTO_RUN_DURATION_MINUTES,
  EROS_CMD_SET_AUTO_PAUSE_DURATION_SECONDS,
  EROS_CMD_SET_AUTO_PENALTY_DURATION_SECONDS,
  EROS_CMD_SET_AUTO_IO_ON_TIME_MS,
  EROS_CMD_SET_AUTO_IO_OFF_TIME_MS,
  EROS_CMD_SET_AUTO_OUTPUT_MODE,
  EROS_CMD_SET_AUTO_OUTPUT_INPUT_INDEX,

  EROS_CMD_REQUEST_SETTINGS_SAVE,
  EROS_CMD_REQUEST_SETTINGS_LOAD,

  EROS_CMD_TRANSPORT_LOOPBACK_PING
};

struct EROS_Command
{
  EROS_CommandType type;

  int index;
  long value;

  bool boolValue;
  bool onSettings;
};

// ------------------------------------------------------------
// Control status snapshot
//
// These fields are the future M4-to-M7 status packet.
// ------------------------------------------------------------

struct EROS_ControlStatus
{
  bool input[InSize];
  bool assignableInput[AssignableInSize];
  bool output[OutSize];

  bool manualOutputRequest[OutSize];

  byte mode;

  bool autoRunning;
  bool autoPaused;

  unsigned int autoRemainingTime;
  unsigned int autoCurrentTime;
  unsigned int autoRunDuration;
  unsigned int autoPauseDuration;
  unsigned int autoPenaltyDuration;

  unsigned int autoIoOnTimeMs;
  unsigned int autoIoOffTimeMs;

  byte autoOutputMode[OutSize];
  int autoOutputInputIndex[OutSize];

  int hitachiCurrentOutput;

  int hitachiModeOn;
  int hitachiModeOff;

  int hitachiSetPointOn;
  int hitachiSetPointOff;

  int hitachiMaxValueOn;
  int hitachiMaxValueOff;

  int hitachiMinValueOn;
  int hitachiMinValueOff;

  int hitachiPeriodOn;
  int hitachiPeriodOff;

  bool hitachiPeriodPreciseOn;
  bool hitachiPeriodPreciseOff;

  int hitachiMinRelayValue;

  // Transport health fields.
  // These are populated by the control side and read by the UI side.
  unsigned long transportStatusCounter;
  unsigned long transportStatusMillis;
  unsigned long transportCommandAcceptedCounter;
  unsigned long transportCommandRejectedCounter;
  unsigned long transportLoopbackRequestId;
  unsigned long transportLoopbackEchoId;
  unsigned long transportLoopbackEchoCounter;
  unsigned long transportLoopbackEchoMillis;
  byte transportCommandQueueDepth;
  byte transportCommandQueueCapacity;

  byte settingsLastAction;
  bool settingsLastOk;
  int settingsLastError;
  unsigned long settingsResultCounter;

  // Performance / transport-rate indicators.
  // M4 values are measured on the M4 control core.
  // M7 poll values are measured on the M7 display core.
  unsigned long m4LoopCounter;
  unsigned int m4AvgLoopPeriodUs;
  unsigned int m4AvgLoopExecUs;
  unsigned long m7StatusPollCounter;
  unsigned int m7StatusPollAvgMsX100;
};

#endif
