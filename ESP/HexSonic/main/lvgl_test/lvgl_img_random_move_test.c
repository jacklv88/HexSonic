#include "lvgl_img_random_move_test.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_random.h"

static const char *TAG = "bounce_move";

typedef struct {
    lv_obj_t *obj;
    float pos_x;
    float pos_y;
    float vel_x;
    float vel_y;
} moving_obj_t;

#define OBJ_COUNT 2
static moving_obj_t movers[OBJ_COUNT];
static lv_timer_t *move_timer = NULL;

// --- 辅助函数：获取随机的偶数速度 (2.0, 4.0, 6.0) ---
// 保持偶数是为了防止出现 0.5 像素导致的画面撕裂和抖动
static float get_rand_vel(bool positive) {
    // 随机生成 1, 2, 3，乘以 2 得到 2, 4, 6
    float v = (float)(lv_rand(1, 3) * 2);
    return positive ? v : -v;
}

static float get_rand_vel_any(void) {
    float v = (float)(lv_rand(1, 3) * 2);
    // 50% 概率为正，50% 概率为负
    return (lv_rand(0, 1) == 0) ? v : -v;
}

static void move_timer_cb(lv_timer_t *timer)
{
    lv_display_t *disp = lv_obj_get_display(movers[0].obj);
    lv_coord_t screen_w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t screen_h = lv_display_get_vertical_resolution(disp);

    for (int i = 0; i < OBJ_COUNT; i++) {
        moving_obj_t *m = &movers[i];
        lv_coord_t obj_w = lv_obj_get_width(m->obj);
        lv_coord_t obj_h = lv_obj_get_height(m->obj);

        // 更新位置
        m->pos_x += m->vel_x;
        m->pos_y += m->vel_y;

        bool hit_x = false;
        bool hit_y = false;

        // --- 水平边缘检测与随机反弹 ---
        if (m->pos_x <= 0) {
            m->pos_x = 0;
            m->vel_x = get_rand_vel(true);  // 碰到左边，X速度必须变为正数（向右）
            hit_x = true;
        } else if (m->pos_x + obj_w >= screen_w) {
            m->pos_x = screen_w - obj_w;
            m->vel_x = get_rand_vel(false); // 碰到右边，X速度必须变为负数（向左）
            hit_x = true;
        }

        // --- 垂直边缘检测与随机反弹 ---
        if (m->pos_y <= 0) {
            m->pos_y = 0;
            m->vel_y = get_rand_vel(true);  // 碰到上边，Y速度必须变为正数（向下）
            hit_y = true;
        } else if (m->pos_y + obj_h >= screen_h) {
            m->pos_y = screen_h - obj_h;
            m->vel_y = get_rand_vel(false); // 碰到下边，Y速度必须变为负数（向上）
            hit_y = true;
        }

        // --- 增加随机性：如果只碰到了一个边，把另一个方向的速度也随机改变 ---
        if (hit_x && !hit_y) {
            m->vel_y = get_rand_vel_any(); // 碰到左右墙壁时，上下方向随机改变
        }
        if (hit_y && !hit_x) {
            m->vel_x = get_rand_vel_any(); // 碰到上下墙壁时，左右方向随机改变
        }

        // 应用新坐标
        lv_obj_set_pos(m->obj, (lv_coord_t)m->pos_x, (lv_coord_t)m->pos_y);
    }

    // 强制全屏重绘，消除拖影
    lv_obj_invalidate(lv_scr_act());
}

void lvgl_img_random_move_test_init(void)
{
    lv_rand_set_seed(esp_random());
    ESP_LOGI(TAG, "启动图片+圆形弹跳移动测试 (修复坐标与随机反弹)");

    // 设置纯黑背景
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    // 获取屏幕尺寸
    lv_display_t *disp = lv_display_get_default();
    lv_coord_t screen_w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t screen_h = lv_display_get_vertical_resolution(disp);

    // --- 1. 创建图片 ---
    lv_obj_t *img = lv_image_create(lv_scr_act());
    lv_image_set_src(img, "S:image2.png");
    lv_obj_set_style_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, 0);
    
    // --- 2. 创建圆形 ---
    lv_obj_t *circle = lv_obj_create(lv_scr_act());
    lv_obj_set_size(circle, 80, 80);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_border_width(circle, 0, 0);

    // 强制更新布局，以便获取到图片和圆形的真实宽高
    lv_obj_update_layout(lv_scr_act());

    // --- 3. 初始化图片位置和速度 ---
    // 【关键修复】：不使用 lv_obj_center，而是手动计算居中坐标，保持左上角对齐(LV_ALIGN_TOP_LEFT)
    movers[0].obj = img;
    movers[0].pos_x = (screen_w - lv_obj_get_width(img)) / 2.0f;
    movers[0].pos_y = (screen_h - lv_obj_get_height(img)) / 2.0f;
    lv_obj_set_pos(img, (lv_coord_t)movers[0].pos_x, (lv_coord_t)movers[0].pos_y);
    
    movers[0].vel_x = get_rand_vel_any();
    movers[0].vel_y = get_rand_vel_any();

    // --- 4. 初始化圆形位置和速度 ---
    movers[1].obj = circle;
    // 让圆形稍微偏离中心一点，避免两个物体完全重叠
    movers[1].pos_x = (screen_w - lv_obj_get_width(circle)) / 2.0f + 50.0f;
    movers[1].pos_y = (screen_h - lv_obj_get_height(circle)) / 2.0f - 50.0f;
    lv_obj_set_pos(circle, (lv_coord_t)movers[1].pos_x, (lv_coord_t)movers[1].pos_y);

    movers[1].vel_x = get_rand_vel_any();
    movers[1].vel_y = get_rand_vel_any();

    // 创建定时器，16ms 约等于 60FPS
    move_timer = lv_timer_create(move_timer_cb, 16, NULL);
    lv_timer_set_repeat_count(move_timer, LV_ANIM_REPEAT_INFINITE);
}