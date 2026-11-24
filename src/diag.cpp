// Diagnostico interactivo: REPL pela porta serie para encontrar o pino do
// backlight (e validar o painel) sem ter de regravar a cada tentativa.
//
//   python -m platformio run -e diag -t upload
//
// Foi com isto que se encontrou o enable do painel no GPIO 1 (ver README):
// o comando 'p' mostrou pull-down externo em 1 e 2, e o 'E' confirmou que
// pondo o 1 a HIGH o CST816 passa a responder.
//
// Comandos (uma linha cada):
//   h <pin>    pino a OUTPUT/HIGH
//   l <pin>    pino a OUTPUT/LOW
//   z <pin>    liberta o pino (INPUT)
//   H          todos os candidatos a HIGH
//   L          todos os candidatos a LOW
//   Z          liberta todos
//   m <a> <b>  so os candidatos no intervalo [a,b] a HIGH, resto libertado
//   d <pin>    acciona e le o pad de volta (distingue pino input-only)
//   D          o mesmo para todos os candidatos
//   p          mapa passivo de pull-ups/pull-downs externos
//   g          inicializa o GC9A01 e pinta ciclo de cores
//   i          scan I2C nos pinos da wiki (6/7 e 38/39)
//   I          forca bruta I2C sobre pares de candidatos
//   S          scan I2C robusto, com retentativas e RST accionado
//   E          caca ao pino de enable do painel
//   t          probe repetido do CST816
//   r          acende o anel WS2812
//   ?          estado actual
//
// Atencao: abrir a porta serie reinicia o chip, logo o estado posto por
// comandos desaparece na ligacao seguinte. O que tiver de persistir para
// inspeccao visual vai no setup() (ver beaconBegin).
//
// Candidatos excluidos e porque:
//   19,20      USB D-/D+ (matava a porta serie)
//   26..37     flash quad + PSRAM octal do ESP32-S3R8
//   48         WS2812 do anel (nao e backlight)

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#include "pins.h"

namespace {

const uint8_t kCandidates[] = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 21, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
};
constexpr size_t kCount = sizeof(kCandidates) / sizeof(kCandidates[0]);

bool g_driven[49] = {false};
bool g_level[49] = {false};
bool g_gfxUp = false;

Arduino_GFX* g_gfx = nullptr;

void drive(uint8_t pin, bool high) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, high ? HIGH : LOW);
  g_driven[pin] = true;
  g_level[pin] = high;
}

void release(uint8_t pin) {
  pinMode(pin, INPUT);
  g_driven[pin] = false;
}

void allTo(bool high) {
  for (size_t i = 0; i < kCount; i++) drive(kCandidates[i], high);
  Serial.printf("todos os %u candidatos a %s\n", (unsigned)kCount,
                high ? "HIGH" : "LOW");
}

void releaseAll() {
  for (size_t i = 0; i < kCount; i++) release(kCandidates[i]);
  Serial.println("todos libertados (INPUT)");
}

void maskRange(int a, int b) {
  for (size_t i = 0; i < kCount; i++) {
    const uint8_t p = kCandidates[i];
    if (p >= a && p <= b) {
      drive(p, true);
    } else {
      release(p);
    }
  }
  Serial.printf("HIGH apenas em [%d..%d]:", a, b);
  for (size_t i = 0; i < kCount; i++) {
    const uint8_t p = kCandidates[i];
    if (p >= a && p <= b) Serial.printf(" %u", p);
  }
  Serial.println();
}

void report() {
  Serial.print("accionados:");
  bool any = false;
  for (int p = 0; p < 49; p++) {
    if (g_driven[p]) {
      Serial.printf(" %d=%c", p, g_level[p] ? 'H' : 'L');
      any = true;
    }
  }
  if (!any) Serial.print(" (nenhum)");
  Serial.println();
  Serial.printf("gfx inicializado: %s\n", g_gfxUp ? "sim" : "nao");
}

void gfxTest() {
  if (!g_gfxUp) {
    Arduino_DataBus* bus = new Arduino_ESP32SPI(
        TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO);
    g_gfx = new Arduino_GC9A01(bus, TFT_RST, 0, true);
    const bool ok = g_gfx->begin(40000000);
    Serial.printf("gfx->begin: %s  (%dx%d)\n", ok ? "true" : "FALSE",
                  g_gfx->width(), g_gfx->height());
    g_gfxUp = ok;
  }
  if (!g_gfx) return;
  const uint16_t cols[] = {RED, GREEN, BLUE, WHITE};
  const char* names[] = {"vermelho", "verde", "azul", "branco"};
  for (int i = 0; i < 4; i++) {
    g_gfx->fillScreen(cols[i]);
    Serial.printf("fillScreen %s\n", names[i]);
    delay(900);
  }
  g_gfx->fillScreen(BLACK);
  g_gfx->fillCircle(120, 120, 100, CYAN);
  g_gfx->fillCircle(120, 120, 60, BLACK);
  Serial.println("anel ciano desenhado");
}

