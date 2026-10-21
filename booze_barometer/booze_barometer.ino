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
 * Serial monitor (115200): JSON status lines for the desktop test app.
 *   {"type":"status",...}
 *   {"type":"event","name":"state","value":"PLAYING"}
 * Host commands (newline-terminated):
 *   hit coin | hit start | hit fail | hit win
 *   hold fail | release fail
 *   dip 1 on|off|clear   (also dip 2..4)
 *   dips clear
 *   spare on | spare off
 *   reset
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

const byte PIN_ATTRACT = 4;
const byte PIN_GAME    = 5;
const byte PIN_RELAY   = 6;
const byte PIN_SPARE   = 7;

// ---- strips (edit to match hardware) ----
// Schematic: FRONT=D2, BACK=D3. If cabinet “front” LEDs stay dark, set
// SWAP_FRONT_BACK to 1 (headers / loom often reversed).
#ifndef SWAP_FRONT_BACK
#define SWAP_FRONT_BACK 1
#endif
#if SWAP_FRONT_BACK
const byte PIN_FRONT = 3;
const byte PIN_BACK  = 2;
#else
const byte PIN_FRONT = 2;
const byte PIN_BACK  = 3;
#endif

const int NUM_FRONT = 30;
const int NUM_BACK  = 30;

Adafruit_NeoPixel front(NUM_FRONT, PIN_FRONT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel back (NUM_BACK,  PIN_BACK,  NEO_GRB + NEO_KHZ800);

const unsigned long DEBOUNCE_MS   = 40;
const unsigned long WIN_HOLD_MS   = 5000;
const unsigned long FAIL_HOLD_MS  = 1500;
const unsigned long RELAY_ON_MS   = 300;   // single pulse length (FAIL)
const unsigned long RELAY_PULSE_MS = 250;  // WIN bell: on time per ring
const unsigned long RELAY_PAUSE_MS = 250;  // WIN bell: off time between rings
const byte WIN_BELL_RINGS = 4;
const unsigned long COIN_DING_MS  = 36;    // short bell chirp per coin (~20% of 180ms)
const unsigned long TELEMETRY_MS  = 100;

// A6/A7 have no INPUT_PULLUP — floating pins sit mid-scale or soft-low together.
// Best fix: 10k pull-ups to 5V. Firmware: hard-GND threshold + pair reject + debounce.
const int DIP_ANALOG_ON_MAX   = 40;   // <= this → candidate ON (hard toward GND)
const int DIP_ANALOG_GAP      = 25;   // min raw gap to call the higher pin a ghost
const int DIP_ANALOG_PAIR_MAX = 12;   // |a6-a7| at/below this → "tracking together"
const byte DIP_DEBOUNCE_HITS  = 8;    // consecutive agreements (~800ms)

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
unsigned long telemetryTick = 0;
unsigned long coinDingEnd = 0;
uint16_t animStep        = 0;

// Attract / win / fail patterns from maze LED sketch (message.txt)
const unsigned long ATTRACT_ANIM_MS = 10000;
unsigned long animStartTime = 0;
int currentAttractAnim = 0;  // 0 fade, 1 flash, 2 scroll
int fadeStep = 0;
bool fadeForward = true;
int flashStep = 0;
unsigned long lastFlashTime = 0;
int scrollOffset = 0;
bool policeFlip = true;

int coinsNeeded  = 1;   // from DIP (stub)
int attemptsLeft = 1;   // from DIP (stub)
int coinsSeen    = 0;

bool attractLedOn = false;
bool gameLedOn    = false;
bool relayOn      = false;

// One-shot virtual presses from serial "hit …" commands
bool simCoin  = false;
bool simStart = false;
bool simFail  = false;
bool simWin   = false;
bool simFailHold = false;  // level hold from "hold fail"
bool failArmed = false;    // false from coin-up until Start (+ brief grace)
unsigned long failArmAt = 0;

bool dip1On = false;
bool dip2On = false;
bool dip3On = false;
bool dip4On = false;
bool dipForce[4] = { false, false, false, false };
bool dipForceOn[4] = { false, false, false, false };
byte dipDebounceCount[2] = { 0, 0 };
bool spareOn = false;
int  a6Raw  = 1023;
int  a7Raw  = 1023;

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

int readAnalogAveraged(byte pin) {
  // Mux settle: discard a few samples so A6 is not contaminated by A7 (GND).
  (void)analogRead(pin);
  delayMicroseconds(100);
  (void)analogRead(pin);
  delayMicroseconds(100);
  long sum = 0;
  for (byte i = 0; i < 6; i++) {
    sum += analogRead(pin);
  }
  return (int)(sum / 6);
}

bool dipAnalogCandidate(int raw) {
  return raw <= DIP_ANALOG_ON_MAX;
}

void debounceAnalogDip(byte idx, bool candidate, bool &state) {
  if (candidate == state) {
    dipDebounceCount[idx] = 0;
    return;
  }
  if (++dipDebounceCount[idx] >= DIP_DEBOUNCE_HITS) {
    state = candidate;
    dipDebounceCount[idx] = 0;
  }
}

// Decide Dip1/Dip2 from a pair of raw reads (crosstalk-aware).
// raw1 = A7 (Dip1), raw2 = A6 (Dip2)
void classifyAnalogDipPair(int raw1, int raw2, bool &out1, bool &out2) {
  bool c1 = dipAnalogCandidate(raw1);
  bool c2 = dipAnalogCandidate(raw2);
  int diff = raw1 > raw2 ? (raw1 - raw2) : (raw2 - raw1);

  // Both pins soft-low and tracking together → float/crosstalk, not a real DIP.
  // (Live bug: a6≈a7≈20..50 made Dip1 chatter ON/OFF.)
  if (c1 && c2 && diff <= DIP_ANALOG_PAIR_MAX && !(raw1 <= 8 && raw2 <= 8)) {
    c1 = false;
    c2 = false;
  } else if (c1 && c2) {
    if (raw2 >= raw1 + DIP_ANALOG_GAP) {
      c2 = false;  // Dip2 higher → ghost
    } else if (raw1 >= raw2 + DIP_ANALOG_GAP) {
      c1 = false;  // Dip1 higher → ghost
    } else if (raw1 <= 8 && raw2 <= 8) {
      // both hard GND → both ON
    } else if (out1 != out2) {
      // Ambiguous: hold existing single-ON
      c1 = out1;
      c2 = out2;
    } else {
      // pick lower raw as the real switch
      if (raw1 <= raw2) c2 = false;
      else c1 = false;
    }
  }

  debounceAnalogDip(0, c1, out1);
  debounceAnalogDip(1, c2, out2);
}

const char *stateName(State s) {
  switch (s) {
    case ATTRACT:    return "ATTRACT";
    case WAIT_START: return "WAIT_START";
    case PLAYING:    return "PLAYING";
    case WIN:        return "WIN";
    case FAIL:       return "FAIL";
  }
  return "?";
}

void emitEvent(const char *name, const char *value) {
  Serial.print(F("{\"type\":\"event\",\"name\":\""));
  Serial.print(name);
  Serial.print(F("\",\"value\":\""));
  Serial.print(value);
  Serial.println(F("\"}"));
}

void emitEventInt(const char *name, int value) {
  Serial.print(F("{\"type\":\"event\",\"name\":\""));
  Serial.print(name);
  Serial.print(F("\",\"value\":"));
  Serial.print(value);
  Serial.println(F("}"));
}

void readDips() {
  // Read A6 (Dip2) before A7 (Dip1) so a grounded Dip1 does not charge the mux first.
  a6Raw = readAnalogAveraged(PIN_DIP2);
  a7Raw = readAnalogAveraged(PIN_DIP1);
  // Dip1 = A7 raw, Dip2 = A6 raw
  classifyAnalogDipPair(a7Raw, a6Raw, dip1On, dip2On);

  dip3On = dipDigitalOn(PIN_DIP3);
  dip4On = dipDigitalOn(PIN_DIP4);

  if (dipForce[0]) dip1On = dipForceOn[0];
  if (dipForce[1]) dip2On = dipForceOn[1];
  if (dipForce[2]) dip3On = dipForceOn[2];
  if (dipForce[3]) dip4On = dipForceOn[3];

  // Stub mapping (change when you decide options):
  //   Dip1+Dip2 = coins required (1..4)
  //   Dip3      = extra attempt if ON
  //   Dip4      = reserved
  int coinCode = (dip1On ? 1 : 0) | (dip2On ? 2 : 0);
  coinsNeeded = coinCode + 1;          // 1..4
  attemptsLeft = dip3On ? 2 : 1;
}

void setSpare(bool on) {
  spareOn = on;
  digitalWrite(PIN_SPARE, on ? HIGH : LOW);
}

void sendTelemetry() {
  readDips();

  Serial.print(F("{\"type\":\"status\",\"ms\":"));
  Serial.print(millis());
  Serial.print(F(",\"state\":\""));
  Serial.print(stateName(state));
  Serial.print(F("\",\"coin\":"));
  Serial.print(pressed(btnCoin) ? 1 : 0);
  Serial.print(F(",\"start\":"));
  Serial.print(pressed(btnStart) ? 1 : 0);
  Serial.print(F(",\"fail\":"));
  Serial.print(pressed(btnFail) ? 1 : 0);
  Serial.print(F(",\"win\":"));
  Serial.print(pressed(btnWin) ? 1 : 0);
  Serial.print(F(",\"coin_raw\":"));
  Serial.print(digitalRead(PIN_COIN) == LOW ? 1 : 0);
  Serial.print(F(",\"start_raw\":"));
  Serial.print(digitalRead(PIN_START) == LOW ? 1 : 0);
  Serial.print(F(",\"fail_raw\":"));
  Serial.print(digitalRead(PIN_FAIL) == LOW ? 1 : 0);
  Serial.print(F(",\"win_raw\":"));
  Serial.print(digitalRead(PIN_WIN) == LOW ? 1 : 0);
  Serial.print(F(",\"dip1\":"));
  Serial.print(dip1On ? 1 : 0);
  Serial.print(F(",\"dip2\":"));
  Serial.print(dip2On ? 1 : 0);
  Serial.print(F(",\"dip3\":"));
  Serial.print(dip3On ? 1 : 0);
  Serial.print(F(",\"dip4\":"));
  Serial.print(dip4On ? 1 : 0);
  Serial.print(F(",\"a6\":"));
  Serial.print(a6Raw);
  Serial.print(F(",\"a7\":"));
  Serial.print(a7Raw);
  Serial.print(F(",\"coins\":"));
  Serial.print(coinsSeen);
  Serial.print(F(",\"need\":"));
  Serial.print(coinsNeeded);
  Serial.print(F(",\"attempts\":"));
  Serial.print(attemptsLeft);
  Serial.print(F(",\"attract\":"));
  Serial.print(attractLedOn ? 1 : 0);
  Serial.print(F(",\"game\":"));
  Serial.print(gameLedOn ? 1 : 0);
  Serial.print(F(",\"relay\":"));
  Serial.print(relayOn ? 1 : 0);
  Serial.print(F(",\"spare\":"));
  Serial.print(spareOn ? 1 : 0);
  Serial.print(F(",\"fail_hold\":"));
  Serial.print(simFailHold ? 1 : 0);
  Serial.println(F("}"));
}

void setBoardLeds(bool attract, bool game) {
  attractLedOn = attract;
  gameLedOn = game;
  digitalWrite(PIN_ATTRACT, attract ? HIGH : LOW);
  digitalWrite(PIN_GAME,    game    ? HIGH : LOW);
}

void relay(bool on) {
  relayOn = on;
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
}

void startCoinDing() {
  relay(true);
  coinDingEnd = millis() + COIN_DING_MS;
}

void serviceCoinDing() {
  if (coinDingEnd == 0) return;
  if ((long)(millis() - coinDingEnd) >= 0) {
    relay(false);
    coinDingEnd = 0;
  }
}

// WIN: ring bell on/off WIN_BELL_RINGS times, then stay off
void updateWinRelay() {
  unsigned long elapsed = millis() - phaseStart;
  unsigned long cycle = RELAY_PULSE_MS + RELAY_PAUSE_MS;
  unsigned long ringWindow = (unsigned long)WIN_BELL_RINGS * cycle;
  if (elapsed >= ringWindow) {
    relay(false);
    return;
  }
  relay((elapsed % cycle) < RELAY_PULSE_MS);
}

void paintStrip(Adafruit_NeoPixel &s, uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < s.numPixels(); i++) {
    s.setPixelColor(i, s.Color(r, g, b));
  }
}

