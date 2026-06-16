#include <stdio.h>
#include <inttypes.h>

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
#define RECORDING_TIME  4

#define I2S_BCLK_PIN    GPIO_NUM_26
#define I2S_WS_PIN      GPIO_NUM_25
#define I2S_DIN_PIN     GPIO_NUM_32

#define SSD1327_ADDR 0x3C


void ssd1327_test();
void app_main(void)
{
    ssd1327_test();
    i2s_chan_handle_t rx_chan;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    i2s_std_config_t std_cfg = {
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

    ESP_ERROR_CHECK(
        i2s_channel_init_std_mode(rx_chan, &std_cfg)
    );

    ESP_ERROR_CHECK(
        i2s_channel_enable(rx_chan)
    );

    static int16_t tbuffer[SAMPLE_RATE * RECORDING_TIME];
    size_t idx = 0;
    size_t idx2 = 0;
    bool done = false;

    int32_t buffer[512];
    size_t bytes_read = 0;

    const uint8_t led = 2;
    gpio_reset_pin(led);
    gpio_set_direction(led, GPIO_MODE_OUTPUT);

    gpio_set_level(led, 0);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    gpio_set_level(led, 1);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("start reading\n");
    while (1)
    {
        if (idx < SAMPLE_RATE * RECORDING_TIME)
        {
            esp_err_t ret = i2s_channel_read(
                rx_chan,
                buffer,
                sizeof(buffer),
                &bytes_read,
                portMAX_DELAY
            );

            if (ret == ESP_OK)
            {
                int num_samples = bytes_read / sizeof(int32_t);

                for (int i = 0; i < num_samples; i++)
                {
                    tbuffer[idx] = (int16_t)(buffer[i] >> 16);
                    idx++;
                    if (idx >= SAMPLE_RATE * RECORDING_TIME)
                    {
                        gpio_set_level(led, 0);
                        break;
                    }
                }
            }
        }
        else if (idx2 < SAMPLE_RATE * RECORDING_TIME)
        {
            printf("%d\n", tbuffer[idx2++]);
            // vTaskDelay(1);
        }
        else{
            printf("end\n");
        }
    }
}

#define I2C_MASTER_SCL_IO           GPIO_NUM_22
#define I2C_MASTER_SDA_IO           GPIO_NUM_21
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

static i2c_master_bus_handle_t bus_handle;

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

void ssd1327_test()
{
    setup_i2c();
    ssd1327_init(SSD1327_ADDR, I2C_MASTER_FREQ_HZ, bus_handle);

    while (1)
    {
        for (int i = 0; i < SSD1327_HEIGHT; i++)
        {
            ssd1327_set_pixel(i, i, 0x0F);
        }

        ssd1327_update();

        vTaskDelay(100000 / portTICK_PERIOD_MS);
    };
}