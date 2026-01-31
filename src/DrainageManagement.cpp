#include "common.h"

TaskHandle_t thp[3];
SENDSSDATATOSS sendHDatatoSS;

void setup()
{
  Serial.begin(115200);

  // logServerのセットアップ
  logServersetup();

  logprintln("");
  logprintln("***********************************");
  logprintln("** " SYSTEM_NAME "            **");
  logprintln("**   (ver" VERSION ")                  **");
  logprintln("***********************************");
#ifdef CONFIG_APP_ROLLBACK_ENABLE
  logprintln("CONFIG_APP_ROLLBACK_ENABLE");
#endif // CONFIG_APP_ROLLBACK_ENABLE
  logprintln("");

  // Watchdog Timerのセットアップ
  watchdog_setup();

  // NTPクライアントのセットアップ
  ntp_setup();

  // wifiのセットアップ
  wifisetup();

  // 超音波センサのセットアップ
  ultrasonicSensor_setup();

  // スプレッドシート送信のセットアップ
  spreadSheetsetup();

  // タスク起動
  xTaskCreatePinnedToCore(ultrasonicSensor_Task, "ULTRASONICSENSOR_TASK", 8192, NULL, 3, &thp[0], APP_CPU_NUM);
  xTaskCreatePinnedToCore(spreadsheet_task, "SPREADSHEET_Task", 8192, NULL, 2, &thp[1], APP_CPU_NUM);
  xTaskCreatePinnedToCore(logServer_task, "LOGSERVER_TASK", 8192, NULL, 1, &thp[2], APP_CPU_NUM);

  // otaのセットアップ
  ota_setup();

  logprintln("<<排液管理システム再起動>>", 1);
}

void loop()
{
  // wifi接続判定
  wificheck();

  // OTA処理（ArduinoOTA + WEB OTA）
  ota_handle();

  delay(1000); // 100ms → 1000ms に変更（WiFi安定化のため）
}