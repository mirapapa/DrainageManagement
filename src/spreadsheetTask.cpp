#include "common.h"

SemaphoreHandle_t xDataSemaphore;

void spreadSheetsetup()
{
  // セマフォ作成
  xDataSemaphore = xSemaphoreCreateBinary();
  if (xDataSemaphore == NULL)
  {
    Serial.println("セマフォ取得失敗");
  }
  else
  {
    xSemaphoreGive(xDataSemaphore);
  }
}

void spreadsheet_task(void *pvParameters)
{
  logprintln("spreadSheet_Task START!!");
  vTaskDelay(pdMS_TO_TICKS(5000));

  // Task WDTに登録
  watchdog_subscribe_task("SPREADSHEET_TASK");

  // 送信フラグ初期化
  takeSemaphore(xDataSemaphore);
  sendHDatatoSS.send_flg = 0;
  giveSemaphore(xDataSemaphore);

  unsigned long lastAttemptTime = 0;
  const unsigned long SEND_TIMEOUT = 25000; // 25秒でタイムアウト
  int consecutiveFailures = 0;

  while (1)
  {
    // WDTリセット（ループの最初で実行）
    watchdog_reset();

    vTaskDelay(pdMS_TO_TICKS(1000));

    // セマフォで保護しながらフラグとデータをコピー
    takeSemaphore(xDataSemaphore);
    bool shouldSend = sendHDatatoSS.send_flg;
    String dataCopy = sendHDatatoSS.data;
    giveSemaphore(xDataSemaphore);

    if (shouldSend)
    {
      logprintln("[SPREADSHEET] 送信開始準備");

      // WiFi接続確認
      if (WiFi.status() != WL_CONNECTED)
      {
        logprintln("⚠ [SPREADSHEET] WiFi切断中のため送信スキップ");
        takeSemaphore(xDataSemaphore);
        sendHDatatoSS.send_flg = 0;
        sendHDatatoSS.last_http_code = -1;
        giveSemaphore(xDataSemaphore);
        consecutiveFailures++;

        logprintln("[SPREADSHEET] 連続失敗カウント: " + String(consecutiveFailures) + "/5");

        // 連続5回失敗したら再起動
        if (consecutiveFailures >= 5)
        {
          logprintln("⚠ [SPREADSHEET] 連続送信失敗5回 - 再起動します");
          addRebootRecord(ESP_RST_SW, "WiFi disconnect 5x");
          vTaskDelay(pdMS_TO_TICKS(1000));
          ESP.restart();
        }
        continue;
      }

      // タイムアウト監視開始
      lastAttemptTime = millis();
      logprintln("[SPREADSHEET] HTTP送信開始 (timeout: 25s)");

      // HTTP送信（セマフォ外で実行）
      sendSpreadsheet(dataCopy);

      // 送信が異常に長引いた場合の検出
      unsigned long sendDuration = millis() - lastAttemptTime;
      logprintln("[SPREADSHEET] HTTP送信完了 (所要時間: " + String(sendDuration) + "ms)");

      if (sendDuration > SEND_TIMEOUT)
      {
        logprintln("⚠ [SPREADSHEET] HTTP送信タイムアウト検出(" + String(sendDuration) + "ms) - 再起動します");
        addRebootRecord(ESP_RST_SW, "HTTP timeout");
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
      }

      // 送信完了後、フラグをクリア
      takeSemaphore(xDataSemaphore);
      int resultCode = sendHDatatoSS.last_http_code;
      sendHDatatoSS.send_flg = 0;
      sendHDatatoSS.data.clear(); // メモリ解放
      giveSemaphore(xDataSemaphore);

      // 送信結果によって連続失敗カウントをリセット
      if (resultCode == 200)
      {
        consecutiveFailures = 0;
        logprintln("✓ [SPREADSHEET] 送信成功 (連続失敗カウントリセット)");
      }
      else
      {
        consecutiveFailures++;
        logprintln("✗ [SPREADSHEET] 送信失敗(code:" + String(resultCode) + ") 連続失敗:" + String(consecutiveFailures) + "/3");

        // 連続3回失敗したら再起動
        if (consecutiveFailures >= 3)
        {
          logprintln("⚠ [SPREADSHEET] 連続送信失敗3回 - 再起動します");
          addRebootRecord(ESP_RST_SW, "HTTP error 3x");
          vTaskDelay(pdMS_TO_TICKS(1000));
          ESP.restart();
        }
      }
    }

    // デバッグ用：スタック残量確認
    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL);
    if (stackRemaining < 512)
    {
      logprintln("⚠ [SPREADSHEET] Low stack! " + String(stackRemaining) + " bytes");
    }
  }

  vTaskDelete(NULL);
}

void sendSpreadsheet(const String &data)
{
  // ローカル変数に変更（メモリ断片化対策）
  String urlFinal = String(GAS_URL) + data;
  logprintln("○[SPREADSHEET] 送信開始(" + String(urlFinal.length()) + "bytes)");
  logprintln("  URL: " + String(GAS_URL));
  logprintln("  Data: " + data);

  // WiFi接続状態を再確認
  if (WiFi.status() != WL_CONNECTED)
  {
    logprintln("●[SPREADSHEET] 送信中止: WiFi切断検出");
    takeSemaphore(xDataSemaphore);
    sendHDatatoSS.last_http_code = -2;
    giveSemaphore(xDataSemaphore);
    return;
  }

  // RSSI（電波強度）を確認
  int rssi = WiFi.RSSI();
  logprintln("  WiFi RSSI: " + String(rssi) + " dBm");
  if (rssi < -85)
  {
    logprintln("⚠ [SPREADSHEET] WiFi電波が弱い！");
  }

  HTTPClient http;

  logprintln("  [1/4] HTTPClient.begin()");
  http.begin(urlFinal.c_str());

  logprintln("  [2/4] setTimeout(20000)");
  http.setTimeout(20000); // タイムアウトを20秒に設定

  logprintln("  [3/4] setFollowRedirects()");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  // 送信開始時刻を記録
  unsigned long startTime = millis();

  logprintln("  [4/4] GET送信開始...");
  watchdog_reset(); // HTTP送信前にWDTリセット

  int httpCode = http.GET();

  unsigned long duration = millis() - startTime;
  String log;

  takeSemaphore(xDataSemaphore);
  sendHDatatoSS.last_http_code = httpCode;
  giveSemaphore(xDataSemaphore);

  if (httpCode > 0)
  {
    log = "●[SPREADSHEET] 受信完了(" + String(duration) + "ms) [HTTP] code: " + String(httpCode);
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      // ペイロードが長すぎる場合は切り詰める
      if (payload.length() > 100)
      {
        payload = payload.substring(0, 100) + "...";
      }
      log += " Payload: " + payload;
    }
    else if (httpCode >= 300 && httpCode < 400)
    {
      log += " (リダイレクト)";
    }
    else if (httpCode >= 400)
    {
      log += " (エラー)";
    }
  }
  else
  {
    log = "●[SPREADSHEET] 受信失敗(" + String(duration) + "ms) error: " + http.errorToString(httpCode);
    log += " (error code: " + String(httpCode) + ")";
  }

  logprintln(log);
  http.end();

  logprintln("  HTTPClient.end() 完了");

  // 明示的にメモリ解放の時間を与える
  vTaskDelay(pdMS_TO_TICKS(100));
}