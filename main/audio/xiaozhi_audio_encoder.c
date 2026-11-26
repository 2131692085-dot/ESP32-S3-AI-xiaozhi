#include "xiaozhi_audio_encoder.h"

#define TAG "xiaozhi_audio_encoder"

void xiaozhi_audio_encoder_task(void *pvParameters);

int input_pcm_size = 0, output_opus_size = 0;
uint8_t *input_pcm_data = NULL;
uint8_t *output_opus_data = NULL;
esp_audio_enc_handle_t encoder_handle = NULL;

void xiaozhi_audio_encoder_init(void)
{
    esp_opus_enc_config_t opus_cfg = {
        .sample_rate = ESP_AUDIO_SAMPLE_RATE_16K,
        .channel = ESP_AUDIO_MONO,
        .bits_per_sample = ESP_AUDIO_BIT16,
        .bitrate = 90000, // 比特率的快慢决定音频的质量，占用CPU越多
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS,
        .application_mode = ESP_OPUS_ENC_APPLICATION_AUDIO,
        .complexity = 6, // 复杂度，0-10，0表示最低，10表示最高 质量越高
        .enable_fec = false,
        .enable_dtx = false,
        .enable_vbr = false,
    };

    esp_opus_enc_open(&opus_cfg, sizeof(esp_opus_enc_config_t), &encoder_handle);

    esp_opus_enc_get_frame_size(encoder_handle, &input_pcm_size, &output_opus_size);
    ESP_LOGI(TAG, "input_pcm_size: %d, output_opus_size: %d", input_pcm_size, output_opus_size);

    // 分配内存
    input_pcm_data = heap_caps_malloc(input_pcm_size, MALLOC_CAP_SPIRAM);
    output_opus_data = heap_caps_malloc(output_opus_size, MALLOC_CAP_SPIRAM);

    //注册编码器的任务
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_audio_encoder_task, "xiaozhi_audio_encoder_task", 32
        *1024, NULL, 5, NULL, 1,MALLOC_CAP_SPIRAM);
}

// 编码器的任务
void xiaozhi_audio_encoder_task(void *pvParameters)
{
    esp_audio_enc_in_frame_t in_frame = {
        .buffer = input_pcm_data,
        .len = input_pcm_size,
    };
    esp_audio_enc_out_frame_t out_frame = {
        .buffer = output_opus_data,
        .len = output_opus_size,
    };
    while (1)
    {
        // 从环形缓冲区获取数据
        // 环形缓冲区的情况
        // 1. 0
        // 2. >=1920
        // 3. <1920&&>0

        // 放行情况：1.超时 2.取到了1920 3.取到了数据，但是小于1920
        size_t remain_size = in_frame.len;

        uint8_t *temp_pointer = in_frame.buffer;
        while (remain_size > 0)
        {
            size_t received_size = 0;
            void *rev_pointer= xRingbufferReceiveUpTo(xiaozhi_handle.ring_buffer_sr_to_encoder_handle, &received_size, portMAX_DELAY, remain_size);
            memcpy(temp_pointer, rev_pointer, received_size);
            temp_pointer += received_size;
            remain_size -= received_size;

            //释放数据
            vRingbufferReturnItem(xiaozhi_handle.ring_buffer_sr_to_encoder_handle, rev_pointer);
        }
        //开始编码
        esp_opus_enc_process(encoder_handle, &in_frame, &out_frame);

        //将编码后的数据写入环形缓冲区
        xRingbufferSend(xiaozhi_handle.ring_buffer_encoder_to_ws_handle, out_frame.buffer, out_frame.encoded_bytes, portMAX_DELAY);
    }
}
 