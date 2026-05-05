// ------------------------------------------
// 📄 main/audio_gmf_setup.h
// ------------------------------------------
#ifndef AUDIO_GMF_SETUP_H
#define AUDIO_GMF_SETUP_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 定义播放器状态枚举
typedef enum {
    AUDIO_STATE_NONE = 0,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
    AUDIO_STATE_STOPPED,
    AUDIO_STATE_FINISHED,
    AUDIO_STATE_ERROR
} audio_play_state_t;

// 定义状态回调函数指针类型
typedef void (*audio_state_cb_t)(audio_play_state_t state);

/**
 * @brief 1. 初始化整个硬件和音频底层框架
 */
esp_err_t audio_gmf_system_init(void);

/**
 * @brief 2. 注册播放状态监听回调
 */
void audio_player_set_callback(audio_state_cb_t cb);

/**
 * @brief 3. 播放指定的音乐 (支持本地路径 /sdcard/... 或 网络 http://...)
 * @param uri 音乐路径或URL
 * @param gapless 是否开启无缝单曲循环 (针对本地MP3)
 */
esp_err_t audio_player_play(const char *uri, bool gapless);

/**
 * @brief 4. 播放控制接口
 */
esp_err_t audio_player_pause(void);
esp_err_t audio_player_resume(void);
esp_err_t audio_player_stop(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_GMF_SETUP_H