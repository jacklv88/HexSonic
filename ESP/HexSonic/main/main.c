// ------------------------------------------
// 📄 main/main.c
// ------------------------------------------
#include "driver_io.h"
#include "driver_lcd_touch.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 引入我们刚才封装好的音频模块
#include "audio_gmf_setup.h"

// LVGL 界面测试头文件
#include "lvgl_ahv_test.h"
// #include "lvgl_img_jpeg_png_test.h"
// #include "lvgl_img_random_move_test.h"
// #include "lvgl_img_rotate_test.h"
// #include "lvgl_font_size_test.h"
// #include "lvgl_arc_list_test.h"

#define TAG "main"

// 定义一个回调函数来接收播放状态
void my_audio_state_callback(audio_play_state_t state) {
    switch (state) {
    case AUDIO_STATE_PLAYING:
        ESP_LOGI("APP", "🎵 Music is Playing");
        break;
    case AUDIO_STATE_PAUSED:
        ESP_LOGI("APP", "⏸️ Music Paused");
        break;
    case AUDIO_STATE_STOPPED:
        ESP_LOGI("APP", "⏹️ Music Stopped");
        break;
    case AUDIO_STATE_FINISHED:
        ESP_LOGI("APP", "✅ Track Finished");
        break;
    case AUDIO_STATE_ERROR:
        ESP_LOGE("APP", "❌ Playback Error");
        break;
    default:
        break;
    }
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 1. 基础系统与外设初始化
    driver_io_mount_spiffs();
    driver_lcd_touch_init();

    // 3. LVGL UI 界面启动
    lvgl_port_lock(0);
    lvgl_ahv_test_init();
    lvgl_port_unlock();

    // 1. 初始化系统
    audio_gmf_system_init();

    // 2. 注册回调函数监听状态
    audio_player_set_callback(my_audio_state_callback);

    // 3. 测试控制指令 (可以通过 LVGL 按钮去触发这些接口)

    // 场景A: 播放本地音乐并开启无缝循环
    audio_player_play("/spiffs/1.mp3", true);

    // 延时10秒后暂停
    vTaskDelay(pdMS_TO_TICKS(10000));
    audio_player_pause();

    // 延时2秒后继续
    vTaskDelay(pdMS_TO_TICKS(2000));
    audio_player_resume();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        multi_heap_info_t info;

        heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        uint32_t sram_total =
            (info.total_free_bytes + info.total_allocated_bytes) / 1024;
        uint32_t sram_free = info.total_free_bytes / 1024;

        heap_caps_get_info(&info, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        uint32_t psram_total =
            (info.total_free_bytes + info.total_allocated_bytes) / 1024;
        uint32_t psram_free = info.total_free_bytes / 1024;

        ESP_LOGI(TAG, "Heap: SRAM %u/%u KB | PSRAM %u/%u KB", sram_free, sram_total,
                 psram_free, psram_total);
    }
}