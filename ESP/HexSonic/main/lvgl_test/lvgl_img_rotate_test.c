// lvgl_img_rotate_test.c
#include "lvgl_img_rotate_test.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "rotate_test";
static lv_obj_t *rotate_img = NULL;

/* 动画执行回调：将动画当前值（0.1° 为单位）设为图片旋转角度 */
static void anim_set_angle(void *var, int32_t val)
{
    // 1. 更新图片的旋转角度
    lv_image_set_rotation((lv_obj_t *)var, val);

    // 2. 【关键新增】：强制全屏重绘
    // 忽略局部刷新，直接把整个屏幕重新画一遍，彻底清除旋转留下的边缘噪点和残留像素
    lv_obj_invalidate(lv_scr_act());
}

void lvgl_img_rotate_test_init(void)
{
    ESP_LOGI(TAG, "启动 image.png 旋转测试（20 秒 / 圈）");

    // 【关键新增】：显式设置屏幕背景为纯黑（或纯白）
    // 确保全屏重绘时，没有被图片覆盖的地方能被干净的背景色填充
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    // 创建图片对象并居中显示
    rotate_img = lv_image_create(lv_scr_act());
    lv_image_set_src(rotate_img, "S:image2.png");
    lv_obj_center(rotate_img);

    // 配置动画：角度从 0 到 3600（即 0° 到 360°，单位 0.1°）
    // 时长 20000ms = 20s，无限循环
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, rotate_img);
    lv_anim_set_exec_cb(&a, anim_set_angle);
    lv_anim_set_values(&a, 0, 3600);
    lv_anim_set_time(&a, 20000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    ESP_LOGI(TAG, "旋转动画已启动，已开启全屏抗撕裂重绘");
}