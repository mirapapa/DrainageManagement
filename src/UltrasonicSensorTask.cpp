#include "common.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define ECHO_PIN 16 // attach pin D2 Arduino to pin Echo of JSN-SR04T
#define TRIG_PIN 17 // attach pin D3 Arduino to pin Trig of JSN-SR04T

// 距離測定
int distance;
// 送信間隔管理用
unsigned long lastSendTime = 0;

// WiFiアイコンのドット絵定義
byte wifiImg[8] = {
    B00000,
    B00000,
    B01110,
    B10001,
    B00100,
    B01010,
    B00000,
    B00100};

// 読み込み中（ぐるぐる）アニメーション用
const char loader[] = {'|', '/', '-', '\\'};
int loaderIdx = 0;

int distanceSamples[65]; // 1分間のデータを溜める配列
int sampleCount = 0;     // 現在のデータ数

void ultrasonicSensor_setup()
{
  pinMode(TRIG_PIN, OUTPUT); // Sets the TRIG_PIN as an OUTPUT
  pinMode(ECHO_PIN, INPUT);  // Sets the ECHO_PIN as an INPUT

  lcd.init();
  lcd.backlight();
  // WiFiアイコンを 0番 に登録
  lcd.createChar(0, wifiImg);
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM STARTING");
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void ultrasonicSensor_Task(void *pvParameters)
{
  logprintln("ultrasonicSensor_Task START!!");

  // Task WDTに登録
  watchdog_subscribe_task("ULTRASONICSENSOR_TASK");

  // --- WiFi接続完了まで待機してIPを表示するセクション ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connecting");

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20)
  { // 最大10秒待機
    vTaskDelay(pdMS_TO_TICKS(500));
    lcd.setCursor(15, 0);
    lcd.print(loader[loaderIdx]);
    loaderIdx = (loaderIdx + 1) % 4;
    retryCount++;

    // WDTリセット
    watchdog_reset();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi CONNECTED!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());       // IPアドレスを表示
    vTaskDelay(pdMS_TO_TICKS(5000)); // 5秒間表示をキープ
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi TIMEOUT");
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  lcd.clear();

  while (true)
  {
    // WDTリセット（ループの最初で実行）
    watchdog_reset();

    // 1. 距離計測
    distance = measureDistance();

    // 2. 配列に保存（配列が溢れないようにチェック）
    if (sampleCount < 60)
    {
      distanceSamples[sampleCount] = distance;
      sampleCount++;
    }

    // --- 1段目: 距離表示 (単位を右端に固定) ---
    lcd.setCursor(0, 0);
    char line1[17];
    // "DIST:    120 cm" のように右詰めで整形 (全15文字 + WiFiアイコン分)
    sprintf(line1, "DIST: %5d cm ", distance);
    lcd.print(line1);

    // 右上(15列目)にWiFiアイコンを表示（接続時のみ）
    lcd.setCursor(15, 0);
    if (WiFi.status() == WL_CONNECTED)
    {
      lcd.write(0); // 登録したWiFiアイコン
    }
    else
    {
      lcd.print("!"); // 切断時はビックリマーク
    }

    // --- 2段目: 状態表示と点滅アニメーション ---
    lcd.setCursor(0, 1);

    takeSemaphore(xDataSemaphore);
    bool sending = sendHDatatoSS.send_flg;
    giveSemaphore(xDataSemaphore);

    if (sending)
    {
      // 送信中は「Sending...」とアニメーションを表示
      lcd.print("SENDING ");
      lcd.print(loader[loaderIdx]);
      loaderIdx = (loaderIdx + 1) % 4;
      lcd.print("      "); // 残りを空白で掃除
    }
    else
    {
      // 待機中：次回送信までの秒数と、前回の結果を表示
      takeSemaphore(xDataSemaphore);
      int lastCode = sendHDatatoSS.last_http_code;
      giveSemaphore(xDataSemaphore);

      int next = 60 - (millis() - lastSendTime) / 1000;
      if (next < 0)
        next = 0;

      char line2[17];
      if (lastCode == 200)
      {
        // 成功時は「OK」と秒数を表示
        sprintf(line2, "OK!      WAIT%2ds", next);
      }
      else if (lastCode == 0)
      {
        // 初回など、まだ送信していない時
        sprintf(line2, "READY    WAIT%2ds", next);
      }
      else
      {
        // 失敗時はエラーコードを表示
        sprintf(line2, "ERR:%d  WAIT%2ds", lastCode, next);
      }
      lcd.print(line2);
    }

    // --- 60秒に1回、中央値を計算して送信 ---
    if (millis() - lastSendTime > 60000)
    {
      int medianDistance = 0;
      if (sampleCount > 0)
      {
        // 配列を小さい順に並べ替える
        std::sort(distanceSamples, distanceSamples + sampleCount);

        // 真ん中の値（中央値）を取り出す
        medianDistance = distanceSamples[sampleCount / 2];
      }

      takeSemaphore(xDataSemaphore);
      // 中央値を送信データにセット
      sendHDatatoSS.data = "?d4=" + String(medianDistance);
      sendHDatatoSS.send_flg = 1;
      giveSemaphore(xDataSemaphore);

      logprintln("○SS送信(中央値): d4=" + String(medianDistance) + " (" + String(sampleCount) + " samples)");

      // 変数とカウントをリセット
      lastSendTime = millis();
      sampleCount = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  vTaskDelete(NULL);
}

int measureDistance()
{
  long duration; // variable for the duration of sound wave travel
  int distance;  // variable for the distance measurement

  // Clears the TRIG_PIN condition
  digitalWrite(TRIG_PIN, LOW); //
  delayMicroseconds(2);
  // Sets the TRIG_PIN HIGH (ACTIVE) for 10 microseconds
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  // Reads the ECHO_PIN, returns the sound wave travel time in microseconds
  duration = pulseIn(ECHO_PIN, HIGH);
  // Calculating the distance
  distance = duration * 0.034 / 2; // Speed of sound wave divided by 2 (go and back)
  // Displays the distance on the Serial Monitor
  logprintln("Distance: " + String(distance) + " cm");

  return distance;
}