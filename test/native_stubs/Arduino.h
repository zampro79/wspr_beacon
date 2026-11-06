#pragma once

#include <stdint.h>

class Stream {
 public:
  virtual ~Stream() = default;
  virtual int available() = 0;
  virtual int read() = 0;
};

unsigned long millis();
