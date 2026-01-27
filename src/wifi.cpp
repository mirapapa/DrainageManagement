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
unsigned long lastConnectedTime = 0;
const unsigned long MAX_DISCONNECTED_TIME = 300000; // 5分間切断が続いたら再起動

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
    logprintln("Gateway: " + WiFi.gatewayIP().toString());
    logprintln("RSSI: " + String(WiFi.RSSI()) + " dBm");
    lastConnectedTime = millis();
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

bool isWiFiReallyConnected()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  // ゲートウェイIPが有効かチェック
  IPAddress gw = WiFi.gatewayIP();
  if (gw[0] == 0)
  {
    logprintln("⚠ WiFi接続中だがゲートウェイIPが無効");
    return false;
  }

  // RSSI値をチェック（-90dBm以下は実質使用不可）
  int rssi = WiFi.RSSI();
  if (rssi < -90)
  {
    logprintln("⚠ WiFi電波強度が極端に弱い: " + String(rssi) + " dBm");
    return false;
  }

  return true;
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
      lastConnectedTime = millis();
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

  // 接続状態の場合、最終接続時刻を更新
  if (WiFi.status() == WL_CONNECTED)
  {
    lastConnectedTime = currentMillis;
  }
  else
  {
    // 長時間切断が続いている場合は再起動
    if (currentMillis - lastConnectedTime > MAX_DISCONNECTED_TIME)
    {
      logprintln("⚠ WiFi切断が5分以上継続 - 再起動します");
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
    }
  }

  // WiFi切断時の自動再接続（30秒間隔）
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
  {
    logprintln("Reconnecting to WiFi...");
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(1000));
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
}