# Robotron: 2084 gameplay specification (from the original 6809 source)

All references are `FILE:line` in `ext/rt/`. Numbers are quoted from the source unless marked
**(commonly cited, not verified in source)**.

## 0. Units, timing, coordinate system

* **Frame / tick.** The IRQ fires twice per video field ("IRQ 240 (16 MS)", RRF.ASM:29). The executive waits until
  `TIMER>=2` (RRS22.ASM:169-180), so the process scheduler runs **once per frame (~60 Hz)**. `NAP n,addr`
  sleeps a process for **n frames** ("A=SLEEP TIME X 16MSEC", RRS22.ASM:216). Sound timers also use frames (RRS22.ASM:495).
  The player-movement routine `PLAYER` runs once per frame from the IRQ (RRS22.ASM:1564).
  Velocity objects (spheroids, enforcers, sparks, quarks, tank shells) get `OX16+=OXV, OY16+=OYV`
  once per frame from the IRQ (`OPRC80`, RRS22.ASM:1710ff).
* **Coordinates.** Screen memory is byte-addressed: X is a **byte column = 2 horizontal pixels**, Y is a **line = 1 pixel**.
  16-bit positions are 8.8 fixed point: `OX16` = byte.fraction, `OY16` = line.fraction. Below, "px" = horizontal
  pixels (2 per byte), "ln" = lines. Velocities `OXV/OYV` are 8.8 per frame (1/256 byte or 1/256 line per frame).
* **Playfield** (RRF.ASM:67-70): `XMIN=7`, `XMAX=$8F(143)` bytes -> pixels 14..287 (≈274 px wide);
  `YMIN=24`, `YMAX=234` lines (211 lines high). Border drawn at XMIN-1/XMAX+1, YMIN-2/YMAX+2 (RRG23.ASM:1306-1319).
  Most objects are kept so that `x>=XMIN, y>=YMIN, x+w<=XMAX, y+h<=YMAX` (`CKLIMV`, RRH11.ASM:85-96).
  The IRQ updater simply refuses to apply a velocity component that would leave the field (object slides along wall,
  no reflection) (RRS22.ASM:1638-1653).
* **Random helpers.** `RAND` 8-bit LFSR-ish (RRS22.ASM:912). `RANDU(A)` = uniform 1..A (RRS22.ASM:885).
  `RMAX(A)` = "top weighted": r=rand byte; while r>A: r>>=1; if r==0: r=1 (RRS22.ASM:894-904). `SEED/HSEED/LSEED` are the
  three RNG bytes; many decisions test their sign bit (≈50 %).
* **Collision** is pixel-exact mask overlap (`COL0V`, RRS22.ASM:1038ff); when the mover hits something in a list,
  the *hit object's* collision vector (`OCVECT`) is called and only the first hit counts.
  `PCFLG=1` while the player is being checked; every enemy's kill routine checks `PCFLG` and, if set, does **not** die
  (the thing that kills the player survives) (e.g. RRP8.ASM:229-230).
* Sprite sizes (bytes x lines; px width = 2x): player 4x12, grunt 5x13, hulk 7x16, brain 7x16, tank 7x16,
  spheroid 8x15, quark 8x15, enforcer 5x11 (grows through 5 images), spark 4x7, shell 4x7, mommy 4x14, daddy 5x13,
  mikey 3x11, electrode 5x9 (vertical bar 3x9, "2084" 9x7, spiral 5x10), cruise-missile collision box 3x4 (RRF/each module).

## 1. Wave table

Source: `WVTAB` .. `WVTEND`, RRG23.ASM:596-652, loaded by `GETWV` (RRG23.ASM:528-593) into the enemy list `ELIST`
(RRF.ASM:619-643). Row order in ROM == `ELIST` order: 12 difficulty bytes then 9 counts, then `TNKCNT` which
`GETWV` explicitly clears (`CLR ,X+ GET TANK COUNT`, RRG23.ASM:592).

* **Wrap rule** (RRG23.ASM:552-555): `w = wave; while (w > 40) w -= 20;` -> waves 41..60 repeat 21..40, 61..80 repeat
  21..40, etc. The wave number is one byte; `INC` past 255 skips 0 (back to 1) (RRG23.ASM:420-422).
* Tanks and enforcers are **never seeded** at wave start (always 0); they are produced only by quarks and spheroids.
  (After a player death, surviving tanks are re-seeded directly and surviving enforcers are converted to spheroids, see §6.)
* Count rows are copied verbatim (no difficulty scaling) (RRG23.ASM:557-561).

### 1a. Paste-ready C tables

