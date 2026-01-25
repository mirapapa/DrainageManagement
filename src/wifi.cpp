#include "common.h"

const char ssid[] = WIFI_SSID;
const char pass[] = WIFI_PASS;
const char *host = WIFI_HOST;
const IPAddress ip WIFI_IP;
const IPAddress gateway WIFI_GATEWAY;
const IPAddress subnet WIFI_SUBNET;
const IPAddress dns1 WIFI_DNS;
uint16_t wifistatus = 0;
unsigned long previousMillis = 0;
unsigned long interval = 30000;

int wifisetup()
{
  logprintln("WiFi Connecting to " + String(ssid));

  if (!WiFi.config(ip, gateway, subnet, dns1))
  {
    logprintln("Failed to WIFI configure!");
    return 0;
  }

  WiFi.begin(ssid, pass);

  // 接続完了待機（最大10秒）
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20)
  {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
    retry++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    logprintln("WiFi Connected!");
    logprintln("IP address: " + WiFi.localIP().toString());
    return 1;
  }
  else
  {
    logprintln("WiFi Connection Failed!");
    return 0;
  }
}

void mdnssetup()
{
  if (!MDNS.begin(host))
  {
    logprintln("Error setting up MDNS responder!");
  }
  else
  {
    logprintln("MDNS responder started: " + String(host) + ".local");
  }
}

void wificheck()
{
  // WiFi状態の変化を検出
  while (WiFi.status() != wifistatus)
  {
    wifistatus = WiFi.status();
    switch (wifistatus)
    {
    case WL_NO_SHIELD:
      logprintln("WiFi NO_SHIELD.");
      break;
    case WL_IDLE_STATUS:
      logprintln("WiFi IDLE_STATUS.");
      break;
    case WL_NO_SSID_AVAIL:
      logprintln("WiFi NO_SSID_AVAIL.");
      break;
    case WL_SCAN_COMPLETED:
      logprintln("WiFi SCAN_COMPLETED.");
      break;
    case WL_CONNECTED:
      logprintln("WiFi CONNECTED.");
      logprintln("SSID: " + String(ssid));
      logprintln("AP IP address: " + WiFi.localIP().toString());
      logprintln("RSSI: " + String(WiFi.RSSI()) + " dBm");
      mdnssetup();
      break;
    case WL_CONNECT_FAILED:
      logprintln("WiFi CONNECT_FAILED.");
      break;
    case WL_CONNECTION_LOST:
      logprintln("WiFi CONNECTION_LOST.");
      break;
    case WL_DISCONNECTED:
      logprintln("WiFi DISCONNECTED.");
      break;
    default:
      logprintln("WiFi STATUS unknown: " + String(wifistatus));
    }
  }

  unsigned long currentMillis = millis();
  // WiFi切断時の自動再接続（30秒間隔）
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
  {
    logprintln("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
}