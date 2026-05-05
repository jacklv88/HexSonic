idf_component_register(
    SRCS "lvgl_ahv_test.c" "main.c" "driver_io.c" "driver_lcd_touch.c"
         "lvgl_img_jpeg_png_test.c" "lvgl_img_rotate_test.c"
         "lvgl_img_random_move_test.c"
         "lvgl_font_size_test.c" 
         "lvgl_arc_list_test.c" 
        "lvgl_ahv_test.c"           
    PRIV_REQUIRES spi_flash lcd_pro esp_lvgl_port spiffs esp_lv_decoder
    INCLUDE_DIRS "."
)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)