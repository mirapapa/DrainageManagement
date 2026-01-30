#include "common.h"
#include <WebServer.h>
#include <Update.h>

unsigned int progress_percent = 0;
WebServer webOtaServer(8080);

// OTAアップロード用のHTMLページ
const char *otaUploadHtml = R"rawliteral(
<!DOCTYPE html>
<html lang='ja'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>排液管理システム - OTA更新</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .container {
            background: white;
            padding: 40px;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            max-width: 500px;
            width: 100%;
        }
        h1 {
            color: #667eea;
            margin-bottom: 10px;
            text-align: center;
        }
        .version {
            text-align: center;
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .info-box {
            background: #f0f4ff;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            border-left: 4px solid #667eea;
        }
        .info-box p {
            margin: 5px 0;
            font-size: 14px;
            color: #333;
        }
        input[type='file'] {
            display: block;
            width: 100%;
            padding: 15px;
            margin-bottom: 20px;
            border: 2px dashed #667eea;
            border-radius: 8px;
            background: #f9f9f9;
            cursor: pointer;
            transition: all 0.3s;
        }
        input[type='file']:hover {
            background: #f0f4ff;
            border-color: #764ba2;
        }
        button {
            width: 100%;
            padding: 15px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: transform 0.2s;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 20px rgba(102, 126, 234, 0.4);
        }
        button:disabled {
            background: #ccc;
            cursor: not-allowed;
            transform: none;
        }
        .progress-container {
            display: none;
            margin-top: 20px;
        }
        .progress-bar {
            width: 100%;
            height: 30px;
            background: #f0f4ff;
            border-radius: 15px;
            overflow: hidden;
            margin-bottom: 10px;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
            width: 0%;
            transition: width 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
        }
        .message {
            text-align: center;
            padding: 10px;
            border-radius: 8px;
            margin-top: 15px;
            display: none;
        }
        .success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
    </style>
</head>
<body>
    <div class='container'>
        <h1>🔄 OTA ファームウェア更新</h1>
        <div class='version'>排液管理システム ver1.11</div>
        
        <div class='info-box'>
            <p>📱 <strong>IP:</strong> <span id='ipAddr'>読み込み中...</span></p>
            <p>🆔 <strong>ホスト:</strong> <span id='hostname'>読み込み中...</span></p>
            <p>📶 <strong>RSSI:</strong> <span id='rssi'>読み込み中...</span> dBm</p>
            <p>💾 <strong>空きメモリ:</strong> <span id='freeHeap'>読み込み中...</span> bytes</p>
        </div>
        
        <div class='info-box' style='border-left-color: #ff6b6b;'>
            <p>🔄 <strong>総再起動回数:</strong> <span id='totalReboots'>-</span> 回</p>
            <p>⏱️ <strong>稼働時間:</strong> <span id='uptime'>-</span></p>
            <p>📝 <strong>最終再起動:</strong> <span id='lastReboot'>-</span></p>
        </div>
        
        <details style='margin-bottom: 20px;'>
            <summary style='cursor: pointer; padding: 10px; background: #f0f4ff; border-radius: 8px; font-weight: bold;'>
                📜 再起動履歴を表示
            </summary>
            <div id='rebootHistory' style='margin-top: 10px; max-height: 200px; overflow-y: auto;'>
                読み込み中...
            </div>
        </details>
        
        <form id='uploadForm' enctype='multipart/form-data'>
            <input type='file' name='update' id='fileInput' accept='.bin' required>
            <button type='submit' id='uploadBtn'>📤 アップロード開始</button>
        </form>
        
        <div class='progress-container' id='progressContainer'>
            <div class='progress-bar'>
                <div class='progress-fill' id='progressFill'>0%</div>
            </div>
            <p style='text-align: center; color: #666;' id='progressText'>アップロード中...</p>
        </div>
        
        <div class='message' id='message'></div>
    </div>

    <script>
        // システム情報を取得
        fetch('/info')
            .then(r => r.json())
            .then(data => {
                document.getElementById('ipAddr').textContent = data.ip;
                document.getElementById('hostname').textContent = data.hostname;
                document.getElementById('rssi').textContent = data.rssi;
                document.getElementById('freeHeap').textContent = data.freeHeap.toLocaleString();
                
                // 再起動ログ情報を表示
                if (data.rebootLog) {
                    const log = data.rebootLog;
                    
                    // 総再起動回数
                    document.getElementById('totalReboots').textContent = log.totalReboots;
                    
                    // 稼働時間
                    const uptime = formatUptime(log.uptime);
                    document.getElementById('uptime').textContent = uptime;
                    
                    // 最終再起動
                    if (log.records && log.records.length > 0) {
                        const latest = log.records[0];
                        document.getElementById('lastReboot').textContent = 
                            latest.timeStr + ' (' + latest.reason + ')';
                    } else {
                        document.getElementById('lastReboot').textContent = 'データなし';
                    }
                    
                    // 再起動履歴テーブルを作成
                    displayRebootHistory(log.records);
                }
            })
            .catch(e => console.error('Info fetch failed:', e));
        
        function formatUptime(seconds) {
            const days = Math.floor(seconds / 86400);
            const hours = Math.floor((seconds % 86400) / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            
            if (days > 0) {
                return days + '日 ' + hours + '時間 ' + minutes + '分';
            } else if (hours > 0) {
                return hours + '時間 ' + minutes + '分';
            } else {
                return minutes + '分';
            }
        }
        
        function displayRebootHistory(records) {
            const historyDiv = document.getElementById('rebootHistory');
            
            if (!records || records.length === 0) {
                historyDiv.innerHTML = '<p style="color: #999;">履歴データがありません</p>';
                return;
            }
            
            let html = '<table style="width: 100%; border-collapse: collapse; font-size: 14px;">';
            html += '<thead><tr style="background: #667eea; color: white;">';
            html += '<th style="padding: 8px; text-align: left;">日時</th>';
            html += '<th style="padding: 8px; text-align: left;">理由</th>';
            html += '<th style="padding: 8px; text-align: left;">メッセージ</th>';
            html += '</tr></thead><tbody>';
            
            records.forEach((record, index) => {
                const bgColor = index % 2 === 0 ? '#f9f9f9' : 'white';
                html += '<tr style="background: ' + bgColor + ';">';
                html += '<td style="padding: 8px; border-bottom: 1px solid #ddd;">' + record.timeStr + '</td>';
                html += '<td style="padding: 8px; border-bottom: 1px solid #ddd;">' + getReasonIcon(record.reason) + ' ' + record.reason + '</td>';
                html += '<td style="padding: 8px; border-bottom: 1px solid #ddd;">' + record.message + '</td>';
                html += '</tr>';
            });
            
            html += '</tbody></table>';
            historyDiv.innerHTML = html;
        }
        
        function getReasonIcon(reason) {
            if (reason.includes('WDT')) return '⚠️';
            if (reason.includes('Power')) return '🔌';
            if (reason.includes('Software')) return '🔄';
            if (reason.includes('Panic')) return '❌';
            return '❓';
        }

        document.getElementById('uploadForm').onsubmit = async function(e) {
            e.preventDefault();
            
            const fileInput = document.getElementById('fileInput');
            const file = fileInput.files[0];
            
            if (!file) {
                showMessage('error', 'ファイルを選択してください');
                return;
            }
            
            const uploadBtn = document.getElementById('uploadBtn');
            const progressContainer = document.getElementById('progressContainer');
            const progressFill = document.getElementById('progressFill');
            const progressText = document.getElementById('progressText');
            
            uploadBtn.disabled = true;
            progressContainer.style.display = 'block';
            
            const formData = new FormData();
            formData.append('update', file);
            
            try {
                const xhr = new XMLHttpRequest();
                
                xhr.upload.onprogress = function(e) {
                    if (e.lengthComputable) {
                        const percent = Math.round((e.loaded / e.total) * 100);
                        progressFill.style.width = percent + '%';
                        progressFill.textContent = percent + '%';
                        progressText.textContent = 'アップロード中... ' + percent + '%';
                    }
                };
                
                xhr.onload = function() {
                    if (xhr.status === 200) {
                        progressFill.style.width = '100%';
                        progressFill.textContent = '100%';
                        progressText.textContent = '完了！';
                        showMessage('success', '✅ アップロード成功！デバイスが再起動します...');
                        
                        setTimeout(() => {
                            window.location.reload();
                        }, 5000);
                    } else {
                        showMessage('error', '❌ アップロード失敗: ' + xhr.responseText);
                        uploadBtn.disabled = false;
                    }
                };
                
                xhr.onerror = function() {
                    showMessage('error', '❌ 通信エラーが発生しました');
                    uploadBtn.disabled = false;
                };
                
                xhr.open('POST', '/update', true);
                xhr.send(formData);
                
            } catch (error) {
                showMessage('error', '❌ エラー: ' + error.message);
                uploadBtn.disabled = false;
            }
        };
        
        function showMessage(type, text) {
            const message = document.getElementById('message');
            message.className = 'message ' + type;
            message.textContent = text;
            message.style.display = 'block';
        }
    </script>
</body>
</html>
)rawliteral";

void webOtaHandleRoot()
{
    webOtaServer.send(200, "text/html", otaUploadHtml);
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