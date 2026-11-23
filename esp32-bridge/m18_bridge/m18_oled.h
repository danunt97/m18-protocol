// OLED status display of the M18 bridge (0.96" SSD1306, 128x64, I2C) - drawing part.
//
// Pure C++ without Arduino: a 1-bit frame buffer in the SSD1306 page layout, a 5x7 font
// (ASCII plus ä ö ü Ä Ö Ü ß °), a parser that takes the few values the display needs from
// a SCAN result, and the screen layout. The sketch fills an OledView, calls render() and
// sends the changed pages over I2C. The PC test (test/sim_bridge.cpp) uses the same code.

#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace m18 {

// ------------------------------------------------------------------ font
// 5 columns per glyph, bit 0 = top row. Characters 0x20..0x7E, then the extras below.
static const uint8_t OLED_FONT[][5] = {
  {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},  //  !"#
  {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},  // $%&'
  {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x08,0x2A,0x1C,0x2A,0x08}, {0x08,0x08,0x3E,0x08,0x08},  // ()*+
  {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},  // ,-./
  {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},  // 0123
  {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},  // 4567
  {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},  // 89:;
  {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},  // <=>?
  {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},  // @ABC
  {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},  // DEFG
  {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},  // HIJK
  {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},  // LMNO
  {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},  // PQRS
  {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},  // TUVW
  {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},  // XYZ[
  {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},  // \]^_
  {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},  // `abc
  {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E},  // defg
  {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},  // hijk
  {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},  // lmno
  {0x7C,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},  // pqrs
  {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},  // tuvw
  {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},  // xyz{
  {0x00,0x00,0x7F,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x08,0x04,0x08,0x10,0x08},                              // |}~
  // extras (index 95..)
  {0x20,0x55,0x54,0x55,0x78}, {0x38,0x45,0x44,0x45,0x38}, {0x3C,0x41,0x40,0x21,0x7C},                              // ä ö ü
  {0x7C,0x13,0x12,0x13,0x7C}, {0x3C,0x43,0x42,0x43,0x3C}, {0x3C,0x41,0x40,0x41,0x3C},                              // Ä Ö Ü
  {0x7E,0x01,0x49,0x4E,0x30}, {0x00,0x06,0x09,0x09,0x06},                                                          // ß °
};

// Next character of a UTF-8 string as a font index; advances *s. Unknown characters -> '?'.
inline int oledGlyph(const char*& s) {
  unsigned char c = (unsigned char)*s++;
  if (c >= 0x20 && c < 0x7F) return c - 0x20;
  if (c == 0xC3 && *s) {
    unsigned char d = (unsigned char)*s++;
    switch (d) {
      case 0xA4: return 95; case 0xB6: return 96; case 0xBC: return 97;
      case 0x84: return 98; case 0x96: return 99; case 0x9C: return 100;
      case 0x9F: return 101;
    }
    return '?' - 0x20;
  }
  if (c == 0xC2 && *s) { unsigned char d = (unsigned char)*s++; return d == 0xB0 ? 102 : '?' - 0x20; }
  if (c == 0xE2 && s[0] && s[1]) {                           // – — … typographic dashes / ellipsis
    unsigned char d = (unsigned char)s[0], e = (unsigned char)s[1];
    s += 2;
    if (d == 0x80 && (e == 0x93 || e == 0x94)) return '-' - 0x20;
    if (d == 0x80 && e == 0xA6) return '.' - 0x20;
    return '?' - 0x20;
  }
  while ((*s & 0xC0) == 0x80) s++;                          // skip the rest of another sequence
  return '?' - 0x20;
}

// ----------------------------------------------------------- frame buffer
class OledCanvas {
 public:
  static const int W = 128, H = 64, PAGES = H / 8;
  uint8_t fb[W * PAGES];