// Push both strips; short gap so the first strip can latch before the second bit-bang.
void showBoth() {
  front.show();
  delayMicroseconds(300);
  back.show();
}

void bothSolid(uint8_t r, uint8_t g, uint8_t b) {
  paintStrip(front, r, g, b);
  paintStrip(back, r, g, b);
  showBoth();
}

void setPixelBoth(int i, uint8_t r, uint8_t g, uint8_t b) {
  if (i >= 0 && i < NUM_FRONT) front.setPixelColor(i, front.Color(r, g, b));
  if (i >= 0 && i < NUM_BACK)  back.setPixelColor(i, back.Color(r, g, b));
}

// LED looks — warm white front for attract/game; win/fail keep warm scroll / police
//   Attract     → warm white fade in / fade out (breathe)
//   Wait/Play   → solid warm white
//   Win         → warm scroll
//   Fail        → orange/red half “police” flash

// Warm white base (scaled for fade / solid)
// Keep G well below R so WS2812 never reads as green
const uint8_t WW_R = 255;
const uint8_t WW_G = 140;
const uint8_t WW_B = 60;

void setSolidWarm() {
  // Game mode: solid warm white
  bothSolid(WW_R, WW_G, WW_B);
}

void setSolidWarmOrange() {
  bothSolid(255, 100, 20);
}