```c
/* Column order (all counts seeded at wave start):
 *  grunts, electrodes, mommies, daddies, mikeys, hulks, brains, spheroids, quarks, tanks, enforcers
 * tanks and enforcers are never seeded directly (always 0); they are only made by quarks/spheroids. */
static const unsigned char rt_wave_counts[40][11] = {
    {15,  5,  1,  1,  0,  0,  0,  0,  0,  0,  0}, /* wave  1 */
    {17, 15,  1,  1,  1,  5,  0,  1,  0,  0,  0}, /* wave  2 */
    {22, 25,  2,  2,  2,  6,  0,  3,  0,  0,  0}, /* wave  3 */
    {34, 25,  2,  2,  2,  7,  0,  4,  0,  0,  0}, /* wave  4 */
    {20, 20, 15,  0,  1,  0, 15,  1,  0,  0,  0}, /* wave  5 */
    {32, 25,  3,  3,  3,  7,  0,  4,  0,  0,  0}, /* wave  6 */
    { 0,  0,  4,  4,  4, 12,  0,  0, 10,  0,  0}, /* wave  7 */
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave  8 */
    {60,  0,  3,  3,  3,  4,  0,  5,  0,  0,  0}, /* wave  9 */
    {25, 20,  0, 22,  0,  0, 20,  1,  0,  0,  0}, /* wave 10 */
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave 11 */
    { 0,  0,  3,  3,  3, 13,  0,  0, 12,  0,  0}, /* wave 12 */
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave 13 */
    {27,  5,  5,  5,  5, 20,  0,  2,  0,  0,  0}, /* wave 14 */
    {25, 20,  0,  0, 22,  2, 20,  1,  0,  0,  0}, /* wave 15 */
    {35, 25,  3,  3,  3,  3,  0,  5,  0,  0,  0}, /* wave 16 */
    { 0,  0,  3,  3,  3, 14,  0,  0, 12,  0,  0}, /* wave 17 */
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave 18 */
    {70,  0,  3,  3,  3,  3,  0,  5,  0,  0,  0}, /* wave 19 */
    {25, 20,  8,  8,  8,  2, 20,  2,  0,  0,  0}, /* wave 20 */
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave 21 */
    { 0,  0,  3,  3,  3, 15,  0,  0, 12,  0,  0}, /* wave 22 */
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave 23 */
    { 0,  0,  3,  3,  3, 13,  0,  6,  7,  0,  0}, /* wave 24 */
    {25, 20, 25,  0,  1,  1, 21,  1,  0,  0,  0}, /* wave 25 */
    {35, 25,  3,  3,  3,  8,  0,  5,  0,  0,  0}, /* wave 26 */
    { 0,  0,  3,  3,  3, 16,  0,  0, 12,  0,  0}, /* wave 27 */
    {35, 25,  3,  3,  3,  8,  0,  5,  1,  0,  0}, /* wave 28 */
    {75,  0,  3,  3,  3,  4,  0,  5,  1,  0,  0}, /* wave 29 */
    {25, 20,  0, 25,  0,  1, 22,  1,  1,  0,  0}, /* wave 30 */
    {35, 25,  3,  3,  3,  8,  0,  5,  1,  0,  0}, /* wave 31 */
    { 0,  0,  3,  3,  3, 16,  0,  0, 13,  0,  0}, /* wave 32 */
    {35, 25,  3,  3,  3,  8,  0,  5,  1,  0,  0}, /* wave 33 */
    {30,  0,  3,  3,  3, 25,  0,  2,  2,  0,  0}, /* wave 34 */
    {27, 15,  0,  0, 25,  2, 23,  1,  2,  0,  0}, /* wave 35 */
    {35, 25,  3,  3,  3,  8,  0,  5,  2,  0,  0}, /* wave 36 */
    { 0,  0,  3,  3,  3, 16,  0,  0, 14,  0,  0}, /* wave 37 */
    {35, 25,  3,  3,  3,  8,  0,  5,  2,  0,  0}, /* wave 38 */
    {80,  0,  3,  3,  3,  6,  0,  5,  1,  0,  0}, /* wave 39 */
    {30, 15, 10, 10, 10,  2, 25,  1,  1,  0,  0}, /* wave 40 */
};

/* Difficulty-parameter rows: {hdr, min, max, v[40]} ; see RT_P_* enum for row order */
typedef struct { unsigned char hdr, min, max, v[40]; } rt_param_row;
enum { RT_P_ROBSPD, RT_P_RMXSPD, RT_P_ENFNUM, RT_P_ENSTIM, RT_P_CDPTIM, RT_P_HLKSPD,
       RT_P_BSHTIM, RT_P_BRNSPD, RT_P_TNKSHT, RT_P_SHLSPD, RT_P_TDPTIM, RT_P_SQSPD, RT_P_COUNT };
static const rt_param_row rt_wave_params[RT_P_COUNT] = {
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

/* Wave index helper (RRG23.ASM:552-555) */
static int rt_wave_index(int wave) { while (wave > 40) wave -= 20; return wave - 1; }
```

### 1b. Combined table (counts + raw difficulty bytes, difficulty setting 5 = values unchanged)