  OledCanvas() { clear(); }
  void clear() { memset(fb, 0, sizeof(fb)); }
  void pixel(int x, int y, bool on = true) {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    uint8_t& b = fb[x + (y / 8) * W];
    if (on) b |= (uint8_t)(1 << (y & 7)); else b &= (uint8_t)~(1 << (y & 7));
  }
  bool get(int x, int y) const {
    if (x < 0 || y < 0 || x >= W || y >= H) return false;
    return fb[x + (y / 8) * W] >> (y & 7) & 1;
  }
  void fillRect(int x, int y, int w, int h, bool on = true) {
    for (int i = x; i < x + w; i++) for (int j = y; j < y + h; j++) pixel(i, j, on);
  }
  void rect(int x, int y, int w, int h) {
    for (int i = x; i < x + w; i++) { pixel(i, y); pixel(i, y + h - 1); }
    for (int j = y; j < y + h; j++) { pixel(x, j); pixel(x + w - 1, j); }
  }
  void hline(int x, int y, int w) { for (int i = x; i < x + w; i++) pixel(i, y); }

  // Width in pixels (6 per character and scale, without the gap after the last one).
  static int textWidth(const char* s, int scale = 1) {
    int n = 0;
    while (*s) { oledGlyph(s); n++; }
    return n ? (n * 6 - 1) * scale : 0;
  }
  // Draws UTF-8 text with its top left corner at x/y; returns the x after the text.
  int text(int x, int y, const char* s, int scale = 1, bool on = true) {
    while (*s) {
      const uint8_t* g = OLED_FONT[oledGlyph(s)];
      for (int c = 0; c < 5; c++)
        for (int r = 0; r < 7; r++)
          if (g[c] >> r & 1) fillRect(x + c * scale, y + r * scale, scale, scale, on);
      x += 6 * scale;
    }
    return x;
  }
  void textCenter(int y, const char* s, int scale = 1) { text((W - textWidth(s, scale)) / 2, y, s, scale); }
  void textRight(int y, const char* s, int scale = 1) { text(W - textWidth(s, scale), y, s, scale); }
};

// ------------------------------------------------------- read-out summary
// The values of a SCAN result the display shows.
struct OledScan {
  bool valid = false;        // a read-out exists
  bool ok = false;           // it succeeded
  char model[28] = "";
  float packV = -1;          // < 0: unknown
  int health = -1;           // 0..100, < 0: unknown
  float tempC = 0;
  bool haveTemp = false;
  char status[12] = "";      // good | watch | weak | unknown
  char error[64] = "";
};

namespace oled_detail {
// Value of "key": in a flat JSON object (first occurrence). Returns nullptr if missing.
inline const char* find(const char* json, const char* key) {
  char pat[32];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(json, pat);
  return p ? p + strlen(pat) : nullptr;
}
inline bool str(const char* json, const char* key, char* out, size_t cap) {
  const char* p = find(json, key);
  if (!p || *p != '"' || !cap) return false;
  size_t n = 0;
  for (p++; *p && *p != '"' && n + 1 < cap; p++) {
    if (*p == '\\' && p[1]) p++;
    out[n++] = *p;
  }
  out[n] = 0;
  return true;
}
inline bool num(const char* json, const char* key, float& v) {
  const char* p = find(json, key);
  if (!p || *p == 'n') return false;                       // null
  char* end;
  v = strtof(p, &end);
  return end != p;
}
}  // namespace oled_detail

inline OledScan parseOledScan(const char* json) {
  OledScan s;
  if (!json || json[0] != '{' || json[1] == '}') return s;
  const char* ok = oled_detail::find(json, "ok");
  if (!ok) return s;
  s.valid = true;
  s.ok = !strncmp(ok, "true", 4);
  if (!s.ok) {
    if (!oled_detail::str(json, "error", s.error, sizeof(s.error))) strcpy(s.error, "Akku antwortet nicht");
    return s;
  }
  oled_detail::str(json, "model", s.model, sizeof(s.model));
  oled_detail::str(json, "status", s.status, sizeof(s.status));
  float v;
  if (oled_detail::num(json, "pack_v", v)) s.packV = v;
  if (oled_detail::num(json, "health", v)) s.health = (int)(v + 0.5f);
  if (oled_detail::num(json, "temp_c", v)) { s.tempC = v; s.haveTemp = true; }
  return s;
}

