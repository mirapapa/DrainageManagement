#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include "esp_sntp.h"
#include <ArduinoOTA.h>
#include "esp_ota_ops.h"
#include <LiquidCrystal_I2C.h>
#include <ESPmDNS.h>

// --- 構造体の定義 ---
typedef struct
{
    bool send_flg;
    String data;
    int last_http_code;
} SENDSSDATATOSS;

// --- 全ての自作関数のプロトタイプ宣言 ---

// ログ関連
void logServer_task(void *pvParameters);
void logprintln(String log);
void logprintln(String log, bool historyFlg);
void logServersetup();
uint nextnum(uint num);
uint prevnum(uint num);
String getHistoryData();

// ネットワーク関連
void ntp_setup();
int wifisetup();
void wificheck();
void mdnssetup();
void timeavailable(struct timeval *t);
bool isWiFiReallyConnected(); // 新規追加

// OTA関連
void ota_setup();
void verifyFirmware();

// セマフォ関連
void takeSemaphore(SemaphoreHandle_t xSemaphore);
void giveSemaphore(SemaphoreHandle_t xSemaphore);

// 超音波センサ関連
void ultrasonicSensor_setup();
int measureDistance();
void ultrasonicSensor_Task(void *pvParameters);

// スプレッドシート関連
void spreadSheetsetup();
void spreadsheet_task(void *pvParameters);
void sendSpreadsheet(const String &data);

// --- 全ての外部変数の宣言 ---
extern SENDSSDATATOSS sendHDatatoSS;
extern SemaphoreHandle_t xHistorySemaphore; // 履歴データ(Web表示)用
extern SemaphoreHandle_t xDataSemaphore;    // 送信データ(SS/Ambient)用
extern bool firstTimeNtpFlg;

#endif // COMMON_H