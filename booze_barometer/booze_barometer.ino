/*
 * Booze Barometer - Arduino Nano
 *
 * Matches board schematic (XH connectors + J1):
 *
 *   Inputs (J1, NO switch to GND, 100nF C1-C4 on board):
 *     J1-2 Start -> A1
 *     J1-3 Fail  -> A2   (wire / ring sense)
 *     J1-4 Win   -> A3   (end of path)
 *     J1-5 Coin  -> A0
 *     J1-1 GND
 *
 *   Outputs (XH 1=5V, 2=signal, 3=GND):
 *     FRONT  D2  WS2812 data
 *     BACK   D3  WS2812 data
 *     RELAY  D6  relay MODULE (not bare coil)
 *     SPARE  D7
 *
 *   Board LEDs:
 *     POWER    always on (5V)
 *     ATTRACT  D4
 *     GAME     D5
 *
 * Library: Adafruit NeoPixel
 *   Sketch -> Include Library -> Manage Libraries -> "Adafruit NeoPixel"
 */

#include <Adafruit_NeoPixel.h>

// ---- pin map (schematic) ----
const byte PIN_COIN    = A0;
const byte PIN_START   = A1;
const byte PIN_FAIL    = A2;
const byte PIN_WIN     = A3;

const byte PIN_FRONT   = 2;   // WS2812 FRONT
const byte PIN_BACK    = 3;   // WS2812 BACK
const byte PIN_ATTRACT = 4;
const byte PIN_GAME    = 5;
const byte PIN_RELAY   = 6;
const byte PIN_SPARE   = 7;

// ---- strip length (edit to match your WS2812 strips) ----
const int NUM_FRONT = 30;
const int NUM_BACK  = 30;

Adafruit_NeoPixel front(NUM_FRONT, PIN_FRONT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel back (NUM_BACK,  PIN_BACK,  NEO_GRB + NEO_KHZ800);

const unsigned long DEBOUNCE_MS    = 40;     // plus 100nF on board
const unsigned long FAIL_HOLD_MS   = 1200;
const unsigned long WIN_HOLD_MS    = 2500;
const unsigned long RELAY_PULSE_MS = 250;

enum State : byte { IDLE, ARMED, PLAYING, FAIL, WIN };
State state = IDLE;

struct Button {
  byte pin;
  bool stable;
  bool lastRaw;
  unsigned long lastChange;
};

Button btnCoin  = { PIN_COIN,  HIGH, HIGH, 0 };
Button btnStart = { PIN_START, HIGH, HIGH, 0 };
Button btnFail  = { PIN_FAIL,  HIGH, HIGH, 0 };
Button btnWin   = { PIN_WIN,   HIGH, HIGH, 0 };

unsigned long phaseStart = 0;
unsigned long animTick   = 0;
uint16_t animStep        = 0;

bool updateButton(Button &b) {
  bool raw = digitalRead(b.pin);
  unsigned long now = millis();
  if (raw != b.lastRaw) {
    b.lastChange = now;
    b.lastRaw = raw;
  }
  if ((now - b.lastChange) >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    return true;
  }
  return false;
}

bool pressed(const Button &b) { return b.stable == LOW; }

bool justPressed(Button &b) {
  return updateButton(b) && pressed(b);
}

void setBoardLeds(bool attract, bool game) {
  digitalWrite(PIN_ATTRACT, attract ? HIGH : LOW);
  digitalWrite(PIN_GAME,    game    ? HIGH : LOW);
}

void relay(bool on) {
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
}

void fillStrip(Adafruit_NeoPixel &s, uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < s.numPixels(); i++) {
    s.setPixelColor(i, s.Color(r, g, b));
  }
  s.show();
}

void bothSolid(uint8_t r, uint8_t g, uint8_t b) {
  fillStrip(front, r, g, b);
  fillStrip(back,  r, g, b);
}

void animAttract() {
  unsigned long now = millis();
  if (now - animTick < 40) return;
  animTick = now;
  animStep++;

  for (int i = 0; i < NUM_FRONT; i++) {
    uint8_t v = ((i + animStep) % 12 == 0) ? 80 : 8;
    front.setPixelColor(i, front.Color(v, v / 3, 0));
  }
  for (int i = 0; i < NUM_BACK; i++) {
    uint8_t v = ((i + animStep + 6) % 12 == 0) ? 80 : 8;
    back.setPixelColor(i, back.Color(v, v / 3, 0));
  }
  front.show();
  back.show();
  setBoardLeds((now / 500) % 2 == 0, false);
}

void animArmed() {
  unsigned long now = millis();
  if (now - animTick < 80) return;
  animTick = now;
  animStep++;

  bool on = (animStep % 2) == 0;
  bothSolid(0, on ? 40 : 0, on ? 80 : 0);  // blue pulse
  setBoardLeds(on, false);
}

