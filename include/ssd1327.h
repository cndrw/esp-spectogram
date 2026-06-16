#ifndef __SSD1327_H__
#define __SSD1327_H__

#include <inttypes.h>

#include "driver/i2c_master.h"

#define SSD1327_HEIGHT 96
#define SSD1327_WIDTH 128

#define SSD1327_SCREEN_SIZE_BYTES (SSD1327_HEIGHT * (SSD1327_WIDTH / 2))

void ssd1327_init(uint8_t addr, uint32_t i2c_master_freq_hz, i2c_master_bus_handle_t bus_handle);
/// @brief Set pixel on the SSD1327
/// NOTE: (0, 0) is located at the top left on the screen
void ssd1327_set_pixel(uint8_t x, uint8_t y, uint8_t color);
uint8_t* ssd1327_get_screen();
void ssd1327_update();

#endif