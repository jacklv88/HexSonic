#include "lvgl_arc_list_test.h"
#include "esp_log.h"
#include "lvgl.h"
#include <math.h>

static const char *TAG = "arc_list_round";


/* 针对 412x412 圆形屏幕优化的滑动算法 (带性能优化) */
// static void scroll_event_cb(lv_event_t * e)
// {
//     lv_obj_t * cont = lv_event_get_target(e);
    
//     /* 圆心 Y 坐标为 412 / 2 = 206 */
//     const int32_t screen_center_y = 206; 
//     /* 定义视口边界，超过这个距离的 Cell 判定为不可见 (206 + 80的卡片高度 ≈ 286) */
//     const int32_t visible_threshold = 280; 

//     uint32_t child_cnt = lv_obj_get_child_cnt(cont);
//     for(uint32_t i = 0; i < child_cnt; i++) {
//         lv_obj_t * child = lv_obj_get_child(cont, i);
        
//         if(!lv_obj_has_flag(child, LV_OBJ_FLAG_SNAPPABLE)) continue; 

//         /* 获取 Y 坐标 */
//         lv_area_t child_a;
//         lv_obj_get_coords(child, &child_a);
//         int32_t child_y_center = child_a.y1 + lv_area_get_height(&child_a) / 2;
//         int32_t diff_y = child_y_center - screen_center_y;
//         int32_t abs_diff_y = abs(diff_y);

//         /* 🚀 优化 1：视口剔除 (Culling) */
//         /* 如果卡片距离中心太远（完全在屏幕外），直接隐藏并跳过抛物线计算 */
//         if (abs_diff_y > visible_threshold) {
//             /* 避免重复设置样式触发重绘 */
//             if (lv_obj_get_style_opa(child, 0) != 0) {
//                 lv_obj_set_style_opa(child, 0, 0);
//             }
//             continue; // 跳过后续的乘除法计算
//         }

//         /* --- 核心算法：圆弧拟合 --- */
//         int32_t x_ofs = (diff_y * diff_y) / 500; 
        
//         int32_t scale = 256 - (abs_diff_y * 120 / 206);
//         if(scale < 150) scale = 150;
        
//         int32_t opa = 255 - (abs_diff_y * 180 / 206);
//         if(opa < 0) opa = 0;

//         /* 🚀 优化 2：LVGL 底层虽然有状态比对，但我们直接赋值 */
//         lv_obj_set_style_translate_x(child, x_ofs, 0);
//         lv_obj_set_style_transform_scale(child, scale, 0);
//         lv_obj_set_style_opa(child, opa, 0);
//     }
// }

static void scroll_event_cb(lv_event_t * e)
{
    lv_obj_t * cont = lv_event_get_target(e);
    const int32_t screen_center_y = 206; 
    const int32_t visible_threshold = 280; 

    uint32_t child_cnt = lv_obj_get_child_cnt(cont);
    for(uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        
        if(!lv_obj_has_flag(child, LV_OBJ_FLAG_SNAPPABLE)) continue; 

        lv_area_t child_a;
        lv_obj_get_coords(child, &child_a);
        int32_t child_y_center = child_a.y1 + lv_area_get_height(&child_a) / 2;
        int32_t diff_y = child_y_center - screen_center_y;
        
        /* 视口剔除：屏幕外的直接不管 */
        if (abs(diff_y) > visible_threshold) {
            continue; 
        }

        /* 核心视觉保留：X轴的圆弧偏移 (这个运算非常快) */
        int32_t x_ofs = (diff_y * diff_y) / 400; // 稍微改小一点除数，让弧度更明显
        lv_obj_set_style_translate_x(child, x_ofs, 0);

        /* * 💣 性能杀手：全部注释掉！
         * 不要缩放，不要透明度混合！
         */
        // int32_t scale = ...
        // lv_obj_set_style_transform_scale(child, scale, 0);
        // int32_t opa = ...
        // lv_obj_set_style_opa(child, opa, 0);
    }
}

void lvgl_arc_list_test_init(void)
{
    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); // 纯黑底适合圆形屏

    /* 1. 创建全屏容器 */
    lv_obj_t * list = lv_obj_create(scr);
    lv_obj_set_size(list, 412, 412);
    lv_obj_center(list);
    
    /* 关键设置：切圆并隐藏超出部分 */
    lv_obj_set_style_radius(list, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(list, true, 0);
    lv_obj_set_style_bg_opa(list, 0, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    
    /* 布局设置 */
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(list, 150, 0); // 上下留白，确保第一个/最后一个元素能滑到中心
    lv_obj_set_style_pad_row(list, 20, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    /* 滑动吸附：让 Cell 停在屏幕正中央 */
    lv_obj_set_scroll_snap_y(list, LV_SCROLL_SNAP_CENTER);

    /* 2. 创建 30 个 Cell */
    for(int i = 0; i < 30; i++) {
        lv_obj_t * cell = lv_obj_create(list);
        lv_obj_set_size(cell, 280, 80); // 宽度不宜占满 412，留出圆弧移动空间
        lv_obj_set_style_bg_color(cell, lv_color_hex(0x1E1E1E), 0);
        lv_obj_set_style_radius(cell, 20, 0);
        lv_obj_set_style_border_width(cell, 0, 0);
        lv_obj_add_flag(cell, LV_OBJ_FLAG_SNAPPABLE);

        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(cell, 10, 0);

        /* 图片 60px */
        lv_obj_t * img = lv_image_create(cell);
        lv_image_set_src(img, "S:image3.png");
        lv_obj_set_size(img, 60, 60);

        /* 文字 */
        lv_obj_t * label = lv_label_create(cell);
        lv_label_set_text_fmt(label, "Item %02d", i + 1);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    }

    lv_obj_add_event_cb(list, scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_send_event(list, LV_EVENT_SCROLL, NULL); // 初始排列
}