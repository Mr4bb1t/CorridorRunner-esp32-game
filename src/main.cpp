// CorridorRunner — main.cpp
// Jogo de corredor 3D para ESP32 + ST7735 128×160
// Estética idêntica ao screensaver "corredor" do R4BB1T FHC (case 7)
// Toda a lógica de desenho é matemática pura (sem sprites externos)
//
// Controles:
//   BUTTON_LEFT   → mover para a esquerda
//   BUTTON_RIGHT  → mover para a direita
//   BUTTON_SELECT → pular (botão central "o")

#include "Config.h"
#include <TFT_eSPI.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────
//  Display e sprite
// ─────────────────────────────────────────────────────────────
TFT_eSPI   tft;
TFT_eSprite spr(&tft);  // framebuffer principal (128×160)

// ─────────────────────────────────────────────────────────────
//  Dimensões do sprite = tela real
// ─────────────────────────────────────────────────────────────
static const int SW = SCR_W;  // 128
static const int SH = SCR_H;  // 160
static const int CX = SW / 2; // 64
static const int CY = SH / 2; // 80

// Buffer de espelhamento serial
static uint8_t mirrorBuffer[SW * SH * 2];
static volatile bool newFrameReady = false;

// ─────────────────────────────────────────────────────────────
//  Paleta de cores animadas (port de pCol() do R4BB1T)
//  Retorna RGB565 dado brilho [0..15] e matiz [0..359]
// ─────────────────────────────────────────────────────────────
static uint16_t pCol(uint8_t bright, int hue) {
  hue = ((hue % 360) + 360) % 360;
  float h = hue / 60.0f;
  float s = 1.0f;
  float l = (0.08f + bright * 0.022f);
  if (l > 0.55f) l = 0.55f;

  float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
  float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
  float r1, g1, b1;
  if      (h < 1) { r1=c; g1=x; b1=0; }
  else if (h < 2) { r1=x; g1=c; b1=0; }
  else if (h < 3) { r1=0; g1=c; b1=x; }
  else if (h < 4) { r1=0; g1=x; b1=c; }
  else if (h < 5) { r1=x; g1=0; b1=c; }
  else            { r1=c; g1=0; b1=x; }
  float m = l - c / 2.0f;
  uint8_t r = (uint8_t)((r1 + m) * 255);
  uint8_t g = (uint8_t)((g1 + m) * 255);
  uint8_t b = (uint8_t)((b1 + m) * 255);
  return spr.color565(r, g, b);
}

static uint16_t pColDark(uint8_t bright, int hue) {
  return pCol(bright / 4, hue);
}

// ─────────────────────────────────────────────────────────────
//  Corredor — port exato do case 7 do screensaver R4BB1T
// ─────────────────────────────────────────────────────────────
static const int   NS         = 12;
static const float SPACING    = 1.0f;
static const float MAX_Z      = NS * SPACING;

struct Seg { int x1, y1, x2, y2, sz; uint16_t col, drk; };

static float getCurveOffset(float z, float t) {
  // Oscila bem devagar
  float sway = sinf(t * 0.2f);
  // Envelope de força: varia a intensidade da curva (vai de 0 a 1 bem lentamente)
  float intensity = (sinf(t * 0.07f) + 1.0f) * 0.5f;
  // Fator secundário para não ser muito previsível
  float sway2 = sinf(t * 0.11f + 1.5f);
  
  // Combina as ondas para uma curva que surge aos poucos, com força variável
  float curveFactor = (sway + sway2 * 0.5f) * intensity * 0.5f;
  
  return curveFactor * z * z;
}

static void buildSegs(float offset, float t, Seg* s) {
  for (int i = 0; i < NS; i++) {
    float z = MAX_Z - i * SPACING - offset;
    if (z <= 0.1f) z = 0.1f;
    s[i].sz = (int)(140.0f / z);
    int cx2  = CX + (int)getCurveOffset(z, t);
    s[i].x1  = cx2 - s[i].sz / 2;
    s[i].y1  = CY  - s[i].sz / 2;
    s[i].x2  = cx2 + s[i].sz / 2;
    s[i].y2  = CY  + s[i].sz / 2;
    uint16_t base = pCol(15, (int)(t * 10 + i * 20));
    s[i].col = base;
    s[i].drk = pColDark(15, (int)(t * 10 + i * 20));
  }
}

