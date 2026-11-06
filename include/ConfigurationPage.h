#pragma once

#include <stddef.h>

// Self-contained read-only configuration asset. On ESP8266 the bytes reside
// in program memory and are streamed through the existing flash staging path.
const char *configurationPageData();
size_t configurationPageSize();
const char *configurationPageGzipData();
size_t configurationPageGzipSize();
