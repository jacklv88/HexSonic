// ------------------------------------------
// 📄 main/audio_gmf_setup.c
// ------------------------------------------
#include "audio_gmf_setup.h"
#include "esp_log.h"
#include <string.h>

// 硬件驱动层
#include "audio_code.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"

// ESP Codec Dev 抽象层
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

// GMF 核心与组件层
#include "esp_gmf_element.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "gmf_loader_setup_defaults.h"
#include "esp_gmf_io_codec_dev.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ==========================================
// 🎛️ 硬件引脚与参数宏定义 (集中配置区)
// ==========================================
#define AUDIO_I2C_NUM           (1)         
#define AUDIO_I2C_SDA_IO        (11)        
#define AUDIO_I2C_SCL_IO        (10)        
#define AUDIO_I2C_SPEED_HZ      (400000)    

#define AUDIO_I2S_NUM           (1)         
#define AUDIO_I2S_MCLK_IO       (13)        
#define AUDIO_I2S_BCLK_IO       (8)         
#define AUDIO_I2S_WS_IO         (7)         
#define AUDIO_I2S_DOUT_IO       (9)         
#define AUDIO_I2S_DIN_IO        (-1)        

#define AUDIO_DEFAULT_RATE      (44100)     
#define AUDIO_AMP_DEFAULT_VOL   (60)        

static const char *TAG = "AudioPlayer";
static i2s_chan_handle_t tx_chan = NULL;
static esp_codec_dev_handle_t playback_handle = NULL;

typedef struct {
    esp_gmf_pool_handle_t pool;
    esp_gmf_pipeline_handle_t pipe;
    esp_gmf_task_handle_t work_task;
    audio_state_cb_t state_cb;
    char *current_uri;
    bool is_gapless;
} player_ctx_t;

static player_ctx_t s_player = {0};

// ==========================================
// 1. 硬件初始化 (I2C, ACM86xx, I2S)
// ==========================================
static esp_err_t audio_hardware_init(void) {
    esp_err_t ret;

    // 1.1 初始化 I2C
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = AUDIO_I2C_NUM,
        .scl_io_num = AUDIO_I2C_SCL_IO,
        .sda_io_num = AUDIO_I2C_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ret = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
    if (ret != ESP_OK) return ret;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = (0x68 >> 1), 
        .scl_speed_hz = AUDIO_I2C_SPEED_HZ,
    };
    i2c_master_dev_handle_t audio_dev_handle;
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &audio_dev_handle);
    if (ret != ESP_OK) return ret;

    // 1.2 初始化 ACM86xx 功放芯片
    ESP_LOGI(TAG, "Init ACM86xx Codec...");
    if (init_audio_code((void *)audio_dev_handle, AUDIO_AMP_DEFAULT_VOL) != 0) {
        ESP_LOGE(TAG, "ACM86xx Init Failed!");
        return ESP_FAIL;
    } 
    set_mute_audio_code(0); 

    // 1.3 初始化 I2S 
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(AUDIO_I2S_NUM, I2S_ROLE_MASTER); 
    chan_cfg.auto_clear = true; // 🌟 修复点 1：开启 DMA 自动清理
    ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK) return ret;

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_DEFAULT_RATE), 
        // 🌟 修复点 2：改用标准的飞利浦 I2S 协议 (PHILIPS_SLOT)
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            // 🌟 修复点 3：强制不输出 MCLK (因为能响的代码里硬编码了 -1)
            .mclk = -1, 
            .bclk = AUDIO_I2S_BCLK_IO,  
            .ws   = AUDIO_I2S_WS_IO,  
            .dout = AUDIO_I2S_DOUT_IO,  
            .din  = AUDIO_I2S_DIN_IO, 
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        },
    };
    // 🌟 修复点 4：强制将 MCLK 倍数设为 384
    std_cfg.clk_cfg.mclk_multiple = 384;

    ret = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (ret != ESP_OK) return ret;
    i2s_channel_enable(tx_chan);

    // 1.4 I2S 包装为 Codec Dev
    audio_codec_i2s_cfg_t i2s_codec_cfg = {
        .port = AUDIO_I2S_NUM,  
        .rx_handle = NULL,      
        .tx_handle = tx_chan,   
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_codec_cfg);
    
    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = NULL,       
        .data_if = data_if,     
    };
    playback_handle = esp_codec_dev_new(&codec_dev_cfg);
    
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = AUDIO_DEFAULT_RATE,
        .channel = 2,
        .bits_per_sample = 16,
    };
    esp_codec_dev_open(playback_handle, &fs);
    return ESP_OK;
}

