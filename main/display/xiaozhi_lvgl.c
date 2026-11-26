#include "xiaozhi_lvgl.h"

#define TAG "xiaozhi_lvgl"

extern esp_lcd_panel_handle_t panel_handle;
extern esp_lcd_panel_io_handle_t io_handle;

extern const lv_font_t font_puhui_14_1;
extern const lv_font_t font_puhui_16_4;
extern const lv_font_t font_puhui_20_4;

#define QR_DARK lv_color_hex(0x1d4ed8)
#define QR_LIGHT lv_color_hex(0xdbeafe)

#define LCD_H_RES (320)
#define LCD_V_RES (240)
#define LCD_DRAW_BUFF_HEIGHT 16

static lv_display_t *lvgl_disp = NULL;
lv_obj_t *qrcode = NULL;
lv_obj_t *screen = NULL;
lv_obj_t *title_label = NULL;
lv_obj_t *emoji_label = NULL;
lv_obj_t *text_label = NULL;

typedef struct
{
    char *name;
    char *emoji;
} emoji_list_t;

const emoji_list_t emoji_list[21] = {
    {.name = "neutral", .emoji = "😶"},
    {.name = "happy", .emoji = "🙂"},
    {.name = "laughing", .emoji = "😆"},
    {.name = "funny", .emoji = "😂"},
    {.name = "sad", .emoji = "😔"},
    {.name = "angry", .emoji = "😠"},
    {.name = "crying", .emoji = "😭"},
    {.name = "loving", .emoji = "😍"},
    {.name = "embarrassed", .emoji = "😳"},
    {.name = "surprised", .emoji = "😲"},
    {.name = "shocked", .emoji = "😱"},
    {.name = "thinking", .emoji = "🤔"},
    {.name = "winking", .emoji = "😉"},
    {.name = "cool", .emoji = "😎"},
    {.name = "relaxed", .emoji = "😌"},
    {.name = "delicious", .emoji = "🤤"},
    {.name = "kissy", .emoji = "😘"},
    {.name = "confident", .emoji = "😏"},
    {.name = "sleepy", .emoji = "😴"},
    {.name = "silly", .emoji = "😜"},
    {.name = "confused", .emoji = "🙄"}};

void xiaozhi_lvgl_show_init(void);

void xiaozhi_lvgl_init()
{
    // LCD初始化
    xiaozhi_lcd_init();
    // LVGL初始化
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,       /* LVGL task priority */
        .task_stack = 8192,       /* LVGL task stack size */
        .task_affinity = -1,      /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500, /* Maximum sleep in LVGL task */
        .timer_period_ms = 5      /* LVGL timer tick period in ms */
    };
    lvgl_port_init(&lvgl_cfg);

    ESP_LOGI(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT * 2,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        }};

    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    // 全局的默认样式
    // 上锁
    lvgl_port_lock(0);
    screen = lv_screen_active();
    // 设置屏幕背景颜色
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x003a57), 0);
    // 设置屏幕文本颜色
    lv_obj_set_style_text_color(screen, lv_color_hex(0xffffff), 0);

    // 解锁屏幕对象
    lvgl_port_unlock();

    xiaozhi_lvgl_show_init();
}

// 显示hello world
void xiaozhi_lvgl_show_hello_world(void)
{
    //! lvgl操作屏幕对象是非原子性的
    lvgl_port_lock(0);
    // 获取屏幕对象
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x003a57), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xffffff), 0);
    // 创建一个标签对象并设置文本
    lv_obj_t *label = lv_label_create(screen);
    // 设置标签对象的对齐方式为居中
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_label_set_text(label, "Hello world");

    // 解锁屏幕对象
    lvgl_port_unlock();
}

