/*
  GigaSettingsM7.ino

  M7-owned persistent settings storage for split-core EROS Mk VI.

  Architecture:
    - M7 owns QSPI/FAT save/load.
    - M4 owns live control state and physical I/O.
    - Save copies the most recent M4-reported status cache from M7.
    - Load reads storage on M7 and sends settings to M4 through Command4 wrappers.
*/

#include "EROSShared.h"

#if EROS_BUILD_HAS_M7_SIDE

#include <Arduino.h>
#include <BlockDevice.h>
#include <MBRBlockDevice.h>
#include <FATFileSystem.h>
#include <string.h>

#define M7_SETTINGS_VERSION 19
#define M7_SETTINGS_MAGIC   0x45524F53UL  // "EROS"

#define M7_SETTINGS_MOUNT_NAME "fs"
#define M7_SETTINGS_FILE_PATH  "/fs/eros_mkvi_settings_m7.bin"

static bool g_m7SettingsStorageInitialized = false;
static int g_m7SettingsLastError = 0;
static int g_m7SettingsMountedPartition = -1;
static byte g_m7SettingsLastAction = EROS_SETTINGS_ACTION_NONE;
static bool g_m7SettingsLastOk = false;
static unsigned long g_m7SettingsResultCounter = 0;

static mbed::BlockDevice * g_m7QspiRoot = NULL;
static mbed::MBRBlockDevice * g_m7SettingsPartition = NULL;
static mbed::FATFileSystem g_m7SettingsFS(M7_SETTINGS_MOUNT_NAME);

struct EROS_M7PersistedSettings
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  byte mode;

  int hitachiModeOff;
  int hitachiModeOn;
  int hitachiSetPointOff;
  int hitachiSetPointOn;
  int hitachiPeriodOff;
  int hitachiPeriodOn;
  bool hitachiPeriodPreciseOff;
  bool hitachiPeriodPreciseOn;
  int hitachiMaxValueOff;
  int hitachiMaxValueOn;
  int hitachiMinValueOff;
  int hitachiMinValueOn;
  int hitachiMinRelayValue;

  unsigned int autoRunDuration;
  unsigned int autoPauseDuration;
  unsigned int autoPenaltyDuration;
  unsigned int autoIoOnTimeMs;
  unsigned int autoIoOffTimeMs;

  byte autoOutputMode[OutSize];
  int autoOutputInputIndex[OutSize];

  uint32_t checksum;
};

// Functions provided by EROSBridgeM7.ino.
byte State_GetMode();
int State_GetHitachiMode(bool onSettings);
int State_GetHitachiSetPoint(bool onSettings);
int State_GetHitachiMaxValue(bool onSettings);
int State_GetHitachiMinValue(bool onSettings);
int State_GetHitachiPeriod(bool onSettings);
bool State_GetHitachiPeriodPrecise(bool onSettings);
int State_GetHitachiMinRelayValue();
unsigned int State_GetAutoRunDurationSeconds();
unsigned int State_GetAutoPauseDurationSeconds();
unsigned int State_GetAutoPenaltyDurationSeconds();
unsigned int State_GetAutoIoOnTimeMs();
unsigned int State_GetAutoIoOffTimeMs();
byte State_GetAutoOutputMode(int outputIndex);
int State_GetAutoOutputInputIndex(int outputIndex);

void Command_SetMode(byte mode);
void Command_SetHitachiMode(bool onSettings, int mode);
void Command_SetHitachiSetPoint(bool onSettings, int value);
void Command_SetHitachiMaxValue(bool onSettings, int value);
void Command_SetHitachiMinValue(bool onSettings, int value);
void Command_SetHitachiPeriod(bool onSettings, int periodMs);
void Command_SetHitachiPeriodPrecise(bool onSettings, bool precise);
void Command_SetHitachiMinRelayValue(int value);
void Command_SetAutoRunDurationMinutes(unsigned int minutes);
void Command_SetAutoPauseDurationSeconds(unsigned int seconds);
void Command_SetAutoPenaltyDurationSeconds(unsigned int seconds);
void Command_SetAutoIoOnTimeMs(unsigned int ms);
void Command_SetAutoIoOffTimeMs(unsigned int ms);
void Command_SetAutoOutputMode(int outputIndex, byte mode);
void Command_SetAutoOutputInputIndex(int outputIndex, int inputIndex);

static void SettingsM7_RecordResult(byte action, bool ok, int error)
{
  g_m7SettingsLastAction = action;
  g_m7SettingsLastOk = ok;
  g_m7SettingsLastError = error;
  g_m7SettingsResultCounter++;
}

int SettingsM7_GetLastError()
{
  return g_m7SettingsLastError;
}

byte SettingsM7_GetLastAction()
{
  return g_m7SettingsLastAction;
}

bool SettingsM7_GetLastOk()
{
  return g_m7SettingsLastOk;
}

unsigned long SettingsM7_GetResultCounter()
{
  return g_m7SettingsResultCounter;
}

