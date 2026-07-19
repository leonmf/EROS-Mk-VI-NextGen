#ifndef EROS_WEB_CONFIG_H
#define EROS_WEB_CONFIG_H

// Enter the Wi-Fi network used by the phone/tablet and the GIGA.
// Keep this file private if the project is committed to source control.
#define EROS_WIFI_SSID "CHANGE_ME"
#define EROS_WIFI_PASSWORD "CHANGE_ME"

#define EROS_WEB_ENABLED 1
#define EROS_WEB_DIAGNOSTIC_CONNECT_THEN_DISCONNECT 0
#define EROS_WEB_PORT 80
#define EROS_WEB_RECONNECT_INTERVAL_MS 15000UL
#define EROS_WEB_CLIENT_TIMEOUT_MS 2000UL

#endif
