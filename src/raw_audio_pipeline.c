#include <math.h>

#include "esp_utils.h"
#include "pipeline.h"
#include "ssd1327.h"

void draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    int16_t y_diff = y2 - y1;
    int8_t sign = y_diff < 0 ? -1 : 1;

    for (int i = 0; i < abs(y_diff) / 2 + 1; i++)
    {
        ssd1327_set_pixel(x1, y1 + sign * i, 0xF);
        ssd1327_set_pixel(x2, y2 - sign * i, 0xF);
    }
}

void raw_audio_pipeline(int32_t *buffer, size_t size)
{
    adjust_buffer(buffer, size);

    static const float bound = INT16_MAX / 8.0f;

    uint8_t x_pos = 0;
    int64_t sum = 0;
    uint8_t prev_x = 0;
    uint8_t prev_y = 0;
    const uint8_t sample_per_col = size / SSD1327_WIDTH;

    for (int i = 0; i < size; i++)
    {
        sum += buffer[i];

        if (i % sample_per_col == 0 && i != 0)
        {
            float average = sum / (float)sample_per_col;
            float mapped_y = ((SSD1327_HEIGHT - 1) / 2.0) * (1 + average / bound);
            uint8_t clamped_y = floor(fmin(fmax(mapped_y, 0), SSD1327_HEIGHT-1));

            draw_line(prev_x, x_pos == 0 ? clamped_y : prev_y, x_pos, clamped_y);
            
            prev_x = x_pos;
            prev_y = clamped_y;
            x_pos++;
            sum = 0;
        }
    }
}