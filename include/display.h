#pragma once
#include <stdint.h>

// Inicializa GC9A01 + backlight + touch CST816D e registra tudo no LVGL.
void displayBegin();

// Backlight, 0..255 (PWM). Guardado para a UI poder ler o valor actual.
void backlightSet(uint8_t level);
uint8_t backlightGet();
