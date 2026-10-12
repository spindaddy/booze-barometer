# Booze Barometer

Arduino Nano recreation of the vintage **Booz Barometer** coin-op sobriety skill game (Northwestern Corp., ~1950).

Insert a coin (or hit Start for free play), guide the ring along the bent rod without touching, and hit the end switch to win. Touch the wire and you fail.

## Schematic

![Booze Barometer Arduino Nano schematic](schematic.svg)

## Pinout

| Pin | Role |
|-----|------|
| D2 | Start switch (NO to GND) |
| D3 | Wire sense (rod to pin, ring to GND) |
| D4 | End / win switch (NO to GND) |
| D5 | Piezo buzzer |
| D6 | Game lights (220Ω + LED to GND) |
| D8 | Coin switch (NO to GND) |
| D9 | Win LED (220Ω + LED to GND) |
| D13 | Onboard status LED |

All inputs use `INPUT_PULLUP` (active LOW).

## How to play (firmware)

1. **IDLE** — drop a coin → **ARMED** (status blinks), or press Start for free play
2. **ARMED** — press Start → **PLAYING** (lights on)
3. **PLAYING** — wire touch → fail buzz; end switch → win tones + flash
4. Back to **IDLE**

## Upload

1. Open `booze_barometer/booze_barometer.ino` in the Arduino IDE
2. Board: **Arduino Nano**
3. Upload

## Notes

- Keep the wire path at 5V logic only — do not put battery voltage on the rod/ring
- Stay under ~20 mA per LED pin
- Small piezo on D5 is fine direct; a loud 12V bell would need a driver