// 显示二维码
void xiaozhi_lvgl_show_qrcode(char *qrcode_data)
{
    lvgl_port_lock(0);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(screen, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_cross_place(screen, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_track_place(screen, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_row(screen, 16, 0);
    /* 💡 Swap dark_color/light_color for brand colors; keep enough contrast or scanners fail. */
    qrcode = lv_qrcode_create(screen);
    lv_qrcode_set_size(qrcode, 150);
    lv_qrcode_set_dark_color(qrcode, QR_DARK);
    lv_qrcode_set_light_color(qrcode, QR_LIGHT);
    // 设置二维码数据
    lv_qrcode_set_data(qrcode, qrcode_data);
    // 设置二维码边框
    lv_qrcode_set_quiet_zone(qrcode, true);
    // 设置二维码边框颜色
    lv_obj_set_style_border_color(qrcode, QR_DARK, 0);
    // 设置二维码边框宽度
    lv_obj_set_style_border_width(qrcode, 4, 0);
    lvgl_port_unlock();
}

// 删除二维码
void xiaozhi_lvgl_del_qrcode(void)
{
    if (qrcode == NULL)
    {
        return;
    }
    lvgl_port_lock(0);
    lv_obj_delete(qrcode);
    lvgl_port_unlock();
}

// 默认显示界面
void xiaozhi_lvgl_show_init(void)
{
    lvgl_port_lock(0);
    //! 标题区域
    // 创建标题区域容器
    lv_obj_t *title_container = lv_obj_create(screen);
    // 设置圆角
    lv_obj_set_style_radius(title_container, 0, LV_PART_MAIN);
    // 设置滚动条
    lv_obj_set_scrollbar_mode(title_container, LV_SCROLLBAR_MODE_OFF);
    // 设置边框
    lv_obj_set_style_border_width(title_container, 0, LV_PART_MAIN);
    // 设置大小
    lv_obj_set_size(title_container, LCD_H_RES, 30);
    // 设置背景颜色
    lv_obj_set_style_bg_color(title_container, lv_color_hex(0x003a57), 0);
    // 创建label
    title_label = lv_label_create(title_container);
    // 设置字体
    lv_obj_set_style_text_font(title_label, &font_puhui_16_4, 0);
    // 设置文本
    lv_label_set_text(title_label, "AI小智");
    // 设置对齐方式为居中
    lv_obj_set_align(title_label, LV_ALIGN_CENTER);

    //! emoji区域
    // 创建label
    emoji_label = lv_label_create(screen);
    // 设置宽度
    lv_obj_set_width(emoji_label, 320);
    // 设置文本居中
    lv_obj_set_style_text_align(emoji_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(emoji_label, font_emoji_64_init(), 0);
    // 设置文本
    lv_label_set_text(emoji_label, "😠");
    // 设置相对对齐方式为居中
    lv_obj_align_to(emoji_label, title_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 30);

    // ! 文本区域
    // 创建label标签
    text_label = lv_label_create(screen);
    // 设置宽度
    lv_obj_set_width(text_label, 280);
    // 设置文本居中
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);
    // 设置字体
    lv_obj_set_style_text_font(text_label, &font_puhui_14_1, 0);
    lv_label_set_text(text_label, "你好，我是小智");
    // 设置相对对齐方式
    lv_obj_align_to(text_label, emoji_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 30);
    // 解锁
    lvgl_port_unlock();
}

// 修改标题的方法
void xiaozhi_lvgl_set_title(char *title)
{
    lvgl_port_lock(0);
    lv_label_set_text(title_label, title);
    lvgl_port_unlock();
}

// 修改文本的方法
void xiaozhi_lvgl_set_text(char *text)
{
    lvgl_port_lock(0);
    lv_label_set_text(text_label, text);
    lvgl_port_unlock();
}

// 修改emoji的方法
void xiaozhi_lvgl_set_emoji(char *emoji_name)
{
    bool is_find = false;
    lvgl_port_lock(0);
    for (int i = 0; i < 21; i++)
    {
        if (strcmp(emoji_list[i].name, emoji_name) == 0)
        {
            lv_label_set_text(emoji_label, emoji_list[i].emoji);
            break;
        }
    }

    if (!is_find)
    {
        lv_label_set_text(emoji_label, "😠");
    }
    lvgl_port_unlock();
}