void warmWhiteFadeAnimation() {
  // Attract: breathe warm white 0 → full → 0
  if (millis() - animTick < 20) return;
  animTick = millis();

  const int stepMax = 255;
  if (fadeForward) {
    fadeStep++;
    if (fadeStep >= stepMax) {
      fadeStep = stepMax;
      fadeForward = false;
    }
  } else {
    fadeStep--;
    if (fadeStep <= 0) {
      fadeStep = 0;
      fadeForward = true;
    }
  }

  uint8_t r = (uint8_t)((uint16_t)WW_R * fadeStep / 255);
  uint8_t g = (uint8_t)((uint16_t)WW_G * fadeStep / 255);
  uint8_t b = (uint8_t)((uint16_t)WW_B * fadeStep / 255);
  bothSolid(r, g, b);
}

void warmFlashAnimation() {
  // Even steps hold longer (white), odd steps shorter (color) — matches maze sketch
  unsigned long interval = (flashStep % 2 == 0) ? 1000UL : 500UL;
  if (millis() - lastFlashTime < interval) return;
  lastFlashTime = millis();

  uint8_t r = 255, g = 255, b = 255;
  switch (flashStep % 6) {
    case 0: r = 255; g = 255; b = 255; break;  // white
    case 1: r = 255; g = 0;   b = 0;   break;  // red
    case 2: r = 255; g = 255; b = 255; break;
    case 3: r = 255; g = 100; b = 0;   break;  // orange
    case 4: r = 255; g = 255; b = 255; break;
    case 5: r = 255; g = 255; b = 0;   break;  // yellow
  }
  bothSolid(r, g, b);
  flashStep = (flashStep + 1) % 6;
}

