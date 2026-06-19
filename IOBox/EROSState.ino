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


#include "EROSShared.h"

// ------------------------------------------------------------
// Command queue
//
// For now this queue is drained immediately after Command_Send().
// Later, Command_Send() can become the M7-side enqueue/send function,
// while the M4 side drains and executes the commands.
// ------------------------------------------------------------

const int EROS_COMMAND_QUEUE_SIZE = 16;

static EROS_Command g_commandQueue[EROS_COMMAND_QUEUE_SIZE];
static int g_commandQueueHead = 0;
static int g_commandQueueTail = 0;
static int g_commandQueueCount = 0;
static bool g_processingCommands = false;

static EROS_ControlStatus g_controlStatus;

void Command_Execute(const EROS_Command & command);
void State_RefreshControlStatus();
void State_ProcessPendingCommands();

static bool Command_QueuePush(const EROS_Command & command);
static bool Command_QueuePop(EROS_Command & command);

static bool Command_QueuePush(const EROS_Command & command)
{
  if (g_commandQueueCount >= EROS_COMMAND_QUEUE_SIZE)
  {
    return false;
  }

  g_commandQueue[g_commandQueueTail] = command;
  g_commandQueueTail++;

  if (g_commandQueueTail >= EROS_COMMAND_QUEUE_SIZE)
  {
    g_commandQueueTail = 0;
  }

  g_commandQueueCount++;

  return true;
}

static bool Command_QueuePop(EROS_Command & command)
{
  if (g_commandQueueCount <= 0)
  {
    return false;
  }

  command = g_commandQueue[g_commandQueueHead];
  g_commandQueueHead++;

  if (g_commandQueueHead >= EROS_COMMAND_QUEUE_SIZE)
  {
    g_commandQueueHead = 0;
  }

  g_commandQueueCount--;

  return true;
}

static void Command_Send(
  EROS_CommandType type,
  int index = 0,
  long value = 0,
  bool boolValue = false,
  bool onSettings = false
)
{
  EROS_Command command;

  command.type = type;
  command.index = index;
  command.value = value;
  command.boolValue = boolValue;
  command.onSettings = onSettings;

  if (!Command_QueuePush(command))
  {
    // Queue overflow should not silently drop operator commands.
    // For this single-core transition phase, execute immediately as a fallback.
    Command_Execute(command);
    State_RefreshControlStatus();
    return;
  }

  // Single-core compatibility:
  // LVGL slider callbacks expect the state snapshot to be current before the
  // callback finishes. Drain immediately for now.
  //
  // The queue still exists, and loop() can still call State_ProcessPendingCommands().
  // Later, when the display callbacks are made asynchronous-safe, this immediate
  // drain can be removed.
  State_ProcessPendingCommands();
}

void State_ProcessPendingCommands()
{
  if (g_processingCommands)
  {
    return;
  }

  g_processingCommands = true;

  bool processedAnyCommand = false;
  EROS_Command command;

  while (Command_QueuePop(command))
  {
    Command_Execute(command);
    processedAnyCommand = true;
  }

  g_processingCommands = false;

  if (processedAnyCommand)
  {
    State_RefreshControlStatus();
  }
}

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
    g_controlStatus.manualOutputRequest[i] = Manual.out[i];
  }

  g_controlStatus.mode = Mode.Current;

  g_controlStatus.autoRunning = TimeVar.bRunning;
  g_controlStatus.autoPaused = TimeVar.bPaused;

  g_controlStatus.autoRemainingTime = TimeVar.iRemainingTime;
  g_controlStatus.autoCurrentTime = TimeVar.iCurrentTime;
  g_controlStatus.autoRunDuration = TimeVar.iRunDuration;
  g_controlStatus.autoPauseDuration = TimeVar.iPauseDuration;
  g_controlStatus.autoPenaltyDuration = TimeVar.iPenaltyDuration;

  g_controlStatus.autoIoOnTimeMs = EROSFlexSettings.OnTime;
  g_controlStatus.autoIoOffTimeMs = EROSFlexSettings.OffTime;

  for (int i = 0; i < OutSize; i++)
  {
    g_controlStatus.autoOutputMode[i] = EROSFlexSettings.OutMode[i];
    g_controlStatus.autoOutputInputIndex[i] = EROSFlexSettings.OutInIdx[i];
  }

  g_controlStatus.hitachiCurrentOutput = hS.currentOutput;

  g_controlStatus.hitachiModeOn = hS.modeOn;
  g_controlStatus.hitachiModeOff = hS.modeOff;

  g_controlStatus.hitachiSetPointOn = hS.setPointOn;
  g_controlStatus.hitachiSetPointOff = hS.setPointOff;

  g_controlStatus.hitachiMaxValueOn = hS.maxValueOn;
  g_controlStatus.hitachiMaxValueOff = hS.maxValueOff;

  g_controlStatus.hitachiMinValueOn = hS.minValueOn;
  g_controlStatus.hitachiMinValueOff = hS.minValueOff;

  g_controlStatus.hitachiPeriodOn = hS.periodOn;
  g_controlStatus.hitachiPeriodOff = hS.periodOff;

  g_controlStatus.hitachiPeriodPreciseOn = hS.periodPreciseOn;
  g_controlStatus.hitachiPeriodPreciseOff = hS.periodPreciseOff;

  g_controlStatus.hitachiMinRelayValue = hS.minRelayValue;
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

  int inputIndex = g_controlStatus.autoOutputInputIndex[outputIndex];

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

  return g_controlStatus.manualOutputRequest[outputIndex];
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
  Command_Send(EROS_CMD_SET_MANUAL_OUTPUT, outputIndex, 0, state);
}

