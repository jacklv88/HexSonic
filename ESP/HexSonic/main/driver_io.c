#include "driver_io.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "driver_io";
#define SPIFFS_MOUNT_POINT "/spiffs"
#define SPIFFS_PART_LABEL "storage"

void driver_io_init(void) {}

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