#include "driver_io.h"
#include "driver_lcd_touch.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_img_jpeg_png_test.h" 

void app_main(void) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  driver_io_mount_spiffs();
  driver_lcd_touch_init();
  lvgl_port_lock(0);
  lvgl_img_jpeg_png_test_init(); 
  lvgl_port_unlock();
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}