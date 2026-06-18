/*
  EROSState.ino

  Thin command/state interface between UI code and IO/control globals.

  For now, these functions still read/write the existing global variables:
    InValues[]
    OutValues[]
    Manual.out[]
    Mode.Current
    hS

  Later, this layer becomes the bridge between:
    M7: display / web server / settings
    M4: IO / control / dimmer / real-time logic
*/

// ------------------------------------------------------------
// Future command packet definitions
//
// For now these are not used yet.
// Later these will become the M7-to-M4 command messages.
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
  EROS_CMD_REQUEST_SETTINGS_LOAD
};

struct EROS_Command
{
  EROS_CommandType type;

  int index;
  int value;

  bool boolValue;
  bool onSettings;
};

// ------------------------------------------------------------
// Control status snapshot
//
// For now this is refreshed from the existing globals.
// Later this can become the M4-to-M7 status message.
// ------------------------------------------------------------

struct EROS_ControlStatus
{
  bool input[InSize];
  bool assignableInput[AssignableInSize];
  bool output[OutSize];

  bool autoRunning;
  bool autoPaused;

  unsigned int autoRemainingTime;
  unsigned int autoCurrentTime;
  unsigned int autoRunDuration;

  int hitachiCurrentOutput;
};

static EROS_ControlStatus g_controlStatus;

void Command_Execute(const EROS_Command & command);

void State_RefreshControlStatus()
{
  for (int i = 0; i < InSize; i++)
  {
    g_controlStatus.input[i] = InValues[i];
  }

  for (int i = 0; i < AssignableInSize; i++)
  {
    // INPUT_PULLUP logic:
    // LOW means the input is active.
    g_controlStatus.assignableInput[i] = (digitalRead(AssignableInputPins[i]) == LOW);
  }

  for (int i = 0; i < OutSize; i++)
  {
    g_controlStatus.output[i] = OutValues[i];
  }

  g_controlStatus.autoRunning = TimeVar.bRunning;
  g_controlStatus.autoPaused = TimeVar.bPaused;

  g_controlStatus.autoRemainingTime = TimeVar.iRemainingTime;
  g_controlStatus.autoCurrentTime = TimeVar.iCurrentTime;
  g_controlStatus.autoRunDuration = TimeVar.iRunDuration;

  g_controlStatus.hitachiCurrentOutput = hS.currentOutput;
}

// ------------------------------------------------------------
// Input / Output state
// ------------------------------------------------------------
bool State_GetInput(int inputIndex)
{
  if (inputIndex < 0 || inputIndex >= InSize) {
    return false;
  }

  return g_controlStatus.input[inputIndex];
}

bool State_GetAssignableInput(int inputIndex)
{
  if (inputIndex < 0 || inputIndex >= AssignableInSize)
  {
    return false;
  }

  return g_controlStatus.assignableInput[inputIndex];
}

bool State_GetAssignedInputForOutput(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize)
  {
    return false;
  }

  int inputIndex = EROSFlexSettings.OutInIdx[outputIndex];

  if (inputIndex < 0 || inputIndex >= AssignableInSize)
  {
    return false;
  }

  return State_GetAssignableInput(inputIndex);
}

bool State_GetOutput(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return false;
  }

  return g_controlStatus.output[outputIndex];
}

bool State_GetManualOutputRequest(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return false;
  }

  return Manual.out[outputIndex];
}

static void Command_ApplySetManualOutput(int outputIndex, bool state)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  // The Dimmer relay is automatic and should not be directly commanded.
  if (outputIndex == OUT_DIMMER_ENABLE) {
    return;
  }

  OutValues[outputIndex] = state;
  Manual.out[outputIndex] = state;
}

static void Command_ApplyToggleManualOutput(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  // The Dimmer relay is automatic and should not be directly commanded.
  if (outputIndex == OUT_DIMMER_ENABLE) {
    return;
  }

  Command_ApplySetManualOutput(outputIndex, !Manual.out[outputIndex]);
}

static void Command_ApplySetLock(bool state)
{
  OutValues[OUT_LOCK_1] = state;
  OutValues[OUT_LOCK_2] = state;

  Manual.out[OUT_LOCK_1] = state;
  Manual.out[OUT_LOCK_2] = state;
}

static void Command_ApplyToggleLock()
{
  bool newState = !(OutValues[OUT_LOCK_1] || OutValues[OUT_LOCK_2]);
  Command_ApplySetLock(newState);
}

