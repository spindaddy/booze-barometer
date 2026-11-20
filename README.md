# Booze Barometer

An Arduino Nano recreation of the vintage **Booz Barometer** — a 1950s Northwestern Corp. coin-op “sobriety test” bartop game.

## What the original was

For a nickel, you tried to guide a metal ring (or short handle) along a bent rod without touching it. Touch the rod and a bell rang — you failed. Make it to the end without a short and you passed. It was a steady-hand skill game sold as a joke sobriety meter for bars and arcades.

## What this version does

Same idea, modern control board:

1. The machine sits in **attract** mode (lights idle / calling for a player).
2. A **coin pulse** credits the game. Attract lights go off; game lights come on. The machine waits for you to begin.
3. Press **Start**, then move the ring along the wire path.
4. Touch the wire → **Fail** (short fail animation + relay pulse).
5. Reach the end switch without touching → **Win** (5-second light show + relay pulse).
6. Back to attract for the next player.

Optional **DIP switches** can set things like how many coin pulses are needed, or extra attempts — wired in the firmware as stubs you can redefine later.

## Controls (inputs)

| Input | What it means |
|-------|----------------|
| **Coin** | Credits the game (pulse from a coin mech or button) |
| **Start** | Begins the run after credit |
| **Fail** | Ring/handle touched the hot wire |
| **Win** | Player reached the end of the path |

## Feedback (outputs)

| Output | What it does |
|--------|----------------|
| **Attract LED** | Blinks in attract mode |
| **Game LED** | On while credited / playing |
| **FRONT / BACK LED strips** | Attract pattern, then game lighting, then win/fail shows |
| **Light-up header** (Spare port D7) | Lit sign sections that tell the player what to do (see below) |
| **Relay** | Short pulse on win or fail (bell, lockout, lamp, etc. — use a relay *module*) |

### Light-up header

38 WS2812 LEDs on the Spare port (D7), chained in this order from the D7 end:

| # | Section | LEDs |
|---|---------|------|
| 1 | Insert Token | 11 |
| 2 | Proceed to Start | 12 |
| 3 | Go! | 5 |
| 4 | Left | 2 |
| 5 | Attempts | 5 |
| 6 | 1 | 1 |
| 7 | 2 | 1 |
| 8 | 3 | 1 |

| Game state | Header |
|------------|--------|
| Attract | **Insert Token** blinking + Left + Attempts + **3** |
| Wait for start | Left + Attempts + digit + **Proceed to Start** blinking |
| Playing | Left + Attempts + digit + **Go!** |
| Win | Sections fill 1→8, flash all, repeat |
| Fail | Left + Attempts + digit flash red; digit steps **3→2→1**; after 3rd fail → attract |

Each credit gets **3 attempts**. After a fail the digit drops (3→2→1). After the third fail the game ends and returns to attract. After fail 1 or 2 it returns to wait-for-start: the player must get off the wire (Fail clear) and press **Start** before the next attempt.

At power-up the header lights each section one at a time so you can check the section boundaries. Section lengths live in `HDR_LEN` in the sketch.

## Playing a round

```
ATTRACT  --(coin)-->  WAIT FOR START  --(start)-->  PLAYING
                                                      |
                                         +------------+------------+
                                         |                         |
                                       FAIL                      WIN
                                    (short show)            (5 second show)
                                         |                         |
                                         +------------+------------+
                                                      |
                                                   ATTRACT
```

- During **PLAYING**, only Fail and Win matter.
- Win holds for **5 seconds** with a light show on both strips, then returns to attract.
- Fail is a shorter “you lost” flash, then attract again.

## Hardware notes

- Brain: **Arduino Nano**
- Strip lighting: **WS2812** on FRONT and BACK headers (set LED counts in the sketch)
- Firmware may swap D2/D3 via `SWAP_FRONT_BACK` if the cabinet loom is reversed. Boot test: FRONT flashes red, BACK flashes orange.
- Switches are normally-open to ground (active low, with pull-ups in firmware)
- Power: 5 V DC jack on the board; long LED runs should share a solid 5 V supply and common ground (use a supply with enough amps for both strips + relay)

### LED strip hookup map

Wiring diagram for FRONT (D2), BACK (D3), and HEADER / Spare (D7), including header section pixel ranges:

![LED strip hookup map](led-strip-hookup.svg)

| Board header | Schematic data pin | Strip | Count |
|--------------|--------------------|-------|-------|
| FRONT | D2 (via 330 Ω) | Cabinet front | 30 |
| BACK | D3 (via 330 Ω) | Cabinet back | 30 |
| SPARE | D7 (via 330 Ω) | Light-up header | 38 |

Each strip needs **+5 V**, **GND** (common with the board), and **DIN**. Header sections are chained from DIN: Insert Token → Proceed to Start → Go! → Left → Attempts → **1 → 2 → 3**.

## Firmware

Sketch: `booze_barometer/booze_barometer.ino`

1. Install **Adafruit NeoPixel** from the Arduino Library Manager  
2. Open the sketch, set `NUM_FRONT` / `NUM_BACK` to your strip lengths  
3. Board: **Arduino Nano** → Upload  

## Desktop monitor (optional)

Firmware can stream JSON status over USB serial at **115200** for live testing:

```bash
./scripts/run-monitor.sh
# or:
./scripts/run-monitor.sh --port /dev/cu.usbserial-1110
```

Browser UI at http://127.0.0.1:8765/ — watch switches, DIPs, LEDs, relay, and game state. Needs `pyserial` (`python3 -m pip install --user pyserial`).
