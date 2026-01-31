#include "common.h"

#define MAX_HISTORYDATA 64
#define MAX_QUENUM 128
#define MAX_QUESIZE 256
WiFiServer logServer(4444); // ポート設定
QueueHandle_t queue1;
SemaphoreHandle_t xHistorySemaphore;

typedef struct
{
  uint oldest_num;
  uint latest_num;
  String data[MAX_HISTORYDATA];
} HISTORYDATA;
HISTORYDATA historyData;

void logServersetup()
{
  // logServer用のキューを生成
  queue1 = xQueueCreate(MAX_QUENUM, MAX_QUESIZE);

  // セマフォ作成
  xHistorySemaphore = xSemaphoreCreateBinary();
  if (xHistorySemaphore == NULL)
  {
    Serial.println("セマフォ取得失敗");
  }
  else
  {
    xSemaphoreGive(xHistorySemaphore);
  }
}

void logServer_task(void *pvParameters)
{
  logprintln("logServer_task START!!");
  logServer.begin();

  // Task WDTに登録
  watchdog_subscribe_task("LOGSERVER_TASK");

  historyData.oldest_num = 0;
  historyData.latest_num = 0;

  vTaskDelay(pdMS_TO_TICKS(100)); // 各タスク起動待ち

  while (1)
  {
    // WDTリセット（ループの最初で実行）
    watchdog_reset();

    char buf[MAX_QUESIZE];

    WiFiClient logClient = logServer.available();

    if (logClient)
    {
      logprintln("New Client.", false);
      while (logClient.connected())
      {
        // WDTリセット（クライアント接続中）
        watchdog_reset();

        while (xQueueReceive(queue1, buf, 0))
        {
          logClient.println(buf);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }
      logClient.stop();
      logprintln("client.stop()");
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  vTaskDelete(NULL);
}

void logprintln(String log)
{
  // 時刻取得
  String timeStr = "";
  timeStr = getSystemTimeStr();

  String fullLog = timeStr + log;

  // 1. シリアル出力
  Serial.println(fullLog);

  // 2. Queueへの送信（バッファオーバーフロー対策）
  if (queue1 != NULL)
  {
    // 最大サイズを超える場合は切り詰め
    if (fullLog.length() >= MAX_QUESIZE)
    {
      fullLog = fullLog.substring(0, MAX_QUESIZE - 4) + "...";
    }
    xQueueSend(queue1, fullLog.c_str(), 0);
  }
}

void logprintln(String log, bool historyFlg)
{
  // 既存の logprintln(String log) を呼び出す
  logprintln(log);

  if (historyFlg)
  {
    // 時刻取得
    String timeStr = "";
    timeStr = getSystemTimeStr();

    takeSemaphore(xHistorySemaphore);

    historyData.latest_num = nextnum(historyData.latest_num);
    // バッファが満杯の場合、古いデータを削除
    if (historyData.latest_num == historyData.oldest_num)
    {
      historyData.oldest_num = nextnum(historyData.oldest_num);
    }
    historyData.data[historyData.latest_num] = timeStr + log;

    giveSemaphore(xHistorySemaphore);
  }
}

String getHistoryData()
{
  String strbuf;

  takeSemaphore(xHistorySemaphore);
  uint readnum = historyData.latest_num;

  while (1)
  {
    strbuf = strbuf + historyData.data[readnum];
    strbuf = strbuf + "<BR>";
    if (readnum == historyData.oldest_num)
      break;
    readnum = prevnum(readnum);
  }

  giveSemaphore(xHistorySemaphore);
  return strbuf;
}

uint nextnum(uint num)
{
  if (num >= (MAX_HISTORYDATA - 1))
    return 0;
  else
    return num + 1;
}

uint prevnum(uint num)
{
  if (num == 0)
    return MAX_HISTORYDATA - 1;
  else
    return num - 1;
}