static void drawCorridor(float offset, float t) {
  Seg s[NS];
  buildSegs(offset, t, s);

  for (int i = 0; i < NS - 1; i++) {
    if (s[i].sz < 2 || s[i+1].sz > 500) continue;
    // paredes com triângulos (exato do screensaver)
    spr.fillTriangle(s[i+1].x1, s[i+1].y1, s[i+1].x1, s[i+1].y2, s[i].x1, s[i].y2, s[i].drk);
    spr.fillTriangle(s[i+1].x1, s[i+1].y1, s[i+1].x1, s[i+1].y2, s[i].x1, s[i].y1, s[i].drk);
    spr.fillTriangle(s[i+1].x2, s[i+1].y1, s[i+1].x2, s[i+1].y2, s[i].x2, s[i].y2, s[i].drk);
    spr.fillTriangle(s[i+1].x2, s[i+1].y1, s[i+1].x2, s[i+1].y2, s[i].x2, s[i].y1, s[i].drk);
    spr.fillTriangle(s[i+1].x1, s[i+1].y1, s[i+1].x2, s[i+1].y1, s[i].x2, s[i].y1, s[i].drk);
    spr.fillTriangle(s[i+1].x1, s[i+1].y1, s[i+1].x2, s[i+1].y1, s[i].x1, s[i].y1, s[i].drk);
    spr.fillTriangle(s[i+1].x1, s[i+1].y2, s[i+1].x2, s[i+1].y2, s[i].x2, s[i].y2, s[i].drk);
    spr.fillTriangle(s[i+1].x1, s[i+1].y2, s[i+1].x2, s[i+1].y2, s[i].x1, s[i].y2, s[i].drk);
    // arestas luminosas
    spr.drawLine(s[i+1].x1, s[i+1].y1, s[i].x1, s[i].y1, s[i].col);
    spr.drawLine(s[i+1].x1, s[i+1].y2, s[i].x1, s[i].y2, s[i].col);
    spr.drawLine(s[i+1].x2, s[i+1].y1, s[i].x2, s[i].y1, s[i].col);
    spr.drawLine(s[i+1].x2, s[i+1].y2, s[i].x2, s[i].y2, s[i].col);
    spr.drawRect(s[i].x1, s[i].y1, s[i].sz, s[i].sz, s[i].col);
  }
  spr.drawRect(s[NS-1].x1, s[NS-1].y1, s[NS-1].sz, s[NS-1].sz, s[NS-1].col);
}

// ─────────────────────────────────────────────────────────────
//  Personagem — bonequinho humano animado (visto de costas, tipo porta de banheiro)
//  cx,cy = centro do quadril; r = escala (raio base)
//  alive  = vivo/morto
//  phase  = fase da animação de corrida (gameFrame * 0.28f)
//  jumping = true quando no ar
// ─────────────────────────────────────────────────────────────
static void drawCharacter(int cx, int cy, int r,
                          bool alive, float phase, bool jumping, float tAnim) {
  // ── Cores sincronizadas com o corredor ──────────────────
  uint16_t colBody, colHead, colOutline;
  if (alive) {
    uint16_t base = pCol(15, (int)(tAnim * 10));
    uint8_t  br = (base >> 11) << 3;
    uint8_t  bg = ((base >> 5) & 0x3F) << 2;
    uint8_t  bb = (base & 0x1F) << 3;
    colBody    = spr.color565(br, bg, bb); // Cor sólida para o corpo
    colHead    = colBody;
    colOutline = spr.color565(255, 255, 255); // contorno branco
  } else {
    colBody    = spr.color565(120, 10, 10);
    colHead    = colBody;
    colOutline = C_RED;
  }

  // ── Dimensões estilo placa de banheiro ──────────────────
  const int headR   = r - 1;             // raio da cabeça
  const int torsoW  = r * 2;             // largura dos ombros
  const int torsoH  = (int)(r * 1.8f);   // altura do tronco
  const int limbLen = r + 4;             // comprimento de braço/perna
  const int legOff  = r / 2;             // afastamento das pernas
  const int limbW   = 3;                 // espessura dos membros

  // Ancoras anatômicas (de cima para baixo)
  int neckY     = cy - torsoH;              // topo do tronco
  int headY     = neckY - headR - 2;        // centro da cabeça (flutuando levemente)
  int hipY      = cy;                       // quadril (base do tronco)
  int shoulderY = neckY;                    // ombros (no mesmo nível do topo do tronco)

  // ── Sombra no chão ──────────────────────────────────────
  uint16_t shadowCol = alive ? spr.color565(0, 30, 50) : spr.color565(40, 0, 0);
  spr.fillEllipse(cx, hipY + limbLen + 2, r + 2, 3, shadowCol);

  // ── Balanço dos membros (corrida) ────────────────────────
  float swing = alive ? sinf(phase) : 0.0f;

  int legLswingX, legLswingY, footLX, footLY;
  int legRswingX, legRswingY, footRX, footRY;

  if (jumping) {
    // pernas dobradas para trás/cima no pulo
    legLswingX = cx - legOff; legLswingY = hipY + limbLen / 2;
    footLX     = cx - legOff - 3; footLY = hipY + 2;
    legRswingX = cx + legOff; legRswingY = hipY + limbLen / 2;
    footRX     = cx + legOff + 3; footRY = hipY + 2;
  } else {
    // movimento de corrida
    float lSwing =  swing;
    float rSwing = -swing;
    legLswingX = cx - legOff + (int)(lSwing * 1.5f); legLswingY = hipY + limbLen / 2;
    footLX     = cx - legOff + (int)(lSwing * 4.0f); footLY     = hipY + limbLen;
    legRswingX = cx + legOff + (int)(rSwing * 1.5f); legRswingY = hipY + limbLen / 2;
    footRX     = cx + legOff + (int)(rSwing * 4.0f); footRY     = hipY + limbLen;
  }

  int armLelbowX, armLelbowY, handLX, handLY;
  int armRelbowX, armRelbowY, handRX, handRY;
  if (jumping) {
    // braços levantados no pulo
    armLelbowX = cx - torsoW/2 - 1; armLelbowY = shoulderY - 3;
    handLX     = cx - torsoW/2 - 2; handLY     = shoulderY - 8;
    armRelbowX = cx + torsoW/2 + 1; armRelbowY = shoulderY - 3;
    handRX     = cx + torsoW/2 + 2; handRY     = shoulderY - 8;
  } else if (!alive) {
    // braços caídos
    armLelbowX = cx - torsoW/2 - 2; armLelbowY = shoulderY + limbLen/2;
    handLX     = cx - torsoW/2 - 3; handLY     = shoulderY + limbLen;
    armRelbowX = cx + torsoW/2 + 2; armRelbowY = shoulderY + limbLen/2;
    handRX     = cx + torsoW/2 + 3; handRY     = shoulderY + limbLen;
  } else {
    float aLswing = -swing;
    float aRswing =  swing;
    armLelbowX = cx - torsoW/2 - 1 + (int)(aLswing * 1.5f); armLelbowY = shoulderY + limbLen/2;
    handLX     = cx - torsoW/2 - 1 + (int)(aLswing * 4.0f); handLY     = shoulderY + limbLen;
    armRelbowX = cx + torsoW/2 + 1 + (int)(aRswing * 1.5f); armRelbowY = shoulderY + limbLen/2;
    handRX     = cx + torsoW/2 + 1 + (int)(aRswing * 4.0f); handRY     = shoulderY + limbLen;
  }

  // ── Desenho Thick Lines Helper ───────────────────────────
  auto drawThickLine = [&](int x0, int y0, int x1, int y1, uint16_t col) {
    for (int i = 0; i < limbW; i++) {
      spr.drawLine(x0 + i, y0, x1 + i, y1, col);
    }
  };

  // ── Desenho Final (visto de costas) ──────────────────────

  // Pernas
  drawThickLine(cx - legOff - 1, hipY, legLswingX - 1, legLswingY, colBody);
  drawThickLine(legLswingX - 1, legLswingY, footLX - 1, footLY, colBody);
  drawThickLine(cx + legOff - 1, hipY, legRswingX - 1, legRswingY, colBody);
  drawThickLine(legRswingX - 1, legRswingY, footRX - 1, footRY, colBody);

  // Braços
  drawThickLine(cx - torsoW/2 - 1, shoulderY, armLelbowX - 1, armLelbowY, colBody);
  drawThickLine(armLelbowX - 1, armLelbowY, handLX - 1, handLY, colBody);
  drawThickLine(cx + torsoW/2 - 1, shoulderY, armRelbowX - 1, armRelbowY, colBody);
  drawThickLine(armRelbowX - 1, armRelbowY, handRX - 1, handRY, colBody);

  // Tronco (sólido e largo em cima)
  spr.fillRect(cx - torsoW / 2, neckY, torsoW, torsoH, colBody);
  
  // Arredondamento dos ombros
  spr.fillCircle(cx - torsoW / 2 + 1, neckY + 1, 2, colBody);
  spr.fillCircle(cx + torsoW / 2 - 2, neckY + 1, 2, colBody);

  // Cabeça (flutuando, sem rosto)
  spr.fillCircle(cx, headY, headR, colHead);
  
  // Highlight sutil para dar volume (opcional)
  if (alive) {
    spr.drawFastHLine(cx - torsoW/2 + 2, neckY, torsoW - 4, colOutline);
    spr.drawCircle(cx, headY, headR, colOutline);
  } else {
    // Marca de X grande nas costas na morte
    spr.drawLine(cx - 3, neckY + 2, cx + 3, neckY + 8, C_RED);
    spr.drawLine(cx + 3, neckY + 2, cx - 3, neckY + 8, C_RED);
  }
}

