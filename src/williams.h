/* williams.h — the Williams sound board, emulated, to record the original
 * sounds from its own ROM.
 *
 * The board (Defender, Stargate, Robotron, Joust...): a 6808 CPU at
 * 894.886 kHz, 128 + 128 bytes of RAM, a 6821 PIA and an 8-bit DAC on the
 * PIA's port A. The game sends a 6-bit sound number through port B and
 * raises CB1; the board's ROM plays it on the DAC. Running the ROM gives the
 * sounds exactly as the cabinet made them.
 *
 * The ROM is not part of this program: it is read from the owner's own MAME
 * set (see sound.c for where it is looked for).
 */
#ifndef VT_WILLIAMS_H
#define VT_WILLIAMS_H

#include <stddef.h>

#define WMS_CLOCK 894886            /* 3.579545 MHz / 4 */

/* One step of a sound: send `id` `repeat` times, `frames` (1/60 s) apart. */
typedef struct { int repeat, frames, id; } wms_step;

/* Record what the board plays for a sequence of commands, as the original's
 * sound sequencer sends them (SNDLDV / SNDSEQ, RRS22.ASM). The recording
 * runs until the board falls silent after the last command, at most
 * `max_seconds`. Returns a malloc'ed buffer of mono float samples at `rate`
 * (count in *frames), or NULL. */
float *wms_record(const unsigned char *rom, size_t rom_size, const wms_step *steps, int nsteps,
                  int rate, float max_seconds, int *frames);

/* For tests: run the ROM from reset for `cycles` and copy out the RAM. */
void wms_run(const unsigned char *rom, size_t rom_size, long cycles, unsigned char ram[256]);

#endif
