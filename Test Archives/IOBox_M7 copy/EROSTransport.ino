/*
  EROSTransport.ino

  M7-side transport shim for the EROS Mk VI bridge.

  Real split mode:
    - UI code queues command packets locally.
    - A small M7 worker thread performs the blocking RPC.call(...) so LVGL
      callbacks never hang the display if M4/RPC is not ready.
    - Receives a minimal status/loopback packet back from the M4 over RPC.

  This phase intentionally transports only the loopback/debug subset of the
  status packet. Full command/status serialization comes after the pipe is
  proven stable.
*/

#include "EROSShared.h"
#include "RPC.h"
#include "SerialRPC.h"
#include <mbed.h>

#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
bool EROSM4_ReceiveCommandFromTransport(const EROS_Command & command);
#endif

void State_ApplyControlStatus(const EROS_ControlStatus & status);

static bool g_transportInitialized = false;

#if EROS_BUILD_M7_CORE
static const int EROS_M7_TRANSPORT_QUEUE_SIZE = 8;
static EROS_Command g_m7TransportQueue[EROS_M7_TRANSPORT_QUEUE_SIZE];
static volatile int g_m7TransportQueueHead = 0;
static volatile int g_m7TransportQueueTail = 0;
static volatile int g_m7TransportQueueCount = 0;
static volatile bool g_m7TransportWorkerStarted = false;

static rtos::Mutex g_m7TransportMutex;
static rtos::Thread g_m7TransportThread(osPriorityNormal, 8192);

static bool EROSTransport_M7QueuePush(const EROS_Command & command)
{
  bool ok = false;

  g_m7TransportMutex.lock();
  if (g_m7TransportQueueCount < EROS_M7_TRANSPORT_QUEUE_SIZE)
  {
    g_m7TransportQueue[g_m7TransportQueueTail] = command;
    g_m7TransportQueueTail = (g_m7TransportQueueTail + 1) % EROS_M7_TRANSPORT_QUEUE_SIZE;
    g_m7TransportQueueCount++;
    ok = true;
  }
  g_m7TransportMutex.unlock();

  return ok;
}

static bool EROSTransport_M7QueuePop(EROS_Command & command)
{
  bool ok = false;

  g_m7TransportMutex.lock();
  if (g_m7TransportQueueCount > 0)
  {
    command = g_m7TransportQueue[g_m7TransportQueueHead];
    g_m7TransportQueueHead = (g_m7TransportQueueHead + 1) % EROS_M7_TRANSPORT_QUEUE_SIZE;
    g_m7TransportQueueCount--;
    ok = true;
  }
  g_m7TransportMutex.unlock();

  return ok;
}

static int EROSTransport_M7ReadM4StatusValue(int selector)
{
  return RPC.call("EROS_M4_GetStatusValue", selector).as<int>();
}

static void EROSTransport_M7PollM4Status()
{
  // This runs only in the M7 transport worker thread. If RPC is not ready, the
  // worker may stall, but the LVGL/UI thread remains responsive.
  EROS_ControlStatus status;
  memset(&status, 0, sizeof(status));

  status.transportLoopbackRequestId = (unsigned long)EROSTransport_M7ReadM4StatusValue(0);
  status.transportLoopbackEchoId = (unsigned long)EROSTransport_M7ReadM4StatusValue(1);
  status.transportLoopbackEchoCounter = (unsigned long)EROSTransport_M7ReadM4StatusValue(2);
  status.transportLoopbackEchoMillis = (unsigned long)EROSTransport_M7ReadM4StatusValue(3);

  status.transportStatusCounter = (unsigned long)EROSTransport_M7ReadM4StatusValue(4);
  status.transportStatusMillis = (unsigned long)EROSTransport_M7ReadM4StatusValue(5);
  status.transportCommandAcceptedCounter = (unsigned long)EROSTransport_M7ReadM4StatusValue(6);
  status.transportCommandRejectedCounter = (unsigned long)EROSTransport_M7ReadM4StatusValue(7);
  status.transportCommandQueueDepth = (byte)EROSTransport_M7ReadM4StatusValue(8);
  status.transportCommandQueueCapacity = (byte)EROSTransport_M7ReadM4StatusValue(9);

  State_ApplyControlStatus(status);
}