// --------------------------------------------------------------------------
// Probe I2C em bit-bang.
//
// O driver Wire do core 2.0.17 reinicia o chip quando reinicializado em pinos
// sem dispositivo, por isso o barramento e feito a mao. Open-drain emulado:
// "alto" = INPUT_PULLUP (pull-up interno, ~45k), "baixo" = OUTPUT/LOW.
// Chega para ~50kHz, que e o que precisamos para ver um ACK.
// --------------------------------------------------------------------------
inline void sbHigh(uint8_t p) { pinMode(p, INPUT_PULLUP); }
inline void sbLow(uint8_t p)  { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
inline void sbWait()          { delayMicroseconds(6); }

bool bbProbe(uint8_t sda, uint8_t scl, uint8_t addr) {
  // Estado de repouso.
  sbHigh(sda); sbHigh(scl); sbWait();
  // Se as linhas nao sobem, nao ha pull-up: barramento inexistente.
  if (digitalRead(sda) == LOW || digitalRead(scl) == LOW) return false;

  // START
  sbLow(sda); sbWait();
  sbLow(scl); sbWait();

  // 8 bits: endereco + bit de escrita
  const uint8_t byte = (uint8_t)(addr << 1);
  for (int i = 7; i >= 0; i--) {
    if ((byte >> i) & 1) sbHigh(sda); else sbLow(sda);
    sbWait();
    sbHigh(scl); sbWait();
    sbLow(scl); sbWait();
  }

  // ACK: largamos SDA e vemos se o escravo a puxa para baixo
  sbHigh(sda); sbWait();
  sbHigh(scl); sbWait();
  const bool ack = (digitalRead(sda) == LOW);
  sbLow(scl); sbWait();

  // STOP
  sbLow(sda); sbWait();
  sbHigh(scl); sbWait();
  sbHigh(sda); sbWait();

  return ack;
}

int i2cScanPair(uint8_t sda, uint8_t scl, bool verbose) {
  int found = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    if (bbProbe(sda, scl, addr)) {
      found++;
      Serial.printf("  ACK  sda=%-2u scl=%-2u  addr=0x%02X\n", sda, scl, addr);
    }
  }
  if (verbose && found == 0) {
    Serial.printf("  nada em sda=%u scl=%u\n", sda, scl);
  }
  // Deixa os pinos em repouso.
  sbHigh(sda); sbHigh(scl);
  return found;
}

// Forca bruta sobre pares de candidatos, nos enderecos tipicos de touch.
void i2cBruteForce() {
  const uint8_t kTouchAddrs[] = {0x15, 0x2E, 0x38, 0x5A, 0x14};
  Serial.println("--- forca bruta I2C (enderecos de touch) ---");
  int hits = 0;
  for (size_t a = 0; a < kCount; a++) {
    for (size_t b = 0; b < kCount; b++) {
      if (a == b) continue;
      const uint8_t sda = kCandidates[a], scl = kCandidates[b];
      for (uint8_t t = 0; t < sizeof(kTouchAddrs); t++) {
        if (bbProbe(sda, scl, kTouchAddrs[t])) {
          hits++;
          Serial.printf("  ACK  sda=%u scl=%u  addr=0x%02X\n", sda, scl,
                        kTouchAddrs[t]);
        }
      }
    }
    delay(1);  // nao prender o loop
  }
  releaseAll();
  Serial.printf("--- fim, %d resposta(s) ---\n", hits);
}

void ringTest() {
  Adafruit_NeoPixel ring(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);
  ring.begin();
  Serial.printf("anel: %d LEDs no GPIO %d\n", LED_NUM, LED_PIN);
  const uint32_t cols[] = {ring.Color(80, 0, 0), ring.Color(0, 80, 0),
                           ring.Color(0, 0, 80), ring.Color(60, 60, 60)};
  const char* names[] = {"vermelho", "verde", "azul", "branco"};
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < LED_NUM; j++) ring.setPixelColor(j, cols[i]);
    ring.show();
    Serial.printf("anel %s\n", names[i]);
    delay(1000);
  }
  ring.clear();
  ring.show();
  Serial.println("anel apagado");
}