| Wave | Grunt | Elec | Mom | Dad | Mikey | Hulk | Brain | Sphd | Quark | ROBSPD | RMXSPD | ENFNUM | ENSTIM | CDPTIM | HLKSPD | BSHTIM | BRNSPD | TNKSHT | SHLSPD | TDPTIM | SQSPD |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 15 | 5 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 20 | 9 | 10 | 30 | 30 | 8 | 64 | 8 | 32 | 176 | 16 | 50 |
| 2 | 17 | 15 | 1 | 1 | 1 | 5 | 0 | 1 | 0 | 15 | 7 | 10 | 28 | 28 | 8 | 64 | 8 | 32 | 176 | 16 | 50 |
| 3 | 22 | 25 | 2 | 2 | 2 | 6 | 0 | 3 | 0 | 15 | 6 | 10 | 26 | 26 | 7 | 64 | 8 | 32 | 176 | 16 | 50 |
| 4 | 34 | 25 | 2 | 2 | 2 | 7 | 0 | 4 | 0 | 15 | 5 | 10 | 24 | 24 | 7 | 64 | 8 | 32 | 176 | 16 | 50 |
| 5 | 20 | 20 | 15 | 0 | 1 | 0 | 15 | 1 | 0 | 15 | 5 | 10 | 22 | 30 | 7 | 64 | 8 | 32 | 176 | 16 | 50 |
| 6 | 32 | 25 | 3 | 3 | 3 | 7 | 0 | 4 | 0 | 15 | 5 | 10 | 20 | 20 | 7 | 40 | 7 | 32 | 176 | 16 | 50 |
| 7 | 0 | 0 | 4 | 4 | 4 | 12 | 0 | 0 | 10 | 15 | 5 | 10 | 18 | 18 | 7 | 40 | 7 | 32 | 176 | 16 | 50 |
| 8 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 0 | 15 | 4 | 10 | 18 | 16 | 6 | 38 | 7 | 30 | 176 | 16 | 50 |
| 9 | 60 | 0 | 3 | 3 | 3 | 4 | 0 | 5 | 0 | 15 | 4 | 10 | 16 | 18 | 6 | 38 | 7 | 30 | 176 | 16 | 50 |
| 10 | 25 | 20 | 0 | 22 | 0 | 0 | 20 | 1 | 0 | 15 | 4 | 10 | 14 | 25 | 6 | 38 | 7 | 30 | 176 | 16 | 50 |
| 11 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 0 | 14 | 4 | 10 | 14 | 12 | 6 | 38 | 7 | 30 | 176 | 16 | 50 |
| 12 | 0 | 0 | 3 | 3 | 3 | 13 | 0 | 0 | 12 | 14 | 4 | 10 | 14 | 12 | 5 | 38 | 7 | 30 | 176 | 16 | 50 |
| 13 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 0 | 14 | 4 | 10 | 14 | 12 | 5 | 38 | 7 | 30 | 176 | 16 | 56 |
| 14 | 27 | 5 | 5 | 5 | 5 | 20 | 0 | 2 | 0 | 14 | 4 | 10 | 14 | 25 | 5 | 38 | 7 | 28 | 176 | 16 | 56 |
| 15 | 25 | 20 | 0 | 0 | 22 | 2 | 20 | 1 | 0 | 14 | 4 | 10 | 14 | 25 | 5 | 38 | 7 | 28 | 176 | 15 | 56 |
| 16 | 35 | 25 | 3 | 3 | 3 | 3 | 0 | 5 | 0 | 13 | 4 | 10 | 14 | 12 | 5 | 38 | 6 | 28 | 176 | 15 | 56 |
| 17 | 0 | 0 | 3 | 3 | 3 | 14 | 0 | 0 | 12 | 13 | 4 | 10 | 14 | 12 | 5 | 36 | 6 | 28 | 176 | 15 | 56 |
| 18 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 0 | 13 | 4 | 10 | 14 | 12 | 5 | 36 | 6 | 28 | 176 | 15 | 56 |
| 19 | 70 | 0 | 3 | 3 | 3 | 3 | 0 | 5 | 0 | 13 | 4 | 10 | 14 | 18 | 5 | 36 | 6 | 28 | 176 | 15 | 56 |
| 20 | 25 | 20 | 8 | 8 | 8 | 2 | 20 | 2 | 0 | 13 | 4 | 10 | 14 | 20 | 5 | 36 | 6 | 28 | 176 | 15 | 56 |
| 21 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 0 | 14 | 4 | 11 | 15 | 14 | 5 | 32 | 6 | 30 | 184 | 14 | 56 |
| 22 | 0 | 0 | 3 | 3 | 3 | 15 | 0 | 0 | 12 | 14 | 4 | 11 | 15 | 14 | 5 | 32 | 6 | 30 | 184 | 14 | 56 |
| 23 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 0 | 14 | 4 | 11 | 15 | 14 | 5 | 32 | 6 | 30 | 184 | 14 | 56 |
| 24 | 0 | 0 | 3 | 3 | 3 | 13 | 0 | 6 | 7 | 14 | 4 | 11 | 15 | 14 | 5 | 32 | 6 | 30 | 184 | 14 | 56 |
| 25 | 25 | 20 | 25 | 0 | 1 | 1 | 21 | 1 | 0 | 14 | 4 | 11 | 15 | 14 | 5 | 32 | 6 | 30 | 184 | 14 | 56 |
| 26 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 0 | 14 | 4 | 11 | 15 | 25 | 5 | 32 | 6 | 30 | 184 | 14 | 56 |
| 27 | 0 | 0 | 3 | 3 | 3 | 16 | 0 | 0 | 12 | 13 | 3 | 11 | 15 | 14 | 5 | 32 | 6 | 28 | 184 | 14 | 56 |
| 28 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 1 | 13 | 3 | 11 | 15 | 14 | 5 | 30 | 6 | 28 | 184 | 14 | 56 |
| 29 | 75 | 0 | 3 | 3 | 3 | 4 | 0 | 5 | 1 | 13 | 4 | 11 | 15 | 18 | 5 | 30 | 6 | 28 | 184 | 14 | 60 |
| 30 | 25 | 20 | 0 | 25 | 0 | 1 | 22 | 1 | 1 | 13 | 3 | 11 | 14 | 25 | 5 | 30 | 6 | 28 | 184 | 14 | 60 |
| 31 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 1 | 13 | 3 | 11 | 14 | 12 | 5 | 30 | 6 | 28 | 192 | 14 | 60 |
| 32 | 0 | 0 | 3 | 3 | 3 | 16 | 0 | 0 | 13 | 13 | 3 | 11 | 14 | 12 | 5 | 30 | 6 | 26 | 192 | 14 | 60 |
| 33 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 1 | 12 | 3 | 11 | 14 | 12 | 5 | 25 | 6 | 26 | 192 | 14 | 60 |
| 34 | 30 | 0 | 3 | 3 | 3 | 25 | 0 | 2 | 2 | 12 | 3 | 11 | 14 | 12 | 5 | 25 | 6 | 26 | 192 | 14 | 60 |
| 35 | 27 | 15 | 0 | 0 | 25 | 2 | 23 | 1 | 2 | 12 | 3 | 11 | 14 | 25 | 5 | 25 | 6 | 26 | 192 | 14 | 60 |
| 36 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 2 | 12 | 3 | 11 | 14 | 12 | 5 | 25 | 6 | 26 | 192 | 14 | 60 |
| 37 | 0 | 0 | 3 | 3 | 3 | 16 | 0 | 0 | 14 | 12 | 3 | 11 | 14 | 12 | 5 | 25 | 6 | 24 | 192 | 14 | 60 |
| 38 | 35 | 25 | 3 | 3 | 3 | 8 | 0 | 5 | 2 | 12 | 3 | 11 | 14 | 12 | 5 | 25 | 6 | 24 | 192 | 14 | 60 |
| 39 | 80 | 0 | 3 | 3 | 3 | 6 | 0 | 5 | 1 | 15 | 4 | 11 | 14 | 18 | 5 | 25 | 6 | 24 | 192 | 14 | 60 |
| 40 | 30 | 15 | 10 | 10 | 10 | 2 | 25 | 1 | 1 | 12 | 3 | 11 | 14 | 20 | 5 | 25 | 6 | 24 | 192 | 14 | 60 |

Notable waves: brain waves 5,10,15,20,25,30,35,40 (all humans of one type on 5/10/15, mixed on 20, etc.);
"tank/quark" waves 7,12,17,22,27,(24 has 7 quarks+grunts),32,37; hulk waves 14,24,34; grunt-swarm waves 9,19,29,39.

## 2. Per-wave difficulty parameters

| Param | ROM row | Meaning / where used | Units |
|---|---|---|---|
| ROBSPD | RRG23:598 | Grunt "wait": after each step a grunt reloads its counter with `RANDU(ROBSPD)` (RRP8.ASM:167-169). Speeds up during wave. | ticks of 4 frames |
| RMXSPD | RRG23:601 | Floor ("max speed") for ROBSPD; itself lowered over time (RRG23.ASM:437-458). | ticks of 4 frames |
| ENFNUM | RRG23:604 | Per spheroid: enforcers to drop = `ceil(RMAX(ENFNUM)/2)` (RRC11.ASM:42-46). Per quark: tanks = same formula (RRTK4.ASM:45-49). Comment: "# OF ENFORCERS/CIRCLE X2". | count x2 |
| ENSTIM | RRG23:607 | Enforcer shot timer: `RMAX(ENSTIM)` enforcer steps (3 frames each) between sparks (RRC11.ASM:263-265, 347-349). | x3 frames |
| CDPTIM | RRG23:610 | Spheroid drop timer: first drop after `RMAX(CDPTIM)` animation cycles of 10 frames; then `RMAX(CDPTIM/4)` cycles of 16 frames between drops (RRC11.ASM:39-41, 76-80). | x10 / x16 frames |
| HLKSPD | RRG23:613 | Hulk sleeps HLKSPD frames between steps (RRH11.ASM:79-80). | frames |
| BSHTIM | RRG23:616 | Brain fires a cruise missile every `RMAX(BSHTIM)` brain steps (RRB10.ASM:64-66, 620-622). | x BRNSPD frames |
| BRNSPD | RRG23:619 | Brain sleeps BRNSPD frames between steps (RRB10.ASM:229-231). | frames |
| TNKSHT | RRG23:622 | Tank fires every TNKSHT tank steps (tank step = 2 frames, `TNKSPD=2`, RRG23.ASM:677-678) (RRTK4.ASM:298-300, 388-389). | x2 frames |
| SHLSPD | RRG23:625 | Tank-shell speed scale (RRTK4.ASM:421-433, 516-519). | see §3 |
| TDPTIM | RRG23:628 | Quark: first tank after `RMAX(TDPTIM)` cycles of 15 frames; then `RMAX(TDPTIM/2+1)` steps of 3 frames between tanks (RRTK4.ASM:42-44, 144-148). | x15 / x3 frames |
| SQSPD | RRG23:631 | Quark speed: `vx=±RMAX(SQSPD)*4/256` byte/frame, `vy=±RMAX(SQSPD)*8/256` line/frame (RRTK4.ASM:60-95). | 1/64 byte, 1/32 line per frame |

