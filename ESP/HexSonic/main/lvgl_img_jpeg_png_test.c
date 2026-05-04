#include "lvgl_img_jpeg_png_test.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "lvgl.h"
#include <stdio.h>

// LVGL 9.x 解码器头文件

static const char *TAG = "lvgl_img_test";

#define SPIFFS_MOUNT_POINT  "/spiffs"
#define SPIFFS_PART_LABEL   "storage"
#define LV_FS_LETTER        'S'  // 对应 S: 前缀

static lv_obj_t *img_obj = NULL;
static bool showing_png = true;

/* -------------------------- LVGL 文件系统驱动回调 -------------------------- */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    char full_path[64];
    snprintf(full_path, sizeof(full_path), "%s/%s", SPIFFS_MOUNT_POINT, path);
    ESP_LOGI(TAG, "[FS] 打开: %s", full_path);

    FILE *f = NULL;
    if (mode == LV_FS_MODE_WR) f = fopen(full_path, "wb");
    else if (mode == LV_FS_MODE_RD) f = fopen(full_path, "rb");
    else if (mode == (LV_FS_MODE_WR|LV_FS_MODE_RD)) f = fopen(full_path, "rb+");

    if (!f) ESP_LOGE(TAG, "[FS] 打开失败: %s", full_path);
    return f;
}

static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    if (!file_p) return LV_FS_RES_INV_PARAM;
    ESP_LOGI(TAG, "[FS] 关闭文件");
    return (fclose((FILE*)file_p) == 0) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    if (!file_p || !buf || !br) return LV_FS_RES_INV_PARAM;
    *br = fread(buf, 1, btr, (FILE*)file_p);
    // ESP_LOGI(TAG, "[FS] 读取: 请求%u, 实际%u", btr, *br); // 太刷屏可注释
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    if (!file_p) return LV_FS_RES_INV_PARAM;
    int w = (whence==LV_FS_SEEK_SET)?SEEK_SET:(whence==LV_FS_SEEK_CUR)?SEEK_CUR:SEEK_END;
    return (fseek((FILE*)file_p, pos, w) == 0) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    if (!file_p || !pos_p) return LV_FS_RES_INV_PARAM;
    *pos_p = (uint32_t)ftell((FILE*)file_p);
    return LV_FS_RES_OK;
}

/* -------------------------- 按钮切换回调 -------------------------- */
static void toggle_img(lv_event_t *e)
{
    if (showing_png) {
        lv_image_set_src(img_obj, "S:image.jpg");
        showing_png = false;
        ESP_LOGI(TAG, "切换到 JPG");
    } else {
        lv_image_set_src(img_obj, "S:image.png");
        showing_png = true;
        ESP_LOGI(TAG, "切换到 PNG");
    }
}

/* -------------------------- 初始化函数 -------------------------- */
void lvgl_img_jpeg_png_test_init(void)
{
    ESP_LOGI(TAG, "===== 开始初始化 =====");

    // 1. 初始化 SPIFFS
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = SPIFFS_MOUNT_POINT,
        .partition_label = SPIFFS_PART_LABEL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS 挂载失败: %s", esp_err_to_name(ret));
        return;
    }
    size_t total=0, used=0;
    esp_spiffs_info(SPIFFS_PART_LABEL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS: 总%u, 已用%u", total, used);

    // 2. 注册 LVGL 文件系统
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = LV_FS_LETTER;
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    lv_fs_drv_register(&fs_drv);
    ESP_LOGI(TAG, "LVGL 文件系统已注册 (S:)");


    ESP_LOGI(TAG, "LVGL 解码器已初始化");

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