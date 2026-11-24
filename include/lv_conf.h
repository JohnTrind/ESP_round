#pragma once
// lv_conf.h minimo para LVGL 8.3 neste projeto.
// Tudo o que nao estiver aqui fica nos defaults de lv_conf_internal.h.

#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0     // Arduino_GFX trata da ordem de bytes

// Heap do LVGL em RAM interna (os draw buffers sao alocados a parte)
#define LV_MEM_CUSTOM           0
#define LV_MEM_SIZE             (48U * 1024U)

// Tick vindo do millis() do Arduino
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DPI_DEF              130   // 240px num ecra de 1.28"

// Fontes
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_28   1
#define LV_FONT_MONTSERRAT_48   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

// Widgets usados
#define LV_USE_ARC              1
#define LV_USE_LABEL            1
#define LV_USE_ROLLER           1
#define LV_USE_BTN              1
#define LV_USE_SLIDER           1

#define LV_USE_PERF_MONITOR     0
#define LV_USE_LOG              0
