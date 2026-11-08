// M18 Akku-Brücke - ESP32-S3 (N16R8) between the m18 software and a Milwaukee M18 battery.
//
//  - Speaks the battery protocol itself (m18_proto.h) and replaces the
//    USB-serial adapter of m18-protocol.
//  - PC access over USB (115200 baud, text commands) or Wi-Fi (HTTP POST /cmd);
//    m18_bridge.py / the web GUI use this transparently.
//  - Own web interface: Wi-Fi setup in the customer's network (captive portal),
//    direct battery read-out with decoded values (m18_reader.h), status, LED,
//    firmware update.
//  - The PC finds the bridge on the network automatically (UDP discovery on
//    port 18018 + mDNS <name>.local / _m18bridge._tcp).
//  - On-board RGB LED shows the state (see README).
//
// Reliability: task watchdog, battery access serialised by a mutex, web read-out
// in a background task, retries/resync while reading, J2 forced low on boot and
// after 8 s of inactivity, Wi-Fi power save off, mDNS restart on new IP,
// heap guard. Settings changes (Wi-Fi, firmware) need physical presence:
// setup access point, USB, or a short press on BOOT (unlocks for 2 minutes).
//
// Arduino IDE settings for ESP32-S3-DevKitC-1 N16R8 (see README):
//   Board "ESP32S3 Dev Module", Flash Size 16MB, PSRAM "OPI PSRAM",
//   Partition Scheme "16M Flash (3MB APP/9.9MB FATFS)", USB CDC On Boot "Enabled".
// ESP32-S3 SuperMini (own PCB): #define BOARD_SUPERMINI below, Flash Size 4MB, PSRAM "QSPI PSRAM",
//   partition table from ../partitions/supermini-4mb.csv copied as partitions.csv next to the
//   sketch, USB CDC On Boot "Enabled".
//   Arduino-ESP32 core 2.0.x or 3.x, no extra libraries.

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <FS.h>
#include <FFat.h>
#include <LittleFS.h>
#include <sys/time.h>
#include <esp_system.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <stdarg.h>
#include <time.h>
#include "m18_commands.h"
#include "m18_log.h"
#include "web_ui.h"

#define FW_BUILD __DATE__ " " __TIME__

// ----------------------------------------------------------------- settings
// Board variant. Default: ESP32-S3-DevKitC-1 N16R8 with level shifter (hardware/aufsteckgehaeuse).
// BOARD_SUPERMINI: own PCB (hardware/platine) with an ESP32-S3 SuperMini (ESP32-S3FH4R2,
// 4 MB flash, 2 MB quad PSRAM). In the Arduino IDE uncomment the next line; the CI build
// defines it itself.
// #define BOARD_SUPERMINI
#if defined(BOARD_SUPERMINI)
#define BOARD_NAME "supermini"
#define PIN_TX 7           // -> NPN/PNP high-side switch -> battery J2 (not inverting)
#define PIN_RX 6           // <- two-NPN isolator <- battery J1 (not inverting)
#define DEFAULT_LED_PIN 48 // WS2812 on the SuperMini (mostly hidden under the module)
// RGB LED on the PCB (common anode, each colour via an NPN: high = on). Pinout of the
// Lumetheus 5 mm RGB (R, A, G, B): pad 3 is green, pad 4 is blue. For a Dialight 521-8559F
// (R, A, B, G) swap G and B.
#define PIN_RGB_R 13       // -> Q5 -> pad 1
#define PIN_RGB_G 11       // -> Q7 -> pad 3
#define PIN_RGB_B 12       // -> Q6 -> pad 4
#define PIN_CHG_EN 5       // charger connector J2 pin 5: low = do not charge (nothing fitted yet)
#else
#define BOARD_NAME "devkitc"
#endif
#ifndef PIN_TX
#define PIN_TX 17          // -> level shifter -> battery J2
#endif
#ifndef PIN_RX
#define PIN_RX 18          // <- 1 kOhm <- battery J1
#endif
#ifndef DEFAULT_LED_PIN
#define DEFAULT_LED_PIN 48 // on-board WS2812 (DevKitC-1 v1.0: 48, v1.1: 38); changeable in the web UI
#endif
#ifndef PIN_BOOT
#define PIN_BOOT 0         // BOOT button: short press = unlock settings 2 min, hold 5 s = forget Wi-Fi
#endif
#define HOST_BAUD 115200
#define AP_PASSWORD "m18bridge"        // setup access point password (min. 8 chars)
#define DISCOVERY_PORT 18018
#define AUTO_IDLE_MS 8000              // TX low after this much inactivity
#define STA_CONNECT_MS 20000
#define AP_CLOSE_AFTER_MS 90000        // keep setup AP open this long after success
#define AP_FALLBACK_MS 60000           // open setup AP after this long without Wi-Fi
#define STA_RETRY_MS 60000             // retry saved Wi-Fi while in setup mode
#define UNLOCK_MS 120000               // settings unlocked after a short BOOT press
#define BUSY_WAIT_MS 25000             // max wait for the battery line (PC vs. web)
#define MAX_CMD_BODY 16384
#define LOW_HEAP_BYTES 30000
#define LOOP_WDT_S 30                  // loop() watchdog. Not the 5 s default: a phone that stalls mid-request
                                       // can block the web server inside a socket read/write for ~10 s.
#define TX_POWER_NORMAL WIFI_POWER_15dBm   // plenty for a bridge next to the phone/router, lower peak current
#define TX_POWER_LOW WIFI_POWER_8_5dBm     // after a brownout: weak USB port/cable or step-down

#define BAT_UART_NUM UART_NUM_1
HardwareSerial BatSerial(1);          // UART1 = BAT_UART_NUM
WebServer server(80);
DNSServer dns;
WiFiUDP disc;
Preferences prefs;
SemaphoreHandle_t batMutex;            // one user of the battery line at a time
SemaphoreHandle_t stateMutex;          // protects lastScan
SemaphoreHandle_t histMutex;           // protects the history store
TaskHandle_t bridgeLoopTask = nullptr;

// ------------------------------------------------------------ console log
// Status lines go to both USB sockets: native USB (Serial) and, with "USB CDC On Boot", the
// UART socket (Serial0). The boot ROM prints on both, so whichever socket a terminal or the
// web flasher's log window uses, it also sees "Setup-WLAN ..." and "M18BRIDGE READY ...".
//
// Every status line also goes into the event log (see m18_log.h): WLAN, PC commands, web page,
// updates, restarts and the battery trace of each read-out. ev() only logs (any task);
// logc() logs and prints (loop task only - console replies must not be interleaved).
m18::EventLog evlog;
SemaphoreHandle_t logMutex = nullptr;
static void logLock() { if (logMutex) xSemaphoreTake(logMutex, portMAX_DELAY); }
static void logUnlock() { if (logMutex) xSemaphoreGive(logMutex); }

// Lines not yet written to flash are mirrored into RTC memory: it survives watchdog resets,
// crashes and brownouts (not a power cut), so the last lines before such a restart are recovered.
#define RTC_LOG_SIZE 1536
#define RTC_LOG_MAGIC 0x4C4F4731UL
RTC_NOINIT_ATTR uint32_t rtcLogMagic;
RTC_NOINIT_ATTR uint32_t rtcLogLen;
RTC_NOINIT_ATTR char rtcLog[RTC_LOG_SIZE];
static void rtcMirror(const std::string& pending) {
  size_t n = pending.size(), from = n > RTC_LOG_SIZE ? n - RTC_LOG_SIZE : 0;
  memcpy(rtcLog, pending.data() + from, n - from);
  rtcLogLen = n - from;
  rtcLogMagic = RTC_LOG_MAGIC;
}

static void evv(const char* cat, int level, const char* fmt, va_list ap) {
  char buf[200];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  size_t l = strlen(buf);
  while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = 0;
  const char* t = buf;
  while (*t == '\n') t++;
  evlog.addText(cat, level, t);
}
void ev(const char* cat, int level, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  evv(cat, level, fmt, ap);
  va_end(ap);
}
void logc(const char* cat, int level, const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.print(buf);
#endif
  ev(cat, level, "%s", buf);
}
void logf(const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  logc("SYS", 0, "%s", buf);
}

// Restart with the reason in the log (flushed to flash first).
void restartNow(const char* why) {
  ev("SYS", 1, "Neustart: %s", why);
  evlog.flush();
  delay(200);
  ESP.restart();
}

// ---------------------------------------------------------- watchdog helper
static inline void feedWdt() {
  if (bridgeLoopTask && xTaskGetCurrentTaskHandle() == bridgeLoopTask) feedLoopWDT();
}

