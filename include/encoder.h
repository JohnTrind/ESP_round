#pragma once
#include <Arduino.h>

// Encoder rotativo + botao do knob.
// O encoder NAO esta registado como lv_indev: a navegacao e tratada
// directamente pela app (ver ui.cpp). Evita o modo edit/group do LVGL,
// que neste tipo de UI redonda da mais trabalho do que ajuda.
namespace knob {

void begin();

// Passos de detente desde a ultima chamada (negativo = anti-horario).
int32_t takeDelta();

// true uma unica vez por clique (ja debounced).
bool takeClick();

// true se o botao esta premido neste momento.
bool pressed();

}  // namespace knob
