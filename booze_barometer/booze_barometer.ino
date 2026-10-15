/*
 * Booze Barometer - Arduino Nano
 *
 * Flow:
 *   ATTRACT  --coin pulse A0-->  WAIT_START  --start A1-->  PLAYING
 *   PLAYING   --win A3-->  WIN (5s lightshow + relay)
 *   PLAYING   --fail A2--> FAIL (short lightshow + relay)
 *   WIN / FAIL --> ATTRACT
 *
 * Pins (board schematic):
 *   A0 Coin    A1 Start    A2 Fail    A3 Win
 *   A4 Dip4    A5 Dip3     A6 Dip2    A7 Dip1   (DIP to GND when ON)
 *   D2 FRONT WS2812 (via 330R)   D3 BACK WS2812 (via 330R)
 *   D4 Attract LED               D5 Game LED
 *   D6 Relay (via 330R)          D7 Spare (via 330R)
 *
 * Note: Nano A6/A7 are analog-only (no INPUT_PULLUP). Read with analogRead.
 *       If Dip1/Dip2 float when OFF, add 10k pull-ups to 5V on the board.
 *
 * Library: Adafruit NeoPixel
 */

#include <Adafruit_NeoPixel.h>

// ---- pins ----
const byte PIN_COIN    = A0;
const byte PIN_START   = A1;
const byte PIN_FAIL    = A2;
const byte PIN_WIN     = A3;

const byte PIN_DIP4    = A4;  // digital OK
const byte PIN_DIP3    = A5;  // digital OK
const byte PIN_DIP2    = A6;  // analog-only on Nano
const byte PIN_DIP1    = A7;  // analog-only on Nano

const byte PIN_FRONT   = 2;
const byte PIN_BACK    = 3;
const byte PIN_ATTRACT = 4;
const byte PIN_GAME    = 5;
const byte PIN_RELAY   = 6;
const byte PIN_SPARE   = 7;

// ---- strips (edit to match hardware) ----
const int NUM_FRONT = 30;
const int NUM_BACK  = 30;