static void EROSTransport_M7Worker()
{
  unsigned long lastStatusPollMs = 0;

  for (;;)
  {
    EROS_Command command;
    if (EROSTransport_M7QueuePop(command))
    {
      // RPC.call(...) can block if the M4 side is not ready. That is why this
      // lives in this worker thread instead of an LVGL callback.
      int accepted = RPC.call(
        "EROS_M4_Command",
        (int)command.type,
        command.index,
        (int)command.value,
        command.boolValue ? 1 : 0,
        command.onSettings ? 1 : 0
      ).as<int>();

      // Pull status after every command. This avoids needing the M4 to call
      // back into an M7 RPC function, which is a more fragile first bridge.
      if (accepted != 0)
      {
        delay(10);
        EROSTransport_M7PollM4Status();
        lastStatusPollMs = millis();
      }
    }
    else
    {
      if (millis() - lastStatusPollMs >= 500)
      {
        EROSTransport_M7PollM4Status();
        lastStatusPollMs = millis();
      }
      delay(10);
    }
  }
}

static void EROSTransport_StartM7Worker()
{
  if (!g_m7TransportWorkerStarted)
  {
    g_m7TransportWorkerStarted = true;
    g_m7TransportThread.start(EROSTransport_M7Worker);
  }
}
#endif

int EROSTransport_RPCReceiveStatusLoopback(
  int requestId,
  int echoId,
  int echoCounter,
  int echoMillis,
  int statusCounter,
  int m4Accepted,
  int m4Rejected,
  int queueDepth,
  int queueCapacity
)
{
  EROS_ControlStatus status;
  memset(&status, 0, sizeof(status));

  status.transportLoopbackRequestId = (unsigned long)requestId;
  status.transportLoopbackEchoId = (unsigned long)echoId;
  status.transportLoopbackEchoCounter = (unsigned long)echoCounter;
  status.transportLoopbackEchoMillis = (unsigned long)echoMillis;

  status.transportStatusCounter = (unsigned long)statusCounter;
  status.transportStatusMillis = (unsigned long)echoMillis;
  status.transportCommandAcceptedCounter = (unsigned long)m4Accepted;
  status.transportCommandRejectedCounter = (unsigned long)m4Rejected;
  status.transportCommandQueueDepth = (byte)queueDepth;
  status.transportCommandQueueCapacity = (byte)queueCapacity;

  State_ApplyControlStatus(status);
  return 1;
}

void EROSTransport_Setup()
{
#if EROS_BUILD_M7_CORE
  if (g_transportInitialized)
  {
    return;
  }

  // SerialRPC initializes the RPC transport used between the GIGA M7 and M4
  // cores. On the M7 it also starts the M4 firmware when present.
  SerialRPC.begin();
  RPC.bind("EROS_M7_StatusLoopback", EROSTransport_RPCReceiveStatusLoopback);

  EROSTransport_StartM7Worker();

  g_transportInitialized = true;
#endif
}

bool EROSTransport_SendCommandToM4(const EROS_Command & command)
{
#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
  // Single-core simulation path.
  return EROSM4_ReceiveCommandFromTransport(command);
#elif EROS_BUILD_M7_CORE
  if (!g_transportInitialized)
  {
    EROSTransport_Setup();
  }

  // Queue the command and return immediately so display callbacks cannot lock
  // the UI if the M4 RPC endpoint is not available yet.
  return EROSTransport_M7QueuePush(command);
#else
  (void)command;
  return false;
#endif
}

void EROSTransport_PublishStatusToM7(const EROS_ControlStatus & status)
{
#if EROS_BUILD_USES_IN_PROCESS_TRANSPORT
  // Single-core simulation path.
  State_ApplyControlStatus(status);
#else
  // M7 builds do not publish M4 status.
  (void)status;
#endif
}