void Command_SetManualOutput(int outputIndex, bool state)
{
  EROS_Command command;

  command.type = EROS_CMD_SET_MANUAL_OUTPUT;
  command.index = outputIndex;
  command.value = 0;
  command.boolValue = state;
  command.onSettings = false;

  Command_Execute(command);
}

void Command_ToggleManualOutput(int outputIndex)
{
  EROS_Command command;

  command.type = EROS_CMD_TOGGLE_MANUAL_OUTPUT;
  command.index = outputIndex;
  command.value = 0;
  command.boolValue = false;
  command.onSettings = false;

  Command_Execute(command);
}

void Command_SetLock(bool state)
{
  EROS_Command command;

  command.type = EROS_CMD_SET_LOCK;
  command.index = 0;
  command.value = 0;
  command.boolValue = state;
  command.onSettings = false;

  Command_Execute(command);
}

void Command_ToggleLock()
{
  EROS_Command command;

  command.type = EROS_CMD_TOGGLE_LOCK;
  command.index = 0;
  command.value = 0;
  command.boolValue = false;
  command.onSettings = false;

  Command_Execute(command);
}

// ------------------------------------------------------------
// Dimmer Relay enable request
// ------------------------------------------------------------

bool State_GetDimmerEnabledRequest()
{
  // Dimmer relay is automatic. Return actual relay state.
  return State_GetOutput(OUT_DIMMER_ENABLE);
}

void Command_SetDimmerEnabledRequest(bool enabled)
{
  // Deprecated: Dimmer relay is automatic and is controlled by Hitachi().
  // Keep this function so old UI calls do not break compile.
}

void Command_ToggleDimmerEnabledRequest()
{
  // Deprecated: Dimmer relay is automatic and is controlled by Hitachi().
  // Keep this function so old UI calls do not break compile.
}

// ------------------------------------------------------------
// Mode commands
// ------------------------------------------------------------

static void Command_ApplySetMode(byte mode)
{
  Mode.Current = mode;
}

void Command_SetMode(byte mode)
{
  EROS_Command command;

  command.type = EROS_CMD_SET_MODE;
  command.index = 0;
  command.value = mode;
  command.boolValue = false;
  command.onSettings = false;

  Command_Execute(command);
}

byte State_GetMode()
{
  return Mode.Current;
}

// ------------------------------------------------------------
// Hitachi setting helpers
// ------------------------------------------------------------

static int * State_HitachiModePtr(bool onSettings)
{
  return onSettings ? &hS.modeOn : &hS.modeOff;
}

static int * State_HitachiSetPointPtr(bool onSettings)
{
  return onSettings ? &hS.setPointOn : &hS.setPointOff;
}

static int * State_HitachiMaxValuePtr(bool onSettings)
{
  return onSettings ? &hS.maxValueOn : &hS.maxValueOff;
}

static int * State_HitachiMinValuePtr(bool onSettings)
{
  return onSettings ? &hS.minValueOn : &hS.minValueOff;
}

static int * State_HitachiPeriodPtr(bool onSettings)
{
  return onSettings ? &hS.periodOn : &hS.periodOff;
}

int State_GetHitachiMode(bool onSettings)
{
  return *State_HitachiModePtr(onSettings);
}

void Command_SetHitachiMode(bool onSettings, int mode)
{
  *State_HitachiModePtr(onSettings) = mode;
}

int State_GetHitachiSetPoint(bool onSettings)
{
  return *State_HitachiSetPointPtr(onSettings);
}

void Command_SetHitachiSetPoint(bool onSettings, int value)
{
  *State_HitachiSetPointPtr(onSettings) = constrain(value, hS.minRelayValue, 100);
}

void Command_SetHitachiMaxValue(bool onSettings, int value)
{
  *State_HitachiMaxValuePtr(onSettings) = constrain(value, hS.minRelayValue, 100);
}

int State_GetHitachiMaxValue(bool onSettings)
{
  return *State_HitachiMaxValuePtr(onSettings);
}

void Command_SetHitachiMinValue(bool onSettings, int value)
{
  *State_HitachiMinValuePtr(onSettings) = constrain(value, hS.minRelayValue, 100);
}

int State_GetHitachiMinValue(bool onSettings)
{
  return *State_HitachiMinValuePtr(onSettings);
}

int State_GetHitachiPeriod(bool onSettings)
{
  return *State_HitachiPeriodPtr(onSettings);
}

