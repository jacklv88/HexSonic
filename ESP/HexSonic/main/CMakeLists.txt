# 📄 main/CMakeLists.txt

# 定义 lvgl_test 目录下的源文件
set(lvgl_test_srcs
    "lvgl_test/lvgl_ahv_test.c"
    "lvgl_test/lvgl_arc_list_test.c"
    "lvgl_test/lvgl_font_size_test.c"
    "lvgl_test/lvgl_img_jpeg_png_test.c"
    "lvgl_test/lvgl_img_random_move_test.c"
    "lvgl_test/lvgl_img_rotate_test.c"
)

idf_component_register(
    SRCS "main.c" 
         "driver_io.c" 
         "driver_lcd_touch.c"
         "audio_code.c"
         "audio_gmf_setup.c"
         ${lvgl_test_srcs}  # 引入测试界面源码
    
    PRIV_REQUIRES 
        spi_flash 
        lcd_pro 
        esp_lvgl_port 
        spiffs 
        esp_lv_decoder
        driver esp_audio_codec gmf_audio gmf_core gmf_io gmf_loader
        
    INCLUDE_DIRS 
        "." 
        "lvgl_test"  # 必须添加此项，否则 main.c 找不到测试界面的头文件
)

# 创建 SPIFFS 分区镜像
spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)