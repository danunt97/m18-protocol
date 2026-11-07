// Event log of the bridge: what the battery and the system did, for tracing problems.
//
//   B12 +123.4s 2026-11-07 18:43:12Z AKKU Wecken 1/3: keine Antwort auf Sync
//   ^boot ^uptime ^UTC (left out until the clock is set) ^category ^text
//
// Lines live in a RAM ring (fast "last n lines") and are written to flash in small chunk
// files (/log/00000042.txt), so the log survives restarts; the oldest chunks are deleted.
// Warnings and errors are flushed at once, everything else in batches (flash wear).
// Lines not yet in flash can be mirrored by the sketch (RTC memory) and re-added after a
// crash or brownout with recover().
//
// Pure C++ (no Arduino), so the same code runs in the PC simulator and tests.
#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "m18_history.h"   // m18::Storage

namespace m18 {

class EventLog {
 public:
  using Millis = uint32_t (*)();
  using Clock = uint32_t (*)();                       // unix time, 0 = not set
  using Lock = void (*)();
  using Mirror = void (*)(const std::string& pending); // unflushed lines (for RTC memory)

  static const size_t RAM_LINES = 250;
  static const size_t FLUSH_LINES = 25;                // batch size for normal lines
  static const int MAX_CHUNKS = 60;                    // ~60 x 25 lines kept in flash
  static const size_t MAX_LINE = 160;

  void setClock(Millis ms, Clock clock) { millis_ = ms; clock_ = clock; }
  void setLock(Lock l, Lock u) { lock_ = l; unlock_ = u; }
  void setMirror(Mirror m) { mirror_ = m; }

  // Attach the flash store (may be called after the first lines were added).
  void begin(Storage* st, uint32_t bootNo) {
    Guard g(this);
    boot_ = bootNo;
    st_ = (st && st->ok()) ? st : nullptr;
    if (!st_) return;
    st_->mkdir(DIR);
    std::vector<std::string> names;
    st_->list(DIR, names);
    for (auto& n : names) {
      if (n.size() != 12 || n.compare(8, 4, ".txt") != 0) continue;
      unsigned long s = strtoul(n.c_str(), nullptr, 10);
      if (s >= seq_) seq_ = (uint32_t)s + 1;
    }
  }

  // printf-style; category e.g. "SYS", "WLAN", "AKKU", "PC", "WEB". level: 0 info, 1 warning, 2 error
  void add(const char* cat, int level, const char* fmt, ...) {
    char text[MAX_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    addText(cat, level, text);
  }

  void addText(const char* cat, int level, const char* text) {
    std::string line = prefix(cat, level) + clean(text);
    Guard g(this);
    ram_.push_back(line);
    while (ram_.size() > RAM_LINES) ram_.pop_front();
    pending_.push_back(line);
    total_++;
    if (level > 0 || pending_.size() >= FLUSH_LINES) flushLocked();
    else if (mirror_) mirror_(joined(pending_));
  }

  // Lines that were not in flash yet when the chip restarted (from RTC memory).
  void recover(const std::string& lines, const char* why) {
    if (lines.empty()) return;
    std::vector<std::string> out;
    size_t a = 0;
    while (a < lines.size()) {
      size_t b = lines.find('\n', a);
      if (b == std::string::npos) b = lines.size();
      if (b > a) out.push_back(lines.substr(a, b - a));
      a = b + 1;
    }
    char head[96];
    snprintf(head, sizeof(head), "%zu Zeilen vor dem Neustart (%s) wiederhergestellt:", out.size(), why);
    addText("SYS", 1, head);
    Guard g(this);
    for (auto& l : out) { ram_.push_back("  " + l); pending_.push_back("  " + l); }
    while (ram_.size() > RAM_LINES) ram_.pop_front();
    flushLocked();
  }

  void flush() { Guard g(this); flushLocked(); }

  // Last n lines from RAM (newest last).
  std::vector<std::string> tail(size_t n) {
    Guard g(this);
    n = std::min(n, ram_.size());
    return std::vector<std::string>(ram_.end() - n, ram_.end());
  }

  // Whole log: every chunk in flash (oldest first) plus the lines not written yet.
  std::string all() {
    Guard g(this);
    std::string out;
    if (st_) {
      for (auto& n : chunks()) {
        std::string part;
        if (st_->read(std::string(DIR) + "/" + n, part, false)) out += part;
      }
    }
    for (auto& l : pending_) { out += l; out += '\n'; }
    if (!st_) {                                       // no flash: at least the RAM ring
      out.clear();
      for (auto& l : ram_) { out += l; out += '\n'; }
    }
    return out;
  }

  void clear() {
    Guard g(this);
    if (st_) for (auto& n : chunks()) st_->remove(std::string(DIR) + "/" + n);
    ram_.clear();
    pending_.clear();
    if (mirror_) mirror_("");
  }

  uint32_t boot() const { return boot_; }
  uint32_t total() const { return total_; }

  // JSON string escaping for the LOG command / web API.
  static std::string jsonStr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
      if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
      else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
      else o += (char)c;
    }
    return o + "\"";
  }

