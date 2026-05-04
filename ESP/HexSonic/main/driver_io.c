#include "driver_io.h"
#include "driver/gpio.h"

void driver_io_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 15),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(15, 1);  // 高电平使能
}