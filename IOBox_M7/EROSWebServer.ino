#include "WiFi.h"
#include "EROSWebConfig.h"

// Stage 1 is intentionally read-only. No HTTP route calls Command_*.

static WiFiServer g_webServer(EROS_WEB_PORT);
static WiFiClient g_webClient;
static bool g_webInitialized = false;
static bool g_webListening = false;
static unsigned long g_webLastConnectAttemptMs = 0;
static unsigned long g_webClientLastActivityMs = 0;
static unsigned long g_webSuspendUntilMs = 0;

static char g_webRequest[512];
static size_t g_webRequestLength = 0;
static char g_webHeader[256];
static size_t g_webHeaderLength = 0;
static char g_webJson[1024];
static const char * g_webResponseBody = NULL;
static size_t g_webResponseLength = 0;
static size_t g_webResponseOffset = 0;

enum EROSWebClientState {
  WEB_CLIENT_IDLE,
  WEB_CLIENT_READING,
  WEB_CLIENT_SENDING_HEADER,
  WEB_CLIENT_SENDING_BODY
};

static EROSWebClientState g_webClientState = WEB_CLIENT_IDLE;

static const char EROS_WEB_PAGE[] =
"<!doctype html><html lang=en><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'>"
"<title>EROS Mk VI</title><style>"
":root{color-scheme:dark;--bg:#0b1014;--card:#151d23;--line:#293740;--on:#1fd17b;--off:#65737b;--accent:#55b8ff}"
"*{box-sizing:border-box}body{margin:0;background:var(--bg);color:#eef5f8;font:16px system-ui,-apple-system,sans-serif}"
"header{position:sticky;top:0;padding:16px 18px;background:#10171ccc;border-bottom:1px solid var(--line);backdrop-filter:blur(8px)}"
"h1{font-size:21px;margin:0}.sub{color:#9fb0ba;font-size:13px;margin-top:3px}"
"main{max-width:760px;margin:auto;padding:14px;display:grid;gap:12px}.card{background:var(--card);border:1px solid var(--line);border-radius:15px;padding:15px}"
"h2{font-size:15px;margin:0 0 12px;color:#bcd0db;text-transform:uppercase;letter-spacing:.08em}"
".grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}.item{display:flex;align-items:center;gap:9px;padding:10px;background:#10171c;border-radius:10px}"
".dot{width:13px;height:13px;border-radius:50%;background:var(--off);box-shadow:0 0 0 3px #ffffff0b}.dot.on{background:var(--on);box-shadow:0 0 12px #1fd17b88}"
".value{font-size:28px;font-weight:700}.row{display:flex;justify-content:space-between;gap:10px;margin:8px 0;color:#cbd8de}.ok{color:var(--on)}.bad{color:#ff7777}"
".readonly{float:right;padding:5px 9px;border:1px solid #527388;border-radius:999px;color:#9fd4f5;font-size:11px}"
"@media(min-width:620px){main{grid-template-columns:1fr 1fr}.wide{grid-column:1/-1}.grid{grid-template-columns:repeat(3,minmax(0,1fr))}}"
"</style><header><span class=readonly>READ ONLY</span><h1>EROS Mk VI</h1><div class=sub id=conn>Connecting…</div></header>"
"<main><section class='card wide'><h2>System</h2><div class=row><span>Mode</span><strong id=mode>—</strong></div>"
"<div class=row><span>M4 status</span><strong id=health>—</strong></div></section>"
"<section class=card><h2>Inputs</h2><div class=grid id=inputs></div></section>"
"<section class=card><h2>Outputs</h2><div class=grid id=outputs></div></section>"
"<section class=card><h2>Auto</h2><div class=row><span>Status</span><strong id=autoState>—</strong></div>"
"<div class=row><span>Remaining</span><strong id=remaining>—</strong></div></section>"
"<section class=card><h2>Hitachi</h2><div class=value><span id=hitachi>—</span>%</div></section></main>"
"<script>"
"const ins=['Start','Stop','Pause','Input 1','Input 2','Input 3'];"
"const outs=['Lock 1','Lock 2','AC','Dimmer','Dry 1','Dry 2','Dry 3','Dry 4','Hitachi'];"
"function bits(el,names,a,b=0){el.innerHTML=names.map((n,i)=>{let on=i<a?((b>>i)&1):((arguments[4]>>(i-a))&1);return `<div class=item><i class='dot ${on?'on':''}'></i><span>${n}</span></div>`}).join('')}"
"function time(s){s=Number(s)||0;return `${Math.floor(s/60)}:${String(s%60).padStart(2,'0')}`}"
"let pollTimer=0;"
"async function poll(){clearTimeout(pollTimer);try{let r=await fetch('/api/state',{cache:'no-store'});if(!r.ok)throw 0;let d=await r.json();"
"conn.textContent=`Live · ${new Date().toLocaleTimeString()}`;conn.className='sub ok';mode.textContent=d.mode?'Auto':'Manual';"
"health.textContent=d.fresh&&d.loopback?'Online':'Stale';health.className=d.fresh&&d.loopback?'ok':'bad';"
"bits(inputs,ins,3,d.inputs,d.assignableInputs);bits(outputs,outs,9,d.outputs);"
"autoState.textContent=!d.autoRunning?'Stopped':d.autoPaused?'Paused':'Running';remaining.textContent=time(d.autoRemaining);hitachi.textContent=d.hitachiOutput;"
"}catch(e){conn.textContent='Disconnected';conn.className='sub bad';health.textContent='Offline';health.className='bad'}"
"finally{pollTimer=setTimeout(poll,document.hidden?5000:500)}}"
"document.addEventListener('visibilitychange',()=>{clearTimeout(pollTimer);pollTimer=setTimeout(poll,document.hidden?5000:100)});"
"poll();</script></html>";

