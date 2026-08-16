#include "log.h"

#include <Arduino.h>
#include <stdarg.h>

static LogBuffer buffer;

void logLine(const char *fmt, ...) {
  char line[LOG_LINE_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);

  Serial.println(line);
  buffer.push(line);
}

const LogBuffer &logBuffer() { return buffer; }
