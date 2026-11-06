#pragma once

#include <stddef.h>
#include <stdint.h>

enum class HttpSocketWriteStatus : uint8_t {
  PROGRESS,
  WOULD_BLOCK,
  INTERRUPTED,
  DISCONNECTED,
  ERROR
};

struct HttpSocketWriteResult {
  HttpSocketWriteResult(HttpSocketWriteStatus valueStatus,
                        size_t valueBytesWritten)
      : status(valueStatus), bytesWritten(valueBytesWritten) {}

  HttpSocketWriteStatus status;
  size_t bytesWritten;
};

// Platform-neutral policy for one non-blocking socket submission. The caller
// supplies the actual send operation so native tests never need a real socket.
class HttpNonBlockingWriter {
 public:
  using SendFunction = int (*)(void *context, int socketDescriptor,
                               const uint8_t *data, size_t length,
                               int &errorCode);

  static HttpSocketWriteResult attempt(int socketDescriptor,
                                       const uint8_t *data, size_t length,
                                       SendFunction sendFunction,
                                       void *context);
};