// ------------------------------------------------------------ battery port
// J2 idle (low) is produced by inverting the UART's TX line: an inverted UART idles
// low, exactly like a held break. The UART is installed once at boot; idle/high only
// flips the inversion flag (no pin re-routing, no driver re-install), which behaves
// the same on Arduino-ESP32 2.0.x and 3.x (both use the ESP-IDF UART driver).
class EspPort : public m18::Port {
 public:
  void begin() {
    pinMode(PIN_TX, OUTPUT);
    digitalWrite(PIN_TX, LOW);         // J2 low as early as possible
    // After waking from the energy-saving deep sleep the pin is still held low: release the hold
    // only now that the output latch is low too, so J2 never goes high in between.
    gpio_hold_dis((gpio_num_t)PIN_TX);
    gpio_deep_sleep_hold_dis();
  }
  void startUart() {                   // right after begin(); TX is high for a few µs only
    BatSerial.setRxBufferSize(512);
    BatSerial.begin(4800, SERIAL_8N2, PIN_RX, PIN_TX);
    invertTx(true);
    idle_ = true;
    ready_ = true;
  }
  void setIdle(bool idle) override {
    if (!ready_) return;
    if (idle) {
      BatSerial.flush();               // let the last stop bits go out first
      invertTx(true);
    } else {
      invertTx(false);
    }
    idle_ = idle;
  }
  void flushInput() override { while (BatSerial.available()) BatSerial.read(); }
  void write(const uint8_t* d, size_t n) override {
    if (!ready_ || idle_) return;      // never clock data out while J2 must stay low
    BatSerial.write(d, n);
    BatSerial.flush();
  }
  int readByte(uint32_t timeoutMs) override {
    if (!ready_) return -1;
    uint32_t t0 = ::millis();
    do {
      if (BatSerial.available()) return BatSerial.read();
      delay(1);
      feedWdt();
    } while (::millis() - t0 < timeoutMs);
    return -1;
  }
  void delayMs(uint32_t ms) override {
    while (ms) { uint32_t s = ms > 100 ? 100 : ms; delay(s); ms -= s; feedWdt(); }
  }
  uint32_t millis() override { return ::millis(); }
 private:
  static void invertTx(bool on) {
    uart_set_line_inverse(BAT_UART_NUM, on ? UART_SIGNAL_TXD_INV : UART_SIGNAL_INV_DISABLE);
  }
  bool idle_ = true;
  bool ready_ = false;
};

EspPort port;
m18::Link batLink(port);
m18::Reader reader(batLink, port);
m18::CommandProcessor proc(batLink, reader);

// ---------------------------------------------------------------- storage
// Measurement history lives in the FAT partition ("16M Flash (3MB APP/9.9MB FATFS)");
// LittleFS is used if the board was flashed with a SPIFFS/LittleFS partition instead.
class FsStorage : public m18::Storage {
 public:
  const char* kind = "none";
  bool mount() {
    if (FFat.begin(true)) { fs_ = &FFat; fat_ = true; kind = "FAT"; return true; }
    if (LittleFS.begin(true)) { fs_ = &LittleFS; fat_ = false; kind = "LittleFS"; return true; }
    return false;
  }
  bool ok() override { return fs_ != nullptr; }
  bool mkdir(const std::string& d) override { return fs_->exists(d.c_str()) || fs_->mkdir(d.c_str()); }
  void list(const std::string& d, std::vector<std::string>& names) override {
    File dir = fs_->open(d.c_str());
    if (!dir || !dir.isDirectory()) return;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String n = f.name();
      int slash = n.lastIndexOf('/');
      names.push_back((slash >= 0 ? n.substring(slash + 1) : n).c_str());
      f.close();
      feedWdt();
    }
  }
  bool read(const std::string& p, std::string& out, bool firstLine) override {
    File f = fs_->open(p.c_str(), "r");
    if (!f) return false;
    out.clear();
    char buf[256];
    while (f.available()) {
      int n = f.read((uint8_t*)buf, sizeof(buf));
      if (n <= 0) break;
      if (firstLine) {
        char* nl = (char*)memchr(buf, '\n', n);
        if (nl) { out.append(buf, nl - buf); break; }
      }
      out.append(buf, n);
    }
    f.close();
    return true;
  }
  bool write(const std::string& p, const std::string& data) override {
    std::string tmp = p + ".tmp";
    File f = fs_->open(tmp.c_str(), "w");
    if (!f) return false;
    size_t w = f.write((const uint8_t*)data.data(), data.size());
    f.close();
    if (w != data.size()) { fs_->remove(tmp.c_str()); return false; }
    fs_->remove(p.c_str());
    return fs_->rename(tmp.c_str(), p.c_str());
  }
  bool remove(const std::string& p) override { return fs_->remove(p.c_str()); }
  bool rename(const std::string& a, const std::string& b) override { return fs_->rename(a.c_str(), b.c_str()); }
  uint64_t freeBytes() override {
    if (!fs_) return 0;
    return fat_ ? (uint64_t)FFat.freeBytes() : (uint64_t)(LittleFS.totalBytes() - LittleFS.usedBytes());
  }
  uint64_t totalBytes() override {
    if (!fs_) return 0;
    return fat_ ? (uint64_t)FFat.totalBytes() : (uint64_t)LittleFS.totalBytes();
  }
 private:
  fs::FS* fs_ = nullptr;
  bool fat_ = false;
};
FsStorage storage;
m18::History history(storage);

static void histLock() { xSemaphoreTake(histMutex, portMAX_DELAY); }
static void histUnlock() { xSemaphoreGive(histMutex); }

static uint32_t unixNow() {
  time_t t = time(nullptr);
  return t > 1700000000 ? (uint32_t)t : 0;   // 0 = not synced, reader then uses the battery clock
}

// Time source for stored records: NTP when online; otherwise the phone's browser or
// the PC set the clock (TIME command / read request), so offline records get a date.
String timeSource = "none";
uint32_t bootId = 0;
String scanLoc, scanComment;                 // entered on the web page for the next read-out

static bool setClockFrom(uint32_t t, const char* source) {
  if (unixNow() || t < 1700000000UL) return false;    // already valid (NTP) or nonsense
  struct timeval tv = {(time_t)t, 0};
  settimeofday(&tv, nullptr);
  timeSource = source;
  ev("SYS", 0, "Uhr gestellt (Quelle: %s)", source);
  return true;
}
static bool setClockFromPc(uint32_t t) { return setClockFrom(t, "pc"); }

static void recordMeta(m18::RecordMeta& m) {
  m.ts = unixNow();
  if (m.ts && timeSource == "none") timeSource = "ntp";
  m.tsSrc = m.ts ? timeSource.c_str() : "none";
  m.boot = bootId;
  m.upMs = millis();
  m.loc = scanLoc.c_str();
  m.comment = scanComment.c_str();
}

// Take the battery line, waiting (and feeding the watchdog) if the other side is busy.
static bool lockBattery(uint32_t maxWaitMs) {
  uint32_t t0 = millis();
  while (xSemaphoreTake(batMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    feedWdt();
    if (millis() - t0 > maxWaitMs) return false;
  }
  return true;
}
static void unlockBattery() { xSemaphoreGive(batMutex); }

// ------------------------------------------------------------------- state
enum class WState { AP, CONNECTING, STA, FAILED };
WState wstate = WState::AP;
String savedSsid, savedPass, hostName, apName, wifiError;
String pendSsid, pendPass;
bool apActive = false, dnsActive = false, portalConnect = false;
uint32_t connectStart = 0, apCloseAt = 0, lostSince = 0, lastStaRetry = 0;
volatile bool gotIp = false;
uint32_t unlockUntil = 0;

String lastScan = "{}";
String webPass;                               // optional web/HTTP password (user "m18"), empty = off
uint32_t authFails = 0;
volatile bool scanRunning = false;
volatile bool scanResultOk = false;
uint32_t scanDoneAt = 0;
bool otaRunning = false;
uint32_t lowHeapSince = 0;

String infoExtra;

// ------------------------------------------------------- power supply
// Wi-Fi start-up (RF calibration, first beacons) pulls current peaks of several 100 mA. On a weak
// USB port/cable or a step-down set too low the voltage dips and the brownout detector resets the
// chip - often several times in a row. We count those resets (RTC memory survives them) and run
// Wi-Fi with lower transmit power after one, so the bridge comes up instead of boot-looping.
RTC_NOINIT_ATTR uint32_t rtcBrownMagic;
RTC_NOINIT_ATTR uint32_t rtcBrownouts;
uint32_t brownouts = 0;                 // since power-on
bool lowTxPower = false;

void powerCheck() {
  esp_reset_reason_t r = esp_reset_reason();
  if (rtcBrownMagic != 0x4D313842UL || r == ESP_RST_POWERON) { rtcBrownMagic = 0x4D313842UL; rtcBrownouts = 0; }
  if (r == ESP_RST_BROWNOUT) rtcBrownouts++;
  brownouts = rtcBrownouts;
  lowTxPower = (r == ESP_RST_BROWNOUT);
}

// Every Wi-Fi mode change goes through here so the transmit power is applied to the running driver.
void wifiMode(wifi_mode_t m) {
  WiFi.mode(m);
  if (m != WIFI_OFF) WiFi.setTxPower(lowTxPower ? TX_POWER_LOW : TX_POWER_NORMAL);
}

// Task watchdog for loop(): longer timeout than the core's 5 s (see LOOP_WDT_S).
void setupWatchdog() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  uint32_t idleMask = 0;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  idleMask |= 1;
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  idleMask |= 2;
#endif
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = LOOP_WDT_S * 1000;
  cfg.idle_core_mask = idleMask;
  cfg.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(LOOP_WDT_S, true);    // ESP-IDF 4.4: re-init just changes timeout/panic
#endif
}

