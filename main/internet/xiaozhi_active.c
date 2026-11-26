#include "xiaozhi_active.h"

#define TAG "xiaozhi_active"

esp_http_client_handle_t client;

esp_err_t _http_event_handler(esp_http_client_event_t *evt);

char mac_str[18];
char uuid_str[37];

static char *output_buffer;   // Buffer to store response of http request from event handler
static int output_len;        // Stores number of bytes read
static int current_index = 0; // Current index in the output_buffer

void generate_uuid_v4(char *uuid_str);

// 设置请求头
void xiaozhi_active_set_header(void);

// 设置请求体
void xiaozhi_active_set_body(void);

// 提取想要的数据
void xiaozhi_active_parse_json(char *json_data);

void xiaozhi_active_init(void)
{
    esp_http_client_config_t config = {
        .host = "api.tenclass.net",
        .path = "/xiaozhi/ota/",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        //.cert_pem = howsmyssl_com_root_cert_pem_start,
        .crt_bundle_attach = esp_crt_bundle_attach, // SSL证书验证
        .method = HTTP_METHOD_POST,                 // POST方法
    };
    // 客户端的初始化
    client = esp_http_client_init(&config);

    // 设置请求头
    xiaozhi_active_set_header();

    // 设置请求体
    xiaozhi_active_set_body();

    // 发起请求，打开链接，交换数据，关闭连接
    while (esp_http_client_perform(client) != ESP_OK)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        xiaozhi_lvgl_set_title("网络较差...");
        xiaozhi_lvgl_set_emoji("thinking");
        xiaozhi_lvgl_set_text("正在积极连接中，请稍后...");
    }
}

void xiaozhi_active_set_header(void)
{
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "User-Agent", "bread-compact-wifi-128x64/1.0.1");
    esp_http_client_set_header(client, "Activation-Version", "1");
    uint8_t eth_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);

    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             eth_mac[0], eth_mac[1], eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]);
    esp_http_client_set_header(client, "Device-Id", mac_str);
    ESP_LOGI(TAG, "Device-Id: %s", mac_str);

    generate_uuid_v4(uuid_str);
    esp_http_client_set_header(client, "Client-Id", uuid_str);
    ESP_LOGI(TAG, "Client-Id: %s", uuid_str);
}

void xiaozhi_active_set_body(void)
{
    // {
    //     "application" : {
    //         "version" : "1.0.1",
    //         "elf_sha256" : "c8a8ecb6d6fbcda682494d9675cd1ead240ecf38bdde75282a42365a0e396033"
    //     },
    //                     "board" : {
    //                         "type" : "bread-compact-wifi",
    //                         "name" : "bread-compact-wifi-128x64",
    //                         "ssid" : "卧室",
    //                         "rssi" : -55,
    //                         "channel" : 1,
    //                         "ip" : "192.168.1.11",
    //                         "mac" : "11:22:33:44:55:66"
    //                     }
    // }
    cJSON *root = cJSON_CreateObject();
    cJSON *application = cJSON_CreateObject();
    cJSON *board = cJSON_CreateObject();

    // 将 application 和 board 添加到 root 中
    cJSON_AddItemToObject(root, "application", application);
    cJSON_AddItemToObject(root, "board", board);

    // 设置 application 字段
    cJSON_AddStringToObject(application, "version", "1.0.1");
    cJSON_AddStringToObject(application, "elf_sha256", esp_app_get_elf_sha256_str());

    // 设置 board 字段
    cJSON_AddStringToObject(board, "type", "bread-compact-wifi");
    cJSON_AddStringToObject(board, "name", "bread-compact-wifi-128x64");
    cJSON_AddStringToObject(board, "ssid", "卧室");
    cJSON_AddNumberToObject(board, "rssi", -55);
    cJSON_AddNumberToObject(board, "channel", 1);
    cJSON_AddStringToObject(board, "ip", "192.168.1.11");
    cJSON_AddStringToObject(board, "mac", mac_str);

    // 将 JSON 对象转换为字符串
    char *post_data = cJSON_PrintUnformatted(root);

    // 设置请求体
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // 释放 JSON 对象和字符串
    cJSON_Delete(root);
}

