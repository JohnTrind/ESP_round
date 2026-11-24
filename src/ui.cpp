#include "ui.h"

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <lvgl.h>

#include "display.h"
#include "encoder.h"
#include "pins.h"

namespace {

// --------------------------------------------------------------------------
// Estado
// --------------------------------------------------------------------------
enum Screen : uint8_t {
  SCR_MENU = 0,
  SCR_DIAL,
  SCR_LED,
  SCR_TOUCH,
  SCR_BRIGHT,
  SCR_COUNT,
};

const char* kMenuItems = "Dial\nAnel RGB\nTouch\nBrilho";
constexpr int kMenuCount = 4;

Screen g_screen = SCR_MENU;
int    g_menuIdx = 0;
int    g_dialValue = 42;    // 0..100
int    g_hue = 0;           // 0..359

Adafruit_NeoPixel g_ring(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);

// Objectos por ecra
lv_obj_t* g_scr[SCR_COUNT] = {nullptr};
lv_obj_t* g_menuRoller = nullptr;
lv_obj_t* g_menuArc = nullptr;
lv_obj_t* g_dialArc = nullptr;
lv_obj_t* g_dialLabel = nullptr;
lv_obj_t* g_ledArc = nullptr;
lv_obj_t* g_ledLabel = nullptr;
lv_obj_t* g_touchDot = nullptr;
lv_obj_t* g_touchLabel = nullptr;
lv_obj_t* g_brightArc = nullptr;
lv_obj_t* g_brightLabel = nullptr;

const lv_color_t kBg     = LV_COLOR_MAKE(0x0A, 0x0C, 0x10);
const lv_color_t kAccent = LV_COLOR_MAKE(0x3A, 0xD9, 0xC9);
const lv_color_t kDim    = LV_COLOR_MAKE(0x1E, 0x24, 0x2E);
const lv_color_t kText   = LV_COLOR_MAKE(0xE8, 0xEC, 0xF1);

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------
lv_obj_t* makeScreen() {
  lv_obj_t* s = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(s, kBg, 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(s, 0, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

// Arco decorativo que segue a borda circular do painel.
lv_obj_t* makeRingArc(lv_obj_t* parent, int32_t min, int32_t max) {
  lv_obj_t* arc = lv_arc_create(parent);
  lv_obj_set_size(arc, 232, 232);
  lv_obj_center(arc);
  lv_arc_set_rotation(arc, 270);          // zero fica no topo
  lv_arc_set_bg_angles(arc, 0, 360);
  lv_arc_set_range(arc, min, max);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, kDim, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, kAccent, LV_PART_INDICATOR);
  return arc;
}

lv_obj_t* makeCenterLabel(lv_obj_t* parent, const lv_font_t* font,
                          lv_coord_t dy, lv_color_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  lv_obj_align(l, LV_ALIGN_CENTER, 0, dy);
  return l;
}

void ringSetHue(uint16_t hue) {
  // v=55 em vez de 100: o anel fica a centimetros dos olhos.
  const uint32_t raw = lv_color_to32(lv_color_hsv_to_rgb(hue, 100, 55));
  const uint8_t r = (uint8_t)((raw >> 16) & 0xFF);
  const uint8_t g = (uint8_t)((raw >> 8) & 0xFF);
  const uint8_t b = (uint8_t)(raw & 0xFF);
  const uint32_t packed = g_ring.Color(r, g, b);
  for (uint16_t i = 0; i < LED_NUM; i++) g_ring.setPixelColor(i, packed);
  g_ring.show();
}

void ringOff() {
  g_ring.clear();
  g_ring.show();
}

// --------------------------------------------------------------------------
// Construcao dos ecras
// --------------------------------------------------------------------------
void buildMenu() {
  lv_obj_t* s = makeScreen();
  g_scr[SCR_MENU] = s;

  g_menuArc = makeRingArc(s, 0, kMenuCount);

  g_menuRoller = lv_roller_create(s);
  lv_roller_set_options(g_menuRoller, kMenuItems, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(g_menuRoller, 3);
  lv_obj_set_width(g_menuRoller, 150);
  lv_obj_center(g_menuRoller);
  lv_obj_set_style_bg_opa(g_menuRoller, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_menuRoller, 0, LV_PART_MAIN);
  lv_obj_set_style_text_color(g_menuRoller, kText, LV_PART_MAIN);
  lv_obj_set_style_text_font(g_menuRoller, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_menuRoller, kDim, LV_PART_SELECTED);
  lv_obj_set_style_text_color(g_menuRoller, kAccent, LV_PART_SELECTED);
  lv_obj_set_style_radius(g_menuRoller, 20, LV_PART_SELECTED);
}

void buildDial() {
  lv_obj_t* s = makeScreen();
  g_scr[SCR_DIAL] = s;
  g_dialArc = makeRingArc(s, 0, 100);
  g_dialLabel = makeCenterLabel(s, &lv_font_montserrat_48, -6, kText);
  lv_obj_t* cap = makeCenterLabel(s, &lv_font_montserrat_14, 34, kAccent);
  lv_label_set_text(cap, "DIAL");
}

void buildLed() {
  lv_obj_t* s = makeScreen();
  g_scr[SCR_LED] = s;
  g_ledArc = makeRingArc(s, 0, 359);
  g_ledLabel = makeCenterLabel(s, &lv_font_montserrat_28, -4, kText);
  lv_obj_t* cap = makeCenterLabel(s, &lv_font_montserrat_14, 32, kAccent);
  lv_label_set_text(cap, "ANEL RGB");
}

void onScreenPressing(lv_event_t* e) {
  LV_UNUSED(e);
  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  lv_obj_set_pos(g_touchDot, p.x - 12, p.y - 12);
  lv_label_set_text_fmt(g_touchLabel, "%d, %d", (int)p.x, (int)p.y);
}

void buildTouch() {
  lv_obj_t* s = makeScreen();
  g_scr[SCR_TOUCH] = s;
  makeRingArc(s, 0, 1);

  g_touchDot = lv_obj_create(s);
  lv_obj_set_size(g_touchDot, 24, 24);
  lv_obj_set_style_radius(g_touchDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(g_touchDot, kAccent, 0);
  lv_obj_set_style_border_width(g_touchDot, 0, 0);
  lv_obj_set_pos(g_touchDot, 108, 108);

  g_touchLabel = makeCenterLabel(s, &lv_font_montserrat_20, 60, kText);
  lv_label_set_text(g_touchLabel, "toca");

  lv_obj_add_event_cb(s, onScreenPressing, LV_EVENT_PRESSING, nullptr);
}

void buildBright() {
  lv_obj_t* s = makeScreen();
  g_scr[SCR_BRIGHT] = s;
  g_brightArc = makeRingArc(s, 10, 255);
  g_brightLabel = makeCenterLabel(s, &lv_font_montserrat_48, -6, kText);
  lv_obj_t* cap = makeCenterLabel(s, &lv_font_montserrat_14, 34, kAccent);
  lv_label_set_text(cap, "BRILHO");
}

// --------------------------------------------------------------------------
// Refresh + navegacao
// --------------------------------------------------------------------------
void refresh() {
  switch (g_screen) {
    case SCR_MENU:
      lv_roller_set_selected(g_menuRoller, g_menuIdx, LV_ANIM_ON);
      lv_arc_set_value(g_menuArc, g_menuIdx + 1);
      break;
    case SCR_DIAL:
      lv_arc_set_value(g_dialArc, g_dialValue);
      lv_label_set_text_fmt(g_dialLabel, "%d", g_dialValue);
      break;
    case SCR_LED:
      lv_arc_set_value(g_ledArc, g_hue);
      lv_label_set_text_fmt(g_ledLabel, "%d", g_hue);
      lv_obj_set_style_arc_color(
          g_ledArc, lv_color_hsv_to_rgb(g_hue, 100, 100), LV_PART_INDICATOR);
      ringSetHue(g_hue);
      break;
    case SCR_BRIGHT:
      lv_arc_set_value(g_brightArc, backlightGet());
      lv_label_set_text_fmt(g_brightLabel, "%d", backlightGet() * 100 / 255);
      break;
    default:
      break;
  }
}

void go(Screen s) {
  if (g_screen == SCR_LED && s != SCR_LED) ringOff();
  g_screen = s;
  lv_scr_load_anim(g_scr[s], LV_SCR_LOAD_ANIM_FADE_IN, 180, 0, false);
  refresh();
}

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

// --------------------------------------------------------------------------
void uiBegin() {
  g_ring.begin();
  ringOff();

  buildMenu();
  buildDial();
  buildLed();
  buildTouch();
  buildBright();

  g_screen = SCR_MENU;
  lv_scr_load(g_scr[SCR_MENU]);
  refresh();
}

void uiTick() {
  const int32_t delta = knob::takeDelta();
  const bool click = knob::takeClick();

  if (delta != 0) {
    switch (g_screen) {
      case SCR_MENU:
        g_menuIdx = (int)((g_menuIdx + delta) % kMenuCount);
        if (g_menuIdx < 0) g_menuIdx += kMenuCount;
        break;
      case SCR_DIAL:
        g_dialValue = clampi(g_dialValue + (int)delta, 0, 100);
        break;
      case SCR_LED:
        g_hue = (int)((g_hue + delta * 6) % 360);
        if (g_hue < 0) g_hue += 360;
        break;
      case SCR_BRIGHT:
        backlightSet((uint8_t)clampi(backlightGet() + (int)delta * 8, 10, 255));
        break;
      default:
        break;
    }
    refresh();
  }

  if (click) {
    if (g_screen == SCR_MENU) {
      go((Screen)(SCR_DIAL + g_menuIdx));   // ordem do menu == ordem do enum
    } else {
      go(SCR_MENU);
    }
  }
}