### 2a. Difficulty adjustment (operator "MASTER DIFFICULTY", CMOS `GA1`, 0..10)

Each difficulty row has a 3-byte header `{hdr, min, max}` (RRG23.ASM:597,600,...). `hdr & 0x1F` = fraction (all are
14), `hdr & 0x80` = direction. Algorithm (RRG23.ASM:528-591):

```c
/* diff = GA1 (0..10). "Easy setup" override (RRG23.ASM:532-543):
   if (diff < 5) { if (wave >= 14) diff = 5;
                   if (wave >= 5 && lives_incl_current >= 3) diff = 5; }   (code tests PLAS>=3) */
static unsigned char rt_param(const rt_param_row *r, int wave, int diff)
{
    int v   = r->v[rt_wave_index(wave)];
    int s   = diff - 5, mag = s < 0 ? -s : s;
    int fm  = ((r->hdr & 0x1F) * mag) & 0xFF;
    int p   = v * fm;
    int d   = (p >> 8) + ((p >> 7) & 1);                 /* MUL, ADCA #0 (round) */
    int sub = (s < 0) != ((r->hdr & 0x80) != 0);         /* EORB hdr, BPL */
    int x;
    if (sub && d) { x = v - d; if (x < 0) return r->min; }
    else          { x = v + d; if (x > 255) return r->max; }
    if (x < r->min) x = r->min;
    if (x > r->max) x = r->max;                          /* clamp applies even at diff 5 */
    return (unsigned char)x;
}
```
Effect: rows with hdr `$8E` (timers/waits) get *smaller* when difficulty > 5; rows with `$0E` (ENFNUM, SHLSPD, SQSPD)
get *larger*. Each difficulty step changes values by 14/256 ≈ 5.5 %.
At difficulty 5 the table values are used as-is (all are already within min/max).

**Factory default difficulty:** the CMOS default table sets `GA1 = $02` (= 2) (RRTESTB.ASM:74), while the operator text table labels 0 "extra liberal", 3 "liberal", **5 "recommended"**, 6 "conservative", 8 "extra conservative" (RRTESTB.ASM:213-217). Because 2 < 5, the easy-setup override applies at the source default: difficulty 2 is used only on waves 1-4, and on waves 5-13 while the player has fewer than 3 lives (incl. current); otherwise 5. For a faithful "arcade default" feel use 5 (table values unmodified) unless you specifically want the source's CMOS default.

### 2b. "Bozo" table (RRG23.ASM:478-513)
At every turn start (`PLRES`), on waves 1-4: if this is the **last life** (`PLAS==0`), or on waves 1-2 the player has
fewer than `NSHIP-1` reserve lives, override with easier values:

| Wave | CDPTIM | ENSTIM | ROBSPD | RMXSPD |
|---|---|---|---|---|
| 1 | 38 | 96 | 30 | 15 |
| 2 | 38 | 96 | 25 | 12 |
| 3 | 36 | 48 | 20 | 10 |
| 4 | 30 | 30 | 15 | 7 |

## 3. Enemy / object behaviour

### Grunt (RRP8.ASM)
* Placement: random, outside the wave's **safety box** (§6), not overlapping already-drawn objects (`ASCAN`) (RRP8.ASM:24-58).
  Initial counter `RANDU(ROBSPD)`.
* One process drives all grunts: waits 10 frames after start, then loops every **4 frames** (`NAP 4`, RRP8.ASM:154,227).
  Each grunt's counter is decremented; at 0 it steps and reloads `RANDU(ROBSPD)` (1..ROBSPD).
* Step (RRP8.ASM:170-199): `dy = gruntY - playerY`: if `dy >= 2` move **-4 ln**; if `dy <= -2` move **+4 ln**; else no Y move.
  `dx = gruntX - playerX` (bytes): if `dx >= 1` move **-2 bytes (-4 px)**; if `dx <= 0` move **+2 bytes** (quirk: equal X still
  steps right). Moves are rejected per axis if they'd leave `[YMIN, YMAX-12]` / `[XMIN, XMAX-5]`.
  4-frame walk animation (RWDP1..4). Max speed 1 step / 4 frames = 1 px/frame.
* After moving, checks electrodes: on contact **both grunt and electrode die** and the player is credited 100 (RRP8.ASM:210-219).
  Grunts do **not** interact with humans.
* "Robot move" sound once per pass if any grunt stepped (RRP8.ASM:223-226).
* **Speed-up**:
  - Each grunt killed: `new = ROBSPD*0xE0>>8` (×0.875); applied only if `new >= RMXSPD` (RRP8.ASM:241-246).
  - Game exec every 15 frames; first adjustment after 18 passes (270 frames), then every 15 passes (**225 frames ≈ 3.75 s**),
    only if `ROBCNT < 30`: if the player scored since last check: `RMXSPD-=1, ROBSPD-=2`; if not ("BONE HIM FOR STALLING"):
    `RMXSPD-=2, ROBSPD-=4`; `RMXSPD>=1`, `ROBSPD>=RMXSPD` (RRG23.ASM:437-458; `SCRFLG` set by every score, RRS22.ASM:1454).
* Kill: 100 pts (`$0110`), explosion `EXST`, sound RBSND (RRP8.ASM:229-248).

### Electrode ("post") (RRP8.ASM:102-146, 251-290)
* Static, in the "fatal" list: touching kills the player. Placement: random, outside a safety box slightly shrunk from
  the grunt box (`+$03FC/+$02FD` adjust: Ymin+4, Ymax-4, Xmin+3, Xmax-3 bytes) (RRP8.ASM:116-122) and not overlapping.
* Destroyed by a player shot, by a grunt walking into it, or by a **hulk** walking into it (hulk calls `COL0` on the post
  list, RRH11.ASM:66-71, which invokes `PSTKIL`). **0 points.** 3-image shrink animation (6,3,2 frames), sound PSKSND.
* Humans treat electrodes as obstacles and turn (RRH11.ASM:344-352). Brains, progs, enforcers, sparks, tanks, shells,
  spheroids and quarks ignore them.
* Shape/colour per wave (`GTWCOL`, RRG23.ASM:373-401), index `(wave-1) % 10`:

| (wave-1)%10 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| shape (image offset) | star $00 | snowflake $10 | square $20 | triangle $30 | vertical bar $40 | diamond $50 | spike $70 | spiral $80 | star $00 | "2084" $60 |
| wall colour byte | $22 | $55 | $11 | $EE | $77 | $33 | $44 | $88 | $00 | $CC |
| post colour byte | $FF | $EE | $BB | $DD | $EE | $FF | $11 | $BB | $DD | $AA |
| laser-wall flash colour | $99 | $00 | $99 | $66 | $99 | $99 | $99 | $11 | $AA | $99 |
(Shapes: RRP8.ASM:294-384. Colours are palette indices into the color RAM, not RGB.)

### Hulk (RRH11.ASM:32-261)
* Invulnerable; not counted for wave completion. Placement: outside safety box (RRH11.ASM:211-220).
* Target at spawn: with `SEED <= $C0` (~75 %) a human chosen round-robin from the human table (`GTARG`), else the player
  (RRH11.ASM:224-230). If the target human is gone, seeks the player (RRH11.ASM:146-148).
* Moves only orthogonally. Sleeps `HLKSPD` frames per step. Step tables: left/right alternate **3,4,3,4 half-bytes =
  -1.5/-2 bytes (3/4 px)**; up/down **2 ln** (RRH11.ASM:238-261). 4-frame walk anim.
* Direction choice `HULKND` (RRH11.ASM:142-187): new-direction timer `(LSEED & $1F)+1` steps. Alternates axes: if last
  move was horizontal, choose vertical toward `targetY + rand(-16..15)`; otherwise horizontal toward `targetX + rand(-16..15)`.
  Also re-chooses immediately if the step would leave the field.
* Every step: collides with electrodes (destroys them) and humans (kills them -> skull) (RRH11.ASM:64-71).
* Shot by player: no damage; **knock-back** in the laser's direction: X += laser dx × (1 or 2 bytes, 2 if SEED>=0 ≈50 %),
  Y += laser dy × (1 or 2 lines, 2 if LSEED<$C0 ≈75 %), if in bounds; then re-checks electrodes/humans at new spot;
  sound HKHSND (RRH11.ASM:100-136). The laser is consumed.
* Touching a hulk kills the player. Hulks never die, so `HLKCNT` is preserved across deaths.

### Brain (RRB10.ASM:24-394)
* Spawn near an edge: random point, then 50 %: X forced to within 16 bytes of the left or right wall, else Y within
  32 lines of top or bottom (`GBCORD`, RRB10.ASM:74-104). Initial shot timer `RMAX(BSHTIM)`.
* Target: nearest human by |dx|+|dy| (`GETHTG`, RRB10.ASM:109-141); if none left, the player.
* Process: waits 12 frames, then each `BRNSPD` frames: X step **1 byte (2 px)** toward target unless |dx|<=2 bytes;
  Y step **1 ln** toward target (moves +1 when equal) (RRB10.ASM:163-188).
* **Reprogramming** (RRB10.ASM:189-199, 242-340): when within ±3 bytes and ±3 lines of the target human, the brain
  grabs it: 20 iterations of (flash human ±0..7 ln, 2 frames, 2 frames) ≈ 80 frames with PRGSND each loop; then
  HPSND and the human becomes a **Prog** (§Prog). Human counts are decremented (no skull, no score). If the brain is
  shot during conversion the human dies with a skull (RRB10.ASM:376-388).
* Fires cruise missiles when its shot counter (decremented each step) hits 0; reload `RMAX(BSHTIM)`; max **8** missiles
  alive (`BCMCNT<8`) (RRB10.ASM:619-654).
* Kill: **500** pts (`$0150`), BKSND (RRB10.ASM:368-393).

### Prog (RRB10.ASM:400-565)
* Moves every **3 frames**, orthogonally, **±2 bytes (4 px)** or **±4 ln** per step (RRB10.ASM:590-605), with a trail of
  shadow images (super-process). Seeks `player + offset`, offset X ∈ about -28..+28 bytes, Y ∈ about -16..+18 lines
  (`GPOFF`, RMAX-based). 50 % (HSEED sign) chooses axis X or Y each re-aim (`GPDIR`). Re-aims when blocked or when
  `LSEED>$E4` (~10.5 % per step); new offset when `SEED>$F8` (~2.7 % per step).
* Does not collide with humans/electrodes. Kills player on contact. Kill: **100** pts (`$0110`), PGKSND.
* **Not counted** for wave completion (`WVCHEK`, RRG23.ASM:403-409) and not saved across player death (lost).

### Cruise missile (RRB10.ASM:619-771)
* Spawned at brain centre (+3 bytes,+4 ln). Every 2 frames it makes 2 moves of (dx,dy) where dx∈{-1,0,+1} byte and
  dy∈{-1,0,+1} line -> **2 px/frame horizontally, 1 ln/frame vertically** (RRB10.ASM:703-750).
* Aim (`GCMDIR`, RRB10.ASM:672-698): if SEED<0: dx toward `playerX + (rand&15)-6`; then if LSEED>=0 also dy. If SEED>=0:
  only dy toward `playerY + (rand&15)-6`. => ~50 % vertical only, 25 % horizontal only, 25 % diagonal.
  Re-aim every `RMAX(7)` move-pairs. **Reflects** off walls (RRB10.ASM:715-731). Drawn as an 8-point trail.
* Shootable: **25** pts (`$0025`), CMKSND. Not counted for wave completion.

### Spheroid ("circle") (RRC11.ASM:26-250)
* Spawn: random Y, X = `XMIN+2` (SEED<0) or `XMAX-8` (RRC11.ASM:31-38).
* Motion: random acceleration, X acc = `(HSEED&$1F)-16`, Y acc = `((LSEED^SEED)&$3F)-32` (1/256 per 2 frames), new accel
  every `RANDU(15)` 2-frame steps; `|vx|<=$100` (1 byte = 2 px/frame), `|vy|<=$200` (2 ln/frame), with damping
  `vx -= vx/64`, `vy -= vy/128` each step (RRC11.ASM:134-188).
* Phase 1: 5-image pulsing anim (2 frames each = 10-frame cycle); drop timer `RMAX(CDPTIM)` cycles (only counts while
  game running). Phase 2: 8-image anim (16-frame cycle); every `RMAX(CDPTIM/4)` cycles drops an enforcer, unless
  **8 enforcers** already alive or CPU overloaded (`OVCNT>=17`) (RRC11.ASM:76-104).
  Enforcers per spheroid: `ceil(RMAX(ENFNUM)/2)` → 1..5 (ENFNUM 10) or 1..6 (11).
* Phase 3 (all dropped): flies horizontally at 1 byte (2 px)/frame toward a random side (SEED sign) and disappears
  when within 3 bytes of left wall or 10 of right; `CIRCNT--` (no points) (RRC11.ASM:106-129).
