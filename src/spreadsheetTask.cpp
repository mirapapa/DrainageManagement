#include "common.h"

void spreadsheet_task(void *pvParameters)
{
  // 送信フラグ初期化
  takeSemaphore(xSemaphore);
  sendHDatatoSS.send_flg = 0;
  giveSemaphore(xSemaphore);

  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));

    // セマフォで保護しながらフラグとデータをコピー
    takeSemaphore(xSemaphore);
    bool shouldSend = sendHDatatoSS.send_flg;
    String dataCopy = sendHDatatoSS.data;
    giveSemaphore(xSemaphore);

    if (shouldSend)
    {
      // HTTP送信（セマフォ外で実行）
      sendSpreadsheet(dataCopy);

      // 送信完了後、フラグをクリア
      takeSemaphore(xSemaphore);
      sendHDatatoSS.send_flg = 0;
      sendHDatatoSS.data.clear(); // メモリ解放
      giveSemaphore(xSemaphore);
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
  logprintln("○SpreadSheet 送信(" + String(urlFinal.length()) + ") [" + String(GAS_URL) + "]");

  HTTPClient http;
  http.begin(urlFinal.c_str());
  http.setTimeout(10000); // 10秒タイムアウト
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET();
  String log;

  if (httpCode > 0)
  {
    log = "●SpreadSheet 受信 [HTTP] GET... code: " + String(httpCode);
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      log += " Payload: " + payload;
    }
  }
  else
  {
    log = "●SpreadSheet 受信 [HTTP] GET... failed, error: " + http.errorToString(httpCode);
  }

  logprintln(log);
  http.end();
}