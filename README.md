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
| **Relay** | Short pulse on win or fail (bell, lockout, lamp, etc. — use a relay *module*) |

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
