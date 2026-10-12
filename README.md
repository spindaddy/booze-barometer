# Booze Barometer

Arduino Nano recreation of the vintage **Booz Barometer** coin-op sobriety skill game (Northwestern Corp., ~1950).

Insert a coin (or press Start for free play), guide the ring along the bent rod without touching, then hit the end switch to win. Touch the wire and you fail.

## Schematic

![Booze Barometer board schematic](image.png)


## Pinout

| Pin | Role |
|-----|------|
| A0 | Coin (J1 pin 5) |
| A1 | Start (J1 pin 2) |
| A2 | Fail / wire sense (J1 pin 3) |
| A3 | Win / end of path (J1 pin 4) |
| D2 | FRONT WS2812 data |
| D3 | BACK WS2812 data |
| D4 | Attract LED |
| D5 | Game LED |
| D6 | Relay module |
| D7 | Spare |

POWER LED is hard-wired to 5V (always on).

### J1 inputs (XH-5)

| J1 | Signal | Nano |
|----|--------|------|
| 1 | GND | GND |
| 2 | Start | A1 |
| 3 | Fail | A2 |
| 4 | Win | A3 |
| 5 | Coin | A0 |

Switches: normally open to GND. Firmware uses `INPUT_PULLUP` (active LOW). Board has 100 nF on each signal (C1-C4).

### XH-3 outputs (FRONT / BACK / RELAY / SPARE)

| Pin | Net |
|-----|-----|
| 1 | 5V |
| 2 | Signal (D2 / D3 / D6 / D7) |
| 3 | GND |

FRONT and BACK pin 2 are WS2812 data lines. Set `NUM_FRONT` and `NUM_BACK` in the sketch to your strip lengths.

## Game flow

1. **IDLE** - amber chase on strips, Attract blinks; coin -> **ARMED**, or Start for free play
2. **ARMED** - blue pulse; Start -> **PLAYING**
3. **PLAYING** - Game LED on, green chase, relay held; Fail -> red flash; Win -> rainbow
4. Return to **IDLE**

## Upload

1. Install **Adafruit NeoPixel** from the Library Manager
2. Open `booze_barometer/booze_barometer.ino`
3. Board: **Arduino Nano**
4. Upload

## Wiring notes

- Keep Fail/Win/Coin/Start at 5V logic only (no high voltage on the rod)
- Use a **relay module** on the RELAY header (transistor + diode), not a bare coil
- Power long WS2812 strips from a solid 5V supply; share GND with the Nano
- Attract/Game LEDs use 1k on the board (~few mA)