void Command_ToggleManualOutput(int outputIndex)
{
  Command_Send(EROS_CMD_TOGGLE_MANUAL_OUTPUT, outputIndex);
}

void Command_SetLock(bool state)
{
  Command_Send(EROS_CMD_SET_LOCK, 0, 0, state);
}

void Command_ToggleLock()
{
  Command_Send(EROS_CMD_TOGGLE_LOCK);
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
  Command_Send(EROS_CMD_SET_MODE, 0, mode);
}

byte State_GetMode()
{
  return g_controlStatus.mode;
}

int State_GetSettingsLastError()
{
  return Settings_GetLastError();
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
  return onSettings ? g_controlStatus.hitachiModeOn : g_controlStatus.hitachiModeOff;
}

static void Command_ApplySetHitachiMode(bool onSettings, int mode)
{
  *State_HitachiModePtr(onSettings) = mode;
}

void Command_SetHitachiMode(bool onSettings, int mode)
{
  Command_Send(EROS_CMD_SET_HITACHI_MODE, 0, mode, false, onSettings);
}

int State_GetHitachiSetPoint(bool onSettings)
{
  return onSettings ? g_controlStatus.hitachiSetPointOn : g_controlStatus.hitachiSetPointOff;
}

static void Command_ApplySetHitachiSetPoint(bool onSettings, int value)
{
  *State_HitachiSetPointPtr(onSettings) = constrain(value, hS.minRelayValue, 100);
}

void Command_SetHitachiSetPoint(bool onSettings, int value)
{
  Command_Send(EROS_CMD_SET_HITACHI_SETPOINT, 0, value, false, onSettings);
}

static void Command_ApplySetHitachiMaxValue(bool onSettings, int value)
{
  *State_HitachiMaxValuePtr(onSettings) = constrain(value, hS.minRelayValue, 100);
}

void Command_SetHitachiMaxValue(bool onSettings, int value)
{
  Command_Send(EROS_CMD_SET_HITACHI_MAX_VALUE, 0, value, false, onSettings);
}

int State_GetHitachiMaxValue(bool onSettings)
{
  return onSettings ? g_controlStatus.hitachiMaxValueOn : g_controlStatus.hitachiMaxValueOff;
}

static void Command_ApplySetHitachiMinValue(bool onSettings, int value)
{
  *State_HitachiMinValuePtr(onSettings) = constrain(value, hS.minRelayValue, 100);
}

void Command_SetHitachiMinValue(bool onSettings, int value)
{
  Command_Send(EROS_CMD_SET_HITACHI_MIN_VALUE, 0, value, false, onSettings);
}

int State_GetHitachiMinValue(bool onSettings)
{
  return onSettings ? g_controlStatus.hitachiMinValueOn : g_controlStatus.hitachiMinValueOff;
}

int State_GetHitachiPeriod(bool onSettings)
{
  return onSettings ? g_controlStatus.hitachiPeriodOn : g_controlStatus.hitachiPeriodOff;
}

static void Command_ApplySetHitachiPeriod(bool onSettings, long periodMs)
{
  *State_HitachiPeriodPtr(onSettings) = constrain(periodMs, 100, 300000);
}

void Command_SetHitachiPeriod(bool onSettings, int periodMs)
{
  Command_Send(EROS_CMD_SET_HITACHI_PERIOD, 0, periodMs, false, onSettings);
}

bool State_GetHitachiPeriodPrecise(bool onSettings)
{
  return onSettings ? g_controlStatus.hitachiPeriodPreciseOn : g_controlStatus.hitachiPeriodPreciseOff;
}

static void Command_ApplySetHitachiPeriodPrecise(bool onSettings, bool precise)
{
  if (onSettings) {
    hS.periodPreciseOn = precise;
  }
  else {
    hS.periodPreciseOff = precise;
  }
}

