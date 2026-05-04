#ifndef _CST816D_
#define _CST816D_

#include "esp_err.h"
#include <stdint.h>
#include "driver/i2c_master.h"

// 手势
enum GESTURE {
    None = 0x00,
    SlideDown = 0x01,
    SlideUp = 0x02,
    SlideLeft = 0x03,
    SlideRight = 0x04,
    SingleTap = 0x05,
    DoubleTap = 0x0B,
    LongPress = 0x0C
};

esp_err_t CST816D_init(i2c_master_bus_handle_t bus_handle);
unsigned char CST816D_getTouch(uint16_t *x, uint16_t *y, uint8_t *gesture);

#endif