* Kill: **1000** pts (`$0210`), CRKSND; 7-step burst animation then a "1000" picture shown 30×2=60 frames (RRC11.ASM:192-250).
* Collision box is always the full-size image (`FONIPC=CIRP4`) regardless of animation frame (RRC11.ASM:48-49).

### Enforcer (RRC11.ASM:254-427)
* Dropped at the spheroid position, sound ENDSND. "Grows" through 5 images, 8 frames each (40 frames) (RRC11.ASM:275-281);
  is in the object list (shootable, lethal) while growing.
* Every **3 frames** (RRC11.ASM:285-292): new velocity when its timer expires (timer = `rand & $1F` steps; 0 → 256).
  Velocity (`ENFNV`, RRC11.ASM:297-341): target = `playerX + (rand>>3 signed, -16..15 bytes)`,
  `playerY + (HSEED>>3 signed, -16..15 ln)`; `v = 2*(target - pos)/256` per frame → it would cover the distance in
  ~128 frames (homing with lag, no lead).
* Fires a spark when its shot counter (per 3-frame step) reaches 0; reload `RMAX(ENSTIM)`; max **20** sparks
  (`SPKCNT<20`) and not when overloaded.
* Kill: **150** pts (`$0115`), ENKSND, explosion.

### Enforcer spark (RRC11.ASM:345-462)
* Aim (no lead): target = player + random offset (-16..15 bytes / lines); offset X is zeroed if player is within 16 bytes
  of left wall. `v = 4*(target - pos)/256` per frame (reaches target in ~64 frames). Plus a constant random acceleration
  `(rand&$1F)-16` /256 added to velocity every 4 frames (curving shots).
* Lifetime `(HSEED&$F)+20` steps × 4 frames = 80-140 frames. Not reflected (slides on walls).
* Shootable: **25** pts, SPKSND.

### Quark ("square") (RRTK4.ASM:27-199)
* Spawn: random X, Y = `YMIN+2` or `YMAX-14` (50 %). Velocity from SQSPD (see §2); signs forced away from nearby walls,
  else random; new velocity every `(SEED&$1F)+1` 3-frame steps.
* Phase 1: 5-image anim (3 frames each, 15-frame cycle), timer `RMAX(TDPTIM)` cycles. Phase 2: 9-image anim, drops a
  tank every `RMAX(TDPTIM/2+1)` 3-frame steps, unless **20 tanks** alive / no resources / overloaded.
  Tanks per quark: `ceil(RMAX(ENFNUM)/2)`.
* Phase 3: flies vertically at 2 ln/frame and vanishes near top/bottom (`SQCNT--`, no points).
* Kill: **1000** pts (`$0210`), SQKSND, burst + "1000" popup. Full-size collision box (`FONIPC=SQP4`).

### Tank (RRTK4.ASM:203-382)
* Dropped at quark (+2 bytes,+6 ln) as a mini-tank growing through 4 images, 12 frames each (48 frames), TKDSND.
  (After player death, surviving tanks are re-seeded at random outside the safety box with shot timer `TNKSHT+(SEED&$1F)`.)
* Moves every **2 frames** (`TNKSPD=2`): X **±0.5 byte (1 px)**, Y **±1 ln or 0** (0 if target within 16 lines).
  Target: player if `SEED<=$60` (~38 %), else a random point. New direction every `(SEED&$1F)+1` steps or when blocked.
* Fires every TNKSHT steps (`TNKFIR`, RRTK4.ASM:386-561), TKFSND. Kill: **200** pts (`$0120`), TNKSND.

### Tank shell (RRTK4.ASM:386-603)
* 50 % (`SEED>=$80`) **rebound shot**, else **direct shot**.
  - Direct: target = player + rand(-16..15) (X offset 0 if player near left wall). `v = ((|d|*SHLSPD)>>8)*8/256` per frame
    (≈ d*SHLSPD/8192; with SHLSPD 176 reaches target in ~46 frames).
  - Rebound: aims at a wall point halfway (in the perpendicular axis) between tank and player so the bounce comes back at the
    player (left/right wall or top/bottom wall, chosen at random, wall chosen by player half-screen with 1/8 inversion);
    velocity `(target-pos)/256` doubled repeatedly until `|vx|>=T` or `|vy|>=2T`, `T≈SHLSPD/256` byte/frame (RRTK4.ASM:459-552).
* Bounces off all four walls (velocity component complemented), SRBSND (RRTK4.ASM:565-590).
* Lifetime `(HSEED&$1F)+48` steps × 2 frames = 96-158 frames.
* Shootable: **25** pts, SHKSND.
* **Quirk:** `SHLCNT` is incremented on fire and decremented only when a shell is shot, not when it expires
  (RRTK4.ASM:393-396, 581, 596). Tanks stop firing once `SHLCNT>20` → at most 21 un-shot shells per life/wave
  (reset in `PLINIT`, RRG23.ASM:679).

### Humans (Mommy, Daddy, Mikey) (RRH11.ASM:264-557)
* Placed anywhere at random (no safety box, no overlap check) (RRH11.ASM:391-394); start times staggered 1-8 frames.
* Random walk, **no fleeing/panic logic**: step every **8 frames** (RRH11.ASM:360); 8 directions chosen by `SEED&7`
  (L,R,D,U,UL,UR,DR,DL); horizontal steps alternate 2,1 half-bytes (2 px,1 px), vertical 1 ln (RRH11.ASM:509-557).
  Direction kept for `(LSEED&$7F)+1` steps, re-chosen early if blocked by an electrode or wall.
* Killed by hulks (skull) or converted by brains. Grunts, shots, missiles, sparks, shells do not affect them.
* Player touch = **rescue** (§4). Mikey 3x11, Mommy 4x14, Daddy 5x13.

## 4. Scoring

`SCORE` takes `D=$AXBB`: points = BCD(BB) × 10^A (RRS22.ASM:1450-1484).

| Object | Code | Points | Ref |
|---|---|---|---|
| Grunt (also when it walks into an electrode) | $0110 | 100 | RRP8.ASM:237 |
| Electrode | — | 0 | RRP8.ASM:251-266 |
| Hulk | — | 0 (invulnerable) | RRH11.ASM:100-136 |
| Brain | $0150 | 500 | RRB10.ASM:391 |
| Prog | $0110 | 100 | RRB10.ASM:563 |
| Cruise missile | $0025 | 25 | RRB10.ASM:767 |
| Spheroid | $0210 | 1000 | RRC11.ASM:203 |
| Enforcer | $0115 | 150 | RRC11.ASM:423 |
| Enforcer spark | $0025 | 25 | RRC11.ASM:458 |
| Quark | $0210 | 1000 | RRTK4.ASM:120 |
| Tank | $0120 | 200 | RRTK4.ASM:346 |
| Tank shell | $0025 | 25 | RRTK4.ASM:599 |
| Human rescue | $0210..$0250 | 1000,2000,3000,4000,5000 | RRH11.ASM:498-503 |

