/*
  GigaSettings.ino

  Non-volatile settings storage for EROS Mk VI on Arduino Giga R1.

  Uses QSPI flash partition + FAT filesystem.
  This avoids the global KVStore backend problem.

  IMPORTANT:
    Run File > Examples > STM32H747_System > QSPIFormat first.
    Choose a partition scheme that includes a user data partition.

  This version auto-scans QSPI MBR partitions 1-4 and mounts the first
  FAT partition that works.
*/

#include <Arduino.h>
#include <BlockDevice.h>
#include <MBRBlockDevice.h>
#include <FATFileSystem.h>
#include <string.h>

#define SETTINGS_VERSION 2
#define SETTINGS_MAGIC   0x45524F53UL  // "EROS"

#define SETTINGS_MOUNT_NAME       "fs"
#define SETTINGS_FILE_PATH        "/fs/eros_mkvi_settings.bin"

static bool g_settingsStorageInitialized = false;
static int g_settingsLastError = 0;
static int g_settingsMountedPartition = -1;

static mbed::BlockDevice * g_qspiRoot = NULL;
static mbed::MBRBlockDevice * g_settingsPartition = NULL;
static mbed::FATFileSystem g_settingsFS(SETTINGS_MOUNT_NAME);

struct EROS_PersistedSettings
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;

  // Hitachi settings
  int hitachi_modeOff;
  int hitachi_modeOn;
  int hitachi_setPointOff;
  int hitachi_setPointOn;
  int hitachi_periodOff;
  int hitachi_periodOn;
  bool hitachi_periodPreciseOff;
  bool hitachi_periodPreciseOn;
  int hitachi_maxValueOff;
  int hitachi_maxValueOn;
  int hitachi_minValueOff;
  int hitachi_minValueOn;
  int hitachi_minRelayValue;

  // Auto / timed mode settings
  int time_runDuration;
  int time_pauseDuration;
  int time_penaltyDuration;
  int time_startButton;
  int time_pauseButton;
  int time_stopButton;

  // EROSFlex settings
  int erosFlex_outInIdx[OutSize];
  int erosFlex_outMode[OutSize];
  int erosFlex_onTime;
  int erosFlex_offTime;

  uint32_t checksum;
};

int Settings_GetLastError()
{
  return g_settingsLastError;
}

int Settings_GetMountedPartition()
{
  return g_settingsMountedPartition;
}

bool Settings_Begin()
{
  if (g_settingsStorageInitialized) {
    return true;
  }

  g_qspiRoot = mbed::BlockDevice::get_default_instance();

  if (g_qspiRoot == NULL) {
    g_settingsLastError = -20001;
    Serial.println("Settings: no default QSPI block device.");
    return false;
  }

  // Try MBR partitions 1 through 4.
  // The QSPIFormat example commonly creates a FAT partition with type 0x0B.
  // We do not need to know the type here; mounting will tell us if it works.
  for (int partitionNumber = 1; partitionNumber <= 4; partitionNumber++) {
    Serial.print("Settings: trying QSPI partition ");
    Serial.println(partitionNumber);

    mbed::MBRBlockDevice * testPartition =
      new mbed::MBRBlockDevice(g_qspiRoot, partitionNumber);

    if (testPartition == NULL) {
      continue;
    }

    int result = testPartition->init();

    if (result != 0) {
      Serial.print("Settings: partition init failed for partition ");
      Serial.print(partitionNumber);
      Serial.print(", error: ");
      Serial.println(result);

      delete testPartition;
      continue;
    }

    result = g_settingsFS.mount(testPartition);

    if (result == 0) {
      g_settingsPartition = testPartition;
      g_settingsStorageInitialized = true;
      g_settingsLastError = 0;
      g_settingsMountedPartition = partitionNumber;

      Serial.print("Settings: mounted QSPI FAT filesystem on partition ");
      Serial.println(partitionNumber);

      return true;
    }

    Serial.print("Settings: FAT mount failed for partition ");
    Serial.print(partitionNumber);
    Serial.print(", error: ");
    Serial.println(result);

    testPartition->deinit();
    delete testPartition;
  }

  g_settingsLastError = -20002;
  Serial.println("Settings: no mountable QSPI FAT partition found.");
  Serial.println("Run QSPIFormat and confirm a FAT/user partition exists.");
  return false;
}