static void EROSWeb_CloseClient()
{
  if (g_webClient) {
    g_webClient.stop();
  }
  g_webClientState = WEB_CLIENT_IDLE;
  g_webRequestLength = 0;
  g_webHeaderLength = 0;
  g_webResponseBody = NULL;
  g_webResponseLength = 0;
  g_webResponseOffset = 0;
}

void EROSWebServer_SuspendFor(unsigned long durationMs)
{
  const unsigned long requestedUntilMs = millis() + durationMs;

  if ((long)(requestedUntilMs - g_webSuspendUntilMs) > 0) {
    g_webSuspendUntilMs = requestedUntilMs;
  }

  // This function is called only after lv_timer_handler() has returned.
  // Releasing the current socket before a heavy screen load keeps the Wi-Fi
  // worker from allocating/freeing TCP buffers during LVGL navigation.
  EROSWeb_CloseClient();
}

static int EROSWeb_OutputMask()
{
  int mask = 0;
  for (int i = 0; i < OutSize; i++) {
    if (State_GetOutput(i)) mask |= (1 << i);
  }
  return mask;
}

static int EROSWeb_InputMask()
{
  int mask = 0;
  for (int i = 0; i < InSize; i++) {
    if (State_GetInput(i)) mask |= (1 << i);
  }
  return mask;
}

static int EROSWeb_AssignableInputMask()
{
  int mask = 0;
  for (int i = 0; i < AssignableInSize; i++) {
    if (State_GetAssignableInput(i)) mask |= (1 << i);
  }
  return mask;
}

static void EROSWeb_BeginResponse(
  int statusCode,
  const char * statusText,
  const char * contentType,
  const char * body,
  size_t bodyLength
)
{
  g_webResponseBody = body;
  g_webResponseLength = bodyLength;
  g_webResponseOffset = 0;
  g_webHeaderLength = (size_t)snprintf(
    g_webHeader,
    sizeof(g_webHeader),
    "HTTP/1.1 %d %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %lu\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n",
    statusCode,
    statusText,
    contentType,
    (unsigned long)bodyLength
  );
  g_webClientState = WEB_CLIENT_SENDING_HEADER;
}

static void EROSWeb_RouteRequest()
{
  if (strncmp(g_webRequest, "GET /api/state ", 15) == 0) {
    const int length = snprintf(
      g_webJson,
      sizeof(g_webJson),
      "{\"mode\":%u,\"inputs\":%d,\"assignableInputs\":%d,"
      "\"outputs\":%d,\"autoRunning\":%s,\"autoPaused\":%s,"
      "\"autoRemaining\":%u,\"hitachiOutput\":%d,"
      "\"fresh\":%s,\"loopback\":%s,\"statusAgeMs\":%lu}",
      (unsigned int)State_GetMode(),
      EROSWeb_InputMask(),
      EROSWeb_AssignableInputMask(),
      EROSWeb_OutputMask(),
      State_GetAutoRunning() ? "true" : "false",
      State_GetAutoPaused() ? "true" : "false",
      State_GetAutoRemainingTime(),
      State_GetHitachiCurrentOutput(),
      State_IsTransportStatusFresh(1000) ? "true" : "false",
      State_GetTransportLoopbackOk() ? "true" : "false",
      State_GetTransportStatusAgeMs()
    );
    EROSWeb_BeginResponse(200, "OK", "application/json", g_webJson, (size_t)length);
  }
  else if (strncmp(g_webRequest, "GET / ", 6) == 0 ||
           strncmp(g_webRequest, "GET /index.html ", 16) == 0) {
    EROSWeb_BeginResponse(
      200, "OK", "text/html; charset=utf-8",
      EROS_WEB_PAGE, strlen(EROS_WEB_PAGE)
    );
  }
  else {
    static const char notFound[] = "Not found";
    EROSWeb_BeginResponse(
      404, "Not Found", "text/plain; charset=utf-8",
      notFound, sizeof(notFound) - 1
    );
  }
}

