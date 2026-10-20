# Tombstone on an Arduino Nano

Firmware for a classic Arduino Nano (ATmega328P, 32 KB flash, 2 KB RAM) that plays the Sunbird Tombstone wall game and presents its lamp panel as logic levels.

The pin map is **unconfirmed**. No Sunbird schematic was available. Nothing here is original-board wiring, and the Nano must not be connected to mains lamps.

Game rules are plain C with no Arduino dependency. The sketch is a thin adapter.

## Layout

| File | Role |
| --- | --- |
| `game.c`, `game.h` | Rules: credits, one- and two-player flow, targets, scoring, timer, free game |
| `lamps.h` | Lamp IDs from the flyer playfield |
| `sound.h` | Sound-event IDs |
| `hw.h` | Inputs and the logic-level output image |
| `operator.h` | Free-game combined score (default 50) |
| `pins.h`, `pins.c` | Suggested Nano GPIO. Marked `UNCONFIRMED` |
| `nano_board.c` | Reads those pins, clocks the lamp image, latches a sound code |
| `tombstone-nano.ino` | Sketch |
| `test/test_game.c` | Host tests |

## Host tests

From this directory:

```bash
make test
```

That compiles `game.c` with the host compiler, runs the rule tests, and typechecks the sketch and the Nano adapter against `test/stub/Arduino.h`. No Arduino core is required.

The tests cover fast, slow, and white-hat scoring, the yellow-lamp timer, one- and two-player start and join, the combined free-game award, the operator threshold, attract mode, and the test/play switch.

`make clean` removes `build/`.

## Nano build

Board: **Arduino Nano**. Processor: **ATmega328P** (32 KB flash, 2 KB RAM). The sketch uses only the Arduino core.

An `avr-gcc -Os -mmcu=atmega328p` link of the rules and the GPIO adapter, with the Arduino calls stubbed, was about 3 KB of flash and under 200 bytes of static RAM. `Game` is 77 bytes on the 328P. A real upload also includes the Arduino core, which this number does not. The design stays inside the Nano.

Arduino IDE: open `firmware/tombstone-nano/tombstone-nano.ino` and upload.

arduino-cli (old or new bootloader, both are the 328P):

```bash
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old firmware/tombstone-nano
```

## What the flyer supports

These behaviors are implemented because the flyer (and the labels on the photographed transmitters) describe them:

- One or two players at the same time, red and blue transmitters, scores in those shirt colors
- A coin credit, then a trigger press to start. A later press shoots
- Black hats are the scoring targets. The first (faster) shot in the fast window scores
- A slow shot lets that black hat shoot back and costs points
- A hit on a white hat costs points
- Yellow lamps along the left blink off as time runs out
- A fast hit lights the tombstones and requests thunder and lightning
- Each sheriff's Colt flashes and requests an explosion when that player fires
- Attract mode lights storefront windows and flashes heat lightning
- A free-game lamp. The threshold is operator-adjustable. Default is **50**, the number printed on the photographed transmitters
- Test/play switch. Test walks every lamp ID so a dead bulb can be seen (the flyer describes an on-location lamp check)

Also from the flyer, and **not** done inside this sketch: 110/115 V AC lamps (optional 230 V), two lamps on each physical target, dual 3 by 5 inch speakers, a volume control, a separate coin box, and two radio transmitters on different frequencies. Those stay outside the firmware.

## Rules this firmware had to choose

The flyer does not give a scoring table, a time length, or a lamp chart. These constants in `game.h` are local choices so the game can run:

- Fast black-hat hit: +10. Slow shot and white-hat hit: −5, and the score does not go below 0
- Fast window 600 ms, return-fire window 500 ms, gap 300 ms
- Eight yellow lamps, 4 s each, 32 s of play. The lamp about to go out blinks, then extinguishes
- One armed figure at a time. The transmitters are treated as fire buttons, not aimed guns
- One credit starts that color immediately. A second credit leaves a 2 s join window
- A trigger sample that starts the game does not also count as a shot
- If both guns are sampled on the same tick, red is resolved first
- Free game once per play, when the combined score reaches the threshold. The lamp stays on until the next game starts, and one credit is added. Threshold 0 turns the award off
- Scores are integers in RAM (capped at 999). The flyer shows score windows, not a segment map, so there is no digit wiring

