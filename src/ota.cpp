#include "common.h"
#include <WebServer.h>
#include <Update.h>
#include "ota_html.h"

unsigned int progress_percent = 0;
WebServer webOtaServer(8080);

void webOtaHandleRoot()
{
    // PROGMEMからStringへ読み込む
    String html = String(otaUploadHtml);

    // キーワードを定義名に置換
    html.replace("{{SYS_NAME}}", SYSTEM_NAME);
    html.replace("{{SYS_VER}}", SYSTEM_VERSION);
    html.replace("{{BUILD_DATE}}", __DATE__);

    // 置換後のHTMLを送信
    webOtaServer.send(200, "text/html", html);
}

void webOtaHandleInfo()
{
    String json = "{";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"hostname\":\"" + String(WiFi.getHostname()) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";

    // 再起動ログ情報を追加
    json += "\"rebootLog\":" + getRebootLogJson();

    json += "}";
    webOtaServer.send(200, "application/json", json);
}

void webOtaHandleUpdate()
{
    HTTPUpload &upload = webOtaServer.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        logprintln("[WEB OTA] Update Start: " + String(upload.filename));

        // OTA更新開始
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        {
            logprintln("[WEB OTA] Update Begin Failed");
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        // データを書き込み
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            logprintln("[WEB OTA] Update Write Failed");
            Update.printError(Serial);
        }
        else
        {
            // 進捗表示（5%刻み）
            unsigned int percent = (Update.progress() * 100) / Update.size();
            if ((progress_percent != percent) && (percent % 5 == 0))
            {
                logprintln("[WEB OTA] Progress: " + String(percent) + "%");
                progress_percent = percent;
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
        {
            logprintln("[WEB OTA] Update Success! Total: " + String(upload.totalSize) + " bytes");
            logprintln("[WEB OTA] Rebooting...");
        }
        else
        {
            logprintln("[WEB OTA] Update Failed!");
            Update.printError(Serial);
        }
    }
}

void webOtaHandleUpdatePost()
{
    webOtaServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");

    // 再起動
    delay(1000);
    ESP.restart();
}

void verifyFirmware()
{
    logprintln("[SYSTEM] - Checking firmware...");
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        const char *otaState = ota_state == ESP_OTA_IMG_NEW              ? "ESP_OTA_IMG_NEW"
                               : ota_state == ESP_OTA_IMG_PENDING_VERIFY ? "ESP_OTA_IMG_PENDING_VERIFY"
                               : ota_state == ESP_OTA_IMG_VALID          ? "ESP_OTA_IMG_VALID"
                               : ota_state == ESP_OTA_IMG_INVALID        ? "ESP_OTA_IMG_INVALID"
                               : ota_state == ESP_OTA_IMG_ABORTED        ? "ESP_OTA_IMG_ABORTED"
                                                                         : "ESP_OTA_IMG_UNDEFINED";
        logprintln("[System] - Ota state: " + String(otaState));

        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
            {
                logprintln("[System] - App is valid, rollback cancelled successfully");
            }
            else
            {
                logprintln("[System] - Failed to cancel rollback");
            }
        }
    }
    else
    {
        logprintln("[System] - OTA partition has no record in OTA data");
    }
}

void ota_setup()
{
    // ArduinoOTA (従来のOTA)の設定
    ArduinoOTA.setPassword("drainage");
    ArduinoOTA
        .onStart([]()
                 {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else
      type = "filesystem";
    logprintln("[Arduino OTA] Start updating " + type); })
        .onEnd([]()
               {
    logprintln("[Arduino OTA] Update End!! Reboot!!");
    delay(1000); })
        .onProgress([](unsigned int progress, unsigned int total)
                    {
    unsigned int nowPercent = (progress / (total / 100));
    if ((progress_percent != nowPercent) && (nowPercent%5 == 0)) {
      logprintln("[Arduino OTA] Progress: " + String(nowPercent) + "%");
      progress_percent = nowPercent;
    } })
        .onError([](ota_error_t error)
                 {
    logprintln("[Arduino OTA] Error: " + String(error));
    if (error == OTA_AUTH_ERROR) logprintln("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) logprintln("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) logprintln("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) logprintln("Receive Failed");
    else if (error == OTA_END_ERROR) logprintln("End Failed"); });
    ArduinoOTA.begin();
    logprintln("[Arduino OTA] Ready (Port: 3232)");

    // WEB OTAの設定
    webOtaServer.on("/", HTTP_GET, webOtaHandleRoot);
    webOtaServer.on("/info", HTTP_GET, webOtaHandleInfo);
    webOtaServer.on("/update", HTTP_POST, webOtaHandleUpdatePost, webOtaHandleUpdate);
    webOtaServer.begin();

    logprintln("[WEB OTA] Ready (Port: 8080)");
    logprintln("[WEB OTA] Access: http://" + WiFi.localIP().toString() + ":8080");
}

void ota_handle()
{
    ArduinoOTA.handle();
    webOtaServer.handleClient();
}