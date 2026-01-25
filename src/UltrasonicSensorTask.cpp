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
}

void ultrasonicSensor_Task(void *pvParameters)
{
  logprintln("ultrasonicSensor_Task START!!");
  vTaskDelay(pdMS_TO_TICKS(5000));

  while (true)
  {
    // 距離計測
    distance = measureDistance();

    // --- 1段目: 距離表示 (単位を右端に固定) ---
    lcd.setCursor(0, 0);
    char line1[17];
    // "DIST:    120 cm" のように右詰めで整形 (全15文字 + WiFiアイコン分)
    sprintf(line1, "DIST: %5d cm", distance);
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

    takeSemaphore(xSemaphore);
    bool sending = sendHDatatoSS.send_flg;
    giveSemaphore(xSemaphore);

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
      // 待機中は次回送信までのカウントダウンを表示
      int next = 60 - (millis() - lastSendTime) / 1000;
      if (next < 0)
        next = 0;
      char line2[17];
      sprintf(line2, "WAITING...  %2ds", next);
      lcd.print(line2);
    }

    // 60秒に1回、スプレッドシート送信用のデータを作成
    if (millis() - lastSendTime > 60000)
    {
      takeSemaphore(xSemaphore);
      // 自宅側のGASが受け取れる形式 "?d4=数値" を作成
      sendHDatatoSS.data = "?d4=" + String(distance);
      sendHDatatoSS.send_flg = 1;
      giveSemaphore(xSemaphore);

      lastSendTime = millis();
      logprintln("○SS送信キューに追加: d4=" + String(distance));
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