void warmScrollAnimation() {
  if (millis() - animTick < 60) return;
  animTick = millis();

  int n = NUM_FRONT > NUM_BACK ? NUM_FRONT : NUM_BACK;
  for (int i = 0; i < n; i++) {
    int pos = (i + scrollOffset) % 12;
    uint8_t r, g, b;
    switch (pos) {
      case 0:
      case 1: r = 255; g = 160; b = 60; break;  // pale orange
      case 2:
      case 3: r = 255; g = 100; b = 0;  break;  // orange
      case 4:
      case 5: r = 255; g = 60;  b = 0;  break;  // deep orange
      case 6:
      case 7: r = 255; g = 0;   b = 0;  break;  // red
      default: r = 50;  g = 0;   b = 0;  break;  // dim red
    }
    setPixelBoth(i, r, g, b);
  }
  showBoth();
  scrollOffset = (scrollOffset + 1) % 12;
}

void setPoliceFlash() {
  // Half strip warm-orange, half red — flip each call (maze lose siren look)
  uint8_t r1 = policeFlip ? 255 : 255;
  uint8_t g1 = policeFlip ? 100 : 0;
  uint8_t b1 = policeFlip ? 20  : 0;
  uint8_t r2 = policeFlip ? 255 : 255;
  uint8_t g2 = policeFlip ? 0   : 100;
  uint8_t b2 = policeFlip ? 0   : 20;

  for (int i = 0; i < NUM_FRONT; i++) {
    bool first = i < (NUM_FRONT / 2);
    front.setPixelColor(i, front.Color(first ? r1 : r2, first ? g1 : g2, first ? b1 : b2));
  }
  for (int i = 0; i < NUM_BACK; i++) {
    bool first = i < (NUM_BACK / 2);
    back.setPixelColor(i, back.Color(first ? r1 : r2, first ? g1 : g2, first ? b1 : b2));
  }
  showBoth();
  policeFlip = !policeFlip;
}

