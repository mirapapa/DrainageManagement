#include "common.h"

unsigned int progress_percent = 0;

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
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);
  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");
  // No authentication by default
  ArduinoOTA.setPassword("drainage");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA
      .onStart([]()
               {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    logprintln("Start updating " + type); })
      .onEnd([]()
             {
    logprintln("<< Program Update End!! Reboot!! >>");
    delay(1000); })
      .onProgress([](unsigned int progress, unsigned int total)
                  {
    unsigned int nowPercent = (progress / (total / 100));
    if ((progress_percent != nowPercent) && (nowPercent%5 == 0)) {
      logprintln(String("Program Update...Progress: ") + String(nowPercent) + String("%"));
      progress_percent = nowPercent;
    } })
      .onError([](ota_error_t error)
               {
    logprintln(String("Error[%u]: ") + String(error));
    if (error == OTA_AUTH_ERROR) logprintln("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) logprintln("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) logprintln("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) logprintln("Receive Failed");
    else if (error == OTA_END_ERROR) logprintln("End Failed"); });
  ArduinoOTA.begin();
  logprintln("OTA_Ready");
  //  logprintln(String("IP address: ") + WiFi.localIP().toString());
}