// --------------------------------------------------------------- screen
enum class OledMode : uint8_t { IDLE, SCANNING, PC, OTA, BOOT_HOLD };
enum class OledWifi : uint8_t { SETUP, CONNECTING, CONNECTED, OFFLINE };

struct OledView {
  OledMode mode = OledMode::IDLE;
  int progress = 0;          // SCANNING: 0..100
  uint32_t bootHeldMs = 0;   // BOOT_HOLD
  OledWifi wifi = OledWifi::OFFLINE;
  int rssi = 0;              // dBm, CONNECTED
  char ip[16] = "";          // CONNECTED: own IP; SETUP: IP of the setup access point
  char apName[33] = "";      // SETUP: name of the setup access point
  OledScan scan;
  const char* verdict = nullptr;  // "good"/"watch"/"weak"/"fail" from the PC (SHOW), overrides scan.status
  long ageS = -1;            // age of the read-out in seconds (< 0: do not show)
  int unlockS = 0;           // > 0: settings unlocked for this many seconds
  long sleepInS = -1;        // >= 0: deep sleep in this many seconds
};

namespace oled_detail {
inline const char* statusLabel(const char* st) {
  if (!strcmp(st, "good")) return "Gut";
  if (!strcmp(st, "watch")) return "Beobachten";
  if (!strcmp(st, "weak") || !strcmp(st, "critical")) return "Schwach";
  if (!strcmp(st, "fail")) return "Fehler";
  return "Unbekannt";
}
inline void fmtDecimal(char* out, size_t cap, float v, int dec, const char* unit) {
  snprintf(out, cap, "%.*f%s", dec, (double)v, unit);
  for (char* p = out; *p; p++) if (*p == '.') *p = ',';    // German decimal comma
}
inline void fmtAge(char* out, size_t cap, long s) {
  if (s < 60) snprintf(out, cap, "jetzt");
  else if (s < 3600) snprintf(out, cap, "%ld min", s / 60);
  else if (s < 86400) snprintf(out, cap, "%ld h", s / 3600);
  else snprintf(out, cap, "%ld d", s / 86400);
}
// Word-wraps s into lines of at most `cols` characters, draws at most `maxLines` of them.
inline int wrap(OledCanvas& c, int y, const char* s, int cols, int maxLines) {
  char line[24];
  int lines = 0;
  while (*s && lines < maxLines) {
    while (*s == ' ') s++;
    const char* p = s;
    const char* lastBreak = nullptr;
    int n = 0;
    while (*p && n < cols) {
      if (*p == ' ') lastBreak = p;
      oledGlyph(p);
      n++;
    }
    const char* end = (*p && lastBreak) ? lastBreak : p;
    size_t len = (size_t)(end - s);
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    memcpy(line, s, len);
    line[len] = 0;
    c.text(0, y + lines * 10, line);
    lines++;
    s = end;
  }
  return lines;
}
inline void wifiBars(OledCanvas& c, int x, int rssi) {
  int bars = rssi >= -60 ? 3 : rssi >= -72 ? 2 : 1;
  for (int i = 0; i < 3; i++) {
    int h = 3 + i * 2;
    if (i < bars) c.fillRect(x + i * 3, 7 - h, 2, h);
    else c.pixel(x + i * 3, 6);
  }
}
}  // namespace oled_detail