void animAttract() {
  unsigned long now = millis();
  warmWhiteFadeAnimation();
  setBoardLeds((now / 500) % 2 == 0, false);
}

void animWaitStart() {
  // After coins: solid warm white (+ Game LED) until Start.
  setSolidWarm();
  setBoardLeds(false, true);
}

void animPlaying() {
  // During play: solid warm white, Game LED on
  if (millis() - animTick < 100) {
    setBoardLeds(false, true);
    return;
  }
  animTick = millis();
  setSolidWarm();
  setBoardLeds(false, true);
}

void animWin() {
  warmScrollAnimation();
  setBoardLeds(true, true);
}

void animFail() {
  if (millis() - animTick < 120) return;
  animTick = millis();
  setPoliceFlash();
  setBoardLeds(false, policeFlip);
}

// ---- state entry ----
void clearFailEdge() {
  btnFail.stable = HIGH;
  btnFail.lastRaw = digitalRead(PIN_FAIL);
  btnFail.lastChange = millis();
  simFail = false;
  simFailHold = false;
}

void enterAttract() {
  state = ATTRACT;
  coinsSeen = 0;
  failArmed = false;
  failArmAt = 0;
  relay(false);
  animStep = 0;
  animTick = 0;
  animStartTime = millis();
  fadeStep = 0;
  fadeForward = true;
  flashStep = 0;
  scrollOffset = 0;
  readDips();
  emitEvent("state", "ATTRACT");
}

void enterWaitStart() {
  state = WAIT_START;
  failArmed = false;
  failArmAt = 0;
  clearFailEdge();  // ignore Fail from coin-up until Start
  // Keep coin ding running — do not relay(false) here (1-coin path killed the chirp)
  if (coinDingEnd == 0) relay(false);
  setBoardLeds(false, true);  // Attract off, Game on
  animStep = 0;
  animTick = 0;
  setSolidWarm();
  emitEvent("state", "WAIT_START");
}

void enterPlaying() {
  state = PLAYING;
  relay(false);
  // Arm Fail shortly after Start so a held fail wire cannot instant-fail
  failArmed = false;
  failArmAt = millis() + 300;
  clearFailEdge();
  btnWin.stable = HIGH;
  btnWin.lastRaw = digitalRead(PIN_WIN);
  btnWin.lastChange = millis();
  animStep = 0;
  animTick = 0;
  setSolidWarm();
  setBoardLeds(false, true);
  emitEvent("state", "PLAYING");
}

void enterWin() {
  state = WIN;
  phaseStart = millis();
  coinDingEnd = 0;
  relay(true);
  animStep = 0;
  animTick = 0;
  scrollOffset = 0;
  currentAttractAnim = 2;  // resume attract on scroll after win (maze sketch)
  emitEvent("state", "WIN");
}

void enterFail() {
  state = FAIL;
  phaseStart = millis();
  coinDingEnd = 0;
  relay(true);
  animStep = 0;
  animTick = 0;
  policeFlip = true;
  currentAttractAnim = 0;
  emitEvent("state", "FAIL");
}

