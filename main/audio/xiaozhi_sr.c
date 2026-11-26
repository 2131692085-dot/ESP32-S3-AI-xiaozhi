#include "xiaozhi_sr.h"

void xiaozhi_sr_feed_task(void *arg);
void xiaozhi_sr_fetch_task(void *arg);

esp_afe_sr_data_t *afe_data = NULL;
static esp_afe_sr_iface_t *afe_handle = NULL;

#define TAG "xiaozhi_sr"

void xiaozhi_sr_init(void)
{
    srmodel_list_t *models = esp_srmodel_init("model");
    afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_config->vad_min_noise_ms = 1000; // 判断静默的最小延时时间
    afe_config->vad_min_speech_ms = 128; // 判断是否是人声的 最小延时时间
    afe_config->vad_mode = VAD_MODE_1;   // 判断语音的模式
    //! 解决ringbuff is full的问题
    afe_config->afe_ringbuf_size = 6 * 1024;

    // 关闭一些硬件上本来就不支持的功能 免得出现反效果
    afe_config->aec_init = false; // 禁用回声消除
    afe_config->se_init = false;  // 禁用人声增强
    afe_config->ns_init = false;  // 禁用噪声消除

    // 提升了唤醒的灵敏度
    afe_config->wakenet_mode = DET_MODE_90;
    // 更多的内存分配到外部的sram里去
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    afe_handle = esp_afe_handle_from_config(afe_config);
    afe_data = afe_handle->create_from_config(afe_config);

    xTaskCreatePinnedToCoreWithCaps(&xiaozhi_sr_feed_task, "xiaozhi_sr_feed_task", 32 * 1024, NULL, 5, NULL, 1, MALLOC_CAP_SPIRAM);
    xTaskCreatePinnedToCoreWithCaps(&xiaozhi_sr_fetch_task, "xiaozhi_sr_fetch_task", 32 * 1024, NULL, 5, NULL, 1, MALLOC_CAP_SPIRAM);
}

// feed 数据的任务
void xiaozhi_sr_feed_task(void *arg)
{
    // 获取音频数据的chunk块大小
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    // 获取音频数据的通道数
    int feed_channel = afe_handle->get_feed_channel_num(afe_data);
    // 分配内存
    int16_t *i2s_buff = heap_caps_malloc(audio_chunksize * sizeof(int16_t) * feed_channel, MALLOC_CAP_SPIRAM);
    while (1)
    {
        // 做假数据
        // esp_get_feed_data(true, i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);
        // 问 ES8311 读取音频数据
        xiaozhi_audio_record(i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);
        afe_handle->feed(afe_data, i2s_buff);
    }
}

// fetch 数据的任务
void xiaozhi_sr_fetch_task(void *arg)
{

    while (1)
    {
        // 通过fetch提取处理音频和语音活动检测的结果
        afe_fetch_result_t *result = afe_handle->fetch(afe_data);
        // 从SR的模型中获取处理后的音频数据
        int16_t *processed_audio = result->data;
        // 打印提取数据
        // ESP_LOGI(TAG, "fetch_data size: %d", result->data_size);
        // vad的状态：0-静默 1-人声
        vad_state_t vad_state = result->vad_state;
        // wakeup的状态
        wakenet_state_t wakeup_state = result->wakeup_state;

        if (wakeup_state == WAKENET_DETECTED)
        {
            // 唤醒状态
            ESP_LOGI(TAG, "检测到了唤醒词");
            // 标记为唤醒状态
            xiaozhi_handle.is_wakeup = true;
            // 初始化vad状态为静默
            xiaozhi_handle.vad_current_state = VAD_SILENCE;
            xiaozhi_handle.vad_previous_state = VAD_SILENCE;

            if (xiaozhi_handle.wakeup_callback != NULL)
            {
                xiaozhi_handle.wakeup_callback();
            }
        }
        // 调用被唤醒的回调
        if (xiaozhi_handle.is_wakeup)
        {
            xiaozhi_handle.vad_current_state = vad_state;
            if (xiaozhi_handle.vad_current_state != xiaozhi_handle.vad_previous_state)
            {

                if (xiaozhi_handle.vad_change_callback != NULL)
                {
                    xiaozhi_handle.vad_change_callback();
                }
            }
            // 更新上一个状态
            xiaozhi_handle.vad_previous_state = xiaozhi_handle.vad_current_state;
        }

        // 什么时候将数据放入环形缓冲区
        // 1.检测到被唤醒了
        // 2.检测到人声了
        if (xiaozhi_handle.is_wakeup && xiaozhi_handle.vad_current_state == VAD_SPEECH)
        {
            // if vad cache is exists, please attach the cache to the front of processed_audio to avoid data loss
            if (result->vad_cache_size > 0)
            {
                // 将缓存数据放入环形缓冲区
                xRingbufferSend(xiaozhi_handle.ring_buffer_sr_to_encoder_handle, result->vad_cache, result->vad_cache_size, portMAX_DELAY);
            }
            // 将数据放入环形缓冲区
            xRingbufferSend(xiaozhi_handle.ring_buffer_sr_to_encoder_handle, processed_audio, result->data_size, portMAX_DELAY);
        }
    }
}
