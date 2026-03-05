#include "display_manager.h"
#include "bsp/esp32_s3_eye.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "display_mgr";

/* ── Estado interno ─────────────────────────────────────────────────────── */
static lv_obj_t *s_label = NULL;
static lv_style_t s_text_style;
static bool s_initialized = false;

/* ── Fuente y color por defecto ─────────────────────────────────────────── */
#define DEFAULT_FONT (&lv_font_montserrat_14)
#define DEFAULT_COLOR lv_color_white()
#define DISPLAY_W 240 // resolución ESP32-S3-EYE
#define LABEL_W 220   // ancho del label (margen 10px a cada lado)

/* ── Helpers internos ───────────────────────────────────────────────────── */

static void _apply_style(lv_color_t color, const lv_font_t *font)
{
    lv_style_reset(&s_text_style);
    lv_style_init(&s_text_style);
    lv_style_set_text_color(&s_text_style, color);
    lv_style_set_text_font(&s_text_style, font);
    lv_style_set_text_align(&s_text_style, LV_TEXT_ALIGN_CENTER);
    lv_obj_remove_style_all(s_label);
    lv_obj_add_style(s_label, &s_text_style, 0);
}

static void _set_text_and_center(const char *text)
{
    lv_label_set_text(s_label, text);
    lv_obj_align(s_label, LV_ALIGN_CENTER, 0, 0);
}

/* ── API pública ────────────────────────────────────────────────────────── */

esp_err_t display_init(void)
{
    if (s_initialized)
    {
        ESP_LOGW(TAG, "display_init llamado más de una vez, ignorando");
        return ESP_OK;
    }

    lv_display_t *disp = bsp_display_start();
    if (disp == NULL)
    {
        ESP_LOGE(TAG, "Error iniciando BSP display: puntero nulo");
        return ESP_FAIL;
    }

    bsp_display_brightness_set(80);

    bsp_display_lock(0);

    /* Fondo negro */
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);

    /* Label central único, reutilizado en cada llamada */
    s_label = lv_label_create(screen);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label, LABEL_W);

    /* Estilo inicial */
    lv_style_init(&s_text_style);
    _apply_style(DEFAULT_COLOR, DEFAULT_FONT);

    lv_obj_align(s_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(s_label, "");

    bsp_display_unlock();

    s_initialized = true;
    ESP_LOGI(TAG, "Display listo (%dx%d)", DISPLAY_W, DISPLAY_W);
    return ESP_OK;
}

void display_text(const char *text)
{
    if (!s_initialized)
    {
        ESP_LOGW(TAG, "Llamada antes de display_init");
        return;
    }

    bsp_display_lock(0);
    _apply_style(DEFAULT_COLOR, DEFAULT_FONT); // restablecer estilo por defecto
    _set_text_and_center(text);
    bsp_display_unlock();
}

void display_textf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    display_text(buf);
}

void display_text_styled(const char *text,
                         lv_color_t color,
                         const lv_font_t *font)
{
    if (!s_initialized)
    {
        ESP_LOGW(TAG, "Llamada antes de display_init");
        return;
    }

    bsp_display_lock(0);
    _apply_style(color, font != NULL ? font : DEFAULT_FONT);
    _set_text_and_center(text);
    bsp_display_unlock();
}

void display_set_brightness(uint8_t percent)
{
    if (percent > 100)
        percent = 100;
    bsp_display_brightness_set(percent);
}

void display_clear(void)
{
    if (!s_initialized)
        return;

    bsp_display_lock(0);
    lv_label_set_text(s_label, "");
    bsp_display_unlock();
}
