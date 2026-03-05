#pragma once

#include "esp_err.h"
#include "lvgl.h" // expuesto por si el usuario quiere colores/fuentes de LVGL

#ifdef __cplusplus
extern "C"
{
#endif

    /* ─── Inicialización ──────────────────────────────────────────────────────── */

    /**
     * @brief Inicializa el display. Llamar UNA sola vez en app_main antes de usar
     *        cualquier otra función.
     * @return ESP_OK si todo fue bien.
     */
    esp_err_t display_init(void);

    /* ─── Texto básico ────────────────────────────────────────────────────────── */

    /**
     * @brief Muestra texto centrado. Soporta \n para saltos de línea.
     *        Thread-safe: puede llamarse desde cualquier tarea FreeRTOS.
     */
    void display_text(const char *text);

    /**
     * @brief Igual que display_text pero con formato printf.
     *        Ej: display_textf("Ciclo: %d | Temp: %.1f", n, t);
     */
    void display_textf(const char *fmt, ...);

    /* ─── Texto con estilo ────────────────────────────────────────────────────── */

    /**
     * @brief Texto centrado con color y fuente personalizados.
     *        Las fuentes deben estar habilitadas en menuconfig (Montserrat).
     *
     *        Ej: display_text_styled("ERROR", lv_color_make(255,60,60),
     *                                &lv_font_montserrat_28);
     */
    void display_text_styled(const char *text,
                             lv_color_t color,
                             const lv_font_t *font);

    /* ─── Control del display ─────────────────────────────────────────────────── */

    /**
     * @brief Ajusta el brillo de la pantalla.
     * @param percent  0 (apagado) – 100 (máximo brillo)
     */
    void display_set_brightness(uint8_t percent);

    /**
     * @brief Limpia la pantalla (fondo negro, sin texto).
     */
    void display_clear(void);

#ifdef __cplusplus
}
#endif