void animPlaying() {
  unsigned long now = millis();
  if (now - animTick < 30) return;
  animTick = now;
  animStep++;

  if (NUM_FRONT > 0) {
    for (int i = 0; i < NUM_FRONT; i++) {
      uint8_t v = (i == (int)(animStep % NUM_FRONT)) ? 120 : 15;
      front.setPixelColor(i, front.Color(0, v, v / 4));
    }
    front.show();
  }
  if (NUM_BACK > 0) {
    for (int i = 0; i < NUM_BACK; i++) {
      int idx = NUM_BACK - 1 - (int)(animStep % NUM_BACK);
      uint8_t v = (i == idx) ? 120 : 15;
      back.setPixelColor(i, back.Color(0, v, v / 4));
    }
    back.show();
  }
  setBoardLeds(false, true);
}

void animFail() {
  unsigned long now = millis();
  bool on = ((now - phaseStart) / 100) % 2 == 0;
  bothSolid(on ? 150 : 0, 0, 0);
  setBoardLeds(false, on);
}

void animWin() {
  unsigned long now = millis();
  if (now - animTick < 50) return;
  animTick = now;
  animStep++;

  for (int i = 0; i < NUM_FRONT; i++) {
    uint8_t hue = (animStep * 8 + i * 8) & 0xFF;
    front.setPixelColor(i, front.ColorHSV((uint16_t)hue << 8, 255, 120));
  }
  for (int i = 0; i < NUM_BACK; i++) {
    uint8_t hue = (animStep * 8 + i * 8 + 40) & 0xFF;
    back.setPixelColor(i, back.ColorHSV((uint16_t)hue << 8, 255, 120));
  }
  front.show();
  back.show();
  setBoardLeds(true, true);
}

void enterIdle() {
  state = IDLE;
  relay(false);
  bothSolid(0, 0, 0);
  setBoardLeds(false, false);
  animStep = 0;
  animTick = 0;
}

void enterArmed() {
  state = ARMED;
  relay(false);
  animStep = 0;
  animTick = 0;
}

void enterPlaying() {
  state = PLAYING;
  relay(true);  // active while game runs (relay module, active HIGH)
  // ignore fail briefly so seating the ring does not instant-fail
  btnFail.stable = HIGH;
  btnFail.lastRaw = digitalRead(PIN_FAIL);
  btnFail.lastChange = millis();
  animStep = 0;
  animTick = 0;
  setBoardLeds(false, true);
}

void enterFail() {
  state = FAIL;
  phaseStart = millis();
  relay(true);  // short pulse; cleared in FAIL state
  animStep = 0;
}

void enterWin() {
  state = WIN;
  phaseStart = millis();
  relay(false);
  animStep = 0;
  animTick = 0;
}

void setup() {
  pinMode(PIN_COIN,  INPUT_PULLUP);
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_FAIL,  INPUT_PULLUP);
  pinMode(PIN_WIN,   INPUT_PULLUP);

  pinMode(PIN_ATTRACT, OUTPUT);
  pinMode(PIN_GAME,    OUTPUT);
  pinMode(PIN_RELAY,   OUTPUT);
  pinMode(PIN_SPARE,   OUTPUT);

  relay(false);
  digitalWrite(PIN_SPARE, LOW);
  setBoardLeds(false, false);

  front.begin();
  back.begin();
  front.setBrightness(80);
  back.setBrightness(80);
  front.clear();
  back.clear();
  front.show();
  back.show();

  enterIdle();
}

void loop() {
  switch (state) {
    case IDLE:
      animAttract();
      if (justPressed(btnCoin)) {
        enterArmed();
      } else if (justPressed(btnStart)) {
        // free-play / bench test (no coin)
        enterArmed();
        enterPlaying();
      }
      break;

    case ARMED:
      animArmed();
      updateButton(btnCoin);
      if (justPressed(btnStart)) {
        enterPlaying();
      }
      break;

    case PLAYING:
      animPlaying();
      updateButton(btnStart);
      updateButton(btnFail);
      if (pressed(btnFail)) {
        enterFail();
      } else if (justPressed(btnWin)) {
        enterWin();
      }
      break;

    case FAIL:
      animFail();
      if (millis() - phaseStart >= RELAY_PULSE_MS) {
        relay(false);
      }
      if (millis() - phaseStart >= FAIL_HOLD_MS) {
        enterIdle();
      }
      break;

    case WIN:
      animWin();
      if (millis() - phaseStart >= WIN_HOLD_MS) {
        enterIdle();
      }
      break;
  }
}
