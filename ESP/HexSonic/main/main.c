#include "driver_lcd_touch.h"
#include "lvgl_img_jpeg_png_test.h"  // 重命名
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    driver_lcd_touch_init();
    lvgl_img_jpeg_png_test_init();   // 新函数名

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}