void handleHostCommand(char *line) {
  // trim leading spaces
  while (*line == ' ' || *line == '\t') line++;
  if (line[0] == 0) return;

  if (strncmp(line, "hit ", 4) == 0) {
    const char *sw = line + 4;
    while (*sw == ' ') sw++;
    if (strcmp(sw, "coin") == 0) {
      simCoin = true;
    } else if (strcmp(sw, "start") == 0) {
      simStart = true;
    } else if (strcmp(sw, "fail") == 0) {
      simFail = true;
    } else if (strcmp(sw, "win") == 0) {
      simWin = true;
    } else {
      return;
    }
    emitEvent("sim", sw);
    return;
  }

  if (strcmp(line, "hold fail") == 0) {
    simFailHold = true;
    emitEvent("sim", "hold_fail");
    return;
  }
  if (strcmp(line, "release fail") == 0) {
    simFailHold = false;
    emitEvent("sim", "release_fail");
    return;
  }

  if (strncmp(line, "dip ", 4) == 0) {
    char nch = line[4];
    if (nch < '1' || nch > '4') return;
    byte idx = (byte)(nch - '1');
    const char *rest = line + 5;
    while (*rest == ' ') rest++;
    if (strcmp(rest, "on") == 0) {
      dipForce[idx] = true;
      dipForceOn[idx] = true;
    } else if (strcmp(rest, "off") == 0) {
      dipForce[idx] = true;
      dipForceOn[idx] = false;
    } else if (strcmp(rest, "clear") == 0) {
      dipForce[idx] = false;
    } else {
      return;
    }
    readDips();
    emitEvent("sim", line);
    return;
  }

  if (strcmp(line, "dips clear") == 0) {
    for (byte i = 0; i < 4; i++) dipForce[i] = false;
    readDips();
    emitEvent("sim", "dips_clear");
    return;
  }

  if (strcmp(line, "spare on") == 0) {
    setSpare(true);
    emitEvent("sim", "spare_on");
    return;
  }
  if (strcmp(line, "spare off") == 0) {
    setSpare(false);
    emitEvent("sim", "spare_off");
    return;
  }

  if (strcmp(line, "reset") == 0) {
    simFailHold = false;
    setSpare(false);
    enterAttract();
    emitEvent("sim", "reset");
  }
}

void pollSerialCommands() {
  static char buf[48];
  static byte len = 0;
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = 0;
      len = 0;
      handleHostCommand(buf);
      continue;
    }
    if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    } else {
      len = 0;  // overflow — drop line
    }
  }
}

void setup() {
  Serial.begin(115200);

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
  setSpare(false);
  setBoardLeds(false, false);

  front.begin();
  back.begin();
  front.setBrightness(200);
  back.setBrightness(200);
  front.clear();
  back.clear();
  showBoth();

  // Boot check: FRONT = red, BACK = orange (no green in palette)
  paintStrip(front, 255, 0, 0);
  paintStrip(back, 0, 0, 0);
  showBoth();
  delay(600);
  paintStrip(front, 0, 0, 0);
  paintStrip(back, 255, 100, 0);
  showBoth();
  delay(600);
  bothSolid(0, 0, 0);

  randomSeed(analogRead(A0) ^ micros());
  enterAttract();
  Serial.println(F("{\"type\":\"hello\",\"device\":\"booze-barometer\",\"baud\":115200}"));
#if SWAP_FRONT_BACK
  Serial.println(F("{\"type\":\"event\",\"name\":\"strips\",\"value\":\"swapped_front_on_D3\"}"));
#else
  Serial.println(F("{\"type\":\"event\",\"name\":\"strips\",\"value\":\"front_on_D2\"}"));
#endif
  sendTelemetry();
}

void loop() {
  pollSerialCommands();

  // One update pass so telemetry and game logic share the same edges
  bool coinJust  = (updateButton(btnCoin)  && pressed(btnCoin))  || simCoin;
  bool startJust = (updateButton(btnStart) && pressed(btnStart)) || simStart;
  (void)updateButton(btnFail);
  bool winJust   = (updateButton(btnWin)   && pressed(btnWin))   || simWin;
  bool failDown  = pressed(btnFail) || simFail || simFailHold;
  simCoin = simStart = simFail = simWin = false;

  switch (state) {
    case ATTRACT:
      animAttract();
      if (coinJust) {
        coinsSeen++;
        readDips();
        emitEventInt("coin", coinsSeen);
        startCoinDing();
        if (coinsSeen >= coinsNeeded) {
          enterWaitStart();
        }
      }
      serviceCoinDing();
      break;

    case WAIT_START:
      animWaitStart();
      serviceCoinDing();  // finish ding after last coin → WAIT_START
      if (startJust) {
        emitEvent("start", "1");
        enterPlaying();
      }
      break;

    case PLAYING:
      animPlaying();
      if (!failArmed && failArmAt != 0 && (long)(millis() - failArmAt) >= 0) {
        failArmed = true;
        failArmAt = 0;
        clearFailEdge();  // drop any Fail held through Start
      }
      if (failArmed && failDown) {
        emitEvent("fail", "1");
        enterFail();
      } else if (winJust) {
        emitEvent("win", "1");
        enterWin();
      }
      break;

    case WIN:
      animWin();
      updateWinRelay();
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

  unsigned long now = millis();
  if (now - telemetryTick >= TELEMETRY_MS) {
    telemetryTick = now;
    sendTelemetry();
  }
}
