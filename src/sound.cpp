#include "sound.h"
#include <Audio.h>
#include <SD.h>
#include <Wire.h>
#include "es8311.h"
#include "model.h"

#include "board.h"

static Audio audio;
static ES8311 codec;
static bool ready = false;
// Audio runs in its own task so a slow download in the main loop can't starve playback.
static SemaphoreHandle_t lock = nullptr;
struct Lock {
  Lock() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(lock); }
};

static void audioTask(void *) {
  for (;;) {
    {
      Lock l;
      audio.loop();
    }
    vTaskDelay(1);
  }
}

bool soundBegin() {
  // audio chips' power is switched by the PCA9557 (done in expanderBegin), the amp by AMP_EN
  pinMode(AMP_EN, OUTPUT);
  digitalWrite(AMP_EN, HIGH);
  delay(20);
  if (!codec.begin(I2C_SDA, I2C_SCL, 400000)) {
    Serial.println("[sound] ES8311 codec not found");
    return false;
  }
  codec.setVolume(100);
  codec.setBitsPerSample(16);
  if (!audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK)) {
    Serial.println("[sound] I2S pinout failed");
    return false;
  }
  audio.setVolume(CFG.volume);
  lock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 2, nullptr, 1);
  ready = true;
  Serial.println("[sound] ready");
  return true;
}

void soundLoop() {}  // playback is serviced by audioTask

bool soundBusy() {
  if (!ready) return false;
  Lock l;
  return audio.isRunning();
}

void soundSetVolume(uint8_t v) {
  if (!ready) return;
  static int last = -1;
  if (v == last) return;
  last = v;
  Lock l;
  audio.setVolume(v > 21 ? 21 : v);
}

static String findFile(const String &base) {
  for (const char *ext : {".mp3", ".wav"}) {
    String p = "/sounds/" + base + ext;
    if (SD.exists(p)) return p;
  }
  return "";
}

bool soundPlay(const String &name, const String &team) {
  if (!ready || !CFG.soundOn) return false;
  // team-specific sound wins: /sounds/LV_td.mp3, then /sounds/td.mp3
  String path = team.length() ? findFile(team + "_" + name) : "";
  if (!path.length()) path = findFile(name);
  if (!path.length()) return false;
  Lock l;
  audio.stopSong();
  bool ok = audio.connecttoFS(SD, path.c_str());
  if (!ok) Serial.printf("[sound] couldn't play %s\n", path.c_str());
  return ok;
}
