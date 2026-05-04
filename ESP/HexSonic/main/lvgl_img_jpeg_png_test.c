#include "lvgl_img_jpeg_png_test.h"
#include "esp_log.h"
#include "esp_lv_decoder.h"
#include "esp_spiffs.h"
#include "lvgl.h"
#include <stdio.h>

static const char *TAG = "lvgl_img_test";

#define SPIFFS_MOUNT_POINT "/spiffs"
#define SPIFFS_PART_LABEL "storage"
#define LV_FS_LETTER 'S' // 对应 S: 前缀

static lv_obj_t *img_obj = NULL;
static bool showing_png = true;
static esp_lv_decoder_handle_t decoder_handle =
    NULL; // ⬅️ 2. 新增：定义一个全局句柄保存解码器实例

/* -------------------------- 按钮切换回调 -------------------------- */
static void toggle_img(lv_event_t *e) {
  if (showing_png) {
    ESP_LOGI(TAG, "切换到 JPG");
    lv_image_set_src(img_obj, "S:image.jpg");
    showing_png = false;

  } else {
    ESP_LOGI(TAG, "切换到 PNG");
    lv_image_set_src(img_obj, "S:image.png");
    showing_png = true;
  }
}

/* -------------------------- 初始化函数 -------------------------- */
void lvgl_img_jpeg_png_test_init(void) {
  ESP_LOGI(TAG, "===== 开始初始化 =====");

  // 1. 初始化 SPIFFS
  esp_vfs_spiffs_conf_t spiffs_conf = {.base_path = SPIFFS_MOUNT_POINT,
                                       .partition_label = SPIFFS_PART_LABEL,
                                       .max_files = 5,
                                       .format_if_mount_failed = true};
  esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPIFFS 挂载失败: %s", esp_err_to_name(ret));
    return;
  }
  size_t total = 0, used = 0;
  esp_spiffs_info(SPIFFS_PART_LABEL, &total, &used);
  ESP_LOGI(TAG, "SPIFFS: 总%u, 已用%u", total, used);

  // 👇 3. 新增：接管 LVGL 解码器！(务必在创建 UI 之前调用)
  esp_err_t err = esp_lv_decoder_init(&decoder_handle);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "🔥 乐鑫 esp_lv_decoder 注册成功！硬件加速启动！");
  } else {
    ESP_LOGE(TAG, "❌ 解码器注册失败: %d", err);
  }

  // 4. 创建 UI
  img_obj = lv_image_create(lv_scr_act());
  lv_image_set_src(img_obj, "S:image.png");
  lv_obj_center(img_obj);

  lv_obj_t *btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn, 120, 50);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_t *lab = lv_label_create(btn);
  lv_label_set_text(lab, "Switch");
  lv_obj_center(lab);
  lv_obj_add_event_cb(btn, toggle_img, LV_EVENT_CLICKED, NULL);

  lv_obj_t *hint = lv_label_create(lv_scr_act());
  lv_label_set_text(hint, "Touch to switch PNG/JPG");
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -80);

  ESP_LOGI(TAG, "===== 初始化完成 =====");
}