bool SettingsM7_Begin()
{
  if (g_m7SettingsStorageInitialized) {
    return true;
  }

  g_m7QspiRoot = mbed::BlockDevice::get_default_instance();

  if (g_m7QspiRoot == NULL) {
    g_m7SettingsLastError = -30001;
    Serial.println("M7 Settings: no default QSPI block device.");
    return false;
  }

  for (int partitionNumber = 1; partitionNumber <= 4; partitionNumber++) {
    Serial.print("M7 Settings: trying QSPI partition ");
    Serial.println(partitionNumber);

    mbed::MBRBlockDevice * testPartition =
      new mbed::MBRBlockDevice(g_m7QspiRoot, partitionNumber);

    if (testPartition == NULL) {
      continue;
    }

    int result = testPartition->init();

    if (result != 0) {
      Serial.print("M7 Settings: partition init failed for partition ");
      Serial.print(partitionNumber);
      Serial.print(", error: ");
      Serial.println(result);
      delete testPartition;
      continue;
    }

    result = g_m7SettingsFS.mount(testPartition);

    if (result == 0) {
      g_m7SettingsPartition = testPartition;
      g_m7SettingsStorageInitialized = true;
      g_m7SettingsLastError = 0;
      g_m7SettingsMountedPartition = partitionNumber;

      Serial.print("M7 Settings: mounted QSPI FAT filesystem on partition ");
      Serial.println(partitionNumber);
      return true;
    }

    Serial.print("M7 Settings: FAT mount failed for partition ");
    Serial.print(partitionNumber);
    Serial.print(", error: ");
    Serial.println(result);

    testPartition->deinit();
    delete testPartition;
  }

  g_m7SettingsLastError = -30002;
  Serial.println("M7 Settings: no mountable QSPI FAT partition found.");
  return false;
}

static uint32_t SettingsM7_CalculateChecksum(const struct EROS_M7PersistedSettings & settings)
{
  const uint8_t * data = (const uint8_t *)&settings;
  size_t len = sizeof(struct EROS_M7PersistedSettings) - sizeof(settings.checksum);

  uint32_t hash = 2166136261UL;

  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= 16777619UL;
  }

  return hash;
}

static void SettingsM7_CopyFromStatus(struct EROS_M7PersistedSettings & settings)
{
  memset(&settings, 0, sizeof(settings));

  settings.magic = M7_SETTINGS_MAGIC;
  settings.version = M7_SETTINGS_VERSION;
  settings.size = sizeof(struct EROS_M7PersistedSettings);

  settings.mode = State_GetMode();

  settings.hitachiModeOff = State_GetHitachiMode(false);
  settings.hitachiModeOn = State_GetHitachiMode(true);
  settings.hitachiSetPointOff = State_GetHitachiSetPoint(false);
  settings.hitachiSetPointOn = State_GetHitachiSetPoint(true);
  settings.hitachiPeriodOff = State_GetHitachiPeriod(false);
  settings.hitachiPeriodOn = State_GetHitachiPeriod(true);
  settings.hitachiPeriodPreciseOff = State_GetHitachiPeriodPrecise(false);
  settings.hitachiPeriodPreciseOn = State_GetHitachiPeriodPrecise(true);
  settings.hitachiMaxValueOff = State_GetHitachiMaxValue(false);
  settings.hitachiMaxValueOn = State_GetHitachiMaxValue(true);
  settings.hitachiMinValueOff = State_GetHitachiMinValue(false);
  settings.hitachiMinValueOn = State_GetHitachiMinValue(true);
  settings.hitachiMinRelayValue = State_GetHitachiMinRelayValue();

  settings.autoRunDuration = State_GetAutoRunDurationSeconds();
  settings.autoPauseDuration = State_GetAutoPauseDurationSeconds();
  settings.autoPenaltyDuration = State_GetAutoPenaltyDurationSeconds();
  settings.autoIoOnTimeMs = State_GetAutoIoOnTimeMs();
  settings.autoIoOffTimeMs = State_GetAutoIoOffTimeMs();

  for (int i = 0; i < OutSize; i++) {
    settings.autoOutputMode[i] = State_GetAutoOutputMode(i);
    settings.autoOutputInputIndex[i] = State_GetAutoOutputInputIndex(i);
  }

  settings.checksum = SettingsM7_CalculateChecksum(settings);
}

static bool SettingsM7_IsValid(const struct EROS_M7PersistedSettings & settings)
{
  if (settings.magic != M7_SETTINGS_MAGIC) {
    return false;
  }

  if (settings.version != M7_SETTINGS_VERSION) {
    return false;
  }

  if (settings.size != sizeof(struct EROS_M7PersistedSettings)) {
    return false;
  }

  return settings.checksum == SettingsM7_CalculateChecksum(settings);
}