 private:
  static constexpr const char* DIR = "/log";

  struct Guard {
    EventLog* l;
    explicit Guard(EventLog* x) : l(x) { if (l->lock_) l->lock_(); }
    ~Guard() { if (l->unlock_) l->unlock_(); }
  };

  std::string prefix(const char* cat, int level) const {
    uint32_t ms = millis_ ? millis_() : 0;
    char t[24] = "";
    uint32_t now = clock_ ? clock_() : 0;
    if (now) {
      time_t tt = (time_t)now;
      struct tm g;
      gmtime_r(&tt, &g);
      strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%SZ", &g);
    }
    char p[96];
    snprintf(p, sizeof(p), "B%lu +%lu.%01lus %s%s%s %s", (unsigned long)boot_, (unsigned long)(ms / 1000),
             (unsigned long)(ms % 1000 / 100), t, *t ? " " : "", cat, level >= 2 ? "FEHLER: " : level == 1 ? "WARNUNG: " : "");
    return p;
  }

  static std::string clean(const char* s) {
    std::string o;
    for (; *s && o.size() < MAX_LINE; s++) o += (*s == '\n' || *s == '\r') ? ' ' : *s;
    return o;
  }

  static std::string joined(const std::deque<std::string>& v) {
    std::string o;
    for (auto& l : v) { o += l; o += '\n'; }
    return o;
  }

  std::vector<std::string> chunks() {
    std::vector<std::string> names, out;
    st_->list(DIR, names);
    for (auto& n : names)
      if (n.size() == 12 && n.compare(8, 4, ".txt") == 0) out.push_back(n);
    std::sort(out.begin(), out.end());
    return out;
  }

  void flushLocked() {
    if (pending_.empty()) return;
    if (st_) {
      char name[32];
      snprintf(name, sizeof(name), "%s/%08lu.txt", DIR, (unsigned long)seq_++);
      if (st_->write(name, joined(pending_))) {
        pending_.clear();
        auto c = chunks();
        for (int i = 0; i + MAX_CHUNKS < (int)c.size(); i++) st_->remove(std::string(DIR) + "/" + c[i]);
      }
    } else {
      while (pending_.size() > RAM_LINES) pending_.pop_front();
      if (pending_.size() >= FLUSH_LINES) pending_.clear();   // no flash: RAM ring only
    }
    if (mirror_) mirror_(joined(pending_));
  }

  Storage* st_ = nullptr;
  Millis millis_ = nullptr;
  Clock clock_ = nullptr;
  Lock lock_ = nullptr, unlock_ = nullptr;
  Mirror mirror_ = nullptr;
  std::deque<std::string> ram_, pending_;
  uint32_t boot_ = 0, seq_ = 0, total_ = 0;
};

}  // namespace m18
