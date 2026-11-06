#include "HttpNonBlockingWriter.h"

#include <errno.h>

HttpSocketWriteResult HttpNonBlockingWriter::attempt(
    int socketDescriptor, const uint8_t *data, size_t length,
    SendFunction sendFunction, void *context) {
  if (socketDescriptor < 0 || data == nullptr || length == 0 ||
      sendFunction == nullptr) {
    return {HttpSocketWriteStatus::DISCONNECTED, 0};
  }

  int errorCode = 0;
  const int result = sendFunction(context, socketDescriptor, data, length,
                                  errorCode);
  if (result > 0) {
    const size_t written = static_cast<size_t>(result);
    if (written > length) return {HttpSocketWriteStatus::ERROR, 0};
    return {HttpSocketWriteStatus::PROGRESS, written};
  }
  if (result == 0) return {HttpSocketWriteStatus::DISCONNECTED, 0};
  if (errorCode == EAGAIN || errorCode == EWOULDBLOCK)
    return {HttpSocketWriteStatus::WOULD_BLOCK, 0};
  if (errorCode == EINTR)
    return {HttpSocketWriteStatus::INTERRUPTED, 0};
  return {HttpSocketWriteStatus::ERROR, 0};
}