static uint32_t Settings_CalculateChecksum(const struct EROS_PersistedSettings & settings)
{
  const uint8_t * data = (const uint8_t *)&settings;
  size_t len = sizeof(struct EROS_PersistedSettings) - sizeof(settings.checksum);

  uint32_t hash = 2166136261UL;

  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= 16777619UL;
  }

  return hash;
}

static void Settings_CopyFromRuntime(struct EROS_PersistedSettings & settings)
{
  memset(&settings, 0, sizeof(settings));

  settings.magic = SETTINGS_MAGIC;
  settings.version = SETTINGS_VERSION;
  settings.size = sizeof(struct EROS_PersistedSettings);

  settings.hitachi_modeOff = hS.modeOff;
  settings.hitachi_modeOn = hS.modeOn;
  settings.hitachi_setPointOff = hS.setPointOff;
  settings.hitachi_setPointOn = hS.setPointOn;
  settings.hitachi_periodOff = hS.periodOff;
  settings.hitachi_periodOn = hS.periodOn;
  settings.hitachi_periodPreciseOff = hS.periodPreciseOff;
  settings.hitachi_periodPreciseOn = hS.periodPreciseOn;
  settings.hitachi_maxValueOff = hS.maxValueOff;
  settings.hitachi_maxValueOn = hS.maxValueOn;
  settings.hitachi_minValueOff = hS.minValueOff;
  settings.hitachi_minValueOn = hS.minValueOn;
  settings.hitachi_minRelayValue = hS.minRelayValue;

  settings.time_runDuration = TimeVar.iRunDuration;
  settings.time_pauseDuration = TimeVar.iPauseDuration;
  settings.time_penaltyDuration = TimeVar.iPenaltyDuration;
  settings.time_startButton = TimeVar.StartButton;
  settings.time_pauseButton = TimeVar.PauseButton;
  settings.time_stopButton = TimeVar.StopButton;

  for (int i = 0; i < OutSize; i++) {
    settings.erosFlex_outInIdx[i] = EROSFlexSettings.OutInIdx[i];
    settings.erosFlex_outMode[i] = EROSFlexSettings.OutMode[i];
  }

  settings.erosFlex_onTime = EROSFlexSettings.OnTime;
  settings.erosFlex_offTime = EROSFlexSettings.OffTime;

  settings.checksum = Settings_CalculateChecksum(settings);
}

static bool Settings_IsValid(const struct EROS_PersistedSettings & settings)
{
  if (settings.magic != SETTINGS_MAGIC) {
    return false;
  }

  if (settings.version != SETTINGS_VERSION) {
    return false;
  }

  if (settings.size != sizeof(struct EROS_PersistedSettings)) {
    return false;
  }

  uint32_t expectedChecksum = Settings_CalculateChecksum(settings);

  return settings.checksum == expectedChecksum;
}

static void Settings_CopyToRuntime(const struct EROS_PersistedSettings & settings)
{
  hS.modeOff = settings.hitachi_modeOff;
  hS.modeOn = settings.hitachi_modeOn;

  hS.setPointOff = constrain(settings.hitachi_setPointOff, 25, 100);
  hS.setPointOn = constrain(settings.hitachi_setPointOn, 25, 100);

  hS.periodOff = constrain(settings.hitachi_periodOff, 100, 300000);
  hS.periodOn = constrain(settings.hitachi_periodOn, 100, 300000);

  hS.periodPreciseOff = settings.hitachi_periodPreciseOff ? true : false;
  hS.periodPreciseOn = settings.hitachi_periodPreciseOn ? true : false;

  hS.maxValueOff = constrain(settings.hitachi_maxValueOff, 25, 100);
  hS.maxValueOn = constrain(settings.hitachi_maxValueOn, 25, 100);

  hS.minValueOff = constrain(settings.hitachi_minValueOff, 25, 100);
  hS.minValueOn = constrain(settings.hitachi_minValueOn, 25, 100);

  hS.minRelayValue = constrain(settings.hitachi_minRelayValue, 0, 100);

  TimeVar.iRunDuration = settings.time_runDuration;
  TimeVar.iPauseDuration = settings.time_pauseDuration;
  TimeVar.iPenaltyDuration = settings.time_penaltyDuration;
  TimeVar.StartButton = settings.time_startButton;
  TimeVar.PauseButton = settings.time_pauseButton;
  TimeVar.StopButton = settings.time_stopButton;

  //for (int i = 0; i < OutSize; i++) {
  //  EROSFlexSettings.OutInIdx[i] = settings.erosFlex_outInIdx[i];
  //  EROSFlexSettings.OutMode[i] = settings.erosFlex_outMode[i];
  //}

  for (int i = 0; i < OutSize; i++) {
    EROSFlexSettings.OutInIdx[i] = constrain(settings.erosFlex_outInIdx[i], 0, AssignableInSize - 1);
    EROSFlexSettings.OutMode[i] = constrain(settings.erosFlex_outMode[i], 0, 10);
  }

  EROSFlexSettings.OnTime = settings.erosFlex_onTime;
  EROSFlexSettings.OffTime = settings.erosFlex_offTime;
}