void Command_SetHitachiPeriod(bool onSettings, int periodMs)
{
  *State_HitachiPeriodPtr(onSettings) = constrain(periodMs, 100, 300000);
}

bool State_GetHitachiPeriodPrecise(bool onSettings)
{
  return onSettings ? hS.periodPreciseOn : hS.periodPreciseOff;
}

void Command_SetHitachiPeriodPrecise(bool onSettings, bool precise)
{
  if (onSettings) {
    hS.periodPreciseOn = precise;
  }
  else {
    hS.periodPreciseOff = precise;
  }
}

void Command_ToggleHitachiPeriodPrecise(bool onSettings)
{
  Command_SetHitachiPeriodPrecise(
    onSettings,
    !State_GetHitachiPeriodPrecise(onSettings)
  );
}


void Command_NormalizeHitachiSettings()
{
  hS.minRelayValue = constrain(hS.minRelayValue, 1, 100);

  hS.setPointOn = constrain(hS.setPointOn, hS.minRelayValue, 100);
  hS.maxValueOn = constrain(hS.maxValueOn, hS.minRelayValue, 100);
  hS.minValueOn = constrain(hS.minValueOn, hS.minRelayValue, 100);
  hS.periodOn = constrain(hS.periodOn, 100, 300000);

  hS.setPointOff = constrain(hS.setPointOff, hS.minRelayValue, 100);
  hS.maxValueOff = constrain(hS.maxValueOff, hS.minRelayValue, 100);
  hS.minValueOff = constrain(hS.minValueOff, hS.minRelayValue, 100);
  hS.periodOff = constrain(hS.periodOff, 100, 300000);

  hS.modeOn = constrain(hS.modeOn, hitachiOff, hitachiRandom);
  hS.modeOff = constrain(hS.modeOff, hitachiOff, hitachiRandom);

  // Do NOT infer precise/coarse from period here.
  // Just normalize the stored precise/coarse flags to clean boolean values.
  hS.periodPreciseOn = hS.periodPreciseOn ? true : false;
  hS.periodPreciseOff = hS.periodPreciseOff ? true : false;
}

int State_GetHitachiCurrentOutput()
{
  return g_controlStatus.hitachiCurrentOutput;
}

int State_GetHitachiMinRelayValue()
{
  return hS.minRelayValue;
}

void Command_SetHitachiMinRelayValue(int value)
{
  hS.minRelayValue = constrain(value, 1, 100);
  // Keep existing Hitachi values valid if the relay minimum changes.
  Command_NormalizeHitachiSettings();
}

void Command_AdjustHitachiMinRelayValue(int delta)
{
  Command_SetHitachiMinRelayValue(hS.minRelayValue + delta);
}

// ------------------------------------------------------------
// Hitachi virtual command helpers
// ------------------------------------------------------------

bool State_GetHitachiVirtualRequest()
{
  return State_GetManualOutputRequest(OUT_HITACHI_VIRTUAL);
}

bool State_GetHitachiVirtualOutput()
{
  return State_GetOutput(OUT_HITACHI_VIRTUAL);
}

void Command_SetHitachiVirtualRequest(bool enabled)
{
  Command_SetManualOutput(OUT_HITACHI_VIRTUAL, enabled);
}

void Command_ToggleHitachiVirtualRequest()
{
  Command_ToggleManualOutput(OUT_HITACHI_VIRTUAL);
}

// ------------------------------------------------------------
// Auto mode command helpers
// ------------------------------------------------------------

void Command_RequestAutoStart()
{
  SoftSwitches.Start = true;
}

void Command_RequestAutoStop()
{
  SoftSwitches.Stop = true;
}

void Command_RequestAutoPause()
{
  SoftSwitches.Pause = true;
}

bool State_GetAutoRunning()
{
  return g_controlStatus.autoRunning;
}

bool State_GetAutoPaused()
{
  return g_controlStatus.autoPaused;
}

unsigned int State_GetAutoRemainingTime()
{
  return g_controlStatus.autoRemainingTime;
}

unsigned int State_GetAutoCurrentTime()
{
  return g_controlStatus.autoCurrentTime;
}

unsigned int State_GetAutoRunDuration()
{
  return g_controlStatus.autoRunDuration;
}

// ------------------------------------------------------------
// Auto settings helpers
// ------------------------------------------------------------

unsigned int State_GetAutoRunDurationSeconds()
{
  return TimeVar.iRunDuration;
}