void generate_uuid_v4(char *uuid_str)

{
    // UUID 格式模板: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // 其中 x 是随机十六进制数，y 是 8, 9, a, 或 b
    const char *hex_chars = "0123456789abcdef";

    // 初始化随机数种子（实际项目中建议用硬件随机数或定时器种子）
    srand((unsigned int)time(NULL));

    for (int i = 0; i < 36; i++)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            uuid_str[i] = '-'; // 插入连字符
        }
        else if (i == 14)
        {
            uuid_str[i] = '4'; // 版本号固定为 4
        }
        else if (i == 19)
        {
            // 变体位固定为 10xx，即 8, 9, a, b
            const char *variant_chars = "89ab";
            uuid_str[i] = variant_chars[rand() % 4];
        }
        else
        {
            uuid_str[i] = hex_chars[rand() % 16]; // 其他位置随机填充
        }
    }
    uuid_str[36] = '\0'; // 字符串结束符
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        if (strcmp(evt->header_key, "Content-Length") == 0)
        {
            output_len = atoi(evt->header_value);
            output_buffer = heap_caps_malloc(output_len + 1, MALLOC_CAP_SPIRAM);
            output_buffer[output_len] = '\0';
        }
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        //! 收集服务器返回的数据
        memcpy(output_buffer + current_index, evt->data, evt->data_len);
        current_index += evt->data_len;
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        ESP_LOGI(TAG, "output_buffer: %s", output_buffer);
        // 提取想要的数据
        xiaozhi_active_parse_json(output_buffer);
        if (output_buffer != NULL)
        {
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL)
        {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

void xiaozhi_active_parse_json(char *json_data)
{
    cJSON *root = cJSON_Parse(json_data);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (websocket == NULL)
    {
        ESP_LOGE(TAG, "Failed to get websocket from JSON");
        cJSON_Delete(root);
        return;
    }

    char *websocket_url = cJSON_GetObjectItem(websocket, "url")->valuestring;
    xiaozhi_handle.websocket_url = heap_caps_calloc(strlen(websocket_url) + 1, sizeof(char), MALLOC_CAP_SPIRAM);
    memcpy(xiaozhi_handle.websocket_url, websocket_url, strlen(websocket_url));

    char *websocket_token = cJSON_GetObjectItem(websocket, "token")->valuestring;
    xiaozhi_handle.websocket_token = heap_caps_calloc(strlen(websocket_token) + 1, sizeof(char), MALLOC_CAP_SPIRAM);
    memcpy(xiaozhi_handle.websocket_token, websocket_token, strlen(websocket_token));

    // 获取激活码
    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (activation == NULL)
    {
        ESP_LOGE(TAG, "设备已激活");
        xiaozhi_handle.is_activated = true;
        xiaozhi_lvgl_set_title("AI小智已激活");
        xiaozhi_lvgl_set_emoji("angry");
        xiaozhi_lvgl_set_text("你好,AI小智已激活,欢迎使用");
    }
    else
    {
        ESP_LOGI(TAG, "设备未激活");
        xiaozhi_handle.is_activated = false;
        char *activation_code = cJSON_GetObjectItem(activation, "code")->valuestring;
        xiaozhi_handle.activation_code = heap_caps_calloc(strlen(activation_code) + 1, sizeof(char), MALLOC_CAP_SPIRAM);
        memcpy(xiaozhi_handle.activation_code, activation_code, strlen(activation_code));
        ESP_LOGI(TAG, "activation_code: %s", xiaozhi_handle.activation_code);

        xiaozhi_lvgl_set_title("设备未激活");
        xiaozhi_lvgl_set_emoji("sad");
        char text_arr[128] = {0};
        sprintf(text_arr, "请前往控制台激活 \n 激活码：%s \n 激活后请重启", xiaozhi_handle.activation_code);
        xiaozhi_lvgl_set_text(text_arr);
    }
    ESP_LOGI(TAG, "websocket_url: %s", xiaozhi_handle.websocket_url);
    ESP_LOGI(TAG, "websocket_token: %s", xiaozhi_handle.websocket_token);

    cJSON_Delete(root);
}
