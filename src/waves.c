/* waves.c — the wave table, copied from the original (RRG23.ASM:596-652).
 *
 * Units of the difficulty rows (one frame = 1/60 s):
 *   ROBSPD  a grunt waits RANDU(ROBSPD) ticks of 4 frames between steps
 *   RMXSPD  the floor ROBSPD can fall to while the wave goes on
 *   ENFNUM  enforcers per spheroid, tanks per quark: ceil(RMAX(ENFNUM) / 2)
 *   ENSTIM  enforcer: RMAX(ENSTIM) steps of 3 frames between sparks
 *   CDPTIM  spheroid: first drop after RMAX(CDPTIM) cycles of 10 frames, then
 *           RMAX(CDPTIM / 4) cycles of 16 frames between drops
 *   HLKSPD  hulk: frames between steps
 *   BSHTIM  brain: RMAX(BSHTIM) steps between cruise missiles
 *   BRNSPD  brain: frames between steps
 *   TNKSHT  tank: steps of 2 frames between shots
 *   SHLSPD  tank shell speed scale
 *   TDPTIM  quark: first tank after RMAX(TDPTIM) cycles of 15 frames, then
 *           RMAX(TDPTIM / 2 + 1) steps of 3 frames between tanks
 *   SQSPD   quark speed
 */
#include "waves.h"

