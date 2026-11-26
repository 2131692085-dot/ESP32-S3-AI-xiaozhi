#include <stdio.h>
#include "esp_log.h"
#include "adc_button.h"
#include "xiaozhi_wifi.h"
#include "xiaozhi_lcd.h"
#include "xiaozhi_lvgl.h"
#include "xiaozhi_active.h"
#include "xiaozhi_common.h"
#include "xiaozhi_audio.h"
#include "xiaozhi_sr.h"
#include "xiaozhi_audio_encoder.h"
#include "xiaozhi_audio_decoder.h"
#include "xiaozhi_ws.h"

void adc_button_cb(void *arg, void *usr_data);
// 创建所有的环形缓冲区
void create_all_ring_buffer(void);
// 注册被唤醒的回调函数
void xiaozhi_wakeup_callback(void);
// 注册vad改变的回调
void xiaozhi_sr_vad_change_callback(void);
// 处理文本帧数据的方法
void xiaozhi_process_text_frame(char *text_frame);
// 处理二进制音频数据帧的方法
void xiaozhi_process_binary_audio_frame(char *bin_data, int data_len);
// 创建一个任务 用于处理小智的音频数据发送至服务器
void xiaozhi_send_audio_task(void *pvParameters);
// 封装一个测试任务 用于短接两个环形缓冲区
void xiaozhi_ring_buffer_test(void *pvParameters);
#define TAG "main"

xiaozhi_handle_t xiaozhi_handle;

typedef enum
{
  SW2_SINGLE_CLICK,
  SW3_SINGLE_CLICK,
  SW3_LONG_PRESS_START,
} adc_button_event_t;
void app_main(void)
{
  // 创建所有的环形缓冲区
  create_all_ring_buffer();
  // 初始化和ws相关的事件标志组
  xiaozhi_handle.ws_event_group = xEventGroupCreate();
  // 注册被唤醒的回调函数
  xiaozhi_handle.wakeup_callback = xiaozhi_wakeup_callback;
  //  注册vad改变的回调
  xiaozhi_handle.vad_change_callback = xiaozhi_sr_vad_change_callback;
  // 注册处理文本帧数据的方法
  xiaozhi_handle.process_text_frame = xiaozhi_process_text_frame;
  // 注册处理二进制音频数据帧的方法
  xiaozhi_handle.process_binary_audio_frame = xiaozhi_process_binary_audio_frame;

  // Initialize ADC button
  adc_button_init();
  // 注册sw2注册单击事件的回调
  adc_button_register_sw2_callback(BUTTON_SINGLE_CLICK, adc_button_cb, (void *)SW2_SINGLE_CLICK);
  // 注册sw3的单击事件回调
  adc_button_register_sw3_callback(BUTTON_SINGLE_CLICK, adc_button_cb, (void *)SW3_SINGLE_CLICK);
  // 注册sw3的长按事件回调
  adc_button_register_sw3_callback(BUTTON_LONG_PRESS_START, adc_button_cb, (void *)SW3_LONG_PRESS_START);

  // 初始化LVGL
  xiaozhi_lvgl_init();
  // wifi的初始化
  xiaozhi_wifi_init();
  // 获取验证码的初始化
  xiaozhi_active_init();
  // 初始化音频
  xiaozhi_audio_init();
  //! 初始化小智的状态
  xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_IDLE;
  // 初始化语音识别
  xiaozhi_sr_init();
  // 初始化音频编码器
  xiaozhi_audio_encoder_init();
  // 初始化音频解码器
  xiaozhi_audio_decoder_init();
  // 初始化ws
  xiaozhi_ws_init();

  // TODO 测试任务
  // xTaskCreatePinnedToCoreWithCaps(xiaozhi_ring_buffer_test, "xiaozhi_ring_buffer_test", 16 * 1024, NULL, 5, NULL, 1, MALLOC_CAP_SPIRAM);

  xTaskCreatePinnedToCoreWithCaps(xiaozhi_send_audio_task, "xiaozhi_send_audio_task", 16 * 1024, NULL, 5, NULL, 1, MALLOC_CAP_SPIRAM);
  // uint8_t record_data[1024];
  // while(1)
  // {
  //   //录音
  //   xiaozhi_audio_record(record_data, 1024);
  //   //播放录音
  //   xiaozhi_audio_play(record_data, 1024);
  // }

  // xiaozhi_lvgl_show_hello_world();

  // 测试 初始化LCD
  // xiaozhi_lcd_init();
  // xiaozhi_lcd_show_image();
}

