#include "CST816D.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "CST816D"
#define I2C_ADDR_CST816D 0x15

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;

static esp_err_t cst816d_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg, 1, data, len, pdMS_TO_TICKS(30));
}

static esp_err_t cst816d_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(dev_handle, buf, 2, pdMS_TO_TICKS(30));
}

esp_err_t CST816D_init(i2c_master_bus_handle_t bus_handle)
{
    if (bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    i2c_bus = bus_handle;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDR_CST816D,
        .scl_speed_hz = 100000,
    };
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    // 初始化触摸芯片
    cst816d_write_reg(0xfe, 0xff);
    cst816d_write_reg(0xfa, 0x20);
    return ESP_OK;
}

unsigned char CST816D_getTouch(uint16_t *x, uint16_t *y, uint8_t *gesture)
{
    uint8_t finger = 0;
    if (cst816d_read_reg(0x02, &finger, 1) != ESP_OK) {
        return 0;
    }
    cst816d_read_reg(0x01, gesture, 1);
    if (*gesture != SlideUp && *gesture != SlideDown) {
        *gesture = None;
    }
    uint8_t data[4];
    cst816d_read_reg(0x03, data, 4);
    *x = ((data[0] & 0x0f) << 8) | data[1];
    *y = ((data[2] & 0x0f) << 8) | data[3];
    return finger;
}