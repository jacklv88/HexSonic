#ifndef _AUDIO_CODE_
#define _AUDIO_CODE_

// 将 int i2c_num 替换为 void * 句柄
char init_audio_code(void *i2c_dev_handle, unsigned char vol);
void set_volume_audio_code(int val);
void set_mute_audio_code(char status);

#endif