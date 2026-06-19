/*
  EROSBridgeM7.ino

  M7-facing command/status bridge.

  This file should remain display/UI safe:
    - Builds EROS_Command packets.
    - Submits packets to the control side.
    - Reads the most recent EROS_ControlStatus packet.

  It should not directly touch control globals such as OutValues, InValues,
  TimeVar, Manual, hS, or EROSFlexSettings.
*/

#include "EROSShared.h"

static EROS_ControlStatus g_m7ControlStatus;

bool Command_SubmitToControl(const EROS_Command & command);
void State_ProcessPendingCommands();

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

  // In the split-ready model, the UI side never executes control logic as a
  // queue-overflow fallback. Failure is reported to the operator instead.
  if (!Command_SubmitToControl(command))
  {
    // The current UI does not yet display this error, but the command is
    // intentionally not executed here. The control side owns execution.
    return;
  }
}

void State_ApplyControlStatus(const EROS_ControlStatus & status)
{
  g_m7ControlStatus = status;
}

bool State_GetInput(int inputIndex)
{
  if (inputIndex < 0 || inputIndex >= InSize) {
    return false;
  }

  return g_m7ControlStatus.input[inputIndex];
}

bool State_GetAssignableInput(int inputIndex)
{
  if (inputIndex < 0 || inputIndex >= AssignableInSize)
  {
    return false;
  }

  return g_m7ControlStatus.assignableInput[inputIndex];
}

bool State_GetAssignedInputForOutput(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize)
  {
    return false;
  }

  int inputIndex = g_m7ControlStatus.autoOutputInputIndex[outputIndex];

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

  return g_m7ControlStatus.output[outputIndex];
}

bool State_GetManualOutputRequest(int outputIndex)
{
  if (outputIndex < 0 || outputIndex >= OutSize) {
    return false;
  }

  return g_m7ControlStatus.manualOutputRequest[outputIndex];
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

void Command_SetMode(byte mode)
{
  Command_Send(EROS_CMD_SET_MODE, 0, mode);
}

byte State_GetMode()
{
  return g_m7ControlStatus.mode;
}

int State_GetSettingsLastError()
{
  return g_m7ControlStatus.settingsLastError;
}


int State_GetHitachiMode(bool onSettings)
{
  return onSettings ? g_m7ControlStatus.hitachiModeOn : g_m7ControlStatus.hitachiModeOff;
}

void Command_SetHitachiMode(bool onSettings, int mode)
{
  Command_Send(EROS_CMD_SET_HITACHI_MODE, 0, mode, false, onSettings);
}

int State_GetHitachiSetPoint(bool onSettings)
{
  return onSettings ? g_m7ControlStatus.hitachiSetPointOn : g_m7ControlStatus.hitachiSetPointOff;
}

void Command_SetHitachiSetPoint(bool onSettings, int value)
{
  Command_Send(EROS_CMD_SET_HITACHI_SETPOINT, 0, value, false, onSettings);
}

void Command_SetHitachiMaxValue(bool onSettings, int value)
{
  Command_Send(EROS_CMD_SET_HITACHI_MAX_VALUE, 0, value, false, onSettings);
}

int State_GetHitachiMaxValue(bool onSettings)
{
  return onSettings ? g_m7ControlStatus.hitachiMaxValueOn : g_m7ControlStatus.hitachiMaxValueOff;
}

void Command_SetHitachiMinValue(bool onSettings, int value)
{
  Command_Send(EROS_CMD_SET_HITACHI_MIN_VALUE, 0, value, false, onSettings);
}

int State_GetHitachiMinValue(bool onSettings)
{
  return onSettings ? g_m7ControlStatus.hitachiMinValueOn : g_m7ControlStatus.hitachiMinValueOff;
}

int State_GetHitachiPeriod(bool onSettings)
{
  return onSettings ? g_m7ControlStatus.hitachiPeriodOn : g_m7ControlStatus.hitachiPeriodOff;
}

void Command_SetHitachiPeriod(bool onSettings, int periodMs)
{
  Command_Send(EROS_CMD_SET_HITACHI_PERIOD, 0, periodMs, false, onSettings);
}

bool State_GetHitachiPeriodPrecise(bool onSettings)
{
  return onSettings ? g_m7ControlStatus.hitachiPeriodPreciseOn : g_m7ControlStatus.hitachiPeriodPreciseOff;
}

void Command_SetHitachiPeriodPrecise(bool onSettings, bool precise)
{
  Command_Send(EROS_CMD_SET_HITACHI_PERIOD_PRECISE, 0, 0, precise, onSettings);
}

void Command_ToggleHitachiPeriodPrecise(bool onSettings)
{
  Command_Send(EROS_CMD_TOGGLE_HITACHI_PERIOD_PRECISE, 0, 0, false, onSettings);
}

int State_GetHitachiCurrentOutput()
{
  return g_m7ControlStatus.hitachiCurrentOutput;
}

int State_GetHitachiMinRelayValue()
{
  return g_m7ControlStatus.hitachiMinRelayValue;
}

void Command_SetHitachiMinRelayValue(int value)
{
  Command_Send(EROS_CMD_SET_HITACHI_MIN_RELAY_VALUE, 0, value);
}

void Command_AdjustHitachiMinRelayValue(int delta)
{
  Command_SetHitachiMinRelayValue(State_GetHitachiMinRelayValue() + delta);
}


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
  return g_m7ControlStatus.autoRunning;
}