// --------------------------------------------------------- energy saving
// In the slide-on housing the bridge is powered from the battery (B+ -> step-down -> 5V).
// Awake it draws ~30-40 mA at 18 V, which would empty a pack in about a week, so after
// `sleepMin` minutes without any use it goes to deep sleep (0 = never). J2 stays held low,
// the BOOT button wakes it up again. The step-down and the level shifter's pull-up still
// draw a few mA while asleep - a power switch (or unplugging) is the real cure.
uint32_t sleepMin = 30;
uint32_t lastUserActivity = 0;
void noteActivity() { lastUserActivity = millis(); }
long sleepInSeconds() {
  if (!sleepMin) return -1;
  long left = (long)(sleepMin * 60000UL) - (long)(millis() - lastUserActivity);
  return left > 0 ? left / 1000 : 0;
}

// --------------------------------------------------------------------- LED
// On-board WS2812. Pin and brightness are stored in Preferences (web UI: Gerät).
int ledPin = DEFAULT_LED_PIN;
uint8_t ledBright = 40;                // 0..255, scaled per colour
uint32_t ledTestUntil = 0;
uint32_t lastLedRgb = 0xFFFFFFFF, lastLedWrite = 0;

// Result of the last read-out on the LED for RESULT_SHOW_MS: green = good, yellow = watch,
// red = weak/critical, red blinking = read-out failed. Also used for read-outs started by the
// PC (it sends "SHOW <status>" after evaluating the reading against its database).
enum ResultKind : uint8_t { RES_NONE = 0, RES_GOOD, RES_WATCH, RES_WEAK, RES_FAIL };
uint8_t resultKind = RES_NONE;
uint32_t resultUntil = 0;
#define RESULT_SHOW_MS 30000
void showResult(uint8_t kind) {
  resultKind = kind;
  resultUntil = millis() + (kind == RES_FAIL ? 8000 : RESULT_SHOW_MS);
}
uint8_t resultFromStatus(const String& st) {
  if (st == "good") return RES_GOOD;
  if (st == "watch") return RES_WATCH;
  if (st == "weak" || st == "critical") return RES_WEAK;
  if (st == "fail") return RES_FAIL;
  return RES_NONE;
}
void showScanResult(const String& json, bool ok) {
  if (!ok) { showResult(RES_FAIL); return; }
  int p = json.indexOf("\"status\":\"");
  String st = p >= 0 ? json.substring(p + 10, json.indexOf('"', p + 10)) : String("");
  bool lowCell = json.indexOf("tiefentladen") >= 0;          // warning: cell < 2.5 V
  uint8_t k = resultFromStatus(st);
  if (lowCell) k = RES_WEAK;
  showResult(k ? k : (uint8_t)RES_GOOD);
}

#if defined(PIN_RGB_R)
// Discrete RGB LED on the SuperMini PCB: 8-bit PWM per colour, shows the same colour as the WS2812.
static bool rgbReady = false;
static void rgbBegin() {
  const int pins[3] = {PIN_RGB_R, PIN_RGB_G, PIN_RGB_B};
  for (int i = 0; i < 3; i++) {
    gpio_hold_dis((gpio_num_t)pins[i]);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(pins[i], 5000, 8);
    ledcWrite(pins[i], 0);
#else
    ledcSetup(i + 4, 5000, 8);         // channels 4-6: keep clear of anything the core uses first
    ledcAttachPin(pins[i], i + 4);
    ledcWrite(i + 4, 0);
#endif
  }
  rgbReady = true;
}
static void rgbWrite(uint8_t r, uint8_t g, uint8_t b) {
  if (!rgbReady) return;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_RGB_R, r); ledcWrite(PIN_RGB_G, g); ledcWrite(PIN_RGB_B, b);
#else
  ledcWrite(4, r); ledcWrite(5, g); ledcWrite(6, b);
#endif
}
static void rgbOffForSleep() {         // NPN bases low and held: no glowing LED in deep sleep
  const int pins[3] = {PIN_RGB_R, PIN_RGB_G, PIN_RGB_B};
  for (int i = 0; i < 3; i++) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcDetach(pins[i]);
#else
    ledcDetachPin(pins[i]);
#endif
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
    gpio_hold_en((gpio_num_t)pins[i]);
  }
}
#endif

static void ledWrite(int pin, uint8_t r, uint8_t g, uint8_t b) {
#if defined(PIN_RGB_R)
  rgbWrite(r, g, b);
#endif
  if (pin < 0) return;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  rgbLedWrite(pin, r, g, b);
#else
  neopixelWrite(pin, r, g, b);
#endif
}

static void led(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t rgb = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
  // write on change, refresh every 2 s (recovers from glitches on the data line)
  if (rgb == lastLedRgb && millis() - lastLedWrite < 2000) return;
  lastLedRgb = rgb;
  lastLedWrite = millis();
  ledWrite(ledPin, (uint16_t)r * ledBright / 255, (uint16_t)g * ledBright / 255, (uint16_t)b * ledBright / 255);
}

// Pins the LED may never use on the S3: battery UART, BOOT, native USB (19/20),
// flash/PSRAM of the N16R8 (26-37), UART0 of the second USB socket (43/44).
static bool ledPinAllowed(int pin) {
  if (pin == -1) return true;
  if (pin < 1 || pin > 48) return false;
  if (pin == PIN_TX || pin == PIN_RX || pin == PIN_BOOT || pin == 19 || pin == 20 || pin == 43 || pin == 44) return false;
#if defined(PIN_RGB_R)
  if (pin == PIN_RGB_R || pin == PIN_RGB_G || pin == PIN_RGB_B || pin == PIN_CHG_EN) return false;
#endif
  return !(pin >= 26 && pin <= 37);
}

static void setLedPin(int pin) {
  if (pin == ledPin) return;
  ledWrite(ledPin, 0, 0, 0);
  ledPin = pin;
  lastLedRgb = 0xFFFFFFFF;
}

void ledLoop(uint32_t bootHeldMs) {
  uint32_t now = millis();
  bool fast = (now / 125) % 2, slow = (now / 500) % 2;
  if (ledTestUntil && (int32_t)(ledTestUntil - now) > 0) {     // test: red, green, blue, white
    switch (((ledTestUntil - now) / 500) % 4) {
      case 3: led(255, 0, 0); break;
      case 2: led(0, 255, 0); break;
      case 1: led(0, 0, 255); break;
      default: led(255, 255, 255);
    }
    return;
  }
  if (bootHeldMs > 5000) { led(255, 0, 0); return; }                      // forgetting Wi-Fi
  if (bootHeldMs > 1500) { led(fast ? 255 : 0, 0, 0); return; }           // keep holding...
  if (otaRunning) { led(fast ? 160 : 0, 0, fast ? 255 : 0); return; }     // firmware update
  if (scanRunning) { led(255, slow ? 160 : 60, 0); return; }              // reading the battery
  if (!batLink.isIdle()) { led(slow ? 200 : 40, slow ? 200 : 40, slow ? 200 : 40); return; }  // PC session
  if (resultKind && (int32_t)(resultUntil - now) > 0) {                   // result of the last read-out
    switch (resultKind) {
      case RES_GOOD: led(0, 255, 0); break;
      case RES_WATCH: led(255, 150, 0); break;
      case RES_WEAK: led(255, 0, 0); break;
      default: led(fast ? 255 : 0, 0, 0);
    }
    return;
  }
  if (unlockUntil && (int32_t)(unlockUntil - now) > 0) { led(0, slow ? 200 : 40, slow ? 200 : 40); return; }
  if (wstate == WState::CONNECTING) { led(120, 0, 160); return; }
  if (WiFi.status() == WL_CONNECTED && !apActive) { led(0, 70, 0); return; }
  led(0, 0, slow ? 255 : 30);                                            // setup mode
}