void Settings_LoadDefaults()
{
  hS.modeOff = hitachiOff;
  hS.modeOn = hitachiValue;

  hS.setPointOff = 25;
  hS.setPointOn = 50;

  hS.periodOff = 1000;
  hS.periodOn = 1000;

  hS.periodPreciseOff = true;
  hS.periodPreciseOn = true;

  hS.maxValueOff = 100;
  hS.maxValueOn = 100;

  hS.minValueOff = 25;
  hS.minValueOn = 25;

  hS.minRelayValue = 30;
  hS.currentOutput = 0;




  TimeVar.iRunDuration = 1800;      // 30 minutes
  TimeVar.iPauseDuration = 60;     // 60 seconds
  TimeVar.iPenaltyDuration = 120;  //120 Seconds

  EROSFlexSettings.OnTime = 1000;    // milliseconds
  EROSFlexSettings.OffTime = 1000;   // milliseconds

  for (int i = 0; i < OutSize; i++) {
    EROSFlexSettings.OutInIdx[i] = 0;
    EROSFlexSettings.OutMode[i] = 0;
  }

  EROSFlexSettings.OutMode[OUT_LOCK_1] = 1;
  EROSFlexSettings.OutMode[OUT_LOCK_2] = 1;


  TimeVar.StartButton = IN_START;
  TimeVar.StopButton  = IN_STOP;
  TimeVar.PauseButton = IN_PAUSE;

}

bool Settings_SaveAll()
{
  if (!Settings_Begin()) {
    Serial.println("Settings save failed because storage init failed.");
    return false;
  }

  struct EROS_PersistedSettings settings;
  Settings_CopyFromRuntime(settings);

  FILE * file = fopen(SETTINGS_FILE_PATH, "wb");

  if (file == NULL) {
    g_settingsLastError = -20003;
    Serial.println("Settings: failed to open file for writing.");
    return false;
  }

  size_t written = fwrite(&settings, 1, sizeof(settings), file);
  fflush(file);
  fclose(file);

  if (written != sizeof(settings)) {
    g_settingsLastError = -20004;
    Serial.print("Settings: short write. Written bytes: ");
    Serial.println(written);
    return false;
  }

  g_settingsLastError = 0;

  Serial.print("Settings saved to QSPI partition ");
  Serial.println(g_settingsMountedPartition);

  return true;
}

bool Settings_LoadAll()
{
  if (!Settings_Begin()) {
    Serial.println("Settings load failed because storage init failed.");
    return false;
  }

  FILE * file = fopen(SETTINGS_FILE_PATH, "rb");

  if (file == NULL) {
    g_settingsLastError = -20005;
    Serial.println("Settings: file not found.");
    return false;
  }

  struct EROS_PersistedSettings settings;
  memset(&settings, 0, sizeof(settings));

  size_t bytesRead = fread(&settings, 1, sizeof(settings), file);
  fclose(file);

  if (bytesRead != sizeof(settings)) {
    g_settingsLastError = -20006;
    Serial.print("Settings: short read. Bytes read: ");
    Serial.println(bytesRead);
    return false;
  }

  if (!Settings_IsValid(settings)) {
    g_settingsLastError = -20007;
    Serial.println("Settings invalid. Checksum/version/magic failed.");
    return false;
  }

  Settings_CopyToRuntime(settings);

  g_settingsLastError = 0;

  Serial.print("Settings loaded from QSPI partition ");
  Serial.println(g_settingsMountedPartition);

  return true;
}

bool Settings_LoadAllOrDefaults()
{
  if (Settings_LoadAll()) {
    return true;
  }

  Settings_LoadDefaults();
  Serial.println("Using default settings.");
  return false;
}

// ------------------------------------------------------------
// Legacy compatibility wrappers
// ------------------------------------------------------------

void LoadSettings()
{
  Settings_LoadAllOrDefaults();
}

void SaveSettings()
{
  Settings_SaveAll();
}

void WriteDefaults()
{
  Settings_LoadDefaults();
}