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
#include "pipeline.h"


#define SAMPLE_RATE     16000
#define DMA_BLOCK_SIZE  1024 
#define DMA_DESC_NUM    5

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

#define BTN_PIN GPIO_NUM_13

static i2c_master_bus_handle_t bus_handle;
static i2s_chan_handle_t rx_chan;
static int32_t i2s_buffer[DMA_BLOCK_SIZE * (DMA_DESC_NUM - 1)];


static pipeline_t pipelines[] = {
    { .execute = raw_audio_pipeline },
    { .execute = fft_pipeline },
};
#define NUM_PIPELINES (sizeof(pipelines) / sizeof(pipeline_t))

static uint8_t cur_pipeline = 1;


static volatile int8_t overflow_status = 0;
static IRAM_ATTR bool i2s_rx_queue_overflow_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
    overflow_status = !overflow_status;
    return false;
}

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
    chan_cfg.dma_frame_num = DMA_BLOCK_SIZE;
    chan_cfg.dma_desc_num = DMA_DESC_NUM;

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

    ESP_ERROR_CHECK(
        i2s_channel_init_std_mode(rx_chan, &std_cfg)
    );

    i2s_event_callbacks_t cbs = {
        .on_recv = NULL,
        .on_recv_q_ovf = i2s_rx_queue_overflow_callback,
        .on_sent = NULL,
        .on_send_q_ovf = NULL,
    };

    if (i2s_channel_register_event_callback(rx_chan, &cbs, NULL) != ESP_OK)
    {
        ESP_LOGE("main", "overflow callback not working\n");
    }

    i2s_chan_info_t info;
    i2s_channel_get_info(rx_chan, &info);
    printf("total dma buf size %ld\n", info.total_dma_buf_size);

    ESP_ERROR_CHECK(
        i2s_channel_enable(rx_chan)
    );
}

void main_task(void* pvParameters)
{
    size_t bytes_read = 0;

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

        gpio_set_level(STATUS_LED, overflow_status);

        buffer_full_time = esp_timer_get_time();

        if (res == ESP_OK)
        {
            int num_samples = bytes_read / sizeof(int32_t);

            ssd1327_empty_screen();

            pipelines[cur_pipeline].execute(i2s_buffer, num_samples);

            calc_screen_time = esp_timer_get_time();

            ssd1327_update();
            screen_update_tiem = esp_timer_get_time();
        }

        taskYIELD();

        printf("i2s_read %f\t calc: %f\t screen_update: %f\t total: %f\n",
            US_TO_MS(buffer_full_time - start_time), US_TO_MS(calc_screen_time - buffer_full_time), US_TO_MS(screen_update_tiem - calc_screen_time), US_TO_MS(screen_update_tiem - start_time));
    }
}

void input_task(void* pvParameters)
{
    uint8_t last_state = 1;

    while (1)
    {
        uint8_t cur_state = gpio_get_level(BTN_PIN);
        if (cur_state == 0 && last_state == 1)
        {
            cur_pipeline = (cur_pipeline + 1) % NUM_PIPELINES;
        }

        last_state = cur_state;

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


void app_main(void)
{
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED, 0);

    setup_i2s();

    setup_i2c();
    ssd1327_init(SSD1327_ADDR, I2C_MASTER_FREQ_HZ, bus_handle);

    init_dsps_fft2r();

    gpio_set_direction(BTN_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_PIN, GPIO_PULLUP_ONLY);

    xTaskCreate(main_task, "main_task", 2048, NULL, 1, NULL);
    xTaskCreate(input_task, "input_task", 2048, NULL, 2, NULL);
}