* **Rescue progression** (RRH11.ASM:453-470): `SAVCNT++`, capped at 5; award `SVITAB[SAVCNT-1]` = 1000×SAVCNT and show the
  matching "1000".."5000" picture at the human's position for **60 frames**. `SAVCNT` is cleared by `PLINIT`
  (RRG23.ASM:672), which runs at the start of **every wave and every new life** → resets on wave start and on death.
  It is *not* reset when a human is killed.
* Human killed (hulk, or brain shot mid-conversion): skull picture for **90 frames**, HKSND (RRH11.ASM:473-478).
* **Extra life**: CMOS `REPLAY` (RRS22.ASM:1486-1513). Factory default `$25` → **every 25,000** points (RRTESTB.ASM:64;
  the comment there says "10,000" but the value is $25 = 25,000; operator text marks 25k "RECOMMENDED", 20k/30k/50k options,
  0 = none, RRTESTB.ASM:186-191). Next threshold = previous + interval (repeats indefinitely). Sound RPSND.
* **Starting lives**: CMOS `NSHIP` default **3** (RRTESTB.ASM:65) = 1 in play + 2 in reserve. Reserve display max 7 men.
* **End-of-wave bonus**: none (wave completion awards no score; it only does `PLAS++` to cancel the `PLAS--` of the next
  turn start, RRG23.ASM:420-432).
* Score display: 7 digits (RRS22.ASM:1536).

## 5. Player

* **Movement** (`PLAYRV`, RRG23.ASM:684-770): once per frame; table `PITAB` per stick bits (U=1,D=2,L=4,R=8):
  X **±0.5 byte = 1 px/frame**, Y **±1 ln/frame**. Diagonals are not normalised (1 px + 1 ln). Opposing/3-way
  combinations (U+D, L+R, any 3 bits) = no movement. Clamp: Y ∈ [24, 223] (YMAX-11), X ∈ [7, 140] bytes (XMAX-3).
  Start position each life: `PCOORD=$4A7C` → X byte 74 (px 148), Y 124 (RRF.ASM:78, RRG23.ASM:664).
  Walk animation advances every 2 frames (RRG23.ASM:737-742). Sprite faces move direction, not fire direction.
* **Firing** (`LSPROC`, RRG23.ASM:912-972): fire stick = 4 switches (fire U/D on PIA2 b6/b7, L/R on PIA3 b0/b1) → 8 directions
  (opposing or 3-switch combinations fire nothing). Auto-fire while held:
  - direction change resets a counter; a shot is fired when the (unchanged) direction has been held for **2 frames**, then at
    counts 8,16,24,… → **one shot every 8 frames (7.5 shots/s)**;
  - max **4 shots** on screen (`LCNT<4`); if 4 are out, it retries next frame.
* **Shot speed**: horizontal 3 bytes (**6 px**)/frame, vertical **6 ln**/frame, diagonal 3 bytes + 6 ln per frame
  (RRG23.ASM:988-1208). Spawn offsets from player top-left (`LTAB`, RRG23.ASM:974-986): U (+2,-1), D (+2,+4), L (0,+4),
  R (+2,+4), UL (0,0), UR (+2,0), DL (0,+4), DR (+2,+4) (bytes, lines). Images: horizontal 3 bytes×1 ln,
  vertical 1×6, diagonal 3×6.
* Shot collision order each frame: electrodes, then robot list (grunts, hulks, brains, progs, missiles, tanks), then
  velocity-object list (spheroids, enforcers, sparks, quarks, shells). Only the first hit object is affected; shot dies.
  Shots do not hit humans. At a wall the shot leaves a brief 2-3 frame flash in `LASCOL`.
* **Player death** (`COLCHK`, RRG23.ASM:802-824): every frame checks robot list, electrodes, velocity objects (death), then
  humans (rescue). Sound PDSND.

## 6. Wave flow

* **Turn/wave start** (`PLSTRT`, RRG23.ASM:104-208): clear screen, draw border/score; then in order: restore enemy state
  (`PLRES`), `PLINIT` (player to centre, clears SAVCNT, missile/spark/enforcer/shell counters), hulks, brains, tanks, humans,
  electrodes, grunts, spheroids, quarks are created (frozen, `STATUS=$19`).
  - Non-brain waves: `APPEAR` "materialize": objects in the robot list start their appear effect one per frame
    (every 4th uses the horizontal-line variant `HAPST`, others vertical `APST`) (RRG23.ASM:215-253), then the player
    appears (vertical+horizontal line sweep, then diagonal) (RRG23.ASM:180-184, 327-369).
  - Brain waves (`BRNCNT>0`): "transporter" glitter effect for **150 frames** instead (RRG23.ASM:175-178; RRT2.ASM).
  - Then `CLR STATUS` → play begins; electrodes are refreshed a few more times over the next ~22 frames.
* **Safety box** (`SAFTAB`, RRP8.ASM:62-86) – grunts, hulks and tanks are re-rolled while inside it
  (Ymin, Ymax lines; Xmin, Xmax bytes; exclusive):

| Wave | Y range | X range (bytes) | X range (px) |
|---|---|---|---|
| 1 | 64-176 | 26-122 | 52-244 |
| 2 | 72-168 | 26-122 | 52-244 |
| 3 | 80-160 | 42-106 | 84-212 |
| 4 | 84-157 | 48-96 | 96-192 |
| 5-9 | 93-150 | 53-89 | 106-178 |
| 10+ | 98-148 | 56-92 | 112-184 |

  Electrodes use the box shrunk by ~4 lines / 3 bytes per side. Humans: anywhere. Brains: near edges. Spheroids: left/right
  edge. Quarks: top/bottom edge.
* **Wave complete** when `ROBCNT+CIRCNT+ENFCNT+BRNCNT+TNKCNT+SQCNT == 0` (grunts, spheroids, enforcers, brains, tanks,
  quarks), checked every 15 frames (RRG23.ASM:403-432). **Not required**: hulks, electrodes, progs, cruise missiles,
  sparks, shells, humans. Spheroids/quarks that fly off after dropping their quota count as gone.
  On completion: wave++, WVSND, load next wave, kill all processes, marquee effect (`RMST`, RRM1.ASM), next turn start.
  Remaining humans are simply lost (no bonus).