void adc_button_cb(void *arg, void *usr_data)
{
  uint8_t event_type = (uint8_t)usr_data;
  switch (event_type)
  {
  case SW2_SINGLE_CLICK:
    ESP_LOGI(TAG, "SW2 SINGLE CLICK");
    break;
  case SW3_SINGLE_CLICK:
    ESP_LOGI(TAG, "SW3 SINGLE CLICK");
    break;
  case SW3_LONG_PRESS_START:
    ESP_LOGI(TAG, "SW3 LONG PRESS START");
    xiaozhi_wifi_reset();
    break;
  default:
  }
}

void create_all_ring_buffer(void)
{
  xiaozhi_handle.ring_buffer_sr_to_encoder_handle = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
  xiaozhi_handle.ring_buffer_encoder_to_ws_handle = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
  xiaozhi_handle.ring_buffer_ws_to_encoder_handle = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
}

// 封装一个测试任务 用于短接两个环形缓冲区
void xiaozhi_ring_buffer_test(void *pvParameters)
{
  while (1)
  {
    size_t data_size = 0;
    // 从encoder到ws的环形缓冲区中获取数据
    void *rev_pointer = xRingbufferReceive(xiaozhi_handle.ring_buffer_encoder_to_ws_handle, &data_size, portMAX_DELAY);
    // 放入ws到encoder的环形缓冲区中
    xRingbufferSend(xiaozhi_handle.ring_buffer_ws_to_encoder_handle, rev_pointer, data_size, portMAX_DELAY);
    // 释放环形缓冲区中的数据
    vRingbufferReturnItem(xiaozhi_handle.ring_buffer_encoder_to_ws_handle, rev_pointer);
  }
}