// ─────────────────────────────────────────────────────────────
//  Obstáculo — pilar 3D afixado no chão com perspectiva de lane
//  lane: -1 (esq), 0 (centro), 1 (dir)
//  z: profundidade [0.1 .. MAX_Z]
// ─────────────────────────────────────────────────────────────
static void drawObstacle(float z, int lane, float hScale, float t) {
  if (z <= 0.15f) return;
  
  // -- Frente do pilar --
  float scale = 140.0f / z;
  if (scale > 400) return;
  float halfW  = scale / 2.0f;
  int   curveF = (int)getCurveOffset(z, t);
  int   sx = CX + curveF + (int)(lane * halfW * 0.58f);
  int   syBottom = (int)(CY + halfW); 
  int   sz = (int)(scale * 0.22f); // Mais fino (0.22 ao inves de 0.26)
  if (sz < 2) return;
  int   h = (int)(sz * 2.5f * hScale);
  int   syTop = syBottom - h;

  // -- Costas do pilar (Profundidade simulada) --
  float zBack = z + 0.2f; // Profundidade muito menor (0.2 ao inves de 0.5)
  float scaleB = 140.0f / zBack;
  float halfWB = scaleB / 2.0f;
  int   curveB = (int)getCurveOffset(zBack, t);
  int   sxB = CX + curveB + (int)(lane * halfWB * 0.58f);
  int   syBottomB = (int)(CY + halfWB);
  int   szB = (int)(scaleB * 0.22f);
  int   hB = (int)(szB * 2.5f * hScale);
  int   syTopB = syBottomB - hB;

  int hue = (int)(t * 5 + lane * 40 + 180) % 360;
  uint16_t col   = pCol(12, hue);  // Claro (Topo)
  uint16_t dark  = pCol(6, hue);   // Médio (Frente)
  uint16_t vDark = pCol(2, hue);   // Escuro (Lateral)

  auto fillQuad = [&](int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, uint16_t c) {
    spr.fillTriangle(x1, y1, x2, y2, x3, y3, c);
    spr.fillTriangle(x1, y1, x3, y3, x4, y4, c);
  };

  // Pontos da face frontal: TL (Top-Left), TR, BR (Bottom-Right), BL
  int ftl_x = sx - sz, ftl_y = syTop;
  int ftr_x = sx + sz, ftr_y = syTop;
  int fbr_x = sx + sz, fbr_y = syBottom;
  int fbl_x = sx - sz, fbl_y = syBottom;

  // Pontos da face de trás
  int btl_x = sxB - szB, btl_y = syTopB;
  int btr_x = sxB + szB, btr_y = syTopB;
  int bbr_x = sxB + szB, bbr_y = syBottomB;
  int bbl_x = sxB - szB, bbl_y = syBottomB;

  // Desenha Face Superior (Top)
  fillQuad(btl_x, btl_y, btr_x, btr_y, ftr_x, ftr_y, ftl_x, ftl_y, col);
  
  // Desenha Face Lateral dependendo da posição (lane)
  if (lane < 0) {
    // Esquerda: vemos a face direita do obstáculo
    fillQuad(btr_x, btr_y, bbr_x, bbr_y, fbr_x, fbr_y, ftr_x, ftr_y, vDark);
  } else if (lane > 0) {
    // Direita: vemos a face esquerda do obstáculo
    fillQuad(btl_x, btl_y, bbl_x, bbl_y, fbl_x, fbl_y, ftl_x, ftl_y, vDark);
  }

  // Desenha Face Frontal
  spr.fillRect(ftl_x, ftl_y, sz * 2, h, dark);
}


