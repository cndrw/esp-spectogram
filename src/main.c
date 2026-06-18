#include <stdio.h>
#include <inttypes.h>
#include "esp_timer.h"
#include "math.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "ssd1327.h"


#define SAMPLE_RATE     16000
#define DMA_BLOCK_SIZE  4096

#define I2S_BCLK_PIN    GPIO_NUM_26
#define I2S_WS_PIN      GPIO_NUM_25
#define I2S_DIN_PIN     GPIO_NUM_32

#define SSD1327_ADDR 0x3C

#define I2C_MASTER_SCL_IO           GPIO_NUM_22
#define I2C_MASTER_SDA_IO           GPIO_NUM_21
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          400000

#define US_TO_MS(x) ((x) / 1000.0)
#define STATUS_LED 2

static i2c_master_bus_handle_t bus_handle;
static i2s_chan_handle_t rx_chan;
static int32_t i2s_buffer[DMA_BLOCK_SIZE];



void setup_i2c()
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(
        i2c_new_master_bus(&bus_config, &bus_handle)
    );
}

void setup_i2s()
{
    static i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(
        i2s_new_channel(&chan_cfg, NULL, &rx_chan)
    );

    static i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_DIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    chan_cfg.dma_frame_num = DMA_BLOCK_SIZE;
    chan_cfg.dma_desc_num = 2;

    ESP_ERROR_CHECK(
        i2s_channel_init_std_mode(rx_chan, &std_cfg)
    );

    ESP_ERROR_CHECK(
        i2s_channel_enable(rx_chan)
    );
}

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

void app_main(void)
{
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED, 0);

    setup_i2s();

    setup_i2c();
    ssd1327_init(SSD1327_ADDR, I2C_MASTER_FREQ_HZ, bus_handle);


    size_t bytes_read = 0;

    uint8_t x_pos = 0;


    int64_t start_time = 0;
    int64_t buffer_full_time = 0;
    int64_t calc_screen_time = 0;
    int64_t screen_update_tiem = 0;

    while (1)
    {
        start_time = esp_timer_get_time();
        esp_err_t res = i2s_channel_read(
            rx_chan,
            i2s_buffer,
            sizeof(i2s_buffer),
            &bytes_read,
            portMAX_DELAY
        );

        gpio_set_level(STATUS_LED, 0);

        buffer_full_time = esp_timer_get_time();

        if (res == ESP_OK)
        {
            int num_samples = bytes_read / sizeof(int32_t);
            x_pos = 0;

            for (int i = 0; i < SSD1327_HEIGHT; i++)
            {
                for (int j = 0; j < SSD1327_WIDTH; j++)
                {
                    ssd1327_set_pixel(j, i, 0x0);
                }
            }

            int64_t sum = 0;
            static const float bound = INT16_MAX / 8.0f;
            uint8_t prev_x = 0;
            uint8_t prev_y = 0;

            for (int i = 0; i < num_samples; i++)
            {
                sum += i2s_buffer[i] >> 16;

                if (i % 32 == 0 && i != 0)
                {
                    float average = sum / 32.0;
                    float mapped_y = ((SSD1327_HEIGHT - 1) / 2.0) * (1 + average / bound);
                    uint8_t clamped_y = floor(fmin(fmax(mapped_y, 0), SSD1327_HEIGHT-1));

                    draw_line(prev_x, x_pos == 0 ? clamped_y : prev_y, x_pos, clamped_y);
                    
                    prev_x = x_pos;
                    prev_y = clamped_y;
                    x_pos++;
                    sum = 0;
                }
            }
            calc_screen_time = esp_timer_get_time();

            ssd1327_update();
            screen_update_tiem = esp_timer_get_time();
        }

        printf("i2s_read %f\t calc: %f\t screen_update: %f\t total: %f\n",
            US_TO_MS(buffer_full_time - start_time), US_TO_MS(calc_screen_time - buffer_full_time), US_TO_MS(screen_update_tiem - calc_screen_time), US_TO_MS(screen_update_tiem - start_time));
    }
}
