#pragma once

#include "esp_log.h"
#include "esp_websocket_client.h"
#include "xiaozhi_common.h"
#include "string.h"
#include "esp_heap_caps.h"
#include "esp_crt_bundle.h"



void xiaozhi_ws_init(void);

void xiaozhi_ws_send_hello(void);

void xiaozhi_ws_send_wakeup(void);

void xiaozhi_ws_send_bin(char *data, size_t len);

bool xiaozhi_ws_connect_sync(void);

//发送开始监听的功能函数
void xiaozhi_ws_send_start_listen(void);

//发送停止监听的功能函数
void xiaozhi_ws_send_stop_listen(void);

//封装发送终止消息
void xiaozhi_ws_send_abort(void);

// 发送iot描述
void xiaozhi_ws_send_iot_desc(void);

// 发送远程控制初始化状态
void xiaozhi_ws_send_iot_state(void);