inline void renderOled(OledCanvas& c, const OledView& v) {
  using namespace oled_detail;
  char b[40];
  c.clear();

  // header: name left, Wi-Fi state right
  c.text(0, 0, "M18-Brücke");
  switch (v.wifi) {
    case OledWifi::CONNECTED: c.textRight(0, "WLAN", 1); wifiBars(c, 128 - 6 * 4 - 10, v.rssi); break;
    case OledWifi::SETUP: c.textRight(0, "Setup"); break;
    case OledWifi::CONNECTING: c.textRight(0, "verbinde"); break;
    default: c.textRight(0, "offline");
  }
  c.hline(0, 9, 128);

  // footer: how to reach the bridge, or a countdown
  if (v.mode == OledMode::IDLE && v.sleepInS >= 0 && v.sleepInS <= 60) {
    snprintf(b, sizeof(b), "Schlafe in %ld s", v.sleepInS);
    c.text(0, 56, b);
  } else if (v.unlockS > 0) {
    snprintf(b, sizeof(b), "Entsperrt %d:%02d", v.unlockS / 60, v.unlockS % 60);
    c.text(0, 56, b);
  } else if (v.wifi == OledWifi::SETUP && v.apName[0]) {
    c.text(0, 56, v.apName);
  } else if (v.ip[0]) {
    c.text(0, 56, v.ip);
  }

  switch (v.mode) {
    case OledMode::OTA:
      c.textCenter(20, "Firmware-Update");
      c.textCenter(34, "nicht ausstecken");
      return;
    case OledMode::BOOT_HOLD:
      c.textCenter(14, "BOOT gehalten");
      if (v.bootHeldMs >= 5000) {
        c.textCenter(30, "WLAN wird gelöscht");
      } else {
        snprintf(b, sizeof(b), "WLAN vergessen in %u s", (unsigned)((5000 - v.bootHeldMs + 999) / 1000));
        c.textCenter(30, b);
      }
      return;
    case OledMode::SCANNING: {
      c.textCenter(14, "Lese Akku ...");
      int p = v.progress < 0 ? 0 : v.progress > 100 ? 100 : v.progress;
      c.rect(4, 28, 120, 10);
      c.fillRect(6, 30, 116 * p / 100, 6);
      snprintf(b, sizeof(b), "%d %%", p);
      c.textCenter(42, b);
      return;
    }
    case OledMode::PC:
      c.textCenter(18, "PC verbunden");
      c.textCenter(32, "liest den Akku");
      return;
    default: break;
  }

  if (!v.scan.valid) {                                       // nothing read yet
    c.textCenter(16, "Bereit", 2);
    c.textCenter(36, "Akku aufstecken,");
    c.textCenter(45, "im Browser lesen");
    return;
  }
  if (!v.scan.ok) {                                          // last read-out failed
    c.fillRect(0, 12, 128, 10);
    c.text(2, 13, "Lesen fehlgeschlagen", 1, false);
    wrap(c, 25, v.scan.error, 21, 3);
    return;
  }

  // last read-out: model and age, pack voltage (large), state and health, temperature
  c.text(0, 13, v.scan.model[0] ? v.scan.model : "M18-Akku");
  if (v.ageS >= 0 && OledCanvas::textWidth(v.scan.model) < 128 - 6 * 6) {
    fmtAge(b, sizeof(b), v.ageS);
    c.textRight(13, b);
  }
  if (v.scan.packV >= 0) fmtDecimal(b, sizeof(b), v.scan.packV, 2, " V");
  else snprintf(b, sizeof(b), "-- V");
  c.textCenter(24, b, 2);

  const char* st = v.verdict ? v.verdict : v.scan.status;
  bool bad = !strcmp(st, "weak") || !strcmp(st, "critical") || !strcmp(st, "fail");
  if (v.scan.health >= 0) snprintf(b, sizeof(b), "%s %d %%", statusLabel(st), v.scan.health);
  else snprintf(b, sizeof(b), "%s", statusLabel(st));
  if (bad) {                                                 // weak battery: white bar, dark text
    c.fillRect(0, 42, OledCanvas::textWidth(b) + 4, 10);
    c.text(2, 43, b, 1, false);
  } else {
    c.text(0, 43, b);
  }
  if (v.scan.haveTemp) {
    fmtDecimal(b, sizeof(b), v.scan.tempC, 0, "°C");
    c.textRight(43, b);
  }
}

}  // namespace m18
