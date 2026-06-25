#include <math.h>
#include <stdio.h>

#include "esp_utils.h"

void adjust_buffer(int32_t* buffer, const size_t size)
{
    for (int i = 0; i < size; i++)
    {
        buffer[i] >>= 16;
    }
}

float clamp(const float value, const int16_t max, const int16_t min)
{
    if (value > max)
    {
        return max;
    }
    else if (value < min)
    {
        return min;
    }
    
    return value;
}

float clamp01(const float value)
{
    return clamp(value, 1, 0);
}