* **Player death** (RRG23.ASM:828-893): death animation (player flashes 10×8 frames, then colour cycle; RRX7.ASM:433ff),
  all processes killed. Then:
  - Enemy counts in `ELIST` are live (decremented on each kill/rescue/conversion), so the wave **restarts with the remaining
    grunts, electrodes, humans, hulks, brains, spheroids, quarks and tanks**, all re-placed at new random positions
    (player back at centre, safety box applies again). Progs, missiles, sparks, shells are lost.
  - Leftover **enforcers → spheroids**: `n = floor(ENFCNT/4)`; if `n==0` and no spheroids remain, `n=1` (when ENFCNT>0);
    `CIRCNT = min(CIRCNT+n, CIRCNT saved at last save)` (RRG23.ASM:857-873).
  - `RMXSPD` restored to the saved value and `ROBSPD = max(ROBSPD, RMXSPD)`; then state saved per player (`PLSAV`).
  - In 2-player games control alternates to the other player if they have lives.
  - Game over when both players have 0 lives ("GAME OVER", 120 frames).

## 7. Sounds

Sound tables: `{priority, (repeat, frames, id)*, 0}` (`SNDLDV`, RRS22.ASM:493-507). A request is accepted if its priority
>= the currently playing one; each triple sends `id` to the sound board, waits `frames`, `repeat` times. The byte written to
the sound PIA is `(~id) & $3F` (RRS22.ASM:484-491). The sound ROM is separate, so timbres cannot be determined from this
source; names are from the source comments.

| Name | Ref (table / trigger) | Pri | Sequence (rep×frames:id) | Event |
|---|---|---|---|---|
| LASSND | RRG23:22 / 966 | $D0 | 1×8:$01 | player shot |
| PDSND | RRG23:18 / 828 | $EE | 2×8:$11, 1×32:$17 | player death |
| ST1SND / ST2SND | RRG23:19-20 / 55-58 | $F0 | 1×16:$28 / 1×16:$25 | 1- / 2-player start |
| WVSND | RRG23:21 / 424 | $E0 | 29×4:$0E | wave end |
| RPSND | RRS22:86 / 1507 | $EF | 1×32:$1E | extra man |
| CNSND (8 variants) | RRS22:87-94 / 826-830 | $FF | 1×32:$0C; 1×32:$20; 3×16:$24; 1×32:$27; 1×32:$2D; 2×16:$35; 1×32:$3A; 1×32:$3E | coin (random of 8) |
| RBSND | RRP8:18 / 239 | $D0 | 1×12:$14, 1×8:$17 | grunt killed |
| RMVSND | RRP8:19 / 225 | $C0 | 1×10:$06 | grunt march step |
| PSKSND | RRP8:20 / 265 | $D0 | 1×8:$17 | electrode destroyed |
| HKHSND | RRH11:24 / 134 | $D0 | 1×16:$06 | hulk hit (knock-back) |
| HLKSND | RRH11:25 / unused | $D0 | 3×4:$17 | "hulk kill" (never triggered) |
| SAVSND | RRH11:26 / 469 | $E0 | 1×32:$0D | human rescued |
| HKSND | RRH11:27 / 477 | $E0 | 1×24:$1A | human killed (skull) |
| BKSND | RRB10:14 / 389 | $D0 | 1×4:$14, 1×8:$11 | brain killed |
| CMKSND | RRB10:15 / 769 | $D0 | 2×4:$17 | cruise missile shot |
| PGKSND | RRB10:16 / 561 | $D0 | 1×4:$14, 2×4:$17 | prog killed |
| BSHSND | RRB10:17 / 652 | $C8 | 1×8:$15, 1×8:$14 | brain fires missile |
| PRGSND | RRB10:18 / 285 | $D0 | 2×3:$12 | reprogramming (each of 20 loops) |
| HPSND | RRB10:19 / 322 | $D8 | 1×8:$11 | human→prog conversion done |
| ENDSND | RRC11:17 / 267 | $D8 | 1×8:$18 | spheroid drops enforcer |
| CRKSND | RRC11:18 / 205 | $D1 | 1×8:$08 | spheroid killed |
| ENFSND | RRC11:19 / 406 | $D0 | 1×8:$1D | enforcer fires spark |
| ENKSND | RRC11:20 / 425 | $D0 | 3×4:$17 | enforcer killed |
| SPKSND | RRC11:21 / 460 | $D0 | 1×4:$15, 1×8:$14 | spark shot |
| TNKSND | RRTK4:17 / 348 | $D0 | 1×8:$11 | tank killed |
| TKFSND | RRTK4:18 / 559 | $C8 | 1×8:$04 | tank fires |
| SRBSND | RRTK4:19 / 588 | $C8 | 1×4:$14, 1×1:$13 | shell rebounds off wall |
| SHKSND | RRTK4:20 / 601 | $D0 | 1×3:$01, 1×4:$15, 1×4:$13 | shell shot |
| SQKSND | RRTK4:21 / 118 | $D0 | 1×4:$15, 1×8:$11 | quark killed |
| TKDSND | RRTK4:22 / 216 | $D0 | 1×8:$19 | quark drops tank |
| TR1SND + raw | RRT2:30 / 440-452 | $FF | 1×1:$13, then raw id $12 sent 72× every frame, then 36× every 2 frames | brain-wave transporter |

There is no distinct electrode-touch / wave-start sound other than the above (the wave start on brain waves uses the
transporter sound; normal waves start silently after WVSND). High-score entry tunes live in RRTESTC.ASM.
Character of each sound: **cannot be determined from this source** (sound board ROM not included).

## 8. Other details

* **Score popups**: human rescue 1000-5000 picture 60 frames (RRH11.ASM:453-470, 483-494); spheroid/quark "1000" 60 frames
  after the burst (RRC11.ASM:229-250). Skull 90 frames.
* **Enemies that kill the player survive** (all kill vectors check `PCFLG`).
* **CPU-load throttles**: spheroid/quark drops, enforcer and tank shots are skipped when `OVCNT>=17` (frame overrun).
* **Caps**: lasers 4, cruise missiles 8, enforcers 8 alive, sparks 20, tanks 20, shells 21 per life (bug above),
  object pool 180, processes 120.
* **Attract mode** (RRLOG.ASM, RRTABLE.ASM, RRSCRIPT.ASM): Williams logo/"ROBOTRON 2084" title with colour cycling, operator
  message, high-score tables (today's top + all-time, 200-frame hold, RRTABLE.ASM:121-138), and the scripted story pages:
  "DUE TO A GENETIC ENGINEERING ERROR…", family intro (Mommy/Daddy/Mikey), and one page per enemy type (Grunt, Hulk,
  Spheroids+Quarks → Enforcers+Tanks, Brains → Progs, Electrodes) with a scripted player demonstrating shooting
  (RRSCRIPT.ASM:900-1080). Coins switch to the family page / credit screen.
* **Anti-tamper "Jap zappers"** (JAPDATA.ASM): checksum traps that randomly corrupt RAM if the ROM is modified; irrelevant
  for a remake. RRCHRIS.ASM fixes a diagonal-explosion bug (cosmetic).
* Not determinable from this source: exact video refresh (Williams hardware is ≈60 Hz **(commonly cited, not verified in
  source)**), sound timbres, pixel aspect ratio.