// Acciona o pino e le o pad de volta. Um pad com driver de saida acompanha o
// valor escrito; um pino input-only fica preso ao que o pull interno dita.
void driveAndReadback(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delayMicroseconds(200);
  const int rHigh = digitalRead(pin);
  digitalWrite(pin, LOW);
  delayMicroseconds(200);
  const int rLow = digitalRead(pin);
  digitalWrite(pin, HIGH);  // deixa aceso, caso seja backlight
  Serial.printf("pino %-2u : escrito HIGH -> le %d | escrito LOW -> le %d  %s\n",
                pin, rHigh, rLow,
                (rHigh == 1 && rLow == 0) ? "(saida OK)" : "(NAO segue a escrita)");
}

// Repete o probe do CST816 para separar ACK real de ruido.
void touchProbe() {
  int hits = 0;
  for (int i = 0; i < 8; i++) {
    if (bbProbe(TOUCH_SDA, TOUCH_SCL, 0x15)) hits++;
    delay(20);
  }
  Serial.printf("CST816 em sda=%d scl=%d addr=0x15: %d/8 ACK\n",
                TOUCH_SDA, TOUCH_SCL, hits);

  // Com o RST do touch solto vs. activo, para ver se muda algo.
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(20);
  digitalWrite(TOUCH_RST, HIGH);
  delay(80);
  hits = 0;
  for (int i = 0; i < 8; i++) {
    if (bbProbe(TOUCH_SDA, TOUCH_SCL, 0x15)) hits++;
    delay(20);
  }
  Serial.printf("apos pulso no RST (GPIO %d): %d/8 ACK\n", TOUCH_RST, hits);
}

// Mapa passivo do que esta electricamente ligado a cada pino.
// Pull-up interno e depois pull-down interno (~45k cada). Um pull-up externo
// de 10k (tipico num barramento I2C) vence o pull-down interno.
//   1/0 -> nada ligado (segue o pull interno)
//   1/1 -> pull-up externo ou linha accionada a alto
//   0/0 -> pull-down externo ou linha accionada a baixo
void pinMap() {
  Serial.println("--- mapa passivo (pull-up / pull-down internos) ---");
  for (size_t i = 0; i < kCount; i++) {
    const uint8_t p = kCandidates[i];
    pinMode(p, INPUT_PULLUP);
    delayMicroseconds(500);
    const int up = digitalRead(p);
    pinMode(p, INPUT_PULLDOWN);
    delayMicroseconds(500);
    const int dn = digitalRead(p);
    pinMode(p, INPUT);

    const char* verdict;
    if (up == 1 && dn == 0)      verdict = "livre (nada ligado)";
    else if (up == 1 && dn == 1) verdict = "<<< PULL-UP EXTERNO / alto";
    else if (up == 0 && dn == 0) verdict = "<<< PULL-DOWN EXTERNO / baixo";
    else                         verdict = "<<< instavel";
    Serial.printf("  GPIO %-2u  pu=%d pd=%d  %s\n", p, up, dn, verdict);
  }
  Serial.println("--- fim do mapa ---");
}

// Scan robusto: varias tentativas por endereco, tempos de assentamento mais
// largos, e RST do touch mantido alto (o CST816 nao responde em reset).
void robustScan(uint8_t sda, uint8_t scl) {
  Serial.printf("--- scan robusto sda=%u scl=%u ---\n", sda, scl);

  // Candidatos a linha de reset do touch, todos a alto.
  const uint8_t rstCandidates[] = {TOUCH_RST, 13, 14, 21, 47};
  for (uint8_t i = 0; i < sizeof(rstCandidates); i++) {
    pinMode(rstCandidates[i], OUTPUT);
    digitalWrite(rstCandidates[i], HIGH);
  }
  delay(150);

  int total = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    int acks = 0;
    for (int tryN = 0; tryN < 5; tryN++) {
      if (bbProbe(sda, scl, addr)) acks++;
      delayMicroseconds(300);
    }
    if (acks > 0) {
      total++;
      Serial.printf("  addr 0x%02X : %d/5 ACK\n", addr, acks);
    }
  }
  Serial.printf("--- fim: %d endereco(s) com resposta ---\n", total);
  sbHigh(sda); sbHigh(scl);
}

