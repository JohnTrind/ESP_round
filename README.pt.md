# CrowPanel 1.28" HMI ESP32 Rotary Display — Arduino + LVGL

> 🇬🇧 [English version](README.md)

Scaffold de menu rotativo para a board redonda da Elecrow (240×240 IPS, touch
capacitivo, encoder com botão, anel de 5 LEDs WS2812).

Hardware confirmado por `esptool` nesta unidade: ESP32-S3R8, 16 MB flash
(quad, 3,3 V), 8 MB PSRAM octal, USB-Serial/JTAG nativo.

## O detalhe que não está na wiki da Elecrow

**O conjunto display + touch tem um enable no `GPIO 1`, com pull-down externo —
ou seja, arranca desligado.** Sem o pôr a HIGH o ecrã nunca acende e o
controlador de touch não responde em I²C. Não aparece na documentação do
produto; foi encontrado por medição (ver `src/diag.cpp`).

Como foi isolado:

| Teste | Resultado |
|---|---|
| Anel WS2812 no GPIO 48 | acende → placa principal e alimentação OK |
| Backlight GPIO 46 e os outros 28 GPIOs, HIGH e LOW | ecrã apagado |
| Mapa passivo de pull-ups | pull-ups externos em 6/7, 38/39, 41/42/45; **pull-down em 1 e 2** |
| Scan I²C em 6/7 | 0 dispositivos |
| Scan I²C em 6/7 **com GPIO 1 a HIGH** | CST816 em `0x15`, 5/5 ACK |

O `GPIO 2` é o outro pino com pull-down externo e continua sem função
identificada — provável enable de outra coisa.

## Compilar e gravar

```powershell
python -m platformio run -e crowpanel_128_rotary -t upload   # aplicação
python -m platformio run -e diag -t upload                   # diagnóstico
python -m platformio device monitor                          # serial
```

Duas notas de ambiente:

- **A GFX Library está fixada em `~1.3.9`.** As 1.4+ exigem o core
  Arduino-ESP32 3.x; a plataforma estável do PlatformIO traz o 2.0.17. O `~` é
  necessário — com `^1.3.9` o semver resolve até 1.6.9 e volta a falhar.
- **Abrir a porta série reinicia o chip.** Qualquer estado posto por comandos
  do REPL desaparece na ligação seguinte; o que tiver de persistir vai no
  `setup()`.

## Estrutura

| Ficheiro | Papel |
|---|---|
| `include/pins.h` | pinout, incluindo o `PANEL_EN` |
| `include/lv_conf.h` | LVGL mínimo (RGB565, tick via `millis`, fontes) |
| `src/display.cpp` | GC9A01 + CST816D → LVGL, draw buffers duplos em RAM interna |
| `src/encoder.cpp` | quadratura por ISR, detentes e clique com debounce |
| `src/ui.cpp` | menu rotativo e os 4 demos |
| `src/diag.cpp` | REPL de diagnóstico (env `diag`) |

Decisões que valem contexto:

- **O encoder não é um `lv_indev`.** A app lê-o directamente, porque o modo
  encoder do LVGL obriga a grupos e estado edit, que numa UI redonda dá mais
  atrito do que valor. Cada ecrã decide o que o knob faz.
- **O touch tem uma janela de retenção de 70 ms.** O CST816D reporta por
  interrupção, não em contínuo; sem isso o LVGL só vê toques instantâneos e
  nunca um arrasto.
- **Draw buffers de 1/6 do ecrã, duplos, em RAM interna.** São 2×19 KB, mais
  rápidos para SPI que a PSRAM; o framebuffer completo (115 KB) também caberia.

## Diagnóstico (`env:diag`)

REPL pela porta série, uma linha por comando:

```
h/l/z <pin>   pino a HIGH / LOW / libertado
H | L | Z     todos os candidatos a HIGH / LOW / libertados
m <a> <b>     HIGH apenas no intervalo
d <pin> | D   acciona e lê o pad de volta (distingue input-only)
p             mapa passivo de pull-ups/pull-downs externos
i | S         scan I²C (simples / robusto com retentativas)
I             força bruta I²C sobre pares de pinos
E             caça ao pino de enable do painel
t             probe repetido do CST816
r             acende o anel WS2812
g             inicializa o GC9A01 e pinta ciclo de cores
?             estado actual
```

Conduz-se de fora com `C:\tmp\poke.py`:

```powershell
python C:\tmp\poke.py COM11 --wait 20 "h 1" "S"
```

O probe I²C é bit-bang e não `Wire`: o driver do core 2.0.17 reinicia o chip
quando reinicializado em pinos sem dispositivo. Mesmo em bit-bang, um probe
isolado dá falsos positivos em linhas a flutuar — por isso o `S` e o `E` usam
retentativas e só uma resposta consistente (5/5, 4/4) conta.

## Backup de fábrica

`factory_backup.bin` (16 MB) é a imagem original da Elecrow, lida antes de
gravar. Restaurar:

```powershell
python -m esptool --port COM11 write-flash 0 factory_backup.bin
```

Ocupa 16 MB na pasta — se isto for para git, põe-no no `.gitignore`.
