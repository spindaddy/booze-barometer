#ifndef TOMBSTONE_NANO_BOARD_H
#define TOMBSTONE_NANO_BOARD_H

#include <stdint.h>

#include "hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Arduino Nano adapter. Implemented in nano_board.c. Not used by host tests. */

void hw_setup(void);
void hw_read_inputs(GameInputs *in, uint32_t now_ms);
void hw_present(const HardwareOutputs *out, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