bool State_GetAutoPaused()
{
  return g_m7ControlStatus.autoPaused;
}

unsigned int State_GetAutoRemainingTime()
{
  return g_m7ControlStatus.autoRemainingTime;
}

unsigned int State_GetAutoCurrentTime()
{
  return g_m7ControlStatus.autoCurrentTime;
}

unsigned int State_GetAutoRunDuration()
{
  return g_m7ControlStatus.autoRunDuration;
}

unsigned int State_GetAutoRunDurationSeconds()
{
  return g_m7ControlStatus.autoRunDuration;
}

void Command_SetAutoRunDurationMinutes(unsigned int minutes)
{
  Command_Send(EROS_CMD_SET_AUTO_RUN_DURATION_MINUTES, 0, minutes);
}

unsigned int State_GetAutoRunDurationMinutes()
{
  return g_m7ControlStatus.autoRunDuration / 60;
}

unsigned int State_GetAutoPauseDurationSeconds()
{
  return g_m7ControlStatus.autoPauseDuration;
}

void Command_SetAutoPauseDurationSeconds(unsigned int seconds)
{
  Command_Send(EROS_CMD_SET_AUTO_PAUSE_DURATION_SECONDS, 0, seconds);
}

unsigned int State_GetAutoPenaltyDurationSeconds()
{
  return g_m7ControlStatus.autoPenaltyDuration;
}

void Command_SetAutoPenaltyDurationSeconds(unsigned int seconds)
{
  Command_Send(EROS_CMD_SET_AUTO_PENALTY_DURATION_SECONDS, 0, seconds);
}

unsigned int State_GetAutoIoOnTimeMs()
{
  return g_m7ControlStatus.autoIoOnTimeMs;
}

void Command_SetAutoIoOnTimeMs(unsigned int ms)
{
  Command_Send(EROS_CMD_SET_AUTO_IO_ON_TIME_MS, 0, ms);
}

unsigned int State_GetAutoIoOffTimeMs()
{
  return g_m7ControlStatus.autoIoOffTimeMs;
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

  return g_m7ControlStatus.autoOutputMode[outputIndex];
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

  int inputIndex = g_m7ControlStatus.autoOutputInputIndex[outputIndex];

  if (inputIndex < 0 || inputIndex >= AssignableInSize)
  {
    return 0;
  }

  return inputIndex;
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

void Command_RequestSettingsSave()
{
  Command_Send(EROS_CMD_REQUEST_SETTINGS_SAVE);
}

void Command_RequestSettingsLoad()
{
  Command_Send(EROS_CMD_REQUEST_SETTINGS_LOAD);
}


byte State_GetSettingsLastAction()
{
  return g_m7ControlStatus.settingsLastAction;
}

bool State_GetSettingsLastOk()
{
  return g_m7ControlStatus.settingsLastOk;
}

unsigned long State_GetSettingsResultCounter()
{
  return g_m7ControlStatus.settingsResultCounter;
}