Adafruit_NeoPixel front(NUM_FRONT, PIN_FRONT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel back (NUM_BACK,  PIN_BACK,  NEO_GRB + NEO_KHZ800);

const unsigned long DEBOUNCE_MS  = 40;
const unsigned long WIN_HOLD_MS  = 5000;
const unsigned long FAIL_HOLD_MS = 1500;
const unsigned long RELAY_ON_MS  = 300;   // pulse length in WIN/FAIL

enum State : byte { ATTRACT, WAIT_START, PLAYING, WIN, FAIL };
State state = ATTRACT;

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

int coinsNeeded  = 1;   // from DIP (stub)
int attemptsLeft = 1;   // from DIP (stub)
int coinsSeen    = 0;

// ---- debounce helpers (active LOW) ----
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

// DIP ON = switch to GND = true
bool dipDigitalOn(byte pin) {
  return digitalRead(pin) == LOW;
}

bool dipAnalogOn(byte pin) {
  // No pull-up on A6/A7: assume external pull-up or treat low as ON
  return analogRead(pin) < 400;
}

void readDips() {
  bool d1 = dipAnalogOn(PIN_DIP1);
  bool d2 = dipAnalogOn(PIN_DIP2);
  bool d3 = dipDigitalOn(PIN_DIP3);
  bool d4 = dipDigitalOn(PIN_DIP4);

  // Stub mapping (change when you decide options):
  //   Dip1+Dip2 = coins required (1..4)
  //   Dip3      = extra attempt if ON
  //   Dip4      = reserved
  int coinCode = (d1 ? 1 : 0) | (d2 ? 2 : 0);
  coinsNeeded = coinCode + 1;          // 1..4
  attemptsLeft = d3 ? 2 : 1;
  (void)d4;
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

// ---- lighting modes (patterns can be swapped later) ----
void animAttract() {
  unsigned long now = millis();
  if (now - animTick < 40) return;
  animTick = now;
  animStep++;

  for (int i = 0; i < NUM_FRONT; i++) {
    uint8_t v = ((i + animStep) % 12 == 0) ? 80 : 8;
    front.setPixelColor(i, front.Color(v, v / 3, 0));  // amber
  }
  for (int i = 0; i < NUM_BACK; i++) {
    uint8_t v = ((i + animStep + 6) % 12 == 0) ? 80 : 8;
    back.setPixelColor(i, back.Color(v, v / 3, 0));
  }
  front.show();
  back.show();
  setBoardLeds((now / 500) % 2 == 0, false);  // Attract blinks, Game off
}

void animWaitStart() {
  // Game-mode lighting while waiting for Start
  unsigned long now = millis();
  if (now - animTick < 50) return;
  animTick = now;
  animStep++;

  for (int i = 0; i < NUM_FRONT; i++) {
    uint8_t v = 20 + ((i + animStep) % 8 == 0 ? 60 : 0);
    front.setPixelColor(i, front.Color(0, v, v / 3));
  }
  for (int i = 0; i < NUM_BACK; i++) {
    uint8_t v = 20 + ((i + animStep + 4) % 8 == 0 ? 60 : 0);
    back.setPixelColor(i, back.Color(0, v, v / 3));
  }
  front.show();
  back.show();
  setBoardLeds(false, true);  // Attract OFF, Game ON
}

void animPlaying() {
  unsigned long now = millis();
  if (now - animTick < 30) return;
  animTick = now;
  animStep++;

  for (int i = 0; i < NUM_FRONT; i++) {
    uint8_t v = (NUM_FRONT > 0 && i == (int)(animStep % NUM_FRONT)) ? 120 : 15;
    front.setPixelColor(i, front.Color(0, v, v / 4));
  }
  for (int i = 0; i < NUM_BACK; i++) {
    int idx = NUM_BACK > 0 ? (NUM_BACK - 1 - (int)(animStep % NUM_BACK)) : 0;
    uint8_t v = (i == idx) ? 120 : 15;
    back.setPixelColor(i, back.Color(0, v, v / 4));
  }
  front.show();
  back.show();
  setBoardLeds(false, true);
}

void animWin() {
  // Placeholder lightshow — swap later
  unsigned long now = millis();
  if (now - animTick < 40) return;
  animTick = now;
  animStep++;

  for (int i = 0; i < NUM_FRONT; i++) {
    uint8_t hue = (animStep * 8 + i * 8) & 0xFF;
    front.setPixelColor(i, front.ColorHSV((uint16_t)hue << 8, 255, 140));
  }
  for (int i = 0; i < NUM_BACK; i++) {
    uint8_t hue = (animStep * 8 + i * 8 + 40) & 0xFF;
    back.setPixelColor(i, back.ColorHSV((uint16_t)hue << 8, 255, 140));
  }
  front.show();
  back.show();
  setBoardLeds(true, true);
}

void animFail() {
  // Placeholder fail pattern — swap later
  bool on = ((millis() - phaseStart) / 100) % 2 == 0;
  bothSolid(on ? 160 : 0, 0, 0);
  setBoardLeds(false, on);
}

// ---- state entry ----
void enterAttract() {
  state = ATTRACT;
  coinsSeen = 0;
  relay(false);
  animStep = 0;
  animTick = 0;
  readDips();
}

void enterWaitStart() {
  state = WAIT_START;
  relay(false);
  setBoardLeds(false, true);  // Attract off, Game on
  animStep = 0;
  animTick = 0;
}

void enterPlaying() {
  state = PLAYING;
  relay(false);
  // clear fail so seating the ring does not instant-fail
  btnFail.stable = HIGH;
  btnFail.lastRaw = digitalRead(PIN_FAIL);
  btnFail.lastChange = millis();
  btnWin.stable = HIGH;
  btnWin.lastRaw = digitalRead(PIN_WIN);
  btnWin.lastChange = millis();
  animStep = 0;
  animTick = 0;
  setBoardLeds(false, true);
}

void enterWin() {
  state = WIN;
  phaseStart = millis();
  relay(true);
  animStep = 0;
  animTick = 0;
}

void enterFail() {
  state = FAIL;
  phaseStart = millis();
  relay(true);
  animStep = 0;
  animTick = 0;
}

void setup() {
  pinMode(PIN_COIN,  INPUT_PULLUP);
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_FAIL,  INPUT_PULLUP);
  pinMode(PIN_WIN,   INPUT_PULLUP);

  pinMode(PIN_DIP4, INPUT_PULLUP);
  pinMode(PIN_DIP3, INPUT_PULLUP);
  // A6/A7: analog only — no pinMode pull-up

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

  enterAttract();
}

void loop() {
  switch (state) {
    case ATTRACT:
      animAttract();
      if (justPressed(btnCoin)) {
        coinsSeen++;
        readDips();
        if (coinsSeen >= coinsNeeded) {
          enterWaitStart();
        }
      }
      break;

    case WAIT_START:
      animWaitStart();
      updateButton(btnCoin);  // ignore extra coins while waiting
      if (justPressed(btnStart)) {
        enterPlaying();
      }
      break;

    case PLAYING:
      animPlaying();
      updateButton(btnStart);
      updateButton(btnFail);
      updateButton(btnWin);
      if (pressed(btnFail)) {
        enterFail();
      } else if (justPressed(btnWin)) {
        enterWin();
      }
      break;

    case WIN:
      animWin();
      if (millis() - phaseStart >= RELAY_ON_MS) {
        relay(false);
      }
      if (millis() - phaseStart >= WIN_HOLD_MS) {
        enterAttract();
      }
      break;

    case FAIL:
      animFail();
      if (millis() - phaseStart >= RELAY_ON_MS) {
        relay(false);
      }
      if (millis() - phaseStart >= FAIL_HOLD_MS) {
        // Extra attempts (Dip3) can be hooked here later
        enterAttract();
      }
      break;
  }
}