void goToSleep() {
  logf("Energiesparmodus: Tiefschlaf nach %u min ohne Nutzung. Aufwecken mit der BOOT-Taste.\n", (unsigned)sleepMin);
  evlog.flush();
  if (lockBattery(3000)) { batLink.idle(); unlockBattery(); }
  BatSerial.flush();
  BatSerial.end();
  pinMode(PIN_TX, OUTPUT);
  digitalWrite(PIN_TX, LOW);                    // J2 low ...
  gpio_hold_en((gpio_num_t)PIN_TX);             // ... and held low during deep sleep
  gpio_deep_sleep_hold_en();
  ledWrite(ledPin, 0, 0, 0);
#if defined(PIN_RGB_R)
  rgbOffForSleep();
#endif
  MDNS.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT, 0);   // BOOT pressed = low
  delay(100);
  esp_deep_sleep_start();
}

// ------------------------------------------------------------------ helpers
String jsonEsc(String v) {
  v.replace("\\", "\\\\");
  v.replace("\"", "\\\"");
  return v;
}

bool unlocked() { return unlockUntil && (int32_t)(unlockUntil - millis()) > 0; }

// Requests from the setup access point count as physical presence.
bool clientOnSetupAp() { return apActive && server.client().localIP() == WiFi.softAPIP(); }
bool settingsAllowed() { return clientOnSetupAp() || unlocked(); }

String sanitizeHost(String h) {
  h.trim();
  h.toLowerCase();
  String out;
  for (unsigned i = 0; i < h.length() && out.length() < 30; i++) {
    char c = h[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') out += c;
  }
  while (out.length() && out[0] == '-') out.remove(0, 1);
  while (out.length() && out[out.length() - 1] == '-') out.remove(out.length() - 1);
  return out;
}

const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "Einschalten";
    case ESP_RST_SW: return "Neustart (Software)";
    case ESP_RST_PANIC: return "Absturz";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "Watchdog";
    case ESP_RST_BROWNOUT: return "Unterspannung (Brownout)";
    case ESP_RST_DEEPSLEEP: return "Aufgewacht aus dem Energiesparmodus";
    case ESP_RST_EXT: return "Reset-Taste";
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    case ESP_RST_USB: return "USB";          // not in ESP-IDF 4.4 (Arduino core 2.0.x)
#endif
    default: return "unbekannt";
  }
}

String ipString() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return apActive ? WiFi.softAPIP().toString() : String("");
}

// ------------------------------------------------------------------ Wi-Fi
void loadPrefs() {
  prefs.begin("m18", true);
  savedSsid = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  hostName = prefs.getString("host", "");
  ledPin = prefs.getInt("ledpin", DEFAULT_LED_PIN);
  if (!ledPinAllowed(ledPin)) ledPin = DEFAULT_LED_PIN;
  ledBright = (uint8_t)prefs.getInt("ledbright", 40);
  sleepMin = (uint32_t)prefs.getInt("sleepmin", 30);
  webPass = prefs.getString("webpass", "");
  prefs.end();
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[8];
  snprintf(suffix, sizeof(suffix), "%02x%02x", mac[4], mac[5]);
  hostName = sanitizeHost(hostName);
  if (hostName.isEmpty()) hostName = String("m18-bridge-") + suffix;
  String up = suffix;
  up.toUpperCase();
  apName = String("M18-Bridge-") + up;
}

void saveWifi(const String& ssid, const String& pass) {
  prefs.begin("m18", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("host", hostName);
  prefs.end();
  savedSsid = ssid;
  savedPass = pass;
  ev("WLAN", 0, ssid.length() ? "WLAN-Zugang gespeichert: \"%s\"" : "WLAN-Zugang gelöscht%s", ssid.c_str());
}

void savePassword(const String& pw) {
  prefs.begin("m18", false);
  prefs.putString("webpass", pw);
  prefs.end();
  webPass = pw;
  ev("SYS", 0, pw.length() ? "Passwortschutz eingeschaltet/geändert" : "Passwortschutz ausgeschaltet");
}

String apPassword() { return webPass.length() >= 8 ? webPass : String(AP_PASSWORD); }

void saveSleep() {
  prefs.begin("m18", false);
  prefs.putInt("sleepmin", (int)sleepMin);
  prefs.end();
  ev("SYS", 0, "Energiesparen: Tiefschlaf nach %u min (0 = aus)", (unsigned)sleepMin);
}

void saveLed() {
  prefs.begin("m18", false);
  prefs.putInt("ledpin", ledPin);
  prefs.putInt("ledbright", ledBright);
  prefs.end();
  ev("SYS", 0, "Status-LED: GPIO%d, Helligkeit %d", ledPin, ledBright);
}

void startMdns() {
  MDNS.end();
  if (MDNS.begin(hostName.c_str())) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("m18bridge", "tcp", 80);
    MDNS.addServiceTxt("m18bridge", "tcp", "version", M18BRIDGE_VERSION);
  }
}

void onWifiEvent(arduino_event_id_t event) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) gotIp = true;   // handled in loop()
}

void startAP() {
  if (apActive) return;
  wifiMode(savedSsid.length() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(apName.c_str(), apPassword().c_str());
  WiFi.setSleep(false);
  apActive = true;
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());   // captive portal: every name -> us
  dnsActive = true;
  apCloseAt = 0;
  if (wstate != WState::CONNECTING) wstate = WState::AP;
  logc("WLAN", 0, "Setup-WLAN \"%s\" (Passwort %s) -> http://%s\n", apName.c_str(),
                webPass.length() >= 8 ? "= Web-Passwort" : AP_PASSWORD,
                WiFi.softAPIP().toString().c_str());
}

void stopAP() {
  if (!apActive) return;
  if (dnsActive) { dns.stop(); dnsActive = false; }
  WiFi.softAPdisconnect(true);
  wifiMode(WIFI_STA);
  WiFi.setSleep(false);
  apActive = false;
  apCloseAt = 0;
  ev("WLAN", 0, "Einrichtungs-WLAN geschlossen");
}

