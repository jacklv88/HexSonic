#include "lvgl_font_size_test.h"
#include "esp_log.h"
#include "lvgl.h"


static const char *TAG = "font_test";
static lv_obj_t *label;
static lv_obj_t *info_label;

static int font_size = 24;       // 初始大小
static lv_font_t *ttf_font = NULL;

static void update_font(void)
{
    if (ttf_font) {
        lv_tiny_ttf_destroy(ttf_font);
    }

    // 替换为你的 ttf 文件路径或数组
    ttf_font = lv_tiny_ttf_create_file("S:yefont.ttf", font_size);
    if (!ttf_font) {
        ESP_LOGE(TAG, "Failed to create TTF font");
        return;
    }

    lv_obj_set_style_text_font(label, ttf_font, 0);
    lv_obj_center(label);

    lv_label_set_text_fmt(info_label, "Size: %d px", font_size);
}

static void btn_plus_cb(lv_event_t *e)
{
    if (font_size < 60) {
        font_size += 4;
        update_font();
    }
}

static void btn_minus_cb(lv_event_t *e)
{
    if (font_size > 8) {
        font_size -= 4;
        update_font();
    }
}

void lvgl_font_size_test_init(void){

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);

    label = lv_label_create(scr);
    lv_label_set_text(label, "Hello LVGL!");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    info_label = lv_label_create(scr);
    lv_obj_align(info_label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *btn_minus = lv_btn_create(scr);
    lv_obj_set_size(btn_minus, 60, 40);
    lv_obj_align(btn_minus, LV_ALIGN_BOTTOM_MID, -60, -30);
    lv_obj_t *lab_minus = lv_label_create(btn_minus);
    lv_label_set_text(lab_minus, "-");
    lv_obj_center(lab_minus);
    lv_obj_add_event_cb(btn_minus, btn_minus_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_plus = lv_btn_create(scr);
    lv_obj_set_size(btn_plus, 60, 40);
    lv_obj_align(btn_plus, LV_ALIGN_BOTTOM_MID, 60, -30);
    lv_obj_t *lab_plus = lv_label_create(btn_plus);
    lv_label_set_text(lab_plus, "+");
    lv_obj_center(lab_plus);
    lv_obj_add_event_cb(btn_plus, btn_plus_cb, LV_EVENT_CLICKED, NULL);

    update_font();   // 初次渲染
}