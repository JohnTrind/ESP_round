#include "display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <CST816S.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "pins.h"

namespace {

// --- GC9A01 por SPI -------------------------------------------------------
Arduino_DataBus* g_bus = new Arduino_ESP32SPI(
    TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO);

// O painel redondo da Elecrow nao tem offset e usa IPS (cores invertidas).
Arduino_GFX* g_gfx = new Arduino_GC9A01(
    g_bus, TFT_RST, /*rotation=*/0, /*ips=*/true);

// --- touch ----------------------------------------------------------------
CST816S g_touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);

// O CST816D reporta por interrupcao, nao em continuo. Guardamos o ultimo
// ponto e mantemos o estado "pressed" por uma janela curta, senao o LVGL
// nunca ve um arrasto.
constexpr uint32_t kTouchHoldMs = 70;
lv_coord_t g_lastX = 0, g_lastY = 0;
uint32_t   g_lastTouchMs = 0;

// --- draw buffers ---------------------------------------------------------
// 1/6 do ecra, duplo buffer, em RAM interna (mais rapido que PSRAM para SPI).
constexpr uint32_t kBufLines = TFT_HEIGHT / 6;
constexpr uint32_t kBufPixels = TFT_WIDTH * kBufLines;

lv_disp_draw_buf_t g_drawBuf;
lv_disp_drv_t      g_dispDrv;
lv_indev_drv_t     g_indevDrv;

uint8_t g_backlight = 200;

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px) {
  const uint32_t w = area->x2 - area->x1 + 1;
  const uint32_t h = area->y2 - area->y1 + 1;
  g_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)px, w, h);
  lv_disp_flush_ready(drv);
}

void touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  if (g_touch.available()) {
    g_lastX = (lv_coord_t)g_touch.data.x;
    g_lastY = (lv_coord_t)g_touch.data.y;
    g_lastTouchMs = millis();
  }

  const bool held = (millis() - g_lastTouchMs) < kTouchHoldMs;
  data->point.x = g_lastX;
  data->point.y = g_lastY;
  data->state = held ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

}  // namespace

void backlightSet(uint8_t level) {
  g_backlight = level;
  analogWrite(TFT_BL, level);
}

uint8_t backlightGet() { return g_backlight; }

void displayBegin() {
  // Primeiro de tudo: alimentar o conjunto display + touch. Tem pull-down
  // externo, logo arranca desligado -- sem isto nada abaixo funciona.
  pinMode(PANEL_EN, OUTPUT);
  digitalWrite(PANEL_EN, HIGH);
  delay(120);  // deixar as alimentacoes assentar antes de falar com o painel

  // Backlight apagado ate o painel estar pronto, para nao mostrar lixo.
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 0);

  g_gfx->begin(40000000);          // 40MHz; o GC9A01 aguenta mais, mas 40 e seguro
  g_gfx->fillScreen(BLACK);

  // begin() tem dois overloads ambos chamaveis sem argumentos: explicitar.
  g_touch.begin(Wire, RISING);

  lv_init();

  lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(
      kBufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_color_t* buf2 = (lv_color_t*)heap_caps_malloc(
      kBufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_disp_draw_buf_init(&g_drawBuf, buf1, buf2, kBufPixels);

  lv_disp_drv_init(&g_dispDrv);
  g_dispDrv.hor_res  = TFT_WIDTH;
  g_dispDrv.ver_res  = TFT_HEIGHT;
  g_dispDrv.flush_cb = flushCb;
  g_dispDrv.draw_buf = &g_drawBuf;
  g_dispDrv.full_refresh = 0;
  lv_disp_drv_register(&g_dispDrv);

  lv_indev_drv_init(&g_indevDrv);
  g_indevDrv.type    = LV_INDEV_TYPE_POINTER;
  g_indevDrv.read_cb = touchReadCb;
  lv_indev_drv_register(&g_indevDrv);

  backlightSet(g_backlight);
}