void beginConnect(const String& ssid, const String& pass) {
  wifiMode(apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);                // no modem sleep: reliable web server + discovery
  WiFi.setHostname(hostName.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  ev("WLAN", 0, "Verbinde mit \"%s\"%s", ssid.c_str(), portalConnect ? " (über die Einrichtungsseite)" : "");
  wstate = WState::CONNECTING;
  connectStart = millis();
  wifiError = "";
}

void onConnected() {
  wstate = WState::STA;
  lostSince = 0;
  WiFi.setAutoReconnect(true);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  if (portalConnect) {                 // credentials entered on the web page: keep them now
    saveWifi(pendSsid, pendPass);
    portalConnect = false;
  }
  if (apActive) apCloseAt = millis() + AP_CLOSE_AFTER_MS;
  logc("WLAN", 0, "WLAN verbunden: %s, http://%s.local (%s), %d dBm, Kanal %d\n", WiFi.SSID().c_str(), hostName.c_str(),
       WiFi.localIP().toString().c_str(), (int)WiFi.RSSI(), (int)WiFi.channel());
}

void wifiLoop() {
  if (gotIp) {                         // new/changed IP: re-announce via mDNS
    gotIp = false;
    startMdns();
  }
  wl_status_t st = WiFi.status();
  if (wstate == WState::CONNECTING) {
    if (st == WL_CONNECTED) onConnected();
    else if (millis() - connectStart > STA_CONNECT_MS) {
      bool fromPortal = portalConnect;          // entered on the web page vs. quiet background retry
      wstate = fromPortal ? WState::FAILED : WState::AP;
      if (fromPortal)
        wifiError = (st == WL_NO_SSID_AVAIL) ? "Netzwerk nicht gefunden" : "Verbindung fehlgeschlagen – Passwort prüfen";
      ev("WLAN", 1, "Verbindung mit \"%s\" fehlgeschlagen (Status %d%s)", WiFi.SSID().c_str(), (int)st,
         st == WL_NO_SSID_AVAIL ? ", Netzwerk nicht gefunden" : st == WL_CONNECT_FAILED ? ", Passwort?" : "");
      WiFi.setAutoReconnect(false);
      WiFi.disconnect();
      portalConnect = false;
      lastStaRetry = millis();                  // next quiet retry in STA_RETRY_MS, not immediately
      if (!apActive) startAP();
    }
  } else if (wstate == WState::STA) {
    if (st != WL_CONNECTED) {
      if (!lostSince) { lostSince = millis(); ev("WLAN", 1, "WLAN-Verbindung verloren (Status %d)", (int)st); }
      if (millis() - lostSince > AP_FALLBACK_MS && !apActive) {   // network gone: allow reconfiguration
        ev("WLAN", 1, "seit %lu s kein WLAN – Einrichtungs-WLAN wird geöffnet", (unsigned long)(AP_FALLBACK_MS / 1000));
        WiFi.setAutoReconnect(false);
        startAP();
        wstate = WState::AP;
        lastStaRetry = millis();
      }
    } else {
      if (lostSince) ev("WLAN", 0, "WLAN wieder verbunden nach %lu s, %d dBm", (unsigned long)((millis() - lostSince) / 1000), (int)WiFi.RSSI());
      lostSince = 0;
    }
  } else if (savedSsid.length() && apActive && WiFi.softAPgetStationNum() == 0 &&
             millis() - lastStaRetry > STA_RETRY_MS) {
    // In setup mode with saved credentials: retry quietly while nobody uses the setup AP.
    lastStaRetry = millis();
    beginConnect(savedSsid, savedPass);
  }
  if (apCloseAt && (int32_t)(millis() - apCloseAt) > 0 && WiFi.status() == WL_CONNECTED) stopAP();
}

const char* stateName() {
  switch (wstate) {
    case WState::CONNECTING: return "connecting";
    case WState::STA: return WiFi.status() == WL_CONNECTED ? "connected" : "connecting";
    case WState::FAILED: return "failed";
    default: return "ap";
  }
}

void updateInfo() {
  bool sta = WiFi.status() == WL_CONNECTED;
  int unlockS = unlocked() ? (int)((int32_t)(unlockUntil - millis()) / 1000) : 0;
  infoExtra = String("\"wifi\":\"") + (sta ? "sta" : "ap") + "\",\"ip\":\"" + ipString() +
              "\",\"host\":\"" + hostName + ".local\",\"ssid\":\"" + jsonEsc(sta ? WiFi.SSID() : savedSsid) +
              "\",\"ap\":\"" + (apActive ? apName : String("")) + "\",\"ap_ip\":\"" +
              (apActive ? WiFi.softAPIP().toString() : String("")) + "\",\"rssi\":" + String(sta ? WiFi.RSSI() : 0) +
              ",\"mac\":\"" + WiFi.macAddress() + "\",\"uptime_s\":" + String(millis() / 1000) +
              ",\"heap_kb\":" + String(ESP.getFreeHeap() / 1024) + ",\"min_heap_kb\":" + String(ESP.getMinFreeHeap() / 1024) +
              ",\"psram_kb\":" + String(ESP.getFreePsram() / 1024) + ",\"reset_reason\":\"" + resetReason() +
              "\",\"build\":\"" FW_BUILD "\",\"board\":\"" BOARD_NAME "\",\"led_pin\":" + String(ledPin) + ",\"led_brightness\":" + String(ledBright) +
              ",\"unlocked\":" + (unlocked() ? "true" : "false") + ",\"unlock_s\":" + String(unlockS) +
              ",\"setup_client\":" + (clientOnSetupAp() ? "true" : "false") +
              ",\"reading\":" + (scanRunning ? "true" : "false") + ",\"auth\":" + (webPass.length() ? "true" : "false") +
              ",\"storage\":\"" + storage.kind + "\",\"time_src\":\"" + (unixNow() ? timeSource : String("none")) + "\",\"boot\":" + String((unsigned long)bootId) +
              ",\"sleep_min\":" + String((unsigned long)sleepMin) + ",\"sleep_in_s\":" + String((long)sleepInSeconds()) +
              ",\"brownouts\":" + String((unsigned long)brownouts) + ",\"tx_low\":" + (lowTxPower ? "true" : "false");
  uint32_t now = unixNow();
  if (now) {
    char b[32];
    time_t t = now;
    struct tm g;
    gmtime_r(&t, &g);
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M UTC", &g);
    infoExtra += String(",\"time\":\"") + b + "\"";
  }
  proc.setInfoExtra(infoExtra.c_str());
}

// ------------------------------------------------------------- commands
static bool isBatteryCommand(const String& up) {
  return up.startsWith("RESET") || up.startsWith("IDLE") || up.startsWith("HIGH") || up.startsWith("READ") ||
         up.startsWith("NOTE") || up.startsWith("SCAN");
}

bool backgroundRequest = false;    // set while an HTTP handler serves an automatic background sync
String cmdSource = "USB";          // who sent the command (for the event log)

String runCommandInner(const String& lineIn, bool physical);

// Runs a command and writes it to the event log (passwords masked; frequent polling commands only on errors).
String runCommand(const String& lineIn, bool physical) {
  String r = runCommandInner(lineIn, physical);
  String line = lineIn;
  line.trim();
  int sp = line.indexOf(' ');
  String cmd = sp < 0 ? line : line.substring(0, sp);
  cmd.toUpperCase();
  bool err = r.startsWith("ERR");
  static const char* quiet[] = {"PING", "INFO", "LOG", "HSTAT", "HLIST", "HGET", "HACK", "IDLE", "HIGH", "TIME", "SHOW", "READ"};
  bool isQuiet = false;
  for (const char* q : quiet) if (cmd == q) isQuiet = true;
  if (isQuiet && !err) return r;
  String shown = line;
  if (cmd == "WIFI") { int bar = shown.indexOf('|'); if (bar >= 0) shown = shown.substring(0, bar) + "|***"; }
  if (cmd == "PASS") shown = "PASS ***";
  String res = r.length() > 90 ? r.substring(0, 90) + "…" : r;
  if (cmd == "SCAN") {                       // just the essentials, the battery trace is logged separately
    auto field = [&](const char* k) {
      int a = r.indexOf(String("\"") + k + "\":\"");
      if (a < 0) return String("");
      a += strlen(k) + 4;
      return r.substring(a, r.indexOf('"', a));
    };
    res = err ? "ERR " + field("error") : "OK " + field("eserial") + " " + field("model");
  }
  ev("PC", err ? 1 : 0, "%s: %s -> %s", cmdSource.c_str(), shown.c_str(), res.c_str());
  return r;
}

String runCommandInner(const String& lineIn, bool physical) {
  if (!backgroundRequest) noteActivity();
  String line = lineIn;
  line.trim();
  String up = line;
  up.toUpperCase();
  if (up.startsWith("WIFI ") || up.startsWith("HOST ")) {
    if (!physical) return "ERR LOCKED (nur über USB, Einrichtungs-WLAN oder nach BOOT-Tastendruck)";
    if (up.startsWith("HOST ")) {
      String h = sanitizeHost(line.substring(5));
      if (h.isEmpty()) return "ERR HOST (a-z, 0-9, -)";
      hostName = h;
      saveWifi(savedSsid, savedPass);
      return "OK saved, restart with RESTART";
    }
    String rest = line.substring(5);
    if (rest.equalsIgnoreCase("OFF")) { saveWifi("", ""); return "OK saved, restart with RESTART"; }
    int bar = rest.indexOf('|');
    String ssid = bar < 0 ? rest : rest.substring(0, bar), pass = bar < 0 ? String("") : rest.substring(bar + 1);
    if (ssid.isEmpty() || ssid.length() > 32) return "ERR SSID (1-32 Zeichen)";
    if (pass.length() && (pass.length() < 8 || pass.length() > 63)) return "ERR PASSWORD (8-63 Zeichen)";
    saveWifi(ssid, pass);
    return "OK saved, restart with RESTART";
  }
  if (up.startsWith("PASS ")) {                 // PASS <password>  or  PASS OFF
    if (!physical) return "ERR LOCKED";
    String pw = line.substring(5);
    if (pw.equalsIgnoreCase("OFF")) { savePassword(""); return "OK password off"; }
    if (pw.length() < 4 || pw.length() > 63) return "ERR PASSWORD (4-63 Zeichen)";
    savePassword(pw);
    return "OK password set";
  }
  if (up.startsWith("SHOW ")) {                  // SHOW good|watch|weak|critical|fail - LED result from the PC
    String st = line.substring(5);
    st.trim();
    st.toLowerCase();
    uint8_t k = resultFromStatus(st);
    if (!k) return "ERR ARGS (good|watch|weak|critical|fail)";
    showResult(k);
    return "OK";
  }
  if (up == "RESTART") {
    if (!physical) return "ERR LOCKED";
    Serial.println("OK restarting");
    restartNow((String("Befehl RESTART (") + cmdSource + ")").c_str());
    return "OK";
  }
  updateInfo();
  bool battery = isBatteryCommand(up);
  if (battery && !lockBattery(BUSY_WAIT_MS)) return "ERR BUSY (Akku wird gerade ausgelesen)";
  bool scan = up.startsWith("SCAN");
  if (scan) { scanRunning = true; scanLoc = ""; scanComment = ""; }
  std::string reply = proc.handle(line.c_str());
  if (scan) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    lastScan = proc.lastScanJson();
    xSemaphoreGive(stateMutex);
    scanResultOk = reader.ok();
    scanDoneAt = millis();
    scanRunning = false;
    showScanResult(String(proc.lastScanJson()), reader.ok());
  }
  if (battery) unlockBattery();
  return String(reply.c_str());
}

