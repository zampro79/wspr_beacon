#pragma once

#include <stddef.h>

// Self-contained dashboard asset. On ESP8266 the bytes reside in program
// memory and must be written with a flash-aware WiFiClient operation.
const char *dashboardPageData();
size_t dashboardPageSize();
const char *dashboardPageGzipData();
size_t dashboardPageGzipSize();
