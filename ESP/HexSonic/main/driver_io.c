#include "driver_io.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "driver_io";
#define SPIFFS_MOUNT_POINT "/spiffs"
#define SPIFFS_PART_LABEL "storage"

void driver_io_init(void) {
    // 包含 GPIO 45, 46 (电源) 和 GPIO 21 (音频 PA)
    uint64_t pin_mask = (1ULL << 45) | (1ULL << 46) | (1ULL << 21);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // 拉高外设电源和电池 Boost
    gpio_set_level(45, 1);
    gpio_set_level(46, 1);
    
    // 拉高音频 PA 使能引脚
    gpio_set_level(21, 1);
    
    ESP_LOGI("driver_io", "IO Init: GPIO 45, 46 and 21 (PA EN) set to HIGH");
}

void driver_io_mount_spiffs(void) {
  esp_vfs_spiffs_conf_t spiffs_conf = {
      .base_path = SPIFFS_MOUNT_POINT,
      .partition_label = SPIFFS_PART_LABEL,
      .max_files = 5,
      .format_if_mount_failed = true,
  };

  esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
    return;
  }

  size_t total = 0, used = 0;
  esp_spiffs_info(SPIFFS_PART_LABEL, &total, &used);
  ESP_LOGI(TAG, "SPIFFS mounted: total=%u, used=%u", total, used);
}