static void Command_ApplyToggleHitachiPeriodPrecise(bool onSettings)
{
  if (onSettings) {
    Command_ApplySetHitachiPeriodPrecise(true, !hS.periodPreciseOn);
  }
  else {
    Command_ApplySetHitachiPeriodPrecise(false, !hS.periodPreciseOff);
  }
}

void Command_SetHitachiPeriodPrecise(bool onSettings, bool precise)
{
  Command_Send(EROS_CMD_SET_HITACHI_PERIOD_PRECISE, 0, 0, precise, onSettings);
}

void Command_ToggleHitachiPeriodPrecise(bool onSettings)
{
  Command_Send(EROS_CMD_TOGGLE_HITACHI_PERIOD_PRECISE, 0, 0, false, onSettings);
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
  return g_controlStatus.hitachiMinRelayValue;
}

static void Command_ApplySetHitachiMinRelayValue(int value)
{
  hS.minRelayValue = constrain(value, 1, 100);

  // Keep existing Hitachi values valid if the relay minimum changes.
  Command_NormalizeHitachiSettings();
}

void Command_SetHitachiMinRelayValue(int value)
{
  Command_Send(EROS_CMD_SET_HITACHI_MIN_RELAY_VALUE, 0, value);
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

static void Command_ApplyAutoStart()
{
  SoftSwitches.Start = true;
}

static void Command_ApplyAutoStop()
{
  SoftSwitches.Stop = true;
}

static void Command_ApplyAutoPause()
{
  SoftSwitches.Pause = true;
}

void Command_RequestAutoStart()
{
  Command_Send(EROS_CMD_AUTO_START);
}

void Command_RequestAutoStop()
{
  Command_Send(EROS_CMD_AUTO_STOP);
}

void Command_RequestAutoPause()
{
  Command_Send(EROS_CMD_AUTO_PAUSE);
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
  return g_controlStatus.autoRunDuration;
}

static void Command_ApplySetAutoRunDurationMinutes(unsigned int minutes)
{
  minutes = constrain(minutes, 1, 300);
  TimeVar.iRunDuration = minutes * 60;
}

void Command_SetAutoRunDurationMinutes(unsigned int minutes)
{
  Command_Send(EROS_CMD_SET_AUTO_RUN_DURATION_MINUTES, 0, minutes);
}

unsigned int State_GetAutoRunDurationMinutes()
{
  return g_controlStatus.autoRunDuration / 60;
}

unsigned int State_GetAutoPauseDurationSeconds()
{
  return g_controlStatus.autoPauseDuration;
}

static void Command_ApplySetAutoPauseDurationSeconds(unsigned int seconds)
{
  TimeVar.iPauseDuration = constrain(seconds, 0, 300);
}

void Command_SetAutoPauseDurationSeconds(unsigned int seconds)
{
  Command_Send(EROS_CMD_SET_AUTO_PAUSE_DURATION_SECONDS, 0, seconds);
}

unsigned int State_GetAutoPenaltyDurationSeconds()
{
  return g_controlStatus.autoPenaltyDuration;
}

static void Command_ApplySetAutoPenaltyDurationSeconds(unsigned int seconds)
{
  TimeVar.iPenaltyDuration = constrain(seconds, 0, 300);
}

void Command_SetAutoPenaltyDurationSeconds(unsigned int seconds)
{
  Command_Send(EROS_CMD_SET_AUTO_PENALTY_DURATION_SECONDS, 0, seconds);
}

unsigned int State_GetAutoIoOnTimeMs()
{
  return g_controlStatus.autoIoOnTimeMs;
}

static void Command_ApplySetAutoIoOnTimeMs(unsigned int ms)
{
  EROSFlexSettings.OnTime = constrain(ms, 0, 300000);
}

void Command_SetAutoIoOnTimeMs(unsigned int ms)
{
  Command_Send(EROS_CMD_SET_AUTO_IO_ON_TIME_MS, 0, ms);
}

unsigned int State_GetAutoIoOffTimeMs()
{
  return g_controlStatus.autoIoOffTimeMs;
}

static void Command_ApplySetAutoIoOffTimeMs(unsigned int ms)
{
  EROSFlexSettings.OffTime = constrain(ms, 0, 300000);
}

void Command_SetAutoIoOffTimeMs(unsigned int ms)
{
  Command_Send(EROS_CMD_SET_AUTO_IO_OFF_TIME_MS, 0, ms);
}

byte State_GetAutoOutputMode(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return 0;
  }

  return g_controlStatus.autoOutputMode[outputIndex];
}

static void Command_ApplySetAutoOutputMode(int outputIndex, byte mode)
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

