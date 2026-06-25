#ifndef __ESP_UTILS_H__
#define __ESP_UTILS_H__

#include <inttypes.h>

void adjust_buffer(int32_t* buffer, const size_t size);
float clamp(const float value, const int16_t max, const int16_t min);;;;
float clamp01(const float value);

#endif