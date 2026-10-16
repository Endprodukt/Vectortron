/* waves.h — the original's wave table and difficulty rows (RRG23.ASM WVTAB). */
#ifndef VT_WAVES_H
#define VT_WAVES_H

/* Count columns, in the original's order. Tanks and enforcers are never
 * placed at a wave's start: quarks and spheroids make them. */
enum { WC_GRUNT, WC_ELECTRODE, WC_MOMMY, WC_DADDY, WC_MIKEY, WC_HULK, WC_BRAIN, WC_SPHEROID,
       WC_QUARK, WC_TANK, WC_ENFORCER, WC_COUNT };

/* Difficulty rows. Units as in the original (see the notes in waves.c). */
enum { P_ROBSPD, P_RMXSPD, P_ENFNUM, P_ENSTIM, P_CDPTIM, P_HLKSPD, P_BSHTIM, P_BRNSPD,
       P_TNKSHT, P_SHLSPD, P_TDPTIM, P_SQSPD, P_COUNT };

int wave_count(int wave, int column);
/* A difficulty value for this wave at the operator's difficulty (0..10, 5 =
 * the table as is), with the original's "easy setup" rule (`men` = men left
 * counting the one in play). */
int wave_param(int param, int wave, int difficulty, int men);
/* The "bozo" values for a struggling beginner (waves 1-4), or -1. */
int wave_bozo(int param, int wave);

/* The safety box around the player's start: grunts, hulks and tanks are not
 * placed inside it. In the original's pixels (x) and lines (y down). */
void wave_safety_box(int wave, float *x0, float *x1, float *y0, float *y1);

#endif
