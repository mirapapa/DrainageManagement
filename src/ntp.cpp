#include "common.h"

bool firstTimeNtpFlg = false;

void ntp_setup()
{
  sntp_set_time_sync_notification_cb(timeavailable);
  configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
}

// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
  logprintln("Got time adjustment from NTP!");
  // 起動後の初回時刻合わせ後は強制的にフェーズ0にする
  // これをしないと再起動時に換気扇が動かない可能性があるため
  if (firstTimeNtpFlg == false)
  {
    logprintln("<<ＮＴＰ システム起動後初回時刻合わせ完了>>", 1);
    firstTimeNtpFlg = true;
  }

  // 再起動ログのセットアップ（NTP時刻同期後に実行）
  rebootLog_setup();

  // mdns
  // mdnssetup();
}