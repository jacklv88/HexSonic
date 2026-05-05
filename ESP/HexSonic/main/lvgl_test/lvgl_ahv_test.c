// ------------------------------------------
// 📄 main/lvgl_ahv_test.c
// ------------------------------------------
#include "lvgl_ahv_test.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ahv_player";

/* ======================================================= */
/* 🌟 核心魔法：绕过隐式声明报错，强制声明 LVGL 内部的清除缓存函数 */
/* ======================================================= */
extern void lv_image_cache_drop(const void * src);

/* 播放器状态结构体 */
typedef struct {
    lv_fs_file_t file;
    lv_obj_t * img_obj;
    lv_timer_t * timer;
    
    // 视频元数据
    uint32_t frame_count;
    uint16_t width;
    uint16_t height;
    uint16_t interval_ms;
    
    // 双缓冲设计
    uint8_t * buffers[2];           
    size_t buffer_sizes[2];         
    lv_image_dsc_t img_dsc[2];      
    uint8_t active_idx;             
    
    // 循环播放控制
    uint32_t data_start_offset;     
    uint32_t current_frame;         
} ahv_player_t;

static ahv_player_t player = {0};

/* --- 辅助函数：读取大端整数 --- */
static uint32_t ahv_read_u32_be(lv_fs_file_t * f) {
    uint8_t buf[4];
    uint32_t br;
    lv_fs_read(f, buf, 4, &br);
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static uint16_t ahv_read_u16_be(lv_fs_file_t * f) {
    uint8_t buf[2];
    uint32_t br;
    lv_fs_read(f, buf, 2, &br);
    return (buf[0] << 8) | buf[1];
}

/* --- 定时器回调：双缓冲刷帧 --- */
static void ahv_timer_cb(lv_timer_t * t) {
    uint8_t next_idx = 1 - player.active_idx;
    
    // 1. 循环播放检查
    if (player.current_frame >= player.frame_count) {
        ESP_LOGI(TAG, "--- 循环播放: 回到第 0 帧 ---");
        lv_fs_seek(&player.file, player.data_start_offset, LV_FS_SEEK_SET);
        player.current_frame = 0;
    }
    
    uint32_t br;
    uint8_t len_buf[4];
    lv_fs_res_t res = lv_fs_read(&player.file, len_buf, 4, &br);
    if (res != LV_FS_RES_OK || br < 4) {
        lv_fs_seek(&player.file, player.data_start_offset, LV_FS_SEEK_SET);
        lv_fs_read(&player.file, len_buf, 4, &br);
        player.current_frame = 0;
    }
    
    uint32_t frame_len = (len_buf[0] << 24) | (len_buf[1] << 16) | (len_buf[2] << 8) | len_buf[3];
    
    // 2. 检查闲置缓冲区的内存是否足够大
    if (frame_len > player.buffer_sizes[next_idx]) {
        if (player.buffers[next_idx]) {
            heap_caps_free(player.buffers[next_idx]);
        }
        player.buffers[next_idx] = heap_caps_malloc(frame_len, MALLOC_CAP_SPIRAM);
        if (!player.buffers[next_idx]) {
            ESP_LOGE(TAG, "内存分配失败! 帧大小: %lu bytes", frame_len);
            return;
        }
        player.buffer_sizes[next_idx] = frame_len;
    }
    
    // 3. 读取实际帧数据
    lv_fs_read(&player.file, player.buffers[next_idx], frame_len, &br);
    if (br < frame_len) {
        ESP_LOGE(TAG, "读取帧数据不完整!");
        return;
    }
    
    // 4. 更新图像描述符
    player.img_dsc[next_idx].data_size = frame_len;
    player.img_dsc[next_idx].data = player.buffers[next_idx];
    
    // 🌟 在提供给 LVGL 之前，精准爆破对应的旧缓存！
    lv_image_cache_drop(&player.img_dsc[next_idx]);
    
    // 5. 提交刷新
    lv_image_set_src(player.img_obj, NULL);
    lv_image_set_src(player.img_obj, &player.img_dsc[next_idx]);
    
    // 6. 打印日志验证定时器是否存活
    ESP_LOGI(TAG, "正在刷新: 帧索引 %lu, 大小 %lu bytes, 使用缓存区 %d", 
             player.current_frame, frame_len, next_idx);
    
    player.active_idx = next_idx;
    player.current_frame++;
}

/* --- 主初始化入口 --- */
void lvgl_ahv_test_init(void) {
    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    ESP_LOGI(TAG, "正在打开 AHV 视频文件...");
    lv_fs_res_t res = lv_fs_open(&player.file, "S:seao.ahv", LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "无法打开 S:seao.ahv (错误码: %d)", res);
        return;
    }

    uint8_t magic[4], version[4], mode;
    uint32_t br;
    lv_fs_read(&player.file, magic, 4, &br);
    if (memcmp(magic, "\x2e\x61\x68\x76", 4) != 0) {
        ESP_LOGE(TAG, "文件幻数错误！");
        lv_fs_close(&player.file);
        return;
    }
    
    lv_fs_read(&player.file, version, 4, &br);
    lv_fs_read(&player.file, &mode, 1, &br);
    
    player.frame_count = ahv_read_u32_be(&player.file);
    player.width = ahv_read_u16_be(&player.file);
    player.height = ahv_read_u16_be(&player.file);
    player.interval_ms = ahv_read_u16_be(&player.file);
    
    ESP_LOGI(TAG, "总帧数: %lu, 刷新间隔: %d ms", player.frame_count, player.interval_ms);
    
    uint8_t dummy[50];
    lv_fs_read(&player.file, dummy, 50, &br);
    
    lv_fs_tell(&player.file, &player.data_start_offset);
    player.current_frame = 0;
    player.active_idx = 0;

    for (int i = 0; i < 2; i++) {
        memset(&player.img_dsc[i], 0, sizeof(lv_image_dsc_t));
        player.img_dsc[i].header.magic = LV_IMAGE_HEADER_MAGIC;
        player.img_dsc[i].header.cf = LV_COLOR_FORMAT_RAW;
        player.img_dsc[i].header.w = player.width;
        player.img_dsc[i].header.h = player.height;
    }

    player.img_obj = lv_image_create(scr);
    lv_obj_center(player.img_obj);

    uint32_t timer_interval = (player.interval_ms > 0) ? player.interval_ms : 66; 
    player.timer = lv_timer_create(ahv_timer_cb, timer_interval, NULL);
    
    ESP_LOGI(TAG, "乒乓缓冲播放器启动成功！");
}