// ─────────────────────────────────────────────────────────────
//  Pixel art "R4BB1T" (port de Splash.cpp)
// ─────────────────────────────────────────────────────────────
static const uint8_t GLYPH_R[7] = {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001};
static const uint8_t GLYPH_4[7] = {0b10001,0b10001,0b10001,0b11111,0b00001,0b00001,0b00001};
static const uint8_t GLYPH_B[7] = {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110};
static const uint8_t GLYPH_1[7] = {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110};
static const uint8_t GLYPH_T[7] = {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100};
static const uint8_t* const LOGO_GLYPHS[6] = {GLYPH_R,GLYPH_4,GLYPH_B,GLYPH_B,GLYPH_1,GLYPH_T};

static void drawLogo(int y0, int scale) {
  const int COLS = 5, ROWS = 7, GAP = 2;
  const int lw = COLS * scale;
  const int totalW = 6 * lw + 5 * GAP;
  int x0 = (SW - totalW) / 2;
  for (int li = 0; li < 6; li++) {
    int lx = x0 + li * (lw + GAP);
    for (int row = 0; row < ROWS; row++) {
      uint8_t bits = LOGO_GLYPHS[li][row];
      for (int col = 0; col < COLS; col++) {
        if ((bits >> (4 - col)) & 1) {
          spr.fillRect(lx + col * scale + 1, y0 + row * scale + 1, scale, scale, C_GOLD_DIM);
          spr.fillRect(lx + col * scale,     y0 + row * scale,     scale, scale, C_GOLD);
        }
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────
//  Texto pixel helper
// ─────────────────────────────────────────────────────────────
static void drawSmallText(const char* txt, int x, int y, uint16_t col) {
  spr.setTextColor(col);
  spr.setTextSize(1);
  spr.setCursor(x, y);
  spr.print(txt);
}

static void drawCenteredText(const char* txt, int y, uint16_t col) {
  int len = strlen(txt) * 6;
  spr.setTextColor(col);
  spr.setTextSize(1);
  spr.setCursor((SW - len) / 2, y);
  spr.print(txt);
}

static void drawCenteredText2(const char* txt, int y, uint16_t col) {
  int len = strlen(txt) * 12;
  spr.setTextColor(col);
  spr.setTextSize(2);
  spr.setCursor((SW - len) / 2, y);
  spr.print(txt);
}

// ─────────────────────────────────────────────────────────────
//  Estado da máquina
// ─────────────────────────────────────────────────────────────
enum GameState { ST_MENU, ST_PLAYING, ST_DEAD };
static GameState gState = ST_MENU;

enum Difficulty { DIFF_EASY = 0, DIFF_NORMAL, DIFF_HARD };
static Difficulty gDiff = DIFF_NORMAL;

// ─────────────────────────────────────────────────────────────
//  Jogador
// ─────────────────────────────────────────────────────────────
static const int   CHAR_R    = 5;   // raio do personagem px (menor)
static const int   FLOOR_Y   = 138; // Y dos pés do personagem, alinhado com o Z=1.2f do cenário
static const float GRAVITY   = 0.7f;
static const float JUMP_VEL  = -9.5f;

// Lanes: 0=esq, 1=centro, 2=dir → X na tela
static const int LANE_X[3] = { SW/2 - 32, SW/2, SW/2 + 32 };

static int   playerLane   = 1;
static float playerX      = SW / 2;
static float playerTargX  = SW / 2;
static float playerY      = FLOOR_Y;   // pés
static float velY         = 0;
static bool  onGround     = true;

// ─────────────────────────────────────────────────────────────
//  Score & timing
// ─────────────────────────────────────────────────────────────
static uint32_t score    = 0;
static uint32_t hiScore  = 0;
static uint32_t gameFrame = 0;
static float    speed    = 1.0f;

static float corridorOff = 0;
static float t_anim      = 0;   // contador de tempo para cores

// debounce
static unsigned long lastBtn = 0;
static bool   prevLeft  = false;
static bool   prevRight = false;
static bool   prevSel   = false;

// dead menu
static int deadSel = 0;
static uint32_t deadFrame = 0;

// menu
static float menuOff = 0;

// ─────────────────────────────────────────────────────────────
//  Obstáculos
// ─────────────────────────────────────────────────────────────
static const int MAX_OBS = 8;
struct Obstacle {
  float z;
  int   lane;   // 0,1,2
  float hScale; // Escala de altura (1.0 = base)
  bool  active;
};
static Obstacle obs[MAX_OBS];

static void obsInit() {
  for (int i = 0; i < MAX_OBS; i++) obs[i].active = false;
}
static void obsSpawn() {
  int diffScore = score + (gDiff * 30); 
  int pat = 0;
  int r = random(0, 100);
  
  // Decisão do padrão com base na pontuação/dificuldade
  // pat: 0=1 Alto, 1=1 Baixo, 2=2 Altos, 3=2 Baixos, 4=3 Baixos
  if (diffScore < 20) {
     if (r < 50) pat = 1;      // Muito 1 Baixo no começo
     else if (r < 80) pat = 0; // 1 Alto
     else pat = 4;             // 3 Baixos as vezes
  } else if (diffScore < 60) {
     if (r < 30) pat = 0;      
     else if (r < 50) pat = 1; 
     else if (r < 75) pat = 2; // Introduz 2 Altos (deixa 1 livre)
     else if (r < 85) pat = 3; 
     else pat = 4;             
  } else {
     if (r < 25) pat = 0;
     else if (r < 35) pat = 1;
     else if (r < 65) pat = 2; // Muitos de 2 Altos
     else if (r < 75) pat = 3; 
     else pat = 4;             
  }

  int numSlotsNeeded = 1;
  if (pat == 2 || pat == 3) numSlotsNeeded = 2;
  if (pat == 4) numSlotsNeeded = 3;

  int slots[3];
  int found = 0;
  for (int i = 0; i < MAX_OBS && found < numSlotsNeeded; i++) {
    if (!obs[i].active) slots[found++] = i;
  }
  if (found < numSlotsNeeded) return; // Tela cheia

  // Sorteia as vias
  int lanes[3] = {0, 1, 2};
  for (int i = 0; i < 3; i++) {
     int swapIdx = random(0, 3);
     int temp = lanes[i]; lanes[i] = lanes[swapIdx]; lanes[swapIdx] = temp;
  }

  float zSpawn = MAX_Z * 0.88f;

  for (int i = 0; i < numSlotsNeeded; i++) {
    obs[slots[i]].z = zSpawn;
    obs[slots[i]].lane = lanes[i];
    obs[slots[i]].active = true;
    
    if (pat == 0 || pat == 2) {
      obs[slots[i]].hScale = random(15, 25) / 10.0f; // Alto
    } else {
      obs[slots[i]].hScale = random(2, 5) / 10.0f;   // Baixo
    }
  }
}

// -- variáveis globais movidas para cima --

// ─────────────────────────────────────────────────────────────
//  Collision — projeta obstáculo e verifica sobreposição
// ─────────────────────────────────────────────────────────────
static bool collides(const Obstacle& o, float t) {
  float scale = 140.0f / o.z;
  float halfW  = scale / 2.0f;
  int curve = (int)getCurveOffset(o.z, t);
  int sx = CX + curve + (int)((o.lane - 1) * halfW * 0.58f);
  int bottomY = (int)(CY + halfW);
  int sz = (int)(scale * 0.22f); // Mais fino
  if (sz < 2) return false;

  int h = (int)(sz * 2.5f * o.hScale);
  int topY = bottomY - h;

  // Caixa do obstáculo (Hitbox ligeiramente menor para perdoar)
  int ox1 = sx - sz + 2, oy1 = topY + 2;
  int ox2 = sx + sz - 2, oy2 = bottomY - 2;

  // Caixa do personagem (estimativa simplificada)
  int px1 = (int)playerX - CHAR_R + 2;
  int py1 = (int)playerY - CHAR_R * 3;
  int px2 = (int)playerX + CHAR_R - 2;
  int py2 = (int)playerY;

  return (px1 < ox2 && px2 > ox1 && py1 < oy2 && py2 > oy1);
}

// ─────────────────────────────────────────────────────────────
//  Reset do jogo
// ─────────────────────────────────────────────────────────────
static void resetGame() {
  playerLane  = 1;
  playerX     = LANE_X[1];
  playerTargX = LANE_X[1];
  playerY     = FLOOR_Y;
  velY        = 0;
  onGround    = true;
  obsInit();
  score      = 0;
  gameFrame  = 0;
  speed      = 1.0f;
  corridorOff = 0;
  t_anim      = 0;
  deadSel     = 0;
  deadFrame   = 0;
}

// ─────────────────────────────────────────────────────────────
//  Leitura de botões com debounce (edge-detect)
// ─────────────────────────────────────────────────────────────
static bool edgeLeft  = false;
static bool edgeRight = false;
static bool edgeSel   = false;

static void readButtons() {
  bool curLeft  = (digitalRead(BUTTON_LEFT)   == LOW);
  bool curRight = (digitalRead(BUTTON_RIGHT)  == LOW);
  bool curSel   = (digitalRead(BUTTON_SELECT) == LOW);

  unsigned long now = millis();
  if (now - lastBtn > DEBOUNCE_MS) {
    edgeLeft  = (curLeft  && !prevLeft);
    edgeRight = (curRight && !prevRight);
    edgeSel   = (curSel   && !prevSel);
    if (edgeLeft || edgeRight || edgeSel) lastBtn = now;
  } else {
    edgeLeft = edgeRight = edgeSel = false;
  }
  prevLeft  = curLeft;
  prevRight = curRight;
  prevSel   = curSel;
}

// ─────────────────────────────────────────────────────────────
//  HUD in-sprite
// ─────────────────────────────────────────────────────────────
static void drawHUD() {
  // barra superior
  spr.fillRect(0, 0, SW, 12, C_GREY_DARK);
  spr.drawFastHLine(0, 12, SW, C_GOLD);

  char buf[32];
  snprintf(buf, sizeof(buf), "%06lu", (unsigned long)score);
  spr.setTextColor(C_GOLD);
  spr.setTextSize(1);
  spr.setCursor(3, 3);
  spr.print(buf);

  spr.setCursor(SW - 7 * 6 - 2, 3);
  snprintf(buf, sizeof(buf), "HI%05lu", (unsigned long)hiScore);
  spr.print(buf);

  // barra inferior com indicador de speed
  spr.fillRect(0, SH - 10, SW, 10, C_GREY_DARK);
  spr.drawFastHLine(0, SH - 10, SW, C_GREY);
  int spd_bar = (int)((speed - 1.0f) / 3.0f * 80.0f);
  if (spd_bar > 80) spd_bar = 80;
  spr.fillRect(24, SH - 7, 80, 5, C_BLACK);
  uint16_t spCol = spr.color565(
    (uint8_t)(spd_bar * 3),
    (uint8_t)((80 - spd_bar) * 3),
    0);
  if (spd_bar > 0) spr.fillRect(24, SH - 7, spd_bar, 5, spCol);
  spr.drawRect(24, SH - 7, 80, 5, C_GREY);
  spr.setTextColor(C_GREY);
  spr.setTextSize(1);
  spr.setCursor(3, SH - 8);
  spr.print("SPD");
  spr.setCursor(SW - 18, SH - 8);
  snprintf(buf, 8, "x%.1f", speed);
  spr.print(buf);
}

// ─────────────────────────────────────────────────────────────
//  Telas de renderização
// ─────────────────────────────────────────────────────────────
static void renderMenu() {
  spr.fillSprite(TFT_BLACK);
  menuOff = fmodf(menuOff + 0.015f, SPACING);
  drawCorridor(menuOff, t_anim);

  // overlay escuro radial simulado
  for (int r2 = 60; r2 >= 0; r2 -= 10) {
    uint8_t a = (uint8_t)(200 - r2 * 2);
    spr.fillCircle(CX, CY, r2, spr.color565(0, 0, 0));
  }
  spr.fillRect(0, 0, SW, SH, spr.color565(0, 0, 0)); // overlay

  // re-desenha corridora leve por cima
  drawCorridor(menuOff, t_anim);

  // Logo R4BB1T pixel art (Opcional, podemos manter ou subir um pouco)
  drawLogo(30, 2);

  // Fundo escuro (painel) para a área de texto do menu para melhor legibilidade
  spr.fillRect(10, 50, SW - 20, 72, spr.color565(15, 15, 20));
  spr.drawRect(10, 50, SW - 20, 72, C_CYAN);

  // Subtítulo / Título
  drawCenteredText("CORRIDOR RUNNER", 56, C_CYAN);
  spr.drawFastHLine(15, 66, SW - 30, C_CYAN);

  // Instruções compactas (Maior contraste)
  spr.setTextSize(1);
  drawCenteredText("< > MOVER/NIVEL", 74, C_WHITE);
  drawCenteredText("o PULAR/INICIAR", 84, C_WHITE);
  
  spr.drawFastHLine(15, 96, SW - 30, C_CYAN);

  // Seleção de dificuldade
  const char* diffStr = "";
  uint16_t diffCol = C_WHITE;
  if (gDiff == DIFF_EASY)   { diffStr = "< FACIL >"; diffCol = C_GREEN; }
  if (gDiff == DIFF_NORMAL) { diffStr = "< NORMAL >"; diffCol = C_GOLD; }
  if (gDiff == DIFF_HARD)   { diffStr = "< DIFICIL >"; diffCol = C_RED; }
  drawCenteredText(diffStr, 104, diffCol);

  // Press any button — pisca FORA do painel
  uint16_t pulse = ((millis() / 500) % 2) ? C_GOLD : C_GOLD_DIM;
  drawCenteredText("APERTE o PARA JOGAR", 132, pulse);

  // Mini personagem no fundo — animado no menu
  float menuPhase = millis() * 0.008f;
  drawCharacter(CX, 142, CHAR_R - 1, true, menuPhase, false, t_anim);

  spr.pushSprite(0, 0);

  if (!newFrameReady) {
    memcpy(mirrorBuffer, spr.getPointer(), SW * SH * 2);
    newFrameReady = true;
  }
}

static void renderGame() {
  spr.fillSprite(TFT_BLACK);
  drawCorridor(corridorOff, t_anim);

  // Coleta obstáculos ativos
  Obstacle* activeObs[MAX_OBS];
  int count = 0;
  for (int i = 0; i < MAX_OBS; i++) {
    if (obs[i].active) activeObs[count++] = &obs[i];
  }
  
  // Ordena por z (decrescente, do mais fundo pro mais raso)
  for (int i = 0; i < count - 1; i++) {
    for (int j = 0; j < count - i - 1; j++) {
      if (activeObs[j]->z < activeObs[j+1]->z) {
        Obstacle* temp = activeObs[j];
        activeObs[j] = activeObs[j+1];
        activeObs[j+1] = temp;
      }
    }
  }

  const float PLAYER_Z = 1.2f; // Posição virtual do jogador em Z
  int idx = 0;

  // 1. Desenha obstáculos que estão atrás do jogador
  for (; idx < count; idx++) {
    if (activeObs[idx]->z <= PLAYER_Z) break;
    drawObstacle(activeObs[idx]->z, activeObs[idx]->lane - 1, activeObs[idx]->hScale, t_anim);
  }

  // 2. Desenha o jogador (oculta o que está atrás e é sobreposto pelo que vem depois)
  int charBodyY = (int)playerY - CHAR_R;
  float runPhase = gameFrame * 0.28f * speed;
  drawCharacter((int)playerX, charBodyY, CHAR_R, true, runPhase, !onGround, t_anim);

  // 3. Desenha obstáculos que estão na frente do jogador
  for (; idx < count; idx++) {
    drawObstacle(activeObs[idx]->z, activeObs[idx]->lane - 1, activeObs[idx]->hScale, t_anim);
  }

  drawHUD();
  spr.pushSprite(0, 0);

  // Copia pro buffer do espelhamento
  if (!newFrameReady) {
    memcpy(mirrorBuffer, spr.getPointer(), SW * SH * 2);
    newFrameReady = true;
  }
}

static void renderDead() {
  spr.fillSprite(TFT_BLACK);
  // corredor desacelerado
  menuOff = fmodf(menuOff + 0.003f, SPACING);
  drawCorridor(menuOff, t_anim);

  // Painel de fundo para o GAME OVER e SCORE

  spr.fillRect(10, 15, SW - 20, 72, spr.color565(20, 0, 0));
  spr.drawRect(10, 15, SW - 20, 72, C_RED);

  // GAME OVER (Texto tamanho 2)
  spr.setTextColor(C_RED);
  spr.setTextSize(2);
  int golen = 4 * 12; // 4 letras ("GAME" e "OVER") * 12 px de largura cada
  spr.setCursor((SW - golen) / 2, 22);
  spr.print("GAME");
  spr.setCursor((SW - golen) / 2, 40);
  spr.print("OVER");

  // Score
  spr.setTextSize(1);
  char buf[32];
  snprintf(buf, sizeof(buf), "SCORE: %lu", (unsigned long)score);
  drawCenteredText(buf, 62, C_GOLD);

  if (score >= hiScore && score > 0) {
    drawCenteredText("** RECORDE! **", 74, C_CYAN);
  } else {
    snprintf(buf, sizeof(buf), "MELHOR: %lu", (unsigned long)hiScore);
    drawCenteredText(buf, 74, C_WHITE);
  }

  // personagem morto — braços caídos, sem animação
  drawCharacter(CX, 95, CHAR_R, false, 0.0f, false, t_anim);

  // opções — pisca a selecionada
  bool blink = ((millis() / 350) % 2) == 0;

  // TENTAR DE NOVO
  {
    bool sel = (deadSel == 0);
    uint16_t bg  = sel ? C_GOLD_SEL : TFT_BLACK;
    uint16_t brd = sel ? C_GOLD     : C_GREY;
    uint16_t txt = sel ? (blink ? C_GOLD : C_GOLD_DIM) : C_GREY;
    spr.fillRect(10, 110, SW - 20, 16, bg);
    spr.drawRect(10, 110, SW - 20, 16, brd);
    if (sel) spr.fillRect(10, 110, 3, 16, C_GOLD);
    spr.setTextColor(txt); spr.setTextSize(1);
    spr.setCursor(20, 115);
    spr.print("TENTAR NOVAMENTE");
  }

  // SAIR
  {
    bool sel = (deadSel == 1);
    uint16_t bg  = sel ? C_GOLD_SEL : TFT_BLACK;
    uint16_t brd = sel ? C_GOLD     : C_GREY;
    uint16_t txt = sel ? (blink ? C_GOLD : C_GOLD_DIM) : C_GREY;
    spr.fillRect(10, 132, SW - 20, 16, bg);
    spr.drawRect(10, 132, SW - 20, 16, brd);
    if (sel) spr.fillRect(10, 132, 3, 16, C_GOLD);
    spr.setTextColor(txt); spr.setTextSize(1);
    spr.setCursor(20, 137);
    spr.print("MENU INICIAL");
  }

  // instrução de navegação
  drawCenteredText("< > NAVEGAR  o CONFIRMAR", 152, C_GREY_DARK);

  spr.pushSprite(0, 0);

  if (!newFrameReady) {
    memcpy(mirrorBuffer, spr.getPointer(), SW * SH * 2);
    newFrameReady = true;
  }
}

// ─────────────────────────────────────────────────────────────
//  Update de jogo (lógica de um frame)
// ─────────────────────────────────────────────────────────────
static void updateGame() {
  gameFrame++;
  score = gameFrame / 5;
  if (score > hiScore) hiScore = score;

  // perfis de dificuldade
  float baseSpeed = 1.8f;
  float maxSpeed  = 5.0f;
  float growDiv   = 1500.0f;
  
  if (gDiff == DIFF_EASY) {
      baseSpeed = 1.3f;
      maxSpeed  = 3.5f;
      growDiv   = 2500.0f;
  } else if (gDiff == DIFF_HARD) {
      baseSpeed = 2.4f;
      maxSpeed  = 6.5f;
      growDiv   = 1000.0f;
  }
  
  speed = baseSpeed + gameFrame / growDiv;
  if (speed > maxSpeed) speed = maxSpeed;

  // scroll do corredor
  corridorOff = fmodf(corridorOff + 0.02f * speed, SPACING);
  t_anim += 0.04f * speed;

  // spawn de obstáculos influenciado pela dificuldade
  float spawnMult = 1.0f;
  if (gDiff == DIFF_EASY) spawnMult = 1.4f; // Demora mais (menos denso)
  if (gDiff == DIFF_HARD) spawnMult = 0.8f; // Demora menos (mais denso)

  int spawnInterval = (int)((90.0f / speed) * spawnMult);
  if (spawnInterval < 25) spawnInterval = 25;
  if (gameFrame % spawnInterval == 0) {
    obsSpawn();
  }

  // mover obstáculos em direção ao jogador
  for (int i = 0; i < MAX_OBS; i++) {
    if (!obs[i].active) continue;
    obs[i].z -= 0.07f * speed;
    if (obs[i].z < 0.1f) { obs[i].active = false; continue; }

    // colisão — checa apenas quando obstáculo cruza a exata linha do jogador (Z=1.2)
    if (obs[i].z <= 1.5f && obs[i].z >= 0.9f) {
      // apenas checa a lane certa
      if (obs[i].lane == playerLane) {
        if (collides(obs[i], t_anim)) {
          if (score > hiScore) hiScore = score;
          gState = ST_DEAD;
          return;
        }
      }
    }
  }

  // movimento horizontal
  if (edgeLeft && playerLane > 0) playerLane--;
  if (edgeRight && playerLane < 2) playerLane++;
  playerTargX = LANE_X[playerLane] + getCurveOffset(1.2f, t_anim); // acompanha a curva!
  playerX += (playerTargX - playerX) * 0.3f;

  // pulo
  if (edgeSel && onGround) {
    velY     = JUMP_VEL;
    onGround = false;
  }

  // física vertical
  if (!onGround) {
    velY    += GRAVITY;
    playerY += velY;
    if (playerY >= FLOOR_Y) {
      playerY  = FLOOR_Y;
      velY     = 0;
      onGround = true;
    }
  }
}

// ─────────────────────────────────────────────────────────────
//  Tarefa Assíncrona para Espelhamento (Core 0) com Double Buffer
// ─────────────────────────────────────────────────────────────


void mirrorTaskCode(void * parameter) {
  for(;;) {
    if (Serial.available()) {
      if (Serial.read() == 'F') {
        while(Serial.available()) Serial.read(); // limpa sujeira
        
        // Aguarda um frame inteirinho e limpo ser copiado pelo Core 1
        while (!newFrameReady) {
          vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        uint8_t header[4] = {0xAA, 0x55, (uint8_t)SW, (uint8_t)SH};
        Serial.write(header, 4);

        // FATIAMENTO DE CARGA (Chunking) para não engasgar o CH340 USB
        int chunkSize = 10240; // 10KB por vez
        int totalSize = SW * SH * 2;
        for (int i = 0; i < totalSize; i += chunkSize) {
          int toSend = (totalSize - i > chunkSize) ? chunkSize : (totalSize - i);
          Serial.write(mirrorBuffer + i, toSend);
          vTaskDelay(pdMS_TO_TICKS(2)); // Respiro de 2ms para o cabo USB transferir ao PC
        }
        
        newFrameReady = false; // Libera para receber o próximo frame
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2)); // Cede tempo para o FreeRTOS/Watchdog
  }
}

// ─────────────────────────────────────────────────────────────
//  Setup e Loop
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(2000000); // Tentar 2M dnv agora que o Python esta super rapido

  // Botões com pull-up interno
  pinMode(BUTTON_LEFT,   INPUT_PULLUP);
  pinMode(BUTTON_RIGHT,  INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);

  // Backlight ON
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Display
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Sprite como framebuffer
  spr.createSprite(SW, SH);
  spr.setSwapBytes(false);

  resetGame();
  gState = ST_MENU;

  // Inicia a tarefa de espelhamento no Core 0 (evita travar o jogo no Core 1)
  xTaskCreatePinnedToCore(
    mirrorTaskCode,
    "MirrorTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {

  readButtons();
  t_anim += 0.02f;

  switch (gState) {

    // ── MENU ────────────────────────────────────────────────
    case ST_MENU:
      renderMenu();
      if (edgeLeft) {
        if (gDiff > 0) gDiff = (Difficulty)(gDiff - 1);
      }
      if (edgeRight) {
        if (gDiff < 2) gDiff = (Difficulty)(gDiff + 1);
      }
      if (edgeSel) {
        resetGame();
        gState = ST_PLAYING;
      }
      break;

    // ── JOGANDO ─────────────────────────────────────────────
    case ST_PLAYING:
      updateGame();
      renderGame();
      break;

    // ── MORREU ──────────────────────────────────────────────
    case ST_DEAD:
      deadFrame++;
      renderDead();
      if (deadFrame > 50) {
        if (edgeLeft  || edgeRight) {
          deadSel ^= 1;  // alterna entre 0 e 1
        }
        if (edgeSel) {
          if (deadSel == 0) {
            resetGame();
            gState = ST_PLAYING;
          } else {
            resetGame();
            gState = ST_MENU;
          }
        }
      }
      break;
  }

  // Taxa de frame alvo ~30 fps
  delay(8);
}
