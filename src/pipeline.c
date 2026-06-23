#include <math.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_dsp.h" 

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

void adjust_buffer(int32_t* buffer, size_t size)
{
    for (int i = 0; i < size; i++)
    {
        buffer[i] >>= 16;
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

void draw_column(const uint8_t x, const uint8_t y)
{
    for (int i = y; i < SSD1327_HEIGHT; i++)
    {
        ssd1327_set_pixel(x, i, 0xF);
    }
}

#define N_SAMPLES 4096 
int N = N_SAMPLES;

__attribute__((aligned(16)))
float wind[N_SAMPLES];
// working complex array
__attribute__((aligned(16)))
float y_cf[N_SAMPLES * 2];
// Pointers to result arrays
float *y1_cf = &y_cf[0];

const char* TAG_FFTP = "FFT-Pipeline";

void init_dsps_fft2r()
{
    if (dsps_fft2r_init_fc32(NULL, N_SAMPLES) != ESP_OK)
    {
        ESP_LOGE(TAG_FFTP, "Not possible to initialize FFT.");
        return;
    }

    // Generate hann window
    dsps_wind_hann_f32(wind, N);
}

void fft_pipeline(int32_t *buffer, size_t size)
{
    adjust_buffer(buffer, size);

    memset(y_cf, 0, sizeof(y_cf));

    for (int i = 0 ; i < N ; i++)
    {
        y_cf[i * 2 + 0] = buffer[i] * wind[i];
        y_cf[i * 2 + 1] = 0;
    }

    dsps_fft2r_fc32(y_cf, N);
    // Bit reverse
    dsps_bit_rev_fc32(y_cf, N);
    // Convert one complex vector to two complex vectors
    dsps_cplx2reC_fc32(y_cf, N);

    int64_t sum = 0;
    uint8_t x_pos = 0;
    const uint8_t sample_per_col =  N / 2 / SSD1327_WIDTH;
    static const float bound = 200.0;

    for (int i = 0 ; i < N / 2 ; i++)
    {
        // power spectrum
        sum += (y1_cf[i * 2 + 0] * y1_cf[i * 2 + 0] + y1_cf[i * 2 + 1] * y1_cf[i * 2 + 1]) / (N * N);

        if (i % sample_per_col == 0 && i != 0)
        {
            float average = sum / (float)sample_per_col;
            uint8_t mapped_y = -(fmin(fmax(average / bound, 0), 1) - 1) * (SSD1327_HEIGHT-1);

            draw_column(x_pos, mapped_y);
            x_pos++;
            sum = 0;
        }
    }
}
