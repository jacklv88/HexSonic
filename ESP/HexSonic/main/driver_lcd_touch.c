// main/driver_lcd_touch.c
#include "driver_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_spd2010.h"
#include "CST816D.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#define TAG "LCD_TOUCH"

// ----------------------- 屏幕引脚定义 -----------------------
#define LCD_HOST        SPI2_HOST
#define LCD_CS          35
#define LCD_SCLK        36
#define LCD_D0          37
#define LCD_D1          38
#define LCD_D2          39
#define LCD_D3          40
#define LCD_BK          41
#define LCD_RST         42

// ----------------------- 触摸引脚定义 -----------------------
#define TP_I2C_PORT     I2C_NUM_0
#define TP_SCL          33
#define TP_SDA          34
#define TP_RST          47
// #define TP_INT          4   // 未使用触摸中断引脚

// ----------------------- 屏幕参数 -----------------------
#define LCD_H_RES       412
#define LCD_V_RES       412
#define LCD_BIT_PER_PIXEL  16

#define MAX_TRANSFER_SZ (LCD_H_RES * 80 * sizeof(uint16_t))
#define LVGL_BUF_LINES  50
#define LVGL_BUF_SIZE   (LCD_H_RES * LVGL_BUF_LINES)

// ----------------------- 全局句柄 -----------------------
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;

// ----------------------- 触摸读回调（LVGL 9.x 接口） -----------------------
static void lvgl_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x, y;
    uint8_t gesture;
    uint8_t num = CST816D_getTouch(&x, &y, &gesture);
    if (num > 0) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;       // 按下状态
    } else {
        data->state = LV_INDEV_STATE_REL;      // 释放状态
    }
}

// ----------------------- 初始化入口 -----------------------
void driver_lcd_touch_init(void)
{
    // 1. 背光使能
    gpio_config_t bk_conf = {
        .pin_bit_mask = (1ULL << LCD_BK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&bk_conf);
    gpio_set_level(LCD_BK, 1);

    // 2. 初始化 SPI 总线（QSPI）
    spi_bus_config_t buscfg = {
        .data0_io_num = LCD_D0,
        .data1_io_num = LCD_D1,
        .data2_io_num = LCD_D2,
        .data3_io_num = LCD_D3,
        .sclk_io_num = LCD_SCLK,
        .max_transfer_sz = MAX_TRANSFER_SZ,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. 创建面板 IO（QSPI）
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_CS,
        .dc_gpio_num = -1,              // QSPI 无 DC
        .spi_mode = 3,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
        .user_ctx = NULL,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags = {
            .quad_mode = true,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                             &io_config, &io_handle));

    // 4. 创建 SPD2010 面板（使用硬件复位引脚）
    const spd2010_vendor_config_t vendor_config = {
        .flags = { .use_qspi_interface = 1 },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,      // 硬件复位引脚
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = (void *)&vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_spd2010(io_handle, &panel_config, &panel_handle));

    // 5. 复位 & 初始化面板
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // 6. 初始化 I2C 主总线（触摸）
    i2c_master_bus_config_t i2c_mst_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = TP_I2C_PORT,
        .scl_io_num = TP_SCL,
        .sda_io_num = TP_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_cfg, &i2c_bus));

    // 7. 触摸芯片硬件复位
    gpio_config_t tp_rst_conf = {
        .pin_bit_mask = (1ULL << TP_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&tp_rst_conf);
    gpio_set_level(TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));   // 等待触摸芯片稳定

    // 8. 初始化触摸芯片
    ESP_ERROR_CHECK(CST816D_init(i2c_bus));

    // 9. 初始化 LVGL 端口
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    // 10. 注册显示器
    const lvgl_port_display_cfg_t disp_cfg =
    {.io_handle = io_handle,
     .panel_handle = panel_handle,
     .buffer_size = LVGL_BUF_SIZE,
     .double_buffer = true,
     .hres = LCD_H_RES,
     .vres = LCD_V_RES,
     .monochrome = false,
     .rotation =
         {
             .swap_xy = false,
             .mirror_x = false,
             .mirror_y = false,
         },
     .flags = {
         .buff_dma = true,
         .swap_bytes = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    ESP_LOGI(TAG, "Display registered: %dx%d", LCD_H_RES, LCD_V_RES);

    // 11. 注册触摸输入设备（LVGL 9.x 方式）
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read);
    lv_indev_set_disp(indev, disp);

    ESP_LOGI(TAG, "Touch input registered (LVGL 9.x)");
}