void EROSWebServer_Setup()
{
#if !EROS_WEB_ENABLED
  return;
#endif

  if (g_webInitialized) return;
  g_webInitialized = true;

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WEB: Wi-Fi module not available");
    return;
  }

  if (strcmp(EROS_WIFI_SSID, "CHANGE_ME") == 0) {
    Serial.println("WEB: set Wi-Fi credentials in EROSWebConfig.h");
    return;
  }

  g_webLastConnectAttemptMs = millis() - EROS_WEB_RECONNECT_INTERVAL_MS;
}

static void EROSWeb_ServiceWiFi()
{
  if (!g_webInitialized || strcmp(EROS_WIFI_SSID, "CHANGE_ME") == 0) return;

  if (WiFi.status() == WL_CONNECTED) {
    if (!g_webListening) {
      g_webServer.begin();
      g_webListening = true;
      Serial.print("WEB: ready at http://");
      Serial.println(WiFi.localIP());
    }
    return;
  }

  g_webListening = false;
  const unsigned long nowMs = millis();
  if (nowMs - g_webLastConnectAttemptMs < EROS_WEB_RECONNECT_INTERVAL_MS) return;

  g_webLastConnectAttemptMs = nowMs;
  Serial.print("WEB: connecting to ");
  Serial.println(EROS_WIFI_SSID);
  WiFi.begin(EROS_WIFI_SSID, EROS_WIFI_PASSWORD);
}

void EROSWebServer_Task()
{
#if !EROS_WEB_ENABLED
  return;
#endif

  if ((long)(g_webSuspendUntilMs - millis()) > 0) {
    EROSWeb_CloseClient();
    return;
  }

  EROSWeb_ServiceWiFi();
  if (!g_webListening) return;

  if (g_webClientState == WEB_CLIENT_IDLE) {
    WiFiClient incoming = g_webServer.accept();
    if (!incoming) return;
    g_webClient = incoming;
    g_webClientState = WEB_CLIENT_READING;
    g_webClientLastActivityMs = millis();
    g_webRequestLength = 0;
  }

  if (!g_webClient || !g_webClient.connected()) {
    EROSWeb_CloseClient();
    return;
  }

  if (millis() - g_webClientLastActivityMs > EROS_WEB_CLIENT_TIMEOUT_MS) {
    EROSWeb_CloseClient();
    return;
  }

  if (g_webClientState == WEB_CLIENT_READING) {
    int budget = 64;
    while (budget-- > 0 && g_webClient.available()) {
      const char c = (char)g_webClient.read();
      g_webClientLastActivityMs = millis();
      if (g_webRequestLength >= sizeof(g_webRequest) - 1) {
        EROSWeb_CloseClient();
        return;
      }
      g_webRequest[g_webRequestLength++] = c;
      g_webRequest[g_webRequestLength] = '\0';
      if (strstr(g_webRequest, "\r\n\r\n") != NULL) {
        EROSWeb_RouteRequest();
        break;
      }
    }
    return;
  }

  const char * source = NULL;
  size_t remaining = 0;
  if (g_webClientState == WEB_CLIENT_SENDING_HEADER) {
    source = g_webHeader + g_webResponseOffset;
    remaining = g_webHeaderLength - g_webResponseOffset;
  }
  else {
    source = g_webResponseBody + g_webResponseOffset;
    remaining = g_webResponseLength - g_webResponseOffset;
  }

  const size_t chunk = remaining > 256 ? 256 : remaining;
  if (chunk > 0) {
    const size_t written = g_webClient.write((const uint8_t *)source, chunk);
    g_webResponseOffset += written;
    g_webClientLastActivityMs = millis();
  }

  if (g_webResponseOffset >=
      (g_webClientState == WEB_CLIENT_SENDING_HEADER
        ? g_webHeaderLength : g_webResponseLength)) {
    if (g_webClientState == WEB_CLIENT_SENDING_HEADER) {
      g_webClientState = WEB_CLIENT_SENDING_BODY;
      g_webResponseOffset = 0;
    }
    else {
      EROSWeb_CloseClient();
    }
  }
}