// Web read-out runs in its own task so the web server, LED and USB stay responsive.
void scanTask(void*) {
  String result;
  if (lockBattery(BUSY_WAIT_MS)) {
    uint32_t id = proc.scan();                   // reads, decodes and stores in the history
    result = proc.lastScanJson();
    if (id) result = result.substring(0, result.length() - 1) + ",\"history_id\":" + String(id) + "}";
    unlockBattery();
  } else {
    result = "{\"ok\":false,\"error\":\"Akku-Leitung belegt (PC liest gerade)\"}";
  }
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  lastScan = result;
  xSemaphoreGive(stateMutex);
  scanResultOk = reader.ok();
  scanDoneAt = millis();
  showScanResult(result, reader.ok());
  scanRunning = false;
  vTaskDelete(nullptr);
}

bool startScan() {
  if (scanRunning || otaRunning) return false;
  scanRunning = true;
  if (xTaskCreatePinnedToCore(scanTask, "m18scan", 8192, nullptr, 1, nullptr, 1) != pdPASS) {
    scanRunning = false;
    return false;
  }
  return true;
}

// ------------------------------------------------------------------- HTTP
void sendJson(const String& s, int code = 200) { server.send(code, "application/json", s); }

bool captiveRedirect() {
  // Phones probe random URLs when joining the setup AP; send them to our page.
  // Only for clients on the setup AP - not for the customer's network.
  if (!clientOnSetupAp()) return false;
  String host = server.hostHeader();
  if (host == WiFi.softAPIP().toString() || host == hostName + ".local") return false;
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
  return true;
}

// Optional password (HTTP Basic, user "m18"). Wrong attempts are slowed down.
bool authOk() {
  if (webPass.isEmpty()) return true;
  if (server.authenticate("m18", webPass.c_str())) { authFails = 0; return true; }
  if (server.header("Authorization").length()) {
    authFails++;
    ev("WEB", 1, "Falsches Passwort von %s (%d. Versuch)", server.client().remoteIP().toString().c_str(), authFails);
    delay(authFails * 250 > 2000 ? 2000 : authFails * 250);
  }
  server.requestAuthentication(BASIC_AUTH, "M18-Bruecke", "Passwort erforderlich");
  return false;
}

// Wraps a handler with the password check.
std::function<void()> guarded(std::function<void()> fn) {
  return [fn]() {
    // Automatic background sync of the PC software must not keep a battery-powered bridge awake.
    backgroundRequest = server.hasHeader("X-M18-Background");
    cmdSource = String("Webseite ") + server.client().remoteIP().toString();
    if (!backgroundRequest) noteActivity();
    if (authOk()) fn();
    backgroundRequest = false;
  };
}

// Runs a history command and sends its JSON (strips "OK ").
void sendCmdJson(const String& cmd) {
  String r = runCommand(cmd, false);
  if (r.startsWith("OK ")) sendJson(r.substring(3));
  else sendJson(String("{\"ok\":false,\"error\":\"") + jsonEsc(r) + "\"}", 400);
}

bool requireSettings() {
  if (settingsAllowed()) return true;
  sendJson("{\"ok\":false,\"locked\":true,\"error\":\"Gesperrt: kurz die BOOT-Taste am Gerät drücken "
           "(entsperrt 2 Minuten) oder über das Einrichtungs-WLAN ändern.\"}", 403);
  return false;
}

