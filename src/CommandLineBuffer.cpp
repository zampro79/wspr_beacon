#include "CommandLineBuffer.h"

LineInputResult CommandLineBuffer::push(char value) {
  if (value == '\r' || value == '\n') {
    if (overflow_) {
      length_ = 0;
      overflow_ = false;
      return LineInputResult::OVERFLOW;
    }
    if (length_ == 0) return LineInputResult::NONE;
    buffer_[length_] = '\0';
    length_ = 0;
    return LineInputResult::READY;
  }
  if (value == '\b' || value == 0x7f) {
    if (!overflow_ && length_ > 0) --length_;
    return LineInputResult::NONE;
  }
  if (overflow_) return LineInputResult::NONE;
  if (length_ + 1 >= CAPACITY) {
    overflow_ = true;
    return LineInputResult::NONE;
  }
  buffer_[length_++] = value;
  return LineInputResult::NONE;
}

void CommandLineBuffer::clear() {
  volatile char *cursor = buffer_;
  for (size_t index = 0; index < sizeof(buffer_); ++index) cursor[index] = '\0';
  length_ = 0;
  overflow_ = false;
}
