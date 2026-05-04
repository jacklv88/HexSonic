# 📄 main/CMakeLists.txt
idf_component_register(
    SRCS "main.c" "driver_io.c" "driver_lcd_touch.c" "lvgl_img_jpeg_png_test.c"
    # 👇 在下面这行追加 esp_lv_decoder
    PRIV_REQUIRES spi_flash lcd_pro esp_lvgl_port spiffs esp_lv_decoder
    INCLUDE_DIRS "."
)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)