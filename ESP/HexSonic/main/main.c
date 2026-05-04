#include "driver_io.h"
#include "driver_lcd_touch.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_img_jpeg_png_test.h"
#include "lvgl_img_random_move_test.h" // 添加头文件
#include "lvgl_img_rotate_test.h"
#include "lvgl_font_size_test.h"

#define TAG "main"

void app_main(void) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  driver_io_mount_spiffs();
  driver_lcd_touch_init();
  lvgl_port_lock(0);

  // 选择要运行的测试，只保留一个避免冲突
  // lvgl_img_jpeg_png_test_init();
  // lvgl_img_rotate_test_init();
  // lvgl_img_random_move_test_init(); // 随机移动测试
  lvgl_font_size_test_init(); // 字体缩放测试

  lvgl_port_unlock();
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    multi_heap_info_t info;
    // SRAM 信息
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t sram_total =
        (info.total_free_bytes + info.total_allocated_bytes) / 1024;
    uint32_t sram_free = info.total_free_bytes / 1024;
    uint32_t sram_max = info.largest_free_block / 1024;
    // PSRAM 信息
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint32_t psram_total =
        (info.total_free_bytes + info.total_allocated_bytes) / 1024;
    uint32_t psram_free = info.total_free_bytes / 1024;
    uint32_t psram_max = info.largest_free_block / 1024;
    ESP_LOGI(TAG,
             "Heap: SRAM total=%uKB free=%uKB max=%uKB | PSRAM total=%uKB "
             "free=%uKB max=%uKB",
             sram_total, sram_free, sram_max, psram_total, psram_free,
             psram_max);
  }
}