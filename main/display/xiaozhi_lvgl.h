#pragma once
#include "esp_log.h"
#include "xiaozhi_lcd.h"
#include "esp_lvgl_port.h"
#include "font_emoji.h"

void xiaozhi_lvgl_init();

void xiaozhi_lvgl_show_hello_world(void);

void xiaozhi_lvgl_show_qrcode(char *qrcode_data);

void xiaozhi_lvgl_del_qrcode(void);

void xiaozhi_lvgl_set_title(char *title);

void xiaozhi_lvgl_set_emoji(char *emoji);

void xiaozhi_lvgl_set_text(char *text);