static void SettingsM7_ApplyToM4(const struct EROS_M7PersistedSettings & settings)
{
  Command_SetMode(settings.mode);

  Command_SetHitachiMode(false, settings.hitachiModeOff);
  Command_SetHitachiMode(true, settings.hitachiModeOn);

  Command_SetHitachiSetPoint(false, constrain(settings.hitachiSetPointOff, 25, 100));
  Command_SetHitachiSetPoint(true, constrain(settings.hitachiSetPointOn, 25, 100));

  Command_SetHitachiMaxValue(false, constrain(settings.hitachiMaxValueOff, 25, 100));
  Command_SetHitachiMaxValue(true, constrain(settings.hitachiMaxValueOn, 25, 100));

  Command_SetHitachiMinValue(false, constrain(settings.hitachiMinValueOff, 25, 100));
  Command_SetHitachiMinValue(true, constrain(settings.hitachiMinValueOn, 25, 100));

  Command_SetHitachiPeriod(false, constrain(settings.hitachiPeriodOff, 100, 300000));
  Command_SetHitachiPeriod(true, constrain(settings.hitachiPeriodOn, 100, 300000));

  Command_SetHitachiPeriodPrecise(false, settings.hitachiPeriodPreciseOff ? true : false);
  Command_SetHitachiPeriodPrecise(true, settings.hitachiPeriodPreciseOn ? true : false);

  Command_SetHitachiMinRelayValue(constrain(settings.hitachiMinRelayValue, 0, 100));

  Command_SetAutoRunDurationMinutes(constrain((int)(settings.autoRunDuration / 60), 1, 300));
  Command_SetAutoPauseDurationSeconds(constrain((int)settings.autoPauseDuration, 0, 300));
  Command_SetAutoPenaltyDurationSeconds(constrain((int)settings.autoPenaltyDuration, 0, 300));
  Command_SetAutoIoOnTimeMs(constrain((int)settings.autoIoOnTimeMs, 0, 100000));
  Command_SetAutoIoOffTimeMs(constrain((int)settings.autoIoOffTimeMs, 0, 100000));

  for (int i = 0; i < OutSize; i++) {
    Command_SetAutoOutputMode(i, constrain((int)settings.autoOutputMode[i], 0, 10));
    Command_SetAutoOutputInputIndex(i, constrain(settings.autoOutputInputIndex[i], 0, AssignableInSize - 1));
  }
}

bool SettingsM7_SaveAll()
{
  if (!SettingsM7_Begin()) {
    Serial.println("M7 Settings: save failed because storage init failed.");
    SettingsM7_RecordResult(EROS_SETTINGS_ACTION_SAVE, false, g_m7SettingsLastError);
    return false;
  }

  struct EROS_M7PersistedSettings settings;
  SettingsM7_CopyFromStatus(settings);

  FILE * file = fopen(M7_SETTINGS_FILE_PATH, "wb");

  if (file == NULL) {
    SettingsM7_RecordResult(EROS_SETTINGS_ACTION_SAVE, false, -30003);
    Serial.println("M7 Settings: failed to open file for writing.");
    return false;
  }

  size_t written = fwrite(&settings, 1, sizeof(settings), file);
  fflush(file);
  fclose(file);

  if (written != sizeof(settings)) {
    SettingsM7_RecordResult(EROS_SETTINGS_ACTION_SAVE, false, -30004);
    Serial.print("M7 Settings: short write. Written bytes: ");
    Serial.println(written);
    return false;
  }

  g_m7SettingsLastError = 0;
  SettingsM7_RecordResult(EROS_SETTINGS_ACTION_SAVE, true, 0);

  Serial.print("M7 Settings: saved to QSPI partition ");
  Serial.println(g_m7SettingsMountedPartition);

  return true;
}

bool SettingsM7_LoadAll()
{
  if (!SettingsM7_Begin()) {
    Serial.println("M7 Settings: load failed because storage init failed.");
    SettingsM7_RecordResult(EROS_SETTINGS_ACTION_LOAD, false, g_m7SettingsLastError);
    return false;
  }

  FILE * file = fopen(M7_SETTINGS_FILE_PATH, "rb");

  if (file == NULL) {
    SettingsM7_RecordResult(EROS_SETTINGS_ACTION_LOAD, false, -30005);
    Serial.println("M7 Settings: file not found.");
    return false;
  }

  struct EROS_M7PersistedSettings settings;
  memset(&settings, 0, sizeof(settings));

  size_t bytesRead = fread(&settings, 1, sizeof(settings), file);
  fclose(file);

  if (bytesRead != sizeof(settings)) {
    SettingsM7_RecordResult(EROS_SETTINGS_ACTION_LOAD, false, -30006);
    Serial.print("M7 Settings: short read. Bytes read: ");
    Serial.println(bytesRead);
    return false;
  }

  if (!SettingsM7_IsValid(settings)) {
    SettingsM7_RecordResult(EROS_SETTINGS_ACTION_LOAD, false, -30007);
    Serial.println("M7 Settings: invalid checksum/version/magic.");
    return false;
  }

  SettingsM7_ApplyToM4(settings);

  g_m7SettingsLastError = 0;
  SettingsM7_RecordResult(EROS_SETTINGS_ACTION_LOAD, true, 0);

  Serial.print("M7 Settings: loaded from QSPI partition ");
  Serial.println(g_m7SettingsMountedPartition);

  return true;
}

#endif  // EROS_BUILD_HAS_M7_SIDE
