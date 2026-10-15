# Booze Barometer

Arduino Nano recreation of the vintage **Booz Barometer** coin-op sobriety skill game (Northwestern Corp., ~1950).

## Game flow

1. **ATTRACT** — Attract LED (D4) blinks, FRONT/BACK in attract lighting  
2. **Coin pulse (A0)** — Attract LED off, Game LED (D5) on, strips switch to game lighting → **WAIT_START**  
3. **Start (A1)** → **PLAYING** — watch for Win (A3) or Fail (A2)  
4. **Win (A3)** → **WIN** for 5 seconds (lightshow on D2/D3 + relay pulse on D6)  
5. **Fail (A2)** → short **FAIL** (fail lights + relay pulse)  
6. Back to **ATTRACT**

DIP switches are read for future options (coins required, extra attempts). Patterns on WIN/FAIL can be swapped later.

## Schematic

![Booze Barometer board schematic](schematic.svg)

## Pinout

| Pin | Role |
|-----|------|
| A0 | Coin (J1-5) |
| A1 | Start (J1-2) |
| A2 | Fail / wire (J1-3) |
| A3 | Win / end (J1-4) |
| A4 | Dip4 |
| A5 | Dip3 |
| A6 | Dip2 (analog-only on Nano) |
| A7 | Dip1 (analog-only on Nano) |
| D2 | FRONT WS2812 data (330Ω on board) |
| D3 | BACK WS2812 data (330Ω on board) |
| D4 | Attract LED |
| D5 | Game LED |
| D6 | Relay (330Ω on board) |
| D7 | Spare (330Ω on board) |

POWER LED is hard-wired to 5V.

### J1 (XH-5)

| Pin | Signal | Nano |
|-----|--------|------|
| 1 | GND | GND |
| 2 | Start | A1 |
| 3 | Fail | A2 |
| 4 | Win | A3 |
| 5 | Coin | A0 |

NO to GND, `INPUT_PULLUP`, 100 nF on each signal.

### XH-3 outputs (FRONT / BACK / RELAY / SPARE)

Signal is the center pin (via 330Ω). Outer pins are 5V and GND per the board silkscreen.

### DIP (SW1)

ON = to GND. Stub mapping in firmware:

| Switch | Idea (change later) |
|--------|---------------------|
| Dip1 + Dip2 | Coins required (1–4) |
| Dip3 | Extra attempt if ON |
| Dip4 | Reserved |

**Nano note:** A6/A7 cannot use `INPUT_PULLUP`. Firmware uses `analogRead`. Add 10k pull-ups to 5V on Dip1/Dip2 if those switches float when OFF.

## Upload

1. Install **Adafruit NeoPixel**
2. Open `booze_barometer/booze_barometer.ino`
3. Board: **Arduino Nano**
4. Set `NUM_FRONT` / `NUM_BACK` to your strip lengths
5. Upload

## Desktop monitor (switch / function tester)

Firmware streams JSON status over USB serial at **115200**. Use the Python app to watch switches, DIPs, LEDs, relay, and game state live.

```bash
./scripts/run-monitor.sh
# or:
./scripts/run-monitor.sh --port /dev/cu.usbserial-1110
```

Opens a browser UI at http://127.0.0.1:8765/ (no Tkinter). Install serial support once with:
`/usr/bin/python3 -m pip install --user pyserial`

Click **Connect**. Use switch controls (keys `1`–`4`, `R`), **Full win/fail run** scripts, DIP/spare simulation, and LED preview. A “next step” hint and telemetry-stale warning help while testing. **Download log** exports the event list.

Host serial commands: `hit coin|start|fail|win`, `hold fail`, `release fail`, `dip N on|off|clear`, `dips clear`, `spare on|off`, `reset`.

## Wiring notes

- Fail/Win/Coin/Start stay at 5V logic only
- RELAY header → relay **module**, not a bare coil
- Long WS2812 runs need a solid 5V supply; share GND with the Nano
- Board has DC jack + 100 nF / 220 µF on the 5V rail
