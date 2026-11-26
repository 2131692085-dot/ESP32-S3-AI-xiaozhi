#include "xiaozhi_ws.h"

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
esp_websocket_client_handle_t ws_client;

extern char mac_str[18];
extern char uuid_str[37];

#define TAG "xiaozhi_ws"

static char *iot_desc_json_str =
#include "iot_desc.json"
    ;

static char *iot_state_json_str =
#include "iot_init_status.json"
    ;

void xiaozhi_ws_init(void)
{
    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = xiaozhi_handle.websocket_url;
    // 设置请求方式是wss
    websocket_cfg.transport = WEBSOCKET_TRANSPORT_OVER_SSL;
    // 证书验证
    websocket_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    websocket_cfg.reconnect_timeout_ms = 5000;
    websocket_cfg.network_timeout_ms = 5000;
    // 初始化websocket客户端
    ws_client = esp_websocket_client_init(&websocket_cfg);
    // 设置请求头
    size_t authorization_len = strlen("Bearer ") + strlen(xiaozhi_handle.websocket_token) + 1;
    char *authorization_str = heap_caps_malloc(authorization_len, MALLOC_CAP_SPIRAM);
    sprintf(authorization_str, "Bearer %s", xiaozhi_handle.websocket_token);
    esp_websocket_client_append_header(ws_client, "Authorization", authorization_str);
    esp_websocket_client_append_header(ws_client, "Protocol-Version", "1");
    esp_websocket_client_append_header(ws_client, "Device-Id", mac_str);
    esp_websocket_client_append_header(ws_client, "Client-Id", uuid_str);

    // 注册事件处理函数
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    // // 发请求，建立连接
    // esp_websocket_client_start(ws_client);
    // vTaskDelay(5000 / portTICK_PERIOD_MS);
    // // 发送hello消息
    // xiaozhi_ws_send_hello();
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_BEGIN:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        //! 修改成功的标志位为1
        xEventGroupSetBits(xiaozhi_handle.ws_event_group, WS_CONNECTED_BIT);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        // 还原小智所有的状态
        xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_IDLE;
        xiaozhi_handle.is_wakeup = false;
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(TAG, "Received opcode=%d", data->op_code);
        // 处理文本帧
        if (data->op_code == WS_TRANSPORT_OPCODES_TEXT)
        {
            ESP_LOGI(TAG, "Received text: %s", data->data_ptr);
            if (xiaozhi_handle.process_text_frame != NULL)
            {
                xiaozhi_handle.process_text_frame(data->data_ptr);
            }
        }
        // 处理音频帧
        if (data->op_code == WS_TRANSPORT_OPCODES_BINARY)
        {
            ESP_LOGI(TAG, "Received binary: %d", data->data_len);
            if (xiaozhi_handle.process_binary_audio_frame != NULL)
            {
                xiaozhi_handle.process_binary_audio_frame(data->data_ptr, data->data_len);
            }
        }
        // 判断通道被服务器端自动断开
        if (data->op_code == WS_TRANSPORT_OPCODES_CLOSE)
        {
            // 还原小智所有的状态
            xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_IDLE;
            xiaozhi_handle.is_wakeup = false;
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        // 还原小智所有的状态
        xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_IDLE;
        xiaozhi_handle.is_wakeup = false;
        break;
    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
        break;
    }
}

// 封装发送文本帧的方法
void xiaozhi_ws_send_text(char *data, size_t len)
{
    esp_websocket_client_send_text(ws_client, data, len, portMAX_DELAY);
}

/* 封装发送二进制数据帧的方法 */
void xiaozhi_ws_send_bin(char *data, size_t len)
{
    esp_websocket_client_send_bin(ws_client, data, len, portMAX_DELAY);
}
// 封装发送hello消息
void xiaozhi_ws_send_hello(void)
{
    char *hello_str = "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\",\"features\":{\"mcp\":true},\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}";
    xiaozhi_ws_send_text(hello_str, strlen(hello_str));
}

// 封装发送唤醒词消息
void xiaozhi_ws_send_wakeup(void)
{
    char *wakeup_str = "{\"type\":\"listen\",\"state\":\"detect\",\"text\":\"小特小特\"}";
    xiaozhi_ws_send_text(wakeup_str, strlen(wakeup_str));
}

// 封装将建立连接的方法异步转同步
bool xiaozhi_ws_connect_sync(void)
{
    // 判断是否已经初始化了客户端
    if (esp_websocket_client_is_connected(ws_client) == true)
    {
        return true;
    }
    // 发请求，建立连接
    esp_websocket_client_start(ws_client);
    // 以阻塞的形式等待连接成功
    EventBits_t res = xEventGroupWaitBits(xiaozhi_handle.ws_event_group, WS_CONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    // 判断是否超时
    if (res & WS_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "WS_CONNECTED_BIT");
        return true;
    }
    else
    {
        ESP_LOGI(TAG, "WS_CONNECTED_BIT timeout");
        return false;
    }
}

// 发送开始监听的功能函数
void xiaozhi_ws_send_start_listen(void)
{
    char *start_listen_str = "{\"type\":\"listen\",\"state\":\"start\",\"mode\":\"manual\"}";
    xiaozhi_ws_send_text(start_listen_str, strlen(start_listen_str));
}

// 发送停止监听的功能函数
void xiaozhi_ws_send_stop_listen(void)
{
    char *stop_listen_str = "{\"type\":\"listen\",\"state\":\"stop\"}";
    xiaozhi_ws_send_text(stop_listen_str, strlen(stop_listen_str));
}

// 封装发送终止消息
void xiaozhi_ws_send_abort(void)
{
    char *abort_str = "{\"type\":\"abort\"}";
    xiaozhi_ws_send_text(abort_str, strlen(abort_str));
}

void xiaozhi_ws_send_iot_desc(void)
{
  xiaozhi_ws_send_text(iot_desc_json_str, strlen(iot_desc_json_str));
}

/* 发送远程控制初始化状态 */
void xiaozhi_ws_send_iot_state(void)
{
  xiaozhi_ws_send_text(iot_state_json_str, strlen(iot_state_json_str));
}