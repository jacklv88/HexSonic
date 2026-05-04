#include "driver_lcd_touch.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



void app_main(void)
{
    // 初始化屏幕、触摸、LVGL
    driver_lcd_touch_init();

    // 创建一个简单的 LVGL 界面
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello HexSonic!");
    lv_obj_center(label);



    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}