/*
  EROSBridgeM4.ino

  M4-facing command/status bridge.

  This file owns the control-side command queue, command execution, and
  publication of the EROS_ControlStatus packet.
*/

#include "EROSShared.h"

const int EROS_COMMAND_QUEUE_SIZE = 16;

static EROS_Command g_commandQueue[EROS_COMMAND_QUEUE_SIZE];
static int g_commandQueueHead = 0;
static int g_commandQueueTail = 0;
static int g_commandQueueCount = 0;
static bool g_processingCommands = false;

static EROS_ControlStatus g_controlStatus;

static byte g_settingsLastAction = EROS_SETTINGS_ACTION_NONE;
static bool g_settingsLastOk = true;
static int g_bridgeSettingsLastError = 0;
static unsigned long g_settingsResultCounter = 0;

void EROSTransport_PublishStatusToM7(const EROS_ControlStatus & status);
void Command_Execute(const EROS_Command & command);
void Command_NormalizeHitachiSettings();
void Command_ForceFixedAutoOutputModes();

static void Command_RecordSettingsResult(byte action, bool ok)
{
  g_settingsLastAction = action;
  g_settingsLastOk = ok;
  g_bridgeSettingsLastError = Settings_GetLastError();
  g_settingsResultCounter++;
}

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

// Control-side transport receive point.
// Today this receives commands from the in-process transport shim. Later it
// becomes the M4-side receive handler for M7 command packets.
bool EROSM4_ReceiveCommandFromTransport(const EROS_Command & command)
{
  return Command_QueuePush(command);
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


bool Control_GetAssignedInputForOutput(int outputIndex)
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

  // INPUT_PULLUP logic: LOW means active.
  return (digitalRead(AssignableInputPins[inputIndex]) == LOW);
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

  g_controlStatus.settingsLastAction = g_settingsLastAction;
  g_controlStatus.settingsLastOk = g_settingsLastOk;
  g_controlStatus.settingsLastError = g_bridgeSettingsLastError;
  g_controlStatus.settingsResultCounter = g_settingsResultCounter;

  EROSTransport_PublishStatusToM7(g_controlStatus);
}

void State_CopyControlStatus(EROS_ControlStatus & status)
{
  status = g_controlStatus;
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

static void Command_ApplySetMode(byte mode)
{
  Mode.Current = mode;
}

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

static void Command_ApplySetHitachiMode(bool onSettings, int mode)
{
  *State_HitachiModePtr(onSettings) = mode;
}

static void Command_ApplySetHitachiSetPoint(bool onSettings, int value)
{
  *State_HitachiSetPointPtr(onSettings) = constrain(value, hS.minRelayValue, 100);
}

static void Command_ApplySetHitachiMaxValue(bool onSettings, int value)
{
  *State_HitachiMaxValuePtr(onSettings) = constrain(value, hS.minRelayValue, 100);
}

static void Command_ApplySetHitachiMinValue(bool onSettings, int value)
{
  *State_HitachiMinValuePtr(onSettings) = constrain(value, hS.minRelayValue, 100);
}

static void Command_ApplySetHitachiPeriod(bool onSettings, long periodMs)
{
  *State_HitachiPeriodPtr(onSettings) = constrain(periodMs, 100, 300000);
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

static void Command_ApplySetHitachiMinRelayValue(int value)
{
  hS.minRelayValue = constrain(value, 1, 100);

  // Keep existing Hitachi values valid if the relay minimum changes.
  Command_NormalizeHitachiSettings();
}

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

static void Command_ApplySetAutoRunDurationMinutes(unsigned int minutes)
{
  minutes = constrain(minutes, 1, 300);
  TimeVar.iRunDuration = minutes * 60;
}

static void Command_ApplySetAutoPauseDurationSeconds(unsigned int seconds)
{
  TimeVar.iPauseDuration = constrain(seconds, 0, 300);
}

static void Command_ApplySetAutoPenaltyDurationSeconds(unsigned int seconds)
{
  TimeVar.iPenaltyDuration = constrain(seconds, 0, 300);
}

static void Command_ApplySetAutoIoOnTimeMs(unsigned int ms)
{
  EROSFlexSettings.OnTime = constrain(ms, 0, 300000);
}

static void Command_ApplySetAutoIoOffTimeMs(unsigned int ms)
{
  EROSFlexSettings.OffTime = constrain(ms, 0, 300000);
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

void Command_ForceFixedAutoOutputModes()
{
  EROSFlexSettings.OutMode[OUT_LOCK_1] = 1;
  EROSFlexSettings.OutMode[OUT_LOCK_2] = 1;

  // Dimmer relay is automatic and should not be user-configured.
  EROSFlexSettings.OutMode[OUT_DIMMER_ENABLE] = 0;
}


static bool Command_ApplySettingsSave()
{
  bool ok = Settings_SaveAll();
  Command_RecordSettingsResult(EROS_SETTINGS_ACTION_SAVE, ok);
  return ok;
}

static bool Command_ApplySettingsLoad()
{
  bool ok = Settings_LoadAll();

  if (ok)
  {
    Command_NormalizeHitachiSettings();
    Command_ForceFixedAutoOutputModes();
  }

  Command_RecordSettingsResult(EROS_SETTINGS_ACTION_LOAD, ok);
  return ok;
}


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