void Command_SetAutoRunDurationMinutes(unsigned int minutes)
{
  minutes = constrain(minutes, 1, 300);
  TimeVar.iRunDuration = minutes * 60;
}

unsigned int State_GetAutoRunDurationMinutes()
{
  return TimeVar.iRunDuration / 60;
}

unsigned int State_GetAutoPauseDurationSeconds()
{
  return TimeVar.iPauseDuration;
}

void Command_SetAutoPauseDurationSeconds(unsigned int seconds)
{
  TimeVar.iPauseDuration = constrain(seconds, 0, 300);
}

unsigned int State_GetAutoPenaltyDurationSeconds()
{
  return TimeVar.iPenaltyDuration;
}

void Command_SetAutoPenaltyDurationSeconds(unsigned int seconds)
{
  TimeVar.iPenaltyDuration = constrain(seconds, 0, 300);
}

unsigned int State_GetAutoIoOnTimeMs()
{
  return EROSFlexSettings.OnTime;
}

void Command_SetAutoIoOnTimeMs(unsigned int ms)
{
  EROSFlexSettings.OnTime = constrain(ms, 0, 300000);
}

unsigned int State_GetAutoIoOffTimeMs()
{
  return EROSFlexSettings.OffTime;
}

void Command_SetAutoIoOffTimeMs(unsigned int ms)
{
  EROSFlexSettings.OffTime = constrain(ms, 0, 300000);
}

byte State_GetAutoOutputMode(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return 0;
  }

  return EROSFlexSettings.OutMode[outputIndex];
}

void Command_SetAutoOutputMode(int outputIndex, byte mode)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  mode = constrain(mode, 0, 10);

  // Locks are fixed ON.
  if (outputIndex == OUT_LOCK_1 || outputIndex == OUT_LOCK_2) {
    EROSFlexSettings.OutMode[outputIndex] = 1;
    return;
  }

  // Dimmer relay is automatic and should not be user-configured.
  if (outputIndex == OUT_DIMMER_ENABLE) {
    EROSFlexSettings.OutMode[outputIndex] = 0;
    return;
  }

  EROSFlexSettings.OutMode[outputIndex] = mode;
}

int State_GetAutoOutputInputIndex(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize)
  {
    return 0;
  }

  int inputIndex = EROSFlexSettings.OutInIdx[outputIndex];

  if (inputIndex < 0 || inputIndex >= AssignableInSize)
  {
    return 0;
  }

  return inputIndex;
}

void Command_SetAutoOutputInputIndex(int outputIndex, int inputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize)
  {
    return;
  }

  if (inputIndex < 0 || inputIndex >= AssignableInSize)
  {
    return;
  }

  EROSFlexSettings.OutInIdx[outputIndex] = inputIndex;
}

void Command_CycleAutoOutputInputIndex(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize)
  {
    return;
  }

  int inputIndex = State_GetAutoOutputInputIndex(outputIndex);

  inputIndex++;

  if (inputIndex >= AssignableInSize)
  {
    inputIndex = 0;
  }

  Command_SetAutoOutputInputIndex(outputIndex, inputIndex);
}

void Command_ForceFixedAutoOutputModes()
{
  EROSFlexSettings.OutMode[OUT_LOCK_1] = 1;
  EROSFlexSettings.OutMode[OUT_LOCK_2] = 1;

  // Dimmer relay is automatic and should not be user-configured.
  EROSFlexSettings.OutMode[OUT_DIMMER_ENABLE] = 0;
}

// ------------------------------------------------------------
// Command execution
//
// For now this runs immediately on the same core.
// Later this function will be replaced by M7-to-M4 command transport.
// ------------------------------------------------------------

void Command_Execute(const EROS_Command & command)
{
  switch (command.type)
  {
    case EROS_CMD_SET_MODE:
      Command_ApplySetMode((byte)command.value);
      break;

    case EROS_CMD_SET_MANUAL_OUTPUT:
      Command_ApplySetManualOutput(command.index, command.boolValue);
      break;

    case EROS_CMD_TOGGLE_MANUAL_OUTPUT:
      Command_ApplyToggleManualOutput(command.index);
      break;

    case EROS_CMD_SET_LOCK:
      Command_ApplySetLock(command.boolValue);
      break;

    case EROS_CMD_TOGGLE_LOCK:
      Command_ApplyToggleLock();
      break;

    default:
      break;
  }
}