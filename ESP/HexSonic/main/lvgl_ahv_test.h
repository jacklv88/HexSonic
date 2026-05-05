// ------------------------------------------
// 📄 main/lvgl_ahv_test.h
// ------------------------------------------
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 AHV 视频播放测试界面
 * 从 SPIFFS (S: 盘) 中读取 seao.ahv 并逐帧通过定时器刷新到屏幕
 */
void lvgl_ahv_test_init(void);

#ifdef __cplusplus
}
#endif