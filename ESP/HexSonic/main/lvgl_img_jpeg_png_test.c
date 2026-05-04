#include "lvgl_img_jpeg_png_test.h"
#include "esp_log.h"
#include "esp_lv_decoder.h"
#include "esp_spiffs.h"
#include "lvgl.h"
#include <stdio.h>

static const char *TAG = "lvgl_img_test";


static lv_obj_t *img_obj = NULL;
static bool showing_png = true;

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