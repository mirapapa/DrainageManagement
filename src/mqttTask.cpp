#include "common.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void mqttTask(void *pvParameters)
{
    logprintln("mqtt_task START!!");

    mqttClient.setServer("mqtt.beebotte.com", 1883);

    while (true)
    {
        // Wi-Fi/MQTT接続チェック
        if (WiFi.status() == WL_CONNECTED)
        {
            if (!mqttClient.connected())
            {
                logprintln("[MQTT] Connecting...");
                if (mqttClient.connect("ESP32_Drainage", BEEBOTTE_TOKEN, ""))
                {
                    logprintln("[MQTT] Connected!");
                }
            }
        }

        // MQTTの内部処理（キープアライブなど）を実行
        if (mqttClient.connected())
        {
            mqttClient.loop();
        }

        // タスクの暴走を防ぐための待機（重要）
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}