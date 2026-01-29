#include "common.h"
#include "esp_task_wdt.h"

// Task WDTのタイムアウト時間（秒）
#define WDT_TIMEOUT 30

// 各タスクのWDT登録状態
bool ultrasonicSensorWdtSubscribed = false;
bool spreadsheetWdtSubscribed = false;
bool logServerWdtSubscribed = false;

void watchdog_setup()
{
    // Task WDTの初期化（30秒タイムアウト）
    esp_task_wdt_init(WDT_TIMEOUT, true); // true = パニック時にリセット
    logprintln("[WDT] Task Watchdog Timer initialized (timeout: " + String(WDT_TIMEOUT) + "s)");
}

void watchdog_subscribe_task(const char *taskName)
{
    esp_err_t err = esp_task_wdt_add(NULL); // NULLで現在のタスクを登録

    if (err == ESP_OK)
    {
        logprintln("[WDT] Task subscribed: " + String(taskName));
    }
    else if (err == ESP_ERR_INVALID_STATE)
    {
        logprintln("[WDT] Task already subscribed: " + String(taskName));
    }
    else
    {
        logprintln("[WDT] Failed to subscribe task: " + String(taskName) + " Error: " + String(err));
    }
}

void watchdog_reset()
{
    // 現在のタスクのWDTカウンタをリセット
    esp_task_wdt_reset();
}

void watchdog_unsubscribe_task(const char *taskName)
{
    esp_err_t err = esp_task_wdt_delete(NULL);

    if (err == ESP_OK)
    {
        logprintln("[WDT] Task unsubscribed: " + String(taskName));
    }
    else
    {
        logprintln("[WDT] Failed to unsubscribe task: " + String(taskName));
    }
}

// WDTタイムアウト時のコールバック（オプション）
void IRAM_ATTR watchdog_timeout_callback()
{
    // この関数は割り込みコンテキストで呼ばれるため、
    // Serial.printなどのブロッキング関数は使用できない
    // 必要に応じてログを記録するなどの処理を追加
}