// 注册被唤醒的回调函数
void xiaozhi_wakeup_callback(void)
{
  if (xiaozhi_handle.xiaozhi_state == XIAOZHI_STATE_IDLE)
  {
    bool is_connected = false;
    is_connected = xiaozhi_ws_connect_sync();
    if (is_connected)
    {
      // 发送hello消息
      xiaozhi_ws_send_hello();
      // 以阻塞的方式等待hello消息被发送完成
      EventBits_t res = xEventGroupWaitBits(xiaozhi_handle.ws_event_group, WS_HELLO_SENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
      if (res & WS_HELLO_SENT_BIT)
      {
        ESP_LOGI(TAG, "WS_HELLO_SENT_BIT");
        //! 发送iot描述
        xiaozhi_ws_send_iot_desc();
        //! 发送远程控制初始化状态
        xiaozhi_ws_send_iot_state();
        xiaozhi_ws_send_wakeup();
      }
    }
  }
  else if (xiaozhi_handle.xiaozhi_state == XIAOZHI_STATE_SPEAKING)
  {
    // 发送终止消息
    xiaozhi_ws_send_abort();
    // 发送唤醒词
    xiaozhi_ws_send_wakeup();
  }
}

// 注册vad改变的回调
void xiaozhi_sr_vad_change_callback(void)
{
  // 检测到人声：1.我们在说话 2.小智在说话（需排除
  if (xiaozhi_handle.vad_current_state == VAD_SPEECH)
  {
    if (xiaozhi_handle.xiaozhi_state == XIAOZHI_STATE_IDLE)
    {
      // 我们在说话
      xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_LISTENING;
      // 发送开始监听的消息
      xiaozhi_ws_send_start_listen();
    }
  }
  // 检测到静默:1.我们没说话 2.小智在说话，但是卡顿了
  if (xiaozhi_handle.vad_current_state == VAD_SILENCE)
  {
    if (xiaozhi_handle.xiaozhi_state != XIAOZHI_STATE_SPEAKING)
    {
      // 我们没说话
      xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_IDLE;
      // 发送停止监听的消息
      xiaozhi_ws_send_stop_listen();
    }
  }
}

// 处理文本帧数据的方法
void xiaozhi_process_text_frame(char *text_frame)
{
  cJSON *root = cJSON_Parse(text_frame);
  if (root == NULL)
  {
    ESP_LOGI(TAG, "JSON_Parse failed");
    return;
  }
  cJSON *type = cJSON_GetObjectItem(root, "type");
  if (strcmp(type->valuestring, "hello") == 0)
  {
    //! 修改hello回应成功的标志位为1
    xEventGroupSetBits(xiaozhi_handle.ws_event_group, WS_HELLO_SENT_BIT);
  }
  // 判断是不是emoji
  if (strcmp(type->valuestring, "llm") == 0)
  {
    // 处理emoji
    char *emotion = cJSON_GetObjectItem(root, "emotion")->valuestring;
    xiaozhi_lvgl_set_emoji(emotion);
  }

  // 判断是不是文本
  if (strcmp(type->valuestring, "tts") == 0)
  {
    // 处理文本
    char *state = cJSON_GetObjectItem(root, "state")->valuestring;
    if (strcmp(state, "sentence_start") == 0)
    {
      char *text = cJSON_GetObjectItem(root, "text")->valuestring;
      xiaozhi_lvgl_set_text(text);
    }

    // 判断小智开始说话
    if (strcmp(state, "start") == 0)
    {
      xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_SPEAKING;
    }
    // 判断小智结束说话
    if (strcmp(state, "stop") == 0)
    {
      xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_IDLE;
    }
  }

  if (strcmp(type->valuestring, "iot") == 0)
  {
    cJSON *cmdsItem = cJSON_GetObjectItem(root, "commands");
    cJSON *cmdItem = cJSON_GetArrayItem(cmdsItem, 0);
    cJSON *parametersItem = cJSON_GetObjectItem(cmdItem, "parameters");

    char *method = cJSON_GetObjectItem(cmdItem, "method")->valuestring;

    if (strcmp(method, "SetVolume") == 0)
    {
      int volumne = cJSON_GetObjectItem(parametersItem, "volume")->valueint;

      xiaozhi_audio_volume_set(volumne);
    }
    else if (strcmp(method, "SetMute") == 0)
    {
      cJSON *muteItem = cJSON_GetObjectItem(parametersItem, "mute");
      bool is_mute = false;
      if (cJSON_IsBool(muteItem))
      {
        is_mute = cJSON_IsTrue(muteItem);
      }
      xiaozhi_audio_mute_set(is_mute);
    }
  }
}

// 处理二进制音频数据帧的方法
void xiaozhi_process_binary_audio_frame(char *bin_data, int data_len)
{
  // 处理二进制音频数据帧存入ws到encoder的环形缓冲区中
  xRingbufferSend(xiaozhi_handle.ring_buffer_ws_to_encoder_handle, bin_data, data_len, portMAX_DELAY);
}

// 创建一个任务 用于处理小智的音频数据发送至服务器
void xiaozhi_send_audio_task(void *pvParameters)
{
  while (1)
  {
    size_t data_size = 0;
    // 从ws到encoder的环形缓冲区中获取数据
    void *rev_pointer = xRingbufferReceive(xiaozhi_handle.ring_buffer_encoder_to_ws_handle, &data_size, portMAX_DELAY);
    //! 一定要保证小智在听我们说话
    if (xiaozhi_handle.xiaozhi_state == XIAOZHI_STATE_LISTENING)
    {
      // 发送数据到服务器
      xiaozhi_ws_send_bin(rev_pointer, data_size);
    }
    // 释放环形缓冲区中的数据
    vRingbufferReturnItem(xiaozhi_handle.ring_buffer_encoder_to_ws_handle, rev_pointer);
  }
}
