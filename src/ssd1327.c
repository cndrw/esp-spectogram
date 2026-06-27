#include "ssd1327.h"

#include "esp_err.h"
#include "esp_log.h"

#define CMD     0x00
#define DATA    0x40

static i2c_master_dev_handle_t dev_handle_;

static uint8_t raw_data_buffer_[SSD1327_HEIGHT * (SSD1327_WIDTH / 2) + 1];
static uint8_t* screen = &raw_data_buffer_[1]; 

void send_command_list(const uint8_t* cmds, size_t size)
{
    ESP_ERROR_CHECK(
        i2c_master_transmit(dev_handle_, cmds, size, -1)
    );
}

void ssd1327_init(const uint8_t addr, const uint32_t i2c_master_freq_hz, i2c_master_bus_handle_t bus_handle)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = i2c_master_freq_hz,
    };

    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle_)
    );

    raw_data_buffer_[0] = DATA;

    const uint8_t init_cmds[] = {
        CMD, 0xAE,              // Display off
        CMD, 0x15, 0x00, SSD1327_WIDTH / 2 - 1,  // Set Column Address
        CMD, 0x75, 0x00, SSD1327_HEIGHT - 1,     // Set Row Address
        CMD, 0xA0, 0x51,        // Re-map
        CMD, 0xA1, 0x00,        // Set Display Start Line
        CMD, 0xA6,              // Set Display Mode -> all off
        CMD, 0xA8, 0x7F,        // Set MUX Ration ! TBC
        CMD, 0xAB, 0x01,        // Function Selection A -> Enable internal V_dd reculator

        CMD, 0xB3, 0x11,        // Set Front Clock Divider (adafruit)
        CMD, 0xB6, 0x04,        // Set second precharge (adafruit)
        CMD, 0xBE, 0x0F,        // Set V_comh voltage (adafruit)
        CMD, 0xBC, 0x08,        // set precharge (adafrui)
        CMD, 0xD5, 0x62,        // function Selection B (adafruit)
        CMD, 0xFD, 0x12,        // Set Command Lock (adafruit)

        CMD, 0xA4,              // Set Display Mode -> Normal Display
        CMD, 0xAF,              // Display on    
    };

    send_command_list(init_cmds, sizeof(init_cmds));
}

void ssd1327_set_pixel(const uint8_t x, const uint8_t y, const uint8_t color)
{
    if (x >= SSD1327_WIDTH || y >= SSD1327_HEIGHT)
    {
        ESP_LOGE("SSD1327", "Pixel coordinates not within the display! (%d, %d)", x, y);
        return;
    }

    uint8_t* nibble = &screen[(x / 2) + y * (SSD1327_WIDTH / 2)];

    if (x % 2 == 0)
    {
        *nibble = (*nibble & 0x0F) | ((color & 0x0F) << 4); 
    }
    else
    {
        *nibble = (*nibble & 0xF0) | (color & 0x0F); 
    }
}

void ssd1327_empty_screen()
{
    memset(screen, 0, SSD1327_SCREEN_SIZE_BYTES);
}

void ssd1327_scroll_one_left()
{
    for (int i = 0; i < SSD1327_SCREEN_SIZE_BYTES; i++)
    {
        screen[i] <<= 4;
        screen[i] |= (screen[i + 1] & 0xF0) >> 4;
    }
}

uint8_t *ssd1327_get_screen()
{
    return screen;
}

void ssd1327_update()
{
    const uint8_t cmds[] = {
        CMD, 0x15, 0x00, SSD1327_WIDTH / 2 - 1, // Set Column Address
        CMD, 0x75, 0x00, SSD1327_HEIGHT - 1,    // Set Row Address
    };

    // reset row and col pointer in ram
    send_command_list(cmds, sizeof(cmds));

    send_command_list(raw_data_buffer_, sizeof(raw_data_buffer_));
}