Change the free-game default in `operator.h`, or call `game_set_free_game_score()`.

## Lamp IDs

Every ID is taken from the flyer art and callouts. **None of them is from a manual**, and none of them is a measured socket.

`lamp_id_inferred()` in `lamps.h` is 0 when the flyer names that role, and 1 when this firmware inferred a count or a position:

| IDs | Source |
| --- | --- |
| Sheriff flashes, red/blue score windows, "late shot" windows, free-game lamp | Flyer names the role. Still not a socket |
| `LAMP_TIME_0` … `LAMP_TIME_7` | Yellow time lamps are named. The count 8 is inferred |
| Pop and roof pairs for Dry Goods, Tonsorial, Eatery, Last Chance Saloon, assay office | Black-hat role and those storefronts are on the flyer. Pairing each one as a pop-out and a roof step is inferred. A/B is the flyer's "two lamps on each target" |
| `LAMP_WHITE_0` … `LAMP_WHITE_3` | White hats are named. Four positions are inferred |
| Tombstone pairs | Tombstones are named. Two figures are inferred |
| Storefront attract windows | "Lights windows" is named. One lamp per store is inferred |
| Stagecoach window, heat-lightning bulb | The stagecoach is on the art and heat lightning is the attract effect. Using separate lamps for them is inferred |

LampId 0 is bit 0 of the first output byte. `nano_board.c` clocks LampId 0 first and `LAMP_HEAT_LIGHTNING` last. That order is for a new driver, not a Sunbird connector.

## Pin map (unconfirmed)

`kPinMapStatus` is `"UNCONFIRMED"`. Suggested Nano indexes only:

| Signal | Arduino pin | Direction |
| --- | --- | --- |
| Red gun | D2 | Input, assumed active-low |
| Blue gun | D3 | Input, assumed active-low |
| Coin | D4 | Input, assumed active-low |
| Test/play | D5 | Input, assumed active-low = test |
| Sound code bits 0–3 | D6 D7 D8 D9 | Logic outputs |
| Sound strobe | D10 | Logic output, about 30 ms |
| Lamp data, clock, latch | D11 D12 A0 (14) | Logic outputs |

D0 and D1 are left free for serial. D13 is left free (onboard LED). Inputs use the internal pull-up. The adapter debounces about 10 ms. Polarity is an assumption until the harness is measured.

The four sound bits show the highest-priority event of that loop (free game, thunder, return fire, white hat, explosion, game over, start, coin). The game can emit more than one event on a tick; the host tests see the full queue.

## External driver assumption

The Nano GPIO does not source lamp current and does not switch mains. An external board has to take the clocked lamp image and the sound code and drive the real lamps and speakers. Shift registers, MOSFETs, and triacs belong on that board. Power on the flyer is 110/115 V AC, optional 230 V, 50/60 Hz. Do not put that on a Nano pin.

The radio receivers are also outside this sketch. It expects two logic-level trigger inputs, not RF.

## What blocks a real panel hookup

1. No operating and maintenance manual: no lamp chart, scoring table, timing, connector pinout, or free-game adjustment method.
2. No logic, lamp-matrix, power, or receiver schematic.
3. The panel has not been measured: yellow-lamp count, how many hats and tombstones, which bulb is which ID, how the two lamps on a target are paired, and whether scores are digits or just windows.
4. Coin-switch and test/play polarity, and the receiver outputs for the two transmitters, are unpublished. The crystal notes that exist for the transmitters are not a receiver circuit.
5. Sound is an event code only. The flyer shows two speakers and a volume control, not a sound-board schematic.
6. Mains lamp drive (voltage, common, and whether the lamps are switched by triacs or something else) is outside this firmware until a board is measured.