// ==========================================
// 2. 异步重启任务 (解决死锁与超时问题)
// ==========================================
static void gapless_restart_task(void *arg) {
    ESP_LOGI(TAG, "Gapless Loop triggered! Reloading pipeline asynchronously...");
    
    esp_gmf_pipeline_reset(s_player.pipe);
    esp_gmf_io_set_uri(s_player.pipe->in, s_player.current_uri);
    esp_gmf_pipeline_loading_jobs(s_player.pipe);
    esp_gmf_pipeline_run(s_player.pipe);
    
    vTaskDelete(NULL); 
}

// ==========================================
// 3. 事件监听回调
// ==========================================
static esp_gmf_err_t _pipeline_event_cb(esp_gmf_event_pkt_t *event, void *ctx) {
    if (!s_player.state_cb) return ESP_GMF_ERR_OK;

    audio_play_state_t state = AUDIO_STATE_NONE;
    switch (event->sub) {
        case ESP_GMF_EVENT_STATE_RUNNING:  state = AUDIO_STATE_PLAYING; break;
        case ESP_GMF_EVENT_STATE_PAUSED:   state = AUDIO_STATE_PAUSED; break;
        case ESP_GMF_EVENT_STATE_STOPPED:  state = AUDIO_STATE_STOPPED; break;
        case ESP_GMF_EVENT_STATE_ERROR:    state = AUDIO_STATE_ERROR; break;
        case ESP_GMF_EVENT_STATE_FINISHED: 
            if (s_player.is_gapless && s_player.current_uri && strncmp(s_player.current_uri, "http", 4) != 0) {
                xTaskCreate(gapless_restart_task, "gapless_res", 3072, NULL, 5, NULL);
                return ESP_GMF_ERR_OK; 
            } else {
                state = AUDIO_STATE_FINISHED; 
            }
            break;
        default: break;
    }

    if (state != AUDIO_STATE_NONE) {
        s_player.state_cb(state);
    }
    return ESP_GMF_ERR_OK;
}

// ==========================================
// 4. 播放器控制公开接口
// ==========================================
esp_err_t audio_gmf_system_init(void) {
    esp_err_t ret = audio_hardware_init();
    if (ret != ESP_OK) return ret;

    esp_gmf_pool_init(&s_player.pool);
    gmf_loader_setup_io_default(s_player.pool); 
    gmf_loader_setup_audio_codec_default(s_player.pool);
    gmf_loader_setup_audio_effects_default(s_player.pool);

    return ESP_OK;
}

void audio_player_set_callback(audio_state_cb_t cb) {
    s_player.state_cb = cb;
}

esp_err_t audio_player_play(const char *uri, bool gapless) {
    if (!uri) return ESP_ERR_INVALID_ARG;

    if (s_player.pipe) {
        esp_gmf_pipeline_stop(s_player.pipe);
        esp_gmf_task_deinit(s_player.work_task);
        esp_gmf_pipeline_destroy(s_player.pipe);
        s_player.pipe = NULL;
    }

    if (s_player.current_uri) free(s_player.current_uri);
    s_player.current_uri = strdup(uri);
    s_player.is_gapless = gapless;

    const char *in_name = (strncmp(uri, "http", 4) == 0) ? "io_http" : "io_file";
    const char *name[] = {"aud_dec", "aud_ch_cvt", "aud_bit_cvt", "aud_rate_cvt"};
    
    esp_err_t ret = esp_gmf_pool_new_pipeline(s_player.pool, in_name, name, 4, "io_codec_dev", &s_player.pipe);
    if (ret != ESP_OK) return ret;

    esp_gmf_element_handle_t out_el = ESP_GMF_PIPELINE_GET_OUT_INSTANCE(s_player.pipe);
    esp_gmf_io_codec_dev_set_dev(out_el, playback_handle);
    esp_gmf_pipeline_set_in_uri(s_player.pipe, uri);

    esp_gmf_task_cfg_t task_cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    task_cfg.thread.stack_in_ext = false; 
    task_cfg.name = "gmf_pipe_task";
    esp_gmf_task_init(&task_cfg, &s_player.work_task);

    esp_gmf_pipeline_set_event(s_player.pipe, _pipeline_event_cb, NULL);
    esp_gmf_pipeline_bind_task(s_player.pipe, s_player.work_task);
    esp_gmf_pipeline_loading_jobs(s_player.pipe);

    ESP_LOGI(TAG, "Playing: %s (Gapless: %s)", uri, gapless ? "YES" : "NO");
    return esp_gmf_pipeline_run(s_player.pipe);
}

esp_err_t audio_player_pause(void) {
    if (s_player.pipe) return esp_gmf_pipeline_pause(s_player.pipe);
    return ESP_FAIL;
}

esp_err_t audio_player_resume(void) {
    if (s_player.pipe) return esp_gmf_pipeline_resume(s_player.pipe);
    return ESP_FAIL;
}

esp_err_t audio_player_stop(void) {
    if (s_player.pipe) return esp_gmf_pipeline_stop(s_player.pipe);
    return ESP_FAIL;
}