// CrowPanel 1.28" HMI ESP32 Rotary Display - scaffold Arduino + LVGL
//
// Menu rotativo: rodar o knob escolhe, clicar entra, clicar outra vez volta.
// Demos: Dial (arco + valor), Anel RGB (hue -> WS2812), Touch (coordenadas),
// Brilho (backlight por PWM).

#include <Arduino.h>
#include <lvgl.h>

#include "display.h"
#include "encoder.h"
#include "ui.h"

void setup() {
  Serial.begin(115200);

  displayBegin();
  knob::begin();
  uiBegin();

  Serial.printf("pronto | PSRAM livre: %u bytes | heap livre: %u bytes\n",
                (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());
}

void loop() {
  uiTick();
  lv_timer_handler();
  delay(5);
}
