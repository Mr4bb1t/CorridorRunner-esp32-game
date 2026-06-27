// Config.h — CorridorRunner
// Importado do R4BB1T FHC (mesmos pinos de hardware)
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ─────────────────────────────────────────────────────────────
//  Pinos dos botões (idênticos ao R4BB1T)
// ─────────────────────────────────────────────────────────────
#define BUTTON_LEFT   14
#define BUTTON_RIGHT  27
#define BUTTON_SELECT 26

// ─────────────────────────────────────────────────────────────
//  Pinos do display ST7735 (idênticos ao R4BB1T)
// ─────────────────────────────────────────────────────────────
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS    5
#define TFT_DC   17
#define TFT_RST  16
#define TFT_BL   21   // backlight

// ─────────────────────────────────────────────────────────────
//  Dimensões de tela úteis (área visual real do ST7735)
// ─────────────────────────────────────────────────────────────
#define SCR_W 128
#define SCR_H 160

// ─────────────────────────────────────────────────────────────
//  Debounce
// ─────────────────────────────────────────────────────────────
#define DEBOUNCE_MS 180

// ─────────────────────────────────────────────────────────────
//  Paleta Cyber Edition (RGB565) — importada do R4BB1T
// ─────────────────────────────────────────────────────────────
#define C_BG        0x0000   // Preto puro
#define C_GOLD      0xE4A0   // Dourado âmbar
#define C_GOLD_DIM  0x7220   // Dourado escuro
#define C_GOLD_SEL  0x2104   // Fundo seleção
#define C_WHITE     0xFFFF   // Branco
#define C_GREY      0x4208   // Cinza escuro
#define C_GREY_DARK 0x18C3   // Cinza muito escuro
#define C_RED       0xF800   // Vermelho
#define C_GREEN     0x07E0   // Verde
#define C_CYAN      0x07FF   // Ciano
#define C_BLACK     0x0000   // Preto

#endif // CONFIG_H