// Procura um pino que alimente/active o painel: acciona cada candidato e ve
// se o touch passa a responder em 0x15. Totalmente automatico.
void enableHunt() {
  Serial.println("--- caca ao pino de enable do painel ---");
  int found = 0;

  for (size_t i = 0; i < kCount; i++) {
    const uint8_t p = kCandidates[i];
    if (p == TOUCH_SDA || p == TOUCH_SCL) continue;  // nao mexer no bus

    for (int pol = 1; pol >= 0; pol--) {
      pinMode(p, OUTPUT);
      digitalWrite(p, pol ? HIGH : LOW);
      delay(200);

      int acks = 0;
      for (int t = 0; t < 4; t++) {
        if (bbProbe(TOUCH_SDA, TOUCH_SCL, 0x15)) acks++;
      }
      if (acks > 0) {
        found++;
        Serial.printf("  >>> GPIO %u a %s : touch responde %d/4\n", p,
                      pol ? "HIGH" : "LOW", acks);
      }
    }
    pinMode(p, INPUT);
  }

  // Ultima tentativa: todos os livres a HIGH ao mesmo tempo.
  Serial.println("  tentativa final: todos os candidatos a HIGH");
  for (size_t i = 0; i < kCount; i++) {
    const uint8_t p = kCandidates[i];
    if (p == TOUCH_SDA || p == TOUCH_SCL) continue;
    pinMode(p, OUTPUT);
    digitalWrite(p, HIGH);
  }
  delay(300);
  int acks = 0;
  for (int t = 0; t < 6; t++) {
    if (bbProbe(TOUCH_SDA, TOUCH_SCL, 0x15)) acks++;
  }
  Serial.printf("  todos HIGH: touch responde %d/6\n", acks);
  releaseAll();

  Serial.printf("--- fim: %d pino(s) com efeito ---\n", found);
}

void handle(String line) {
  line.trim();
  if (line.length() == 0) return;
  const char cmd = line[0];
  const String rest = line.substring(1);

  switch (cmd) {
    case 'h': drive((uint8_t)rest.toInt(), true);
              Serial.printf("pino %d HIGH\n", (int)rest.toInt()); break;
    case 'l': drive((uint8_t)rest.toInt(), false);
              Serial.printf("pino %d LOW\n", (int)rest.toInt()); break;
    case 'z': release((uint8_t)rest.toInt());
              Serial.printf("pino %d libertado\n", (int)rest.toInt()); break;
    case 'H': allTo(true); break;
    case 'L': allTo(false); break;
    case 'Z': releaseAll(); break;
    case 'g': gfxTest(); break;
    case '?': report(); break;
    case 'i': Serial.printf("--- scan I2C em sda=%d scl=%d ---\n",
                            TOUCH_SDA, TOUCH_SCL);
              i2cScanPair(TOUCH_SDA, TOUCH_SCL, true);
              Serial.printf("--- scan I2C em sda=%d scl=%d (bus 2) ---\n", 38, 39);
              i2cScanPair(38, 39, true);
              break;
    case 'I': i2cBruteForce(); break;
    case 'r': ringTest(); break;
    case 't': touchProbe(); break;
    case 'p': pinMap(); break;
    case 'E': enableHunt(); break;
    case 'S': robustScan(TOUCH_SDA, TOUCH_SCL); robustScan(38, 39); break;
    case 'd': driveAndReadback((uint8_t)rest.toInt()); break;
    case 'D': {  // readback de todos os candidatos, para ver quais tem saida
      Serial.println("--- readback de todos os candidatos ---");
      for (size_t i = 0; i < kCount; i++) driveAndReadback(kCandidates[i]);
      break;
    }
    case 'm': {
      const int sp = rest.indexOf(' ', 1);
      if (sp < 0) { Serial.println("uso: m <a> <b>"); break; }
      maskRange(rest.substring(0, sp).toInt(), rest.substring(sp + 1).toInt());
      break;
    }
    default: Serial.printf("comando desconhecido: %c\n", cmd); break;
  }
}

}  // namespace

// Beacon: estado visivel posto no setup(), logo sobrevive aos resets que a
// abertura da porta serie provoca. Serve para responder a uma pergunta so --
// esta aqui ligado algum hardware de display?
Adafruit_NeoPixel g_beaconRing(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);

void beaconBegin() {
  pinMode(PANEL_EN, OUTPUT);
  digitalWrite(PANEL_EN, HIGH);    // enable do painel, sem isto nada acende
  delay(120);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);      // backlight no pino da wiki
  g_beaconRing.begin();
  for (int i = 0; i < LED_NUM; i++) g_beaconRing.setPixelColor(i, 0x303030);
  g_beaconRing.show();
  Serial.printf("beacon: GPIO %d HIGH (backlight), %d LEDs brancos no GPIO %d\n",
                TFT_BL, LED_NUM, LED_PIN);
}

void setup() {
  Serial.begin(115200);
  delay(600);
  Serial.println();
  Serial.println("=== diag REPL pronto ===");
  beaconBegin();
  Serial.println("comandos: h/l/z <pin> | H | L | Z | m a b | g | i | I | r | ?");
}

void loop() {
  static String buf;
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      handle(buf);
      buf = "";
    } else if (buf.length() < 40) {
      buf += c;
    }
  }
  delay(5);
}
