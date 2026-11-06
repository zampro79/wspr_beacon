#pragma once

#include <stdint.h>
#include <stddef.h>

uint32_t platformEntropySeed();
bool platformFillRandom(void *context, uint8_t *destination, size_t length);