static const unsigned char COUNTS[40][WC_COUNT] = {
    {15,  5,  1,  1,  0,  0,  0,  0,  0,  0,  0}, /* wave  1 */
    {17, 15,  1,  1,  1,  5,  0,  1,  0,  0,  0},
    {22, 25,  2,  2,  2,  6,  0,  3,  0,  0,  0},
    {34, 25,  2,  2,  2,  7,  0,  4,  0,  0,  0},
    {20, 20, 15,  0,  1,  0, 15,  1,  0,  0,  0}, /* wave  5: the first brains */
    {32, 25,  3,  3,  3,  7,  0,  4,  0,  0,  0},
    { 0,  0,  4,  4,  4, 12,  0,  0, 10,  0,  0}, /* wave  7: tanks */
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0},
    {60,  0,  3,  3,  3,  4,  0,  5,  0,  0,  0}, /* wave  9: the grunt swarm */
    {25, 20,  0, 22,  0,  0, 20,  1,  0,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave 11 */
    { 0,  0,  3,  3,  3, 13,  0,  0, 12,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0},
    {27,  5,  5,  5,  5, 20,  0,  2,  0,  0,  0}, /* wave 14: the hulks */
    {25, 20,  0,  0, 22,  2, 20,  1,  0,  0,  0},
    {35, 25,  3,  3,  3,  3,  0,  5,  0,  0,  0}, /* wave 16 */
    { 0,  0,  3,  3,  3, 14,  0,  0, 12,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0},
    {70,  0,  3,  3,  3,  3,  0,  5,  0,  0,  0},
    {25, 20,  8,  8,  8,  2, 20,  2,  0,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave 21 */
    { 0,  0,  3,  3,  3, 15,  0,  0, 12,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0},
    { 0,  0,  3,  3,  3, 13,  0,  6,  7,  0,  0},
    {25, 20, 25,  0,  1,  1, 21,  1,  0,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave 26 */
    { 0,  0,  3,  3,  3, 16,  0,  0, 12,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  1,  0,  0},
    {75,  0,  3,  3,  3,  4,  0,  5,  1,  0,  0},
    {25, 20,  0, 25,  0,  1, 22,  1,  1,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  1,  0,  0}, /* wave 31 */
    { 0,  0,  3,  3,  3, 16,  0,  0, 13,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  1,  0,  0},
    {30,  0,  3,  3,  3, 25,  0,  2,  2,  0,  0},
    {27, 15,  0,  0, 25,  2, 23,  1,  2,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  2,  0,  0}, /* wave 36 */
    { 0,  0,  3,  3,  3, 16,  0,  0, 14,  0,  0},
    {35, 25,  3,  3,  3,  8,  0,  5,  2,  0,  0},
    {80,  0,  3,  3,  3,  6,  0,  5,  1,  0,  0},
    {30, 15, 10, 10, 10,  2, 25,  1,  1,  0,  0}, /* wave 40 */
};

typedef struct { unsigned char hdr, min, max, v[40]; } param_row;

static const param_row PARAMS[P_COUNT] = {
    /* ROBSPD */ {0x8E,  10,  20, {20,15,15,15,15,15,15,15,15,15,14,14,14,14,14,13,13,13,13,13,14,14,14,14,14,14,13,13,13,13,13,13,12,12,12,12,12,12,15,12}},
    /* RMXSPD */ {0x8E,   3,  10, {9,7,6,5,5,5,5,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,3,3,4,3,3,3,3,3,3,3,3,3,4,3}},
    /* ENFNUM */ {0x0E,   8,  12, {10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11}},
    /* ENSTIM */ {0x8E,  13,  40, {30,28,26,24,22,20,18,18,16,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,14,14,14,14,14,14,14,14,14,14,14}},
    /* CDPTIM */ {0x8E,  12,  40, {30,28,26,24,30,20,18,16,18,25,12,12,12,25,25,12,12,12,18,20,14,14,14,14,14,25,14,14,18,25,12,12,12,12,25,12,12,12,18,20}},
    /* HLKSPD */ {0x8E,   5,   9, {8,8,7,7,7,7,7,6,6,6,6,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5}},
    /* BSHTIM */ {0x8E,  25,  80, {64,64,64,64,64,40,40,38,38,38,38,38,38,38,38,38,36,36,36,36,32,32,32,32,32,32,32,30,30,30,30,30,25,25,25,25,25,25,25,25}},
    /* BRNSPD */ {0x8E,   6,  10, {8,8,8,8,8,7,7,7,7,7,7,7,7,7,7,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6}},
    /* TNKSHT */ {0x8E,  20,  40, {32,32,32,32,32,32,32,30,30,30,30,30,30,28,28,28,28,28,28,28,30,30,30,30,30,30,28,28,28,28,28,26,26,26,26,26,24,24,24,24}},
    /* SHLSPD */ {0x0E, 160, 255, {176,176,176,176,176,176,176,176,176,176,176,176,176,176,176,176,176,176,176,176,184,184,184,184,184,184,184,184,184,184,192,192,192,192,192,192,192,192,192,192}},
    /* TDPTIM */ {0x8E,  12,  48, {16,16,16,16,16,16,16,16,16,16,16,16,16,16,15,15,15,15,15,15,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14}},
    /* SQSPD  */ {0x0E,  40,  68, {50,50,50,50,50,50,50,50,50,50,50,50,56,56,56,56,56,56,56,56,56,56,56,56,56,56,56,56,60,60,60,60,60,60,60,60,60,60,60,60}},
};

/* Past 40, waves 21-40 repeat (RRG23.ASM:552-555). */
static int index_of(int wave)
{
    if (wave < 1) wave = 1;
    while (wave > 40) wave -= 20;
    return wave - 1;
}

int wave_count(int wave, int column) { return COUNTS[index_of(wave)][column]; }

int wave_param(int param, int wave, int difficulty, int men)
{
    const param_row *r = &PARAMS[param];
    /* "easy setup" (RRG23.ASM:532-543) */
    if (difficulty < 5) {
        if (wave >= 14) difficulty = 5;
        if (wave >= 5 && men >= 3) difficulty = 5;
    }
    int v = r->v[index_of(wave)];
    int s = difficulty - 5, mag = s < 0 ? -s : s;
    int fm = ((r->hdr & 0x1F) * mag) & 0xFF;
    int p = v * fm;
    int d = (p >> 8) + ((p >> 7) & 1);
    int sub = (s < 0) != ((r->hdr & 0x80) != 0);
    int x;
    if (sub && d) { x = v - d; if (x < 0) return r->min; }
    else          { x = v + d; if (x > 255) return r->max; }
    if (x < r->min) x = r->min;
    if (x > r->max) x = r->max;
    return x;
}

int wave_bozo(int param, int wave)
{
    static const int T[4][4] = {     /* CDPTIM ENSTIM ROBSPD RMXSPD (RRG23.ASM:478-513) */
        {38, 96, 30, 15}, {38, 96, 25, 12}, {36, 48, 20, 10}, {30, 30, 15, 7} };
    if (wave < 1 || wave > 4) return -1;
    switch (param) {
    case P_CDPTIM: return T[wave - 1][0];
    case P_ENSTIM: return T[wave - 1][1];
    case P_ROBSPD: return T[wave - 1][2];
    case P_RMXSPD: return T[wave - 1][3];
    default:       return -1;
    }
}

void wave_safety_box(int wave, float *x0, float *x1, float *y0, float *y1)
{
    /* SAFTAB, RRP8.ASM:62-86: lines, then bytes (2 pixels each) */
    static const int T[6][4] = {
        {64, 176, 26, 122}, {72, 168, 26, 122}, {80, 160, 42, 106},
        {84, 157, 48, 96},  {93, 150, 53, 89},  {98, 148, 56, 92} };
    int i = wave <= 4 ? wave - 1 : wave <= 9 ? 4 : 5;
    if (i < 0) i = 0;
    *y0 = (float)T[i][0];
    *y1 = (float)T[i][1];
    *x0 = (float)T[i][2] * 2;
    *x1 = (float)T[i][3] * 2;
}
