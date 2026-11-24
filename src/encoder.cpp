#include "encoder.h"
#include "pins.h"

namespace knob {
namespace {

// Decodificador de quadratura por tabela de transicoes.
// Indice = (estado_anterior << 2) | estado_actual, valor = -1/0/+1.
const int8_t kTransition[16] = {
    0, -1, +1,  0,
   +1,  0,  0, -1,
   -1,  0,  0, +1,
    0, +1, -1,  0,
};

volatile int32_t g_subSteps = 0;   // 4 sub-passos por detente
volatile uint8_t g_prevState = 0;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

int32_t  g_carry = 0;              // sobra entre chamadas a takeDelta()
bool     g_clickPending = false;
bool     g_lastBtn = false;        // true = premido
uint32_t g_lastBtnChange = 0;

constexpr uint32_t kDebounceMs = 25;
constexpr int32_t  kSubStepsPerDetent = 4;

void IRAM_ATTR onEdge() {
  const uint8_t state = (uint8_t)((digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B));
  portENTER_CRITICAL_ISR(&g_mux);
  g_subSteps += kTransition[(g_prevState << 2) | state];
  g_prevState = state;
  portEXIT_CRITICAL_ISR(&g_mux);
}

}  // namespace

void begin() {
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  g_prevState = (uint8_t)((digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B));
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), onEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), onEdge, CHANGE);

  g_lastBtn = (digitalRead(ENCODER_SW) == LOW);
  g_lastBtnChange = millis();
}

int32_t takeDelta() {
  portENTER_CRITICAL(&g_mux);
  const int32_t raw = g_subSteps;
  g_subSteps = 0;
  portEXIT_CRITICAL(&g_mux);

  g_carry += raw;
  const int32_t detents = g_carry / kSubStepsPerDetent;
  g_carry -= detents * kSubStepsPerDetent;
  return detents;
}

bool pressed() { return digitalRead(ENCODER_SW) == LOW; }

bool takeClick() {
  const bool now = pressed();
  const uint32_t t = millis();

  if (now != g_lastBtn && (t - g_lastBtnChange) > kDebounceMs) {
    g_lastBtnChange = t;
    g_lastBtn = now;
    if (!now) g_clickPending = true;   // dispara ao largar
  }

  if (g_clickPending) {
    g_clickPending = false;
    return true;
  }
  return false;
}

}  // namespace knob
