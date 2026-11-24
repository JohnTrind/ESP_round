#pragma once
// CrowPanel 1.28" HMI ESP32 Rotary Display (240x240 IPS round, touch + knob)
// Board: ESP32-S3R8 - 16MB flash / 8MB PSRAM (confirmado por esptool, MAC 68:EE:8F:5D:AA:64)
// Fonte: https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display.html

// Enable do conjunto display + touch.
//
// NAO esta documentado na wiki da Elecrow, foi encontrado por medicao: tem
// pull-down externo (arranca desligado) e, posto a HIGH, o CST816 passa a
// responder em I2C (5/5 ACK em 0x15 contra 0/5 sem ele). Sem isto o painel
// fica morto e o ecra nunca acende.
#define PANEL_EN    1

// Display GC9A01 (SPI)
#define TFT_SCLK   10
#define TFT_MOSI   11
#define TFT_MISO   -1   // nao ligado
#define TFT_DC      3
#define TFT_CS      9
#define TFT_RST    14
#define TFT_BL     46   // backlight

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// Touch capacitivo CST816D (I2C)
#define TOUCH_SDA   6
#define TOUCH_SCL   7
#define TOUCH_INT   5
#define TOUCH_RST  13

// Encoder rotativo + botao
#define ENCODER_A  45
#define ENCODER_B  42
#define ENCODER_SW 41

// Anel de LEDs WS2812
#define LED_PIN    48
#define LED_NUM     5   // nao confirmado na wiki - verificar

// Diversos
#define PWR_LED    40   // indicador de energia
// GPIO 4 e 12 expostos como test pads / I/O livre
