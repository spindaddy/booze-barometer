/*
 * Booze Barometer - Arduino Nano
 *
 * D2  Start switch   (NO to GND, INPUT_PULLUP)
 * D3  Wire sense     (rod to pin, ring to GND)
 * D4  End / win      (NO to GND, INPUT_PULLUP)
 * D5  Buzzer         (piezo + to pin, - to GND)
 * D6  Game lights    (220R + LED to GND)
 * D8  Coin switch    (NO to GND, INPUT_PULLUP)
 * D9  Win LED        (220R + LED to GND)
 * D13 Status (onboard)
 */

const byte PIN_START = 2;
const byte PIN_WIRE  = 3;
const byte PIN_END   = 4;
const byte PIN_BUZZ  = 5;
const byte PIN_LIGHT = 6;
const byte PIN_COIN  = 8;
const byte PIN_WIN   = 9;
const byte PIN_STAT  = 13;

const unsigned long DEBOUNCE_MS = 40;
const unsigned long FAIL_BUZZ_MS = 800;
const unsigned long WIN_FLASH_MS = 2000;

enum State : byte { IDLE, ARMED, PLAYING, FAIL, WIN };
State state = IDLE;

bool credit = false;

// ---- edge helpers (active LOW, pulled up) ----
struct Button {
  byte pin;
  bool stable;
  bool lastRaw;
  unsigned long lastChange;
};

Button btnStart = { PIN_START, HIGH, HIGH, 0 };
Button btnCoin  = { PIN_COIN,  HIGH, HIGH, 0 };
Button btnEnd   = { PIN_END,   HIGH, HIGH, 0 };
Button btnWire  = { PIN_WIRE,  HIGH, HIGH, 0 };

bool updateButton(Button &b) {
  bool raw = digitalRead(b.pin);
  unsigned long now = millis();
  if (raw != b.lastRaw) {
    b.lastChange = now;
    b.lastRaw = raw;
  }
  if ((now - b.lastChange) >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    return true;  // state changed
  }
  return false;
}

bool pressed(const Button &b)  { return b.stable == LOW; }
bool justPressed(Button &b) {
  return updateButton(b) && pressed(b);
}

void lights(bool on)  { digitalWrite(PIN_LIGHT, on ? HIGH : LOW); }
void winLed(bool on)  { digitalWrite(PIN_WIN,   on ? HIGH : LOW); }
void status(bool on)  { digitalWrite(PIN_STAT,  on ? HIGH : LOW); }

void buzzFail() {
  tone(PIN_BUZZ, 440, FAIL_BUZZ_MS);
}

void buzzWin() {
  tone(PIN_BUZZ, 880, 150);
  delay(180);
  tone(PIN_BUZZ, 1175, 150);
  delay(180);
  tone(PIN_BUZZ, 1568, 300);
}

void enterIdle() {
  state = IDLE;
  credit = false;
  lights(false);
  winLed(false);
  status(false);
  noTone(PIN_BUZZ);
}

void enterArmed() {
  state = ARMED;
  credit = true;
  lights(false);
  winLed(false);
  status(true);  // ready
}

void enterPlaying() {
  state = PLAYING;
  lights(true);
  winLed(false);
  status(true);
  // ignore wire contact briefly so seating the ring doesn't false-fail
  btnWire.stable = HIGH;
  btnWire.lastRaw = digitalRead(PIN_WIRE);
  btnWire.lastChange = millis();
}

void enterFail() {
  state = FAIL;
  lights(false);
  winLed(false);
  status(false);
  credit = false;
  buzzFail();
}

void enterWin() {
  state = WIN;
  lights(false);
  credit = false;
  buzzWin();
}

void setup() {
  pinMode(PIN_START, INPUT_PULLUP);
  pinMode(PIN_WIRE,  INPUT_PULLUP);
  pinMode(PIN_END,   INPUT_PULLUP);
  pinMode(PIN_COIN,  INPUT_PULLUP);

  pinMode(PIN_BUZZ,  OUTPUT);
  pinMode(PIN_LIGHT, OUTPUT);
  pinMode(PIN_WIN,   OUTPUT);
  pinMode(PIN_STAT,  OUTPUT);

  enterIdle();
}

void loop() {
  static unsigned long phaseStart = 0;

  switch (state) {
    case IDLE:
      if (justPressed(btnCoin)) {
        enterArmed();
      }
      // free-play / test: start with no coin
      if (justPressed(btnStart)) {
        enterArmed();
        enterPlaying();
      }
      break;

    case ARMED:
      updateButton(btnCoin);  // keep debounce warm
      if (justPressed(btnStart)) {
        enterPlaying();
      }
      // blink status while armed
      status((millis() / 400) % 2 == 0);
      break;

    case PLAYING:
      updateButton(btnStart);
      updateButton(btnWire);
      if (pressed(btnWire)) {
        enterFail();
        phaseStart = millis();
      } else if (justPressed(btnEnd)) {
        enterWin();
        phaseStart = millis();
      }
      break;

    case FAIL:
      if (millis() - phaseStart >= FAIL_BUZZ_MS) {
        enterIdle();
      }
      break;

    case WIN: {
      // flash win LED for a bit, then idle
      bool on = ((millis() - phaseStart) / 150) % 2 == 0;
      winLed(on);
      lights(on);
      status(on);
      if (millis() - phaseStart >= WIN_FLASH_MS) {
        enterIdle();
      }
      break;
    }
  }
}
