/* cpu_test — 6800 instructions whose results are easy to get wrong: each
 * stores a result (or the flags, through TPA) in RAM; the RAM is checked. */
#include <stdio.h>
#include <string.h>

#include "../src/williams.h"

/* LDAA #a; ADDA #b; STAA r; TPA; STAA r+1 ... built by the helpers below */
static unsigned char rom[4096];
static int pc;

static void emit(int n, ...);
#include <stdarg.h>
static void emit(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) rom[pc++] = (unsigned char)va_arg(ap, int);
    va_end(ap);
}

static void store_a(int addr) { emit(2, 0x97, addr); }
static void store_cc(int addr) { emit(3, 0x07, 0x97, addr); }      /* TPA; STAA addr */

int main(void)
{
    memset(rom, 0x01, sizeof rom);
    pc = 0;
    emit(3, 0x8E, 0x00, 0xFF);                       /* LDS #$FF */
    /* 0x10: $7F + $01 = $80, V and N set, H set */
    emit(1, 0x0C);                                   /* CLC */
    emit(4, 0x86, 0x7F, 0x8B, 0x01); store_cc(0x11);           /* the flags first: STAA clears V */
    emit(4, 0x86, 0x7F, 0x8B, 0x01); store_a(0x10);
    /* 0x12: $10 - $20 = $F0, borrow (C), N */
    emit(4, 0x86, 0x10, 0x80, 0x20); store_cc(0x13);
    emit(4, 0x86, 0x10, 0x80, 0x20); store_a(0x12);
    /* 0x14: ROL of $81 with C set = $03, C set */
    emit(1, 0x0D); emit(3, 0x86, 0x81, 0x49); store_cc(0x15);
    emit(1, 0x0D); emit(3, 0x86, 0x81, 0x49); store_a(0x14);
    /* 0x16: ASR of $82 = $C1, C clear */
    emit(3, 0x86, 0x82, 0x47); store_a(0x16);
    /* 0x17: DAA after $19 + $28 = $47 */
    emit(5, 0x86, 0x19, 0x8B, 0x28, 0x19); store_a(0x17);
    /* 0x18/19: LDX #$1234, STX $18 */
    emit(5, 0xCE, 0x12, 0x34, 0xDF, 0x18);
    /* 0x1A: indexed: LDX #$0018; LDAA 1,X (=$34); STAA $1A */
    emit(5, 0xCE, 0x00, 0x18, 0xA6, 0x01); store_a(0x1A);
    /* 0x1B: JSR to a subroutine that loads $5A, RTS */
    int sub = 0x200;
    emit(3, 0xBD, 0xF0 | (sub >> 8), sub & 0xFF); store_a(0x1B);
    /* 0x1C: PSHA/PULB: A=$66 -> B -> STAB $1C */
    emit(5, 0x86, 0x66, 0x36, 0x5F, 0x33); emit(2, 0xD7, 0x1C);
    /* 0x1D: BGT taken? A=5 CMPA #3 -> greater: store 1 else 0 */
    emit(4, 0x86, 0x05, 0x81, 0x03);
    emit(2, 0x2E, 0x04);                             /* BGT +4 */
    emit(4, 0x86, 0x00, 0x20, 0x02);                 /* LDAA #0; BRA +2 */
    emit(2, 0x86, 0x01);                             /* LDAA #1 */
    store_a(0x1D);
    /* 0x1E: BLS with C set (taken): $10 CMPA #$20 */
    emit(4, 0x86, 0x10, 0x81, 0x20);
    emit(2, 0x23, 0x04);
    emit(4, 0x86, 0x00, 0x20, 0x02);
    emit(2, 0x86, 0x01);
    store_a(0x1E);
    /* 0x1F: CPX #$1234 with X=$1234 -> Z: TPA */
    emit(6, 0xCE, 0x12, 0x34, 0x8C, 0x12, 0x34); store_cc(0x1F);
    /* 0x20: memory RMW: $20 = $0F, INC $0020 (ext) -> $10; NEG idx -> $F0 */
    emit(2, 0x86, 0x0F); store_a(0x20);
    emit(3, 0x7C, 0x00, 0x20);                       /* INC $0020 */
    emit(3, 0xCE, 0x00, 0x20); emit(2, 0x60, 0x00);  /* LDX #$20; NEG 0,X */
    /* 0x21: TSX: SP after all this = $FF -> X = $100 -> store low */
    emit(1, 0x30); emit(2, 0xDF, 0x22);              /* STX $22/$23: $0100 */
    /* 0x24: ADC with carry: SEC; LDAA #1; ADCA #1 = 3 */
    emit(5, 0x0D, 0x86, 0x01, 0x89, 0x01); store_a(0x24);
    /* 0x25: SBC with borrow: SEC; LDAA #5; SBCA #1 = 3 */
    emit(5, 0x0D, 0x86, 0x05, 0x82, 0x01); store_a(0x25);
    /* 0x26: LSR $01 -> 0, C, Z */
    emit(3, 0x86, 0x01, 0x44); store_cc(0x26);
    /* 0x27: ABA $30 + $40, TAB/TBA */
    emit(5, 0x86, 0x30, 0xC6, 0x40, 0x1B); store_a(0x27);
    emit(2, 0x20, 0xFE);                             /* done: BRA * */

    pc = sub;
    emit(3, 0x86, 0x5A, 0x39);                       /* LDAA #$5A; RTS */

    rom[0xFFE] = 0xF0; rom[0xFFF] = 0x00;
    unsigned char ram[256];
    wms_run(rom, sizeof rom, 5000, ram);

    struct { int addr, want, mask; const char *what; } T[] = {
        { 0x10, 0x80, 0xFF, "ADD result" },  { 0x11, 0x2A, 0x2F, "ADD flags H N V" },
        { 0x12, 0xF0, 0xFF, "SUB result" },  { 0x13, 0x09, 0x0F, "SUB flags N C" },
        { 0x14, 0x03, 0xFF, "ROL result" },  { 0x15, 0x01, 0x01, "ROL carry" },
        { 0x16, 0xC1, 0xFF, "ASR" },         { 0x17, 0x47, 0xFF, "DAA" },
        { 0x18, 0x12, 0xFF, "STX hi" },      { 0x19, 0x34, 0xFF, "STX lo" },
        { 0x1A, 0x34, 0xFF, "indexed" },     { 0x1B, 0x5A, 0xFF, "JSR/RTS" },
        { 0x1C, 0x66, 0xFF, "PSH/PUL" },     { 0x1D, 0x01, 0xFF, "BGT" },
        { 0x1E, 0x01, 0xFF, "BLS" },         { 0x1F, 0x04, 0x04, "CPX Z" },
        { 0x20, 0xF0, 0xFF, "INC ext, NEG idx" },
        { 0x22, 0x01, 0xFF, "TSX hi" },      { 0x23, 0x00, 0xFF, "TSX lo" },
        { 0x24, 0x03, 0xFF, "ADC" },         { 0x25, 0x03, 0xFF, "SBC" },
        { 0x26, 0x05, 0x0D, "LSR flags Z C, N clear" },
        { 0x27, 0x70, 0xFF, "ABA" },
    };
    int fails = 0;
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        int got = ram[T[i].addr] & T[i].mask;
        if (got != T[i].want) {
            printf("FAIL %-24s $%02X: got $%02X want $%02X\n", T[i].what, T[i].addr, got, T[i].want);
            fails++;
        }
    }
    printf(fails ? "%d failed\n" : "ok\n", fails);
    return fails != 0;
}