void setupHttp() {
  server.on("/", HTTP_GET, []() {
    noteActivity();
    if (captiveRedirect()) return;
    if (!authOk()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", WEB_UI);
  });
  server.on("/cmd", HTTP_POST, guarded([]() {
    String body = server.arg("plain"), out;
    if (body.length() > MAX_CMD_BODY) { server.send(413, "text/plain", "ERR TOO LARGE\n"); return; }
    bool physical = settingsAllowed();
    cmdSource = String("PC ") + server.client().remoteIP().toString() + (backgroundRequest ? " (autom. Abgleich)" : "");
    int start = 0, count = 0;
    while (start <= (int)body.length() && count < 400) {
      int nl = body.indexOf('\n', start);
      if (nl < 0) nl = body.length();
      String line = body.substring(start, nl);
      line.trim();
      if (line.length()) { out += runCommand(line, physical); out += '\n'; count++; }
      start = nl + 1;
    }
    server.send(200, "text/plain; charset=utf-8", out);
  }));
  server.on("/api/info", HTTP_GET, guarded([]() {
    updateInfo();
    std::string r = proc.handle("INFO");
    sendJson(String(r.c_str() + 3));
  }));
  server.on("/api/read", HTTP_POST, guarded([]() {
    if (server.hasArg("t")) setClockFrom((uint32_t)strtoul(server.arg("t").c_str(), nullptr, 10), "browser");
    if (!scanRunning) {
      scanLoc = server.arg("loc").substring(0, 60);
      scanComment = server.arg("comment").substring(0, 200);
    }
    if (startScan()) {
      ev("WEB", 0, "Auslesen über die Webseite gestartet (%s)%s%s", server.client().remoteIP().toString().c_str(),
         scanLoc.length() ? ", Standort: " : "", scanLoc.c_str());
      sendJson("{\"ok\":true,\"started\":true}");
    } else {
      sendJson("{\"ok\":false,\"busy\":true}", 409);
    }
  }));
  server.on("/api/status", HTTP_GET, guarded([]() {
    sendJson(String("{\"reading\":") + (scanRunning ? "true" : "false") + ",\"progress\":" +
             String(scanRunning ? reader.progressPercent() : 100) + ",\"done_ms_ago\":" +
             String(scanDoneAt ? millis() - scanDoneAt : 0) + "}");
  }));
  server.on("/api/last", HTTP_GET, guarded([]() {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    String copy = lastScan;
    xSemaphoreGive(stateMutex);
    sendJson(copy);
  }));
  server.on("/api/networks", HTTP_GET, guarded([]() {
    // Asynchronous scan: first call starts it, the page polls until the list is ready.
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); sendJson("{\"scanning\":true}"); return; }
    if (n == WIFI_SCAN_RUNNING) { sendJson("{\"scanning\":true}"); return; }
    String out = "{\"scanning\":false,\"networks\":[";
    int shown = 0;
    for (int i = 0; i < n && shown < 25; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;
      if (shown++) out += ",";
      out += "{\"ssid\":\"" + jsonEsc(ssid) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
             ",\"secure\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
    }
    WiFi.scanDelete();
    sendJson(out + "]}");
  }));
  server.on("/api/wifi", HTTP_GET, guarded([]() {
    bool sta = WiFi.status() == WL_CONNECTED;
    int closesIn = apCloseAt ? (int)((int32_t)(apCloseAt - millis()) / 1000) : 0;
    sendJson(String("{\"state\":\"") + stateName() + "\",\"ssid\":\"" +
             jsonEsc(portalConnect || wstate == WState::FAILED ? pendSsid : (sta ? WiFi.SSID() : savedSsid)) +
             "\",\"ip\":\"" + (sta ? WiFi.localIP().toString() : String("")) + "\",\"host\":\"" + hostName +
             ".local\",\"rssi\":" + String(sta ? WiFi.RSSI() : 0) + ",\"ap\":\"" + apName +
             "\",\"ap_closes_in\":" + String(closesIn > 0 ? closesIn : 0) + ",\"error\":\"" + jsonEsc(wifiError) +
             "\",\"allowed\":" + (settingsAllowed() ? "true" : "false") + "}");
  }));
  server.on("/api/wifi", HTTP_POST, guarded([]() {
    if (!requireSettings()) return;
    String ssid = server.arg("ssid"), pass = server.arg("pass"), h = sanitizeHost(server.arg("host"));
    if (ssid.isEmpty() || ssid.length() > 32) { sendJson("{\"ok\":false,\"error\":\"SSID ungültig\"}", 400); return; }
    if (pass.length() && (pass.length() < 8 || pass.length() > 63)) {
      sendJson("{\"ok\":false,\"error\":\"Passwort muss 8–63 Zeichen haben (oder leer für offenes WLAN)\"}", 400);
      return;
    }
    pendSsid = ssid;
    pendPass = pass;
    if (h.length()) hostName = h;
    portalConnect = true;
    if (!apActive && WiFi.status() != WL_CONNECTED) startAP();
    beginConnect(pendSsid, pendPass);
    startMdns();                                  // re-announce with the (possibly) new name
    sendJson("{\"ok\":true}");
  }));
  server.on("/api/wifi/forget", HTTP_POST, guarded([]() {
    if (!requireSettings()) return;
    saveWifi("", "");
    sendJson("{\"ok\":true}");
    delay(500);
    restartNow("WLAN-Daten über die Webseite gelöscht");
  }));
  server.on("/api/led", HTTP_POST, guarded([]() {
    if (server.hasArg("pin")) {
      int pin = server.arg("pin").toInt();
      if (!ledPinAllowed(pin)) {
        sendJson("{\"ok\":false,\"error\":\"Pin nicht erlaubt\"}", 400);
        return;
      }
      setLedPin(pin);
    }
    if (server.hasArg("brightness")) ledBright = (uint8_t)constrain(server.arg("brightness").toInt(), 0, 255);
    lastLedRgb = 0xFFFFFFFF;
    saveLed();
    sendJson("{\"ok\":true}");
  }));
  server.on("/api/power", HTTP_POST, guarded([]() {
    long m = server.arg("sleep_min").toInt();
    if (m < 0 || m > 1440) { sendJson("{\"ok\":false,\"error\":\"0–1440 Minuten\"}", 400); return; }
    sleepMin = (uint32_t)m;
    saveSleep();
    sendJson("{\"ok\":true}");
  }));
  server.on("/api/led/test", HTTP_POST, guarded([]() {
    if (server.hasArg("pin")) {
      int pin = server.arg("pin").toInt();
      if (ledPinAllowed(pin)) setLedPin(pin);
    }
    ledTestUntil = millis() + 2000;
    sendJson("{\"ok\":true}");
  }));
  // Firmware update (.bin from Arduino IDE "Sketch > Export Compiled Binary").
  server.on("/api/update", HTTP_POST, []() {
    if (!authOk()) return;
    if (!settingsAllowed()) { requireSettings(); return; }
    bool ok = !Update.hasError();
    sendJson(ok ? String("{\"ok\":true}") : String("{\"ok\":false,\"error\":\"") + jsonEsc(Update.errorString()) + "\"}",
             ok ? 200 : 500);
    otaRunning = false;
    if (ok) { delay(800); restartNow("Firmware-Update installiert"); }
    else ev("SYS", 2, "Firmware-Update fehlgeschlagen: %s", Update.errorString());
  }, []() {
    HTTPUpload& up = server.upload();
    if (!settingsAllowed() || (webPass.length() && !server.authenticate("m18", webPass.c_str()))) return;
    if (up.status == UPLOAD_FILE_START) {
      if (scanRunning || !lockBattery(2000)) { Update.abort(); return; }
      batLink.idle();                              // J2 low during the update
      unlockBattery();
      otaRunning = true;
      ev("SYS", 0, "Firmware-Update gestartet: %s (von %s)", up.filename.c_str(), server.client().remoteIP().toString().c_str());
      Update.begin(UPDATE_SIZE_UNKNOWN);
    } else if (up.status == UPLOAD_FILE_WRITE) {
      if (Update.isRunning()) Update.write(up.buf, up.currentSize);
      feedWdt();
    } else if (up.status == UPLOAD_FILE_END) {
      if (Update.isRunning()) Update.end(true);
    } else if (up.status == UPLOAD_FILE_ABORTED) {
      Update.abort();
      otaRunning = false;
      ev("SYS", 1, "Firmware-Update abgebrochen (Verbindung unterbrochen)");
    }
  });
  // Event log: last lines as JSON, the whole log (flash) as a text file, clear
  server.on("/api/log", HTTP_GET, guarded([]() {
    String n = server.hasArg("n") ? server.arg("n") : String("150");
    sendCmdJson("LOG " + n);
  }));
  server.on("/api/log.txt", HTTP_GET, guarded([]() {
    evlog.flush();
    std::string all = evlog.all();
    char name[64];
    snprintf(name, sizeof(name), "attachment; filename=\"%s-protokoll.txt\"", hostName.c_str());
    server.sendHeader("Content-Disposition", name);
    server.setContentLength(all.size());
    server.send(200, "text/plain; charset=utf-8", "");
    for (size_t off = 0; off < all.size(); off += 2048) {
      server.sendContent(all.data() + off, std::min((size_t)2048, all.size() - off));
      feedWdt();
    }
  }));
  server.on("/api/log/clear", HTTP_POST, guarded([]() {
    if (!requireSettings()) return;
    evlog.clear();
    ev("SYS", 0, "Protokoll gelöscht (%s)", cmdSource.c_str());
    sendJson("{\"ok\":true}");
  }));
  // Measurement history (offline read-outs, imported by the PC software)
  server.on("/api/history", HTTP_GET, guarded([]() {
    String after = server.hasArg("after") ? server.arg("after") : String("0");
    String max = server.hasArg("max") ? server.arg("max") : String("50");
    sendCmdJson("HLIST " + String(after.toInt()) + " " + String(max.toInt()) + (server.arg("filter") == "new" ? " new" : " all"));
  }));
  server.on("/api/history/item", HTTP_GET, guarded([]() { sendCmdJson("HGET " + String(server.arg("id").toInt())); }));
  server.on("/api/history/stats", HTTP_GET, guarded([]() { sendCmdJson("HSTAT"); }));
  server.on("/api/history/delete", HTTP_POST, guarded([]() {
    String r = runCommand("HDEL " + String(server.arg("id").toInt()), false);
    sendJson(String("{\"ok\":") + (r.startsWith("OK") ? "true" : "false") + "}");
  }));
  server.on("/api/history/clear", HTTP_POST, guarded([]() {
    String r = runCommand(server.arg("all") == "1" ? "HCLEAR all" : "HCLEAR synced", false);
    sendJson(String("{\"ok\":true,\"deleted\":") + String(r.substring(3).toInt()) + "}");
  }));
  // Optional password for the web interface and the PC software.
  server.on("/api/password", HTTP_POST, guarded([]() {
    if (!requireSettings()) return;
    String pw = server.arg("pass");
    if (pw.length() && (pw.length() < 4 || pw.length() > 63)) {
      sendJson("{\"ok\":false,\"error\":\"Passwort: 4–63 Zeichen (leer = aus)\"}", 400);
      return;
    }
    savePassword(pw);
    sendJson(String("{\"ok\":true,\"auth\":") + (pw.length() ? "true" : "false") + "}");
  }));
  server.onNotFound([]() {
    if (captiveRedirect()) return;
    server.send(404, "text/plain", "not found");
  });
  const char* headerKeys[] = {"X-M18-Background"};
  server.collectHeaders(headerKeys, 1);
  server.begin();
}

// ---------------------------------------------------------------- discovery
// The PC broadcasts "M18BRIDGE?" to UDP port 18018; every bridge answers.
void discoveryLoop() {
  int len = disc.parsePacket();
  if (len <= 0) return;
  char buf[32] = {0};
  disc.read(buf, sizeof(buf) - 1);
  if (strncmp(buf, "M18BRIDGE?", 10) != 0) return;
  // (a discovery broadcast alone does not count as use - the automatic sync searches regularly)
  String reply = String("M18BRIDGE ") + M18BRIDGE_VERSION + " host=" + hostName + ".local ip=" + ipString() +
                 " http=80 mac=" + WiFi.macAddress() + " auth=" + (webPass.length() ? "1" : "0") +
                 " unsynced=" + String((unsigned)(history.ready() ? history.unsynced() : 0)) + "\n";
  disc.beginPacket(disc.remoteIP(), disc.remotePort());
  disc.write((const uint8_t*)reply.c_str(), reply.length());
  disc.endPacket();
}

// ------------------------------------------------------------- USB input
struct LineInput {
  Stream* s;
  String buf;
  bool overflow;
  void poll() {
    int budget = 256;                            // never starve the rest of loop()
    while (budget-- > 0 && s->available()) {
      char ch = (char)s->read();
      noteActivity();
      if (ch == '\n' || ch == '\r') {
        if (overflow) s->println("ERR TOOLONG");
        else if (buf.length()) { cmdSource = "PC USB"; s->println(runCommand(buf, true)); }   // USB = physical access
        buf = "";
        overflow = false;
      } else if (buf.length() < 120) {
        buf += ch;
      } else {
        overflow = true;
      }
    }
  }
};
LineInput usbIn{&Serial, "", false};
#if ARDUINO_USB_CDC_ON_BOOT
LineInput uartIn{&Serial0, "", false};          // second USB socket ("UART", CH343/CP2102)
#endif

