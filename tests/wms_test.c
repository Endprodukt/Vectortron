/* wms_test — the sound board emulation against a tiny hand-assembled ROM:
 * at a sound command (PIA CB1 interrupt) it reads the number from port B
 * and plays a square wave of 100 half periods on the DAC. The half period
 * is exactly 279 cycles (counted from the 6800's cycle table), so the tone
 * must come out at 894886 / 558 = 1603.7 Hz and last 100 * 279 cycles.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/williams.h"

static const unsigned char RESET[] = {
    0x8E, 0x00, 0x7F,       /* LDS #$007F */
    0x7F, 0x04, 0x01,       /* CLR $0401     CRA: the DDR */
    0x86, 0xFF,             /* LDAA #$FF */
    0xB7, 0x04, 0x00,       /* STAA $0400    port A all out */
    0x86, 0x04,             /* LDAA #$04 */
    0xB7, 0x04, 0x01,       /* STAA $0401    CRA: the data */
    0x7F, 0x04, 0x03,       /* CLR $0403 */
    0x7F, 0x04, 0x02,       /* CLR $0402     port B all in */
    0x86, 0x07,             /* LDAA #$07     CB1 rising edge, interrupt on */
    0xB7, 0x04, 0x03,       /* STAA $0403 */
    0x0E,                   /* CLI */
    0x20, 0xFE,             /* BRA *  */
};

static const unsigned char IRQ[] = {
    0xB6, 0x04, 0x02,       /* LDAA $0402    the command (clears the flag) */
    0x43,                   /* COMA */
    0x84, 0x3F,             /* ANDA #$3F */
    0x97, 0x10,             /* STAA $10 */
    0xC6, 0x64,             /* LDAB #100 */
    /* sq: */
    0x96, 0x11,             /* LDAA $11      3 */
    0x43,                   /* COMA          2 */
    0x97, 0x11,             /* STAA $11      4 */
    0xB7, 0x04, 0x00,       /* STAA $0400    5 */
    0xCE, 0x00, 0x20,       /* LDX #$0020    3 */
    /* dl: */
    0x09,                   /* DEX           4 */
    0x26, 0xFD,             /* BNE dl        4 */
    0x5A,                   /* DECB          2 */
    0x26, 0xEF,             /* BNE sq        4 */
    0x3B,                   /* RTI */
};

int main(void)
{
    static unsigned char rom[4096];
    memset(rom, 0x01, sizeof rom);                      /* NOPs */
    memcpy(rom, RESET, sizeof RESET);                   /* $F000 */
    memcpy(rom + 0x100, IRQ, sizeof IRQ);               /* $F100 */
    rom[0xFF8] = 0xF1; rom[0xFF9] = 0x00;               /* IRQ */
    rom[0xFFE] = 0xF0; rom[0xFFF] = 0x00;               /* RESET */

    const int rate = 48000;
    wms_step s = { 1, 1, 0x05 };
    int n = 0;
    float *buf = wms_record(rom, sizeof rom, &s, 1, rate, 1.0f, &n);
    if (!buf) { printf("FAIL: nothing recorded\n"); return 1; }

    /* the sounding part: from the first to the last swing */
    int first = -1, last = -1, crossings = 0;
    for (int i = 1; i < n; i++) {
        if ((buf[i - 1] < 0) != (buf[i] < 0) && fabsf(buf[i] - buf[i - 1]) > 0.2f) {
            if (first < 0) first = i;
            last = i;
            crossings++;
        }
    }
    double secs = (double)(last - first) / rate;
    double freq = (crossings - 1) / 2.0 / secs;
    double want = 894886.0 / 558.0;
    double want_len = 99 * 279.0 / 894886.0;
    printf("crossings %d, %.1f Hz (want %.1f), %.4f s (want %.4f)\n", crossings, freq, want, secs, want_len);
    free(buf);
    if (crossings < 95 || crossings > 101) { printf("FAIL: crossings\n"); return 1; }
    if (fabs(freq - want) > want * 0.01) { printf("FAIL: pitch\n"); return 1; }
    printf("ok\n");
    return 0;
}
