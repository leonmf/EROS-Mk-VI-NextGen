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
// Input / Output state
// ------------------------------------------------------------

bool State_GetInput(int inputIndex)
{
  if (inputIndex < 0 || inputIndex >= InSize) {
    return false;
  }

  return InValues[inputIndex];
}

bool State_GetAssignableInput(int inputIndex)
{
  if (inputIndex < 0 || inputIndex >= AssignableInSize)
  {
    return false;
  }

  // INPUT_PULLUP logic:
  // LOW means the input is active.
  return digitalRead(AssignableInputPins[inputIndex]) == LOW;
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

  return OutValues[outputIndex];
}

bool State_GetManualOutputRequest(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return false;
  }

  return Manual.out[outputIndex];
}

void Command_SetManualOutput(int outputIndex, bool state)
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

void Command_ToggleManualOutput(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return;
  }

  // The Dimmer relay is automatic and should not be directly commanded.
  if (outputIndex == OUT_DIMMER_ENABLE) {
    return;
  }

  Command_SetManualOutput(outputIndex, !Manual.out[outputIndex]);
}

void Command_SetLock(bool state)
{
  OutValues[OUT_LOCK_1] = state;
  OutValues[OUT_LOCK_2] = state;

  Manual.out[OUT_LOCK_1] = state;
  Manual.out[OUT_LOCK_2] = state;
}

void Command_ToggleLock()
{
  bool newState = !(OutValues[OUT_LOCK_1] || OutValues[OUT_LOCK_2]);
  Command_SetLock(newState);
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

void Command_SetMode(byte mode)
{
  Mode.Current = mode;
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
  *State_HitachiSetPointPtr(onSettings) = constrain(value, 25, 100);
}

int State_GetHitachiMaxValue(bool onSettings)
{
  return *State_HitachiMaxValuePtr(onSettings);
}

void Command_SetHitachiMaxValue(bool onSettings, int value)
{
  *State_HitachiMaxValuePtr(onSettings) = constrain(value, 25, 100);
}

int State_GetHitachiMinValue(bool onSettings)
{
  return *State_HitachiMinValuePtr(onSettings);
}

void Command_SetHitachiMinValue(bool onSettings, int value)
{
  *State_HitachiMinValuePtr(onSettings) = constrain(value, 25, 100);
}

int State_GetHitachiPeriod(bool onSettings)
{
  return *State_HitachiPeriodPtr(onSettings);
}

void Command_SetHitachiPeriod(bool onSettings, int periodMs)
{
  *State_HitachiPeriodPtr(onSettings) = constrain(periodMs, 100, 300000);
}

int State_GetHitachiCurrentOutput()
{
  return hS.currentOutput;
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
  return TimeVar.bRunning;
}

bool State_GetAutoPaused()
{
  return TimeVar.bPaused;
}

unsigned int State_GetAutoRemainingTime()
{
  return TimeVar.iRemainingTime;
}

unsigned int State_GetAutoCurrentTime()
{
  return TimeVar.iCurrentTime;
}

unsigned int State_GetAutoRunDuration()
{
  return TimeVar.iRunDuration;
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