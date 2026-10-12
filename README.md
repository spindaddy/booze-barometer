# Booze Barometer

Arduino Nano recreation of the vintage **Booz Barometer** coin-op sobriety skill game (Northwestern Corp., ~1950).

Insert a coin (or hit Start for free play), guide the ring along the bent rod without touching, and hit the end switch to win. Touch the wire and you fail.

## Schematic

![Booze Barometer Arduino Nano schematic](schematic.svg)

## Pinout

| Pin | Role |
|-----|------|
| A0 | Coin switch (to GND) |
| A1 | Start switch (to GND) |
| A2 | Fail / wire sense (to GND) |
| A3 | Win / end switch (to GND) |
| D2 | FRONT WS2812 LED strip (data) |
| D3 | BACK WS2812 LED strip (data) |
| D4 | Attract LED |
| D5 | Game LED |
| D6 | Relay module (active HIGH) |
| D7 | Spare |

Inputs use `INPUT_PULLUP` (active LOW). Board has 100 nF on each input.

Set `NUM_FRONT` / `NUM_BACK` in the sketch to match your strip lengths.

## How to play (firmware)

1. **IDLE** — amber chase on FRONT/BACK, Attract blinks; coin → **ARMED**, or Start for free play
2. **ARMED** — blue pulse; Start → **PLAYING**
3. **PLAYING** — Game LED on, green chase on strips, relay held; Fail → red flash; Win → rainbow
4. Back to **IDLE**

## Upload

1. Install **Adafruit NeoPixel** from Library Manager
2. Open `booze_barometer/booze_barometer.ino` in the Arduino IDE
3. Board: **Arduino Nano**
4. Upload

## Notes

- Wire path / switches are 5V logic only
- Drive the relay header with a **relay module**, not a bare coil
- WS2812 5V + GND must be solid; data from D2/D3
- Stay under ~20 mA on the discrete Attract/Game LEDs (1 kΩ on the board)
