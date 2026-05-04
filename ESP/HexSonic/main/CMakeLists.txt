idf_component_register(SRCS "main.c" "driver_io.c" "driver_lcd_touch.c"
                       PRIV_REQUIRES spi_flash lcd_pro esp_lvgl_port
                       INCLUDE_DIRS ".")