void Command_SetAutoOutputMode(int outputIndex, byte mode)
{
  Command_Send(EROS_CMD_SET_AUTO_OUTPUT_MODE, outputIndex, mode);
}

int State_GetAutoOutputInputIndex(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize)
  {
    return 0;
  }

  int inputIndex = g_controlStatus.autoOutputInputIndex[outputIndex];

  if (inputIndex < 0 || inputIndex >= AssignableInSize)
  {
    return 0;
  }

  return inputIndex;
}

static void Command_ApplySetAutoOutputInputIndex(int outputIndex, int inputIndex)
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

void Command_SetAutoOutputInputIndex(int outputIndex, int inputIndex)
{
  Command_Send(EROS_CMD_SET_AUTO_OUTPUT_INPUT_INDEX, outputIndex, inputIndex);
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
// Settings command helpers
// ------------------------------------------------------------

static bool Command_ApplySettingsSave()
{
  return Settings_SaveAll();
}

static bool Command_ApplySettingsLoad()
{
  bool ok = Settings_LoadAll();

  if (ok)
  {
    Command_NormalizeHitachiSettings();
    Command_ForceFixedAutoOutputModes();
    State_RefreshControlStatus();
  }

  return ok;
}

bool Command_RequestSettingsSave()
{
  bool ok = Command_ApplySettingsSave();
  State_RefreshControlStatus();
  return ok;
}

bool Command_RequestSettingsLoad()
{
  bool ok = Command_ApplySettingsLoad();
  State_RefreshControlStatus();
  return ok;
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

    case EROS_CMD_AUTO_START:
      Command_ApplyAutoStart();
      break;

    case EROS_CMD_AUTO_STOP:
      Command_ApplyAutoStop();
      break;

    case EROS_CMD_AUTO_PAUSE:
      Command_ApplyAutoPause();
      break;

    case EROS_CMD_SET_AUTO_RUN_DURATION_MINUTES:
      Command_ApplySetAutoRunDurationMinutes((unsigned int)command.value);
      break;

    case EROS_CMD_SET_AUTO_PAUSE_DURATION_SECONDS:
      Command_ApplySetAutoPauseDurationSeconds((unsigned int)command.value);
      break;

    case EROS_CMD_SET_AUTO_PENALTY_DURATION_SECONDS:
      Command_ApplySetAutoPenaltyDurationSeconds((unsigned int)command.value);
      break;

    case EROS_CMD_SET_AUTO_IO_ON_TIME_MS:
      Command_ApplySetAutoIoOnTimeMs((unsigned int)command.value);
      break;

    case EROS_CMD_SET_AUTO_IO_OFF_TIME_MS:
      Command_ApplySetAutoIoOffTimeMs((unsigned int)command.value);
      break;

    case EROS_CMD_SET_AUTO_OUTPUT_MODE:
      Command_ApplySetAutoOutputMode(command.index, (byte)command.value);
      break;

    case EROS_CMD_SET_AUTO_OUTPUT_INPUT_INDEX:
      Command_ApplySetAutoOutputInputIndex(command.index, command.value);
      break;

    case EROS_CMD_SET_HITACHI_MODE:
      Command_ApplySetHitachiMode(command.onSettings, (int)command.value);
      break;

    case EROS_CMD_SET_HITACHI_SETPOINT:
      Command_ApplySetHitachiSetPoint(command.onSettings, (int)command.value);
      break;

    case EROS_CMD_SET_HITACHI_MAX_VALUE:
      Command_ApplySetHitachiMaxValue(command.onSettings, (int)command.value);
      break;

    case EROS_CMD_SET_HITACHI_MIN_VALUE:
      Command_ApplySetHitachiMinValue(command.onSettings, (int)command.value);
      break;

    case EROS_CMD_SET_HITACHI_PERIOD:
      Command_ApplySetHitachiPeriod(command.onSettings, command.value);
      break;

    case EROS_CMD_SET_HITACHI_PERIOD_PRECISE:
      Command_ApplySetHitachiPeriodPrecise(command.onSettings, command.boolValue);
      break;

    case EROS_CMD_TOGGLE_HITACHI_PERIOD_PRECISE:
      Command_ApplyToggleHitachiPeriodPrecise(command.onSettings);
      break;

    case EROS_CMD_SET_HITACHI_MIN_RELAY_VALUE:
      Command_ApplySetHitachiMinRelayValue((int)command.value);
      break;

    case EROS_CMD_REQUEST_SETTINGS_SAVE:
      Command_ApplySettingsSave();
      break;

    case EROS_CMD_REQUEST_SETTINGS_LOAD:
      Command_ApplySettingsLoad();
      break;

    default:
      break;
  }
}