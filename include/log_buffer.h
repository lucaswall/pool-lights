#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// The last few console lines, kept so they can be read over WiFi once the board is in a
// case with no USB. Fixed size and fixed slots: it has to run for weeks without growing
// and without fragmenting the heap.
//
// 40 x 100 bytes is about 4 KB against roughly 44 KB free.
#define LOG_LINES 40
#define LOG_LINE_LEN 100

class LogBuffer {
 public:
  void push(const char *line) {
    strncpy(_lines[_head], line, LOG_LINE_LEN - 1);
    _lines[_head][LOG_LINE_LEN - 1] = '\0';
    _head = (uint8_t)((_head + 1) % LOG_LINES);
    if (_count < LOG_LINES) {
      _count++;
    }
  }

  uint8_t count() const { return _count; }

  // Index 0 is the oldest line held, so a reader gets the boot banner first.
  const char *line(uint8_t index) const {
    if (index >= _count) {
      return "";
    }
    const uint8_t oldest = (uint8_t)((_head + LOG_LINES - _count) % LOG_LINES);
    return _lines[(oldest + index) % LOG_LINES];
  }

 private:
  char _lines[LOG_LINES][LOG_LINE_LEN] = {{0}};
  uint8_t _head = 0;
  uint8_t _count = 0;
};
