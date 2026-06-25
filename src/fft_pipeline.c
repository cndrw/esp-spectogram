
#include <math.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_dsp.h" 

#include "pipeline.h"
#include "ssd1327.h"
#include "esp_utils.h"

#define N_SAMPLES 4096 
int N = N_SAMPLES;

__attribute__((aligned(16)))
float wind[N_SAMPLES];

__attribute__((aligned(16)))
float y_cf[N_SAMPLES * 2];

float *y1_cf = &y_cf[0];

const char* TAG_FFTP = "FFT-Pipeline";

void draw_column(const uint8_t x, const uint8_t y)
{
    for (int i = y; i < SSD1327_HEIGHT; i++)
    {
        ssd1327_set_pixel(x, i, 0xF);
    }
}

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
    const uint8_t sample_per_col =  N / 4 / SSD1327_WIDTH;
    static const float bound = 85.0;

    // really poor way of removing the dc offset
    for (int i = 0; i < sample_per_col; i++)
    {
        y_cf[i * 2]     = 0;
        y_cf[i * 2 + 1] = 0;
    }

    for (int i = 0 ; i < N / 4 ; i++)
    {
        sum += dsps_sqrtf_f32(y1_cf[i * 2 + 0] * y1_cf[i * 2 + 0] + y1_cf[i * 2 + 1] * y1_cf[i * 2 + 1]);

        if (i % sample_per_col == 0 && i != 0)
        {
            float average = sum / N / (float)sample_per_col;
            uint8_t mapped_y = -(clamp01(average / bound) - 1) * (SSD1327_HEIGHT-1);

            draw_column(x_pos, mapped_y);
            x_pos++;
            sum = 0;
        }
    }
}