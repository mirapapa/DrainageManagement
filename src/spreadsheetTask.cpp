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
  const unsigned long SEND_TIMEOUT = 15000; // 15秒でタイムアウト
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
      // WiFi接続確認
      if (WiFi.status() != WL_CONNECTED)
      {
        logprintln("⚠ WiFi切断中のため送信スキップ");
        takeSemaphore(xDataSemaphore);
        sendHDatatoSS.send_flg = 0;
        sendHDatatoSS.last_http_code = -1;
        giveSemaphore(xDataSemaphore);
        consecutiveFailures++;

        // 連続5回失敗したら再起動
        if (consecutiveFailures >= 10)
        {
          logprintln("⚠ 連続送信失敗10回 - 再起動します");
          vTaskDelay(pdMS_TO_TICKS(1000));
          ESP.restart();
        }
        continue;
      }

      // タイムアウト監視開始
      lastAttemptTime = millis();

      // HTTP送信（セマフォ外で実行）
      sendSpreadsheet(dataCopy);

      // 送信が異常に長引いた場合の検出
      unsigned long sendDuration = millis() - lastAttemptTime;
      if (sendDuration > SEND_TIMEOUT)
      {
        logprintln("⚠ HTTP送信タイムアウト検出(" + String(sendDuration) + "ms) - 再起動します");
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
        logprintln("✓ SpreadSheet送信成功");
      }
      else
      {
        consecutiveFailures++;
        logprintln("✗ SpreadSheet送信失敗(code:" + String(resultCode) + ") 連続失敗:" + String(consecutiveFailures));

        // 連続5回失敗したら再起動
        if (consecutiveFailures >= 5)
        {
          logprintln("⚠ 連続送信失敗5回 - 再起動します");
          vTaskDelay(pdMS_TO_TICKS(1000));
          ESP.restart();
        }
      }
    }

    // デバッグ用：スタック残量確認
    UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL);
    if (stackRemaining < 512)
    {
      logprintln("⚠ SPREADSHEET_TASK: Low stack! " + String(stackRemaining) + " bytes");
    }
  }

  vTaskDelete(NULL);
}

void sendSpreadsheet(const String &data)
{
  // ローカル変数に変更（メモリ断片化対策）
  String urlFinal = String(GAS_URL) + data;
  logprintln("○SpreadSheet 送信開始(" + String(urlFinal.length()) + "bytes)");

  // WiFi接続状態を再確認
  if (WiFi.status() != WL_CONNECTED)
  {
    logprintln("●SpreadSheet 送信中止: WiFi切断検出");
    takeSemaphore(xDataSemaphore);
    sendHDatatoSS.last_http_code = -2;
    giveSemaphore(xDataSemaphore);
    return;
  }

  HTTPClient http;
  http.begin(urlFinal.c_str());
  http.setTimeout(15000); // タイムアウトを15秒に設定
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  // 送信開始時刻を記録
  unsigned long startTime = millis();

  int httpCode = http.GET();

  unsigned long duration = millis() - startTime;
  String log;

  takeSemaphore(xDataSemaphore);
  sendHDatatoSS.last_http_code = httpCode;
  giveSemaphore(xDataSemaphore);

  if (httpCode > 0)
  {
    log = "●SpreadSheet 受信完了(" + String(duration) + "ms) [HTTP] code: " + String(httpCode);
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
  }
  else
  {
    log = "●SpreadSheet 受信失敗(" + String(duration) + "ms) error: " + http.errorToString(httpCode);
  }

  logprintln(log);
  http.end();

  // 明示的にメモリ解放の時間を与える
  vTaskDelay(pdMS_TO_TICKS(100));
}