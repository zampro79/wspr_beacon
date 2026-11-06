#pragma once

#include <stddef.h>
#include <stdint.h>

enum class LineInputResult : uint8_t { NONE, READY, OVERFLOW };

class CommandLineBuffer {
 public:
  static constexpr size_t CAPACITY = 96;

  LineInputResult push(char value);
  char *line() { return buffer_; }
  const char *line() const { return buffer_; }
  size_t length() const { return length_; }
  bool overflowed() const { return overflow_; }
  void clear();

 private:
  char buffer_[CAPACITY] = {};
  size_t length_ = 0;
  bool overflow_ = false;
};
