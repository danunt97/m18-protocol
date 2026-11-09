// Text line protocol over Bluetooth LE (Nordic UART Service).
//
// The phone writes command lines ("INFO\n", "SCAN\n", ...) to the RX characteristic - a line may
// arrive in several writes, several lines in one write. The bridge answers each command with one
// line ("OK ..." / "ERR ...", plus "PROG <percent>" lines while a read-out runs) on the TX
// characteristic as notifications of at most MTU-3 bytes; the phone joins them up to the '\n'.
// These two helpers do the splitting and joining. Pure C++, tested in the PC simulator.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace m18 {

struct BleLine {
  std::string text;
  bool tooLong;    // more than the maximum length: the bridge answers "ERR TOOLONG"
};

// Joins the bytes of BLE writes into command lines ('\n' or '\r' ends a line).
class BleLineIn {
 public:
  explicit BleLineIn(size_t maxLen = 600) : max_(maxLen) {}

  void feed(const char* data, size_t n, std::vector<BleLine>& out) {
    for (size_t i = 0; i < n; i++) {
      char c = data[i];
      if (c == '\n' || c == '\r') {
        if (overflow_ || !buf_.empty()) out.push_back({buf_, overflow_});
        buf_.clear();
        overflow_ = false;
      } else if (buf_.size() < max_) {
        buf_ += c;
      } else {
        overflow_ = true;
      }
    }
  }
  void clear() { buf_.clear(); overflow_ = false; }

 private:
  std::string buf_;
  bool overflow_ = false;
  size_t max_;
};

// Payload size of one notification for a negotiated ATT MTU (3 bytes ATT header).
inline size_t blePayload(uint16_t mtu) {
  size_t p = mtu > 3 ? (size_t)mtu - 3 : 20;
  if (p < 20) p = 20;
  if (p > 244) p = 244;     // keep notifications in one link-layer packet (DLE 251)
  return p;
}

// Splits a reply into notification payloads.
inline std::vector<std::string> bleChunks(const std::string& s, uint16_t mtu) {
  std::vector<std::string> out;
  size_t p = blePayload(mtu);
  for (size_t off = 0; off < s.size(); off += p) out.push_back(s.substr(off, p));
  return out;
}

}  // namespace m18