// --------------------------------------------------------------------- main
uint32_t bootPressedSince = 0;

void setup() {
  port.begin();                                 // J2 low first - before anything else
  port.startUart();                             // UART with inverted (= low) TX idle
  pinMode(PIN_BOOT, INPUT_PULLUP);
#if defined(PIN_CHG_EN)
  pinMode(PIN_CHG_EN, OUTPUT);
  digitalWrite(PIN_CHG_EN, LOW);                // charger (if ever fitted) stays off
#endif
#if defined(PIN_RGB_R)
  rgbBegin();
#endif
  batMutex = xSemaphoreCreateMutex();
  stateMutex = xSemaphoreCreateMutex();
  histMutex = xSemaphoreCreateMutex();
  logMutex = xSemaphoreCreateMutex();
  evlog.setClock([]() -> uint32_t { return millis(); }, unixNow);
  evlog.setLock(logLock, logUnlock);
  reader.setTrace([](void*, const char* line) { evlog.addText("AKKU", 0, line); }, nullptr);
  proc.setLog(&evlog);
  powerCheck();
  bootId = (uint32_t)random(1, 0x7FFFFFFF);   // hardware RNG (Arduino random() uses esp_random)
  bridgeLoopTask = xTaskGetCurrentTaskHandle();

  Serial.begin(HOST_BAUD);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.begin(HOST_BAUD);
#if ARDUINO_USB_MODE
  Serial.setTxTimeoutMs(0);                     // never block when no PC listens on native USB
#endif
#endif
  proc.setClock(unixNow);
  proc.setTimeSetter(setClockFromPc);
  proc.setMetaProvider(recordMeta);
  history.setLock(histLock, histUnlock);
  bool fsOk = storage.mount();
  prefs.begin("m18", false);                    // boot counter: "B12" in every log line
  uint32_t bootNo = prefs.getUInt("bootno", 0) + 1;
  prefs.putUInt("bootno", bootNo);
  prefs.end();
  evlog.begin(fsOk ? &storage : nullptr, bootNo);
  esp_reset_reason_t rr = esp_reset_reason();
  if (rtcLogMagic == RTC_LOG_MAGIC && rtcLogLen > 0 && rtcLogLen <= RTC_LOG_SIZE && rr != ESP_RST_POWERON)
    evlog.recover(std::string(rtcLog, rtcLogLen), resetReason());
  rtcLogLen = 0;
  rtcLogMagic = RTC_LOG_MAGIC;
  evlog.setMirror(rtcMirror);
  ev("SYS", (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT) ? 1 : 0,
     "Start: Firmware %s (Build %s), Grund: %s, Unterspannungs-Neustarts: %lu, Speicher %u KB, PSRAM %u KB, Dateisystem: %s",
     M18BRIDGE_VERSION, FW_BUILD, resetReason(), (unsigned long)brownouts, (unsigned)(ESP.getFreeHeap() / 1024),
     (unsigned)(ESP.getFreePsram() / 1024), fsOk ? storage.kind : "keins");
  if (fsOk && history.begin()) proc.setHistory(&history);
  else logc("SYS", 2, "WARNUNG: kein Dateisystem - Messungen werden nicht gespeichert (Partition Scheme prüfen)\n");

  if (lowTxPower) {
    logc("SYS", 1, "WARNUNG: Neustart wegen Unterspannung (%lu seit dem Einschalten) - WLAN mit reduzierter Sendeleistung. "
                  "USB-Kabel/-Anschluss bzw. Step-down (5,0 V) prüfen.\n", (unsigned long)brownouts);
    delay(300);                                 // let the supply recover before the Wi-Fi current peak
  }
  WiFi.persistent(false);
  WiFi.onEvent(onWifiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  loadPrefs();
  ev("SYS", 0, "Gerätename %s, WLAN %s, Passwortschutz %s, LED GPIO%d, Energiesparen %u min", hostName.c_str(),
     savedSsid.length() ? ("\"" + savedSsid + "\"").c_str() : "nicht eingerichtet", webPass.length() ? "an" : "aus", ledPin,
     (unsigned)sleepMin);
  led(0, 0, 255);
  WiFi.setHostname(hostName.c_str());
  // Non-blocking: USB answers immediately while Wi-Fi connects in the background
  // (wifiLoop opens the setup AP if the saved network is not reachable).
  if (savedSsid.length()) {
    wifiMode(WIFI_STA);
    beginConnect(savedSsid, savedPass);
  } else {
    wstate = WState::AP;
    startAP();
    lastStaRetry = millis();
  }
  startMdns();
  disc.begin(DISCOVERY_PORT);
  setupHttp();
  setupWatchdog();
  enableLoopWDT();                              // loop() must come back within the watchdog timeout
  logf("\nM18BRIDGE READY %s host=%s.local ip=%s reset=%s\n", M18BRIDGE_VERSION, hostName.c_str(),
                ipString().c_str(), resetReason());
}

void loop() {
  static uint32_t lastStatusLog = 0;            // a status line every 30 min helps to place problems in time
  if (millis() - lastStatusLog > 1800000UL) {
    lastStatusLog = millis();
    ev("SYS", 0, "Status: WLAN %s%s, %d dBm, Speicher %u KB (min. %u KB), Messungen nicht übernommen: %u",
       WiFi.status() == WL_CONNECTED ? "verbunden mit " : (apActive ? "Einrichtungsmodus" : "getrennt"),
       WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "", WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : 0,
       (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMinFreeHeap() / 1024),
       (unsigned)(history.ready() ? history.unsynced() : 0));
  }
  server.handleClient();
  if (dnsActive) dns.processNextRequest();
  discoveryLoop();
  usbIn.poll();
#if ARDUINO_USB_CDC_ON_BOOT
  uartIn.poll();
#endif
  wifiLoop();

  // Safety: never leave J2 high when the PC went away mid-session.
  if (!scanRunning && xSemaphoreTake(batMutex, 0) == pdTRUE) {
    if (!batLink.isIdle() && millis() - batLink.lastActivity() > AUTO_IDLE_MS) batLink.idle();
    xSemaphoreGive(batMutex);
  }

  // BOOT button: short press unlocks settings for 2 min, hold 5 s forgets Wi-Fi.
  uint32_t held = 0;
  if (digitalRead(PIN_BOOT) == LOW) {
    if (!bootPressedSince) bootPressedSince = millis();
    held = millis() - bootPressedSince;
    if (held > 5000) {
      led(255, 0, 0);
      saveWifi("", "");
      savePassword("");                       // forgotten password: BOOT 5 s resets it too
      restartNow("BOOT-Taste 5 s gehalten: WLAN-Daten und Passwort gelöscht");
    }
  } else if (bootPressedSince) {
    uint32_t pressed = millis() - bootPressedSince;
    bootPressedSince = 0;
    if (pressed > 50 && pressed < 1500) {
      unlockUntil = millis() + UNLOCK_MS;
      noteActivity();
      logf("Einstellungen 2 Minuten entsperrt\n");
    }
  }

  // Energy saving: nobody used the bridge for sleepMin minutes -> deep sleep.
  if (apActive && WiFi.softAPgetStationNum() > 0) noteActivity();   // phone on the setup AP
  if (sleepMin && !scanRunning && !otaRunning && wstate != WState::CONNECTING && !held &&
      millis() - lastUserActivity > sleepMin * 60000UL) {
    bool idleLine = false;
    if (xSemaphoreTake(batMutex, 0) == pdTRUE) { idleLine = batLink.isIdle(); xSemaphoreGive(batMutex); }
    if (idleLine) goToSleep();
  }

  // Heap guard: restart cleanly (when idle) instead of failing in odd ways.
  if (ESP.getFreeHeap() < LOW_HEAP_BYTES) {
    if (!lowHeapSince) lowHeapSince = millis();
    if (millis() - lowHeapSince > 10000 && !scanRunning && !otaRunning && batLink.isIdle()) {
      char why[64];
      snprintf(why, sizeof(why), "wenig freier Speicher (%u Bytes)", (unsigned)ESP.getFreeHeap());
      restartNow(why);
    }
  } else {
    lowHeapSince = 0;
  }

  static uint32_t lastLed = 0;
  if (millis() - lastLed > 40) { lastLed = millis(); ledLoop(held); }
  delay(1);                                     // let lower-priority tasks (Wi-Fi, idle) run
}
