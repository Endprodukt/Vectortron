/* williams.c — a 6800 CPU, a 6821 PIA and a DAC: the Williams sound board. */
#include "williams.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- the machine ---------------------------------------------------------- */

enum { FC = 0x01, FV = 0x02, FZ = 0x04, FN = 0x08, FI = 0x10, FH = 0x20 };

typedef struct {
    uint8_t  a, b, cc;
    uint16_t x, sp, pc;
    int      wai;               /* waiting for an interrupt (WAI) */
    long     cycles;

    uint8_t  ram[256];
    const uint8_t *rom;
    size_t   rom_size;

    /* the 6821 PIA: port A drives the DAC, port B reads the sound number */
    uint8_t  ora, ddra, cra, orb, ddrb, crb, pb_in;
    int      cb1;
    uint8_t  dac;
} board;

static uint8_t pia_read(board *m, int reg)
{
    switch (reg) {
    case 0:
        if (m->cra & 4) {
            m->cra &= 0x3F;
            return (uint8_t)((m->ora & m->ddra) | (0xFF & ~m->ddra));
        }
        return m->ddra;
    case 1:
        return m->cra;
    case 2:
        if (m->crb & 4) {
            m->crb &= 0x3F;                     /* reading the port clears its interrupt */
            return (uint8_t)((m->orb & m->ddrb) | (m->pb_in & ~m->ddrb));
        }
        return m->ddrb;
    default:
        return m->crb;
    }
}

static void pia_write(board *m, int reg, uint8_t v)
{
    switch (reg) {
    case 0:
        if (m->cra & 4) {
            m->ora = v;
            m->dac = (uint8_t)((m->ora & m->ddra) | (0xFF & ~m->ddra));
        } else {
            m->ddra = v;
            m->dac = (uint8_t)((m->ora & m->ddra) | (0xFF & ~m->ddra));
        }
        break;
    case 1: m->cra = (uint8_t)((m->cra & 0xC0) | (v & 0x3F)); break;
    case 2: if (m->crb & 4) m->orb = v; else m->ddrb = v; break;
    default: m->crb = (uint8_t)((m->crb & 0xC0) | (v & 0x3F)); break;
    }
}

static void pia_cb1(board *m, int level)
{
    int rising = (m->crb & 2) != 0;
    if (level != m->cb1 && (level ? rising : !rising)) m->crb |= 0x80;
    m->cb1 = level;
}

static int pia_irq(const board *m)
{
    return ((m->crb & 0x80) && (m->crb & 1)) || ((m->cra & 0x80) && (m->cra & 1));
}

static uint8_t rd(board *m, uint16_t a)
{
    if (a < 0x0400) return m->ram[a & 0xFF];
    if ((a & 0x7C00) == 0x0400) return pia_read(m, a & 3);
    if (a >= 0xB000) return m->rom[(a - (0x10000 - m->rom_size)) & (m->rom_size - 1)];
    return 0xFF;
}

static void wr(board *m, uint16_t a, uint8_t v)
{
    if (a < 0x0400) m->ram[a & 0xFF] = v;
    else if ((a & 0x7C00) == 0x0400) pia_write(m, a & 3, v);
}

static uint16_t rd16(board *m, uint16_t a) { return (uint16_t)(rd(m, a) << 8 | rd(m, (uint16_t)(a + 1))); }

static void wr16(board *m, uint16_t a, uint16_t v)
{
    wr(m, a, (uint8_t)(v >> 8));
    wr(m, (uint16_t)(a + 1), (uint8_t)v);
}

static void push(board *m, uint8_t v) { wr(m, m->sp--, v); }
static uint8_t pull(board *m) { return rd(m, ++m->sp); }

static void push16(board *m, uint16_t v) { push(m, (uint8_t)v); push(m, (uint8_t)(v >> 8)); }
static uint16_t pull16(board *m) { uint16_t h = pull(m); return (uint16_t)(h << 8 | pull(m)); }

/* ---- flags ----------------------------------------------------------------- */

static void nz(board *m, uint8_t r)
{
    m->cc &= (uint8_t)~(FN | FZ);
    if (r & 0x80) m->cc |= FN;
    if (!r) m->cc |= FZ;
}

static void nz16(board *m, uint16_t r)
{
    m->cc &= (uint8_t)~(FN | FZ);
    if (r & 0x8000) m->cc |= FN;
    if (!r) m->cc |= FZ;
}

static void set(board *m, uint8_t f, int on) { if (on) m->cc |= f; else m->cc &= (uint8_t)~f; }

static uint8_t op_add(board *m, uint8_t a, uint8_t b, int carry)
{
    unsigned r = a + b + (unsigned)carry;
    set(m, FH, ((a & 0xF) + (b & 0xF) + carry) > 0xF);
    set(m, FC, r > 0xFF);
    set(m, FV, (~(a ^ b) & (a ^ r) & 0x80) != 0);
    nz(m, (uint8_t)r);
    return (uint8_t)r;
}

static uint8_t op_sub(board *m, uint8_t a, uint8_t b, int borrow)
{
    unsigned r = (unsigned)a - b - (unsigned)borrow;
    set(m, FC, r > 0xFF);
    set(m, FV, ((a ^ b) & (a ^ r) & 0x80) != 0);
    nz(m, (uint8_t)r);
    return (uint8_t)r;
}

static uint8_t op_logic(board *m, uint8_t r) { nz(m, r); m->cc &= (uint8_t)~FV; return r; }

/* read-modify-write operations, by the low nibble of the opcode */
static uint8_t rmw(board *m, int op, uint8_t v)
{
    uint8_t r;
    switch (op & 0x0F) {
    case 0x0: r = (uint8_t)(0 - v); nz(m, r); set(m, FV, r == 0x80); set(m, FC, r != 0); return r;   /* NEG */
    case 0x3: r = (uint8_t)~v; nz(m, r); m->cc &= (uint8_t)~FV; m->cc |= FC; return r;               /* COM */
    case 0x4: set(m, FC, v & 1); r = v >> 1; nz(m, r); set(m, FV, (m->cc & FC) != 0); return r;     /* LSR */
    case 0x6: { int c = m->cc & FC; set(m, FC, v & 1); r = (uint8_t)(v >> 1 | (c ? 0x80 : 0)); nz(m, r);  /* ROR */
                set(m, FV, ((m->cc & FN) != 0) != ((m->cc & FC) != 0)); return r; }
    case 0x7: set(m, FC, v & 1); r = (uint8_t)((v >> 1) | (v & 0x80)); nz(m, r);                     /* ASR */
              set(m, FV, ((m->cc & FN) != 0) != ((m->cc & FC) != 0)); return r;
    case 0x8: set(m, FC, v & 0x80); r = (uint8_t)(v << 1); nz(m, r);                                 /* ASL */
              set(m, FV, ((m->cc & FN) != 0) != ((m->cc & FC) != 0)); return r;
    case 0x9: { int c = m->cc & FC; set(m, FC, v & 0x80); r = (uint8_t)(v << 1 | (c ? 1 : 0)); nz(m, r); /* ROL */
                set(m, FV, ((m->cc & FN) != 0) != ((m->cc & FC) != 0)); return r; }
    case 0xA: r = (uint8_t)(v - 1); nz(m, r); set(m, FV, v == 0x80); return r;                       /* DEC */
    case 0xC: r = (uint8_t)(v + 1); nz(m, r); set(m, FV, v == 0x7F); return r;                       /* INC */
    case 0xD: nz(m, v); m->cc &= (uint8_t)~(FV | FC); return v;                                      /* TST */
    case 0xF: m->cc &= (uint8_t)~(FN | FV | FC); m->cc |= FZ; return 0;                              /* CLR */
    default:  return v;
    }
}

static void interrupt(board *m, uint16_t vector)
{
    if (!m->wai) {
        push16(m, m->pc);
        push16(m, m->x);
        push(m, m->a);
        push(m, m->b);
        push(m, m->cc);
    }
    m->wai = 0;
    m->cc |= FI;
    m->pc = rd16(m, vector);
    m->cycles += 12;
}

/* The accumulator ops of rows 8-B (A) and C-F (B), by the low nibble. */
static void acc_op(board *m, int op, uint8_t *r, uint8_t v)
{
    switch (op & 0x0F) {
    case 0x0: *r = op_sub(m, *r, v, 0); break;                   /* SUB */
    case 0x1: op_sub(m, *r, v, 0); break;                        /* CMP */
    case 0x2: *r = op_sub(m, *r, v, m->cc & FC); break;          /* SBC */
    case 0x4: *r = op_logic(m, *r & v); break;                   /* AND */
    case 0x5: op_logic(m, *r & v); break;                        /* BIT */
    case 0x6: *r = op_logic(m, v); break;                        /* LDA */
    case 0x8: *r = op_logic(m, *r ^ v); break;                   /* EOR */
    case 0x9: *r = op_add(m, *r, v, m->cc & FC); break;          /* ADC */
    case 0xA: *r = op_logic(m, *r | v); break;                   /* ORA */
    case 0xB: *r = op_add(m, *r, v, 0); break;                   /* ADD */
    default: break;
    }
}

static int branch(board *m, int op)
{
    int n = (m->cc & FN) != 0, z = (m->cc & FZ) != 0, v = (m->cc & FV) != 0, c = (m->cc & FC) != 0;
    switch (op) {
    case 0x20: return 1;
    case 0x22: return !(c || z);
    case 0x23: return c || z;
    case 0x24: return !c;
    case 0x25: return c;
    case 0x26: return !z;
    case 0x27: return z;
    case 0x28: return !v;
    case 0x29: return v;
    case 0x2A: return !n;
    case 0x2B: return n;
    case 0x2C: return n == v;
    case 0x2D: return n != v;
    case 0x2E: return !z && n == v;
    case 0x2F: return z || n != v;
    default: return 0;
    }
}

/* One instruction (or an interrupt, or a cycle of waiting). */
static void step(board *m)
{
    if (pia_irq(m) && !(m->cc & FI)) {
        interrupt(m, 0xFFF8);
        return;
    }
    if (m->wai) {
        m->cycles += 4;
        return;
    }

    uint8_t op = rd(m, m->pc++);
    uint16_t ea = 0;
    int hi = op >> 4;

    /* the effective address for the memory forms of rows 6-7 and 8-F */
    if (hi == 0x6 || hi == 0xA || hi == 0xE) ea = (uint16_t)(m->x + rd(m, m->pc++));
    else if (hi == 0x7 || hi == 0xB || hi == 0xF) { ea = rd16(m, m->pc); m->pc += 2; }
    else if (hi == 0x9 || hi == 0xD) ea = rd(m, m->pc++);

    switch (hi) {
    case 0x0:
        m->cycles += (op == 0x08 || op == 0x09) ? 4 : 2;
        switch (op) {
        case 0x06: m->cc = m->a | 0xC0; break;                                  /* TAP */
        case 0x07: m->a = m->cc | 0xC0; break;                                  /* TPA */
        case 0x08: m->x++; set(m, FZ, m->x == 0); break;                         /* INX */
        case 0x09: m->x--; set(m, FZ, m->x == 0); break;                         /* DEX */
        case 0x0A: m->cc &= (uint8_t)~FV; break;
        case 0x0B: m->cc |= FV; break;
        case 0x0C: m->cc &= (uint8_t)~FC; break;
        case 0x0D: m->cc |= FC; break;
        case 0x0E: m->cc &= (uint8_t)~FI; break;
        case 0x0F: m->cc |= FI; break;
        default: break;                                                          /* NOP */
        }
        break;
    case 0x1:
        m->cycles += 2;
        switch (op) {
        case 0x10: m->a = op_sub(m, m->a, m->b, 0); break;                        /* SBA */
        case 0x11: op_sub(m, m->a, m->b, 0); break;                               /* CBA */
        case 0x16: m->b = op_logic(m, m->a); break;                               /* TAB */
        case 0x17: m->a = op_logic(m, m->b); break;                               /* TBA */
        case 0x19: {                                                              /* DAA */
            unsigned a = m->a, adj = 0;
            int c = m->cc & FC;
            if ((m->cc & FH) || (a & 0x0F) > 9) adj |= 0x06;
            if (c || a > 0x99 || ((a & 0xF0) > 0x80 && (a & 0x0F) > 9)) { adj |= 0x60; c = 1; }
            a += adj;
            m->a = (uint8_t)a;
            nz(m, m->a);
            set(m, FC, c);
            break;
        }
        case 0x1B: m->a = op_add(m, m->a, m->b, 0); break;                        /* ABA */
        default: break;
        }
        break;
    case 0x2: {
        int8_t off = (int8_t)rd(m, m->pc++);
        m->cycles += 4;
        if (branch(m, op)) m->pc = (uint16_t)(m->pc + off);
        break;
    }
    case 0x3:
        switch (op) {
        case 0x30: m->x = (uint16_t)(m->sp + 1); m->cycles += 4; break;          /* TSX */
        case 0x31: m->sp++; m->cycles += 4; break;                               /* INS */
        case 0x32: m->a = pull(m); m->cycles += 4; break;
        case 0x33: m->b = pull(m); m->cycles += 4; break;
        case 0x34: m->sp--; m->cycles += 4; break;                               /* DES */
        case 0x35: m->sp = (uint16_t)(m->x - 1); m->cycles += 4; break;          /* TXS */
        case 0x36: push(m, m->a); m->cycles += 4; break;
        case 0x37: push(m, m->b); m->cycles += 4; break;
        case 0x39: m->pc = pull16(m); m->cycles += 5; break;                     /* RTS */
        case 0x3B:                                                               /* RTI */
            m->cc = pull(m) | 0xC0;
            m->b = pull(m);
            m->a = pull(m);
            m->x = pull16(m);
            m->pc = pull16(m);
            m->cycles += 10;
            break;
        case 0x3E:                                                               /* WAI */
            push16(m, m->pc);
            push16(m, m->x);
            push(m, m->a);
            push(m, m->b);
            push(m, m->cc);
            m->wai = 1;
            m->cycles += 9;
            break;
        case 0x3F:                                                               /* SWI */
            push16(m, m->pc);
            push16(m, m->x);
            push(m, m->a);
            push(m, m->b);
            push(m, m->cc);
            m->cc |= FI;
            m->pc = rd16(m, 0xFFFA);
            m->cycles += 12;
            break;
        default: m->cycles += 2; break;
        }
        break;
    case 0x4: m->a = rmw(m, op, m->a); m->cycles += 2; break;
    case 0x5: m->b = rmw(m, op, m->b); m->cycles += 2; break;
    case 0x6:
    case 0x7:
        if ((op & 0x0F) == 0xE) { m->pc = ea; m->cycles += hi == 6 ? 4 : 3; break; }   /* JMP */
        {
            uint8_t r = rmw(m, op, rd(m, ea));
            if ((op & 0x0F) != 0xD) wr(m, ea, r);
            m->cycles += hi == 6 ? 7 : 6;
        }
        break;
    default: {
        /* rows 8-F: 8/C immediate, 9/D direct, A/E indexed, B/F extended */
        int mode = hi & 3;                  /* 0 imm, 1 dir, 2 idx, 3 ext */
        uint8_t *r = hi < 0xC ? &m->a : &m->b;
        int lo = op & 0x0F;
        static const int base[4] = { 2, 3, 5, 4 };
        if (lo == 0x3 || lo == 0xC || lo == 0xE || lo == 0xF || lo == 0xD || lo == 0x7) {
            /* the 16-bit loads, stores and compares, the stores, JSR and BSR */
            switch (lo) {
            case 0x7:                                                            /* STA */
                if (mode == 0) { m->cycles += 2; break; }
                wr(m, ea, op_logic(m, *r));
                m->cycles += base[mode] + 1;
                break;
            case 0xC:                                                            /* CPX (A rows only) */
                if (hi >= 0xC) { m->cycles += 2; break; }
                {
                    uint16_t v = mode == 0 ? rd16(m, m->pc) : rd16(m, ea);
                    if (mode == 0) m->pc += 2;
                    uint32_t res = (uint32_t)m->x - v;
                    nz16(m, (uint16_t)res);
                    set(m, FV, ((m->x ^ v) & (m->x ^ res) & 0x8000) != 0);
                    m->cycles += base[mode] + 1;
                }
                break;
            case 0xD:                                                            /* BSR / JSR */
                if (hi >= 0xC) { m->cycles += 2; break; }
                if (mode == 0) {
                    int8_t off = (int8_t)rd(m, m->pc++);
                    push16(m, m->pc);
                    m->pc = (uint16_t)(m->pc + off);
                    m->cycles += 8;
                } else {
                    push16(m, m->pc);
                    m->pc = ea;
                    m->cycles += mode == 2 ? 8 : 9;
                }
                break;
            case 0xE: {                                                          /* LDS (A) / LDX (B) */
                uint16_t v = mode == 0 ? rd16(m, m->pc) : rd16(m, ea);
                if (mode == 0) m->pc += 2;
                if (hi < 0xC) m->sp = v; else m->x = v;
                nz16(m, v);
                m->cc &= (uint8_t)~FV;
                m->cycles += base[mode] + 1;
                break;
            }
            case 0xF:                                                            /* STS (A) / STX (B) */
                if (mode == 0) { m->cycles += 2; break; }
                {
                    uint16_t v = hi < 0xC ? m->sp : m->x;
                    wr16(m, ea, v);
                    nz16(m, v);
                    m->cc &= (uint8_t)~FV;
                    m->cycles += base[mode] + 2;
                }
                break;
            default: m->cycles += 2; break;                                      /* 0x3: none */
            }
        } else {
            uint8_t v = mode == 0 ? rd(m, m->pc++) : rd(m, ea);
            acc_op(m, op, r, v);
            m->cycles += base[mode];
        }
        break;
    }
    }
}

static void reset(board *m, const unsigned char *rom, size_t size)
{
    memset(m, 0, sizeof *m);
    m->rom = rom;
    m->rom_size = size;
    m->cc = 0xC0 | FI;
    m->pb_in = 0xFF;
    m->dac = 0x80;
    m->pc = rd16(m, 0xFFFE);
}

/* ---- recording -------------------------------------------------------------- */

typedef struct {
    float *buf;
    int    n, cap;
    double per_sample;          /* board cycles per output sample */
    double next;                /* the cycle the next sample ends at */
    double acc, acc_cycles;
    long   last_cycle;
    uint8_t last_dac;
    long   last_change;
} recorder;

/* Run the board up to cycle `until`, averaging the DAC into samples. */
static int run_until(board *m, recorder *rc, long until)
{
    while (m->cycles < until) {
        uint8_t before = m->dac;
        long c0 = m->cycles;
        step(m);
        long c1 = m->cycles;
        /* the DAC held `before` from c0 to c1 (a write lands at the end) */
        double t = (double)c0;
        while (t < c1) {
            double end = rc->next < c1 ? rc->next : (double)c1;
            rc->acc += (before - 128.0) * (end - t);
            rc->acc_cycles += end - t;
            t = end;
            if (t >= rc->next) {
                if (rc->n == rc->cap) {
                    rc->cap = rc->cap ? rc->cap * 2 : 65536;
                    float *nb = realloc(rc->buf, sizeof(float) * (size_t)rc->cap);
                    if (!nb) return 0;
                    rc->buf = nb;
                }
                rc->buf[rc->n++] = (float)(rc->acc / (rc->acc_cycles > 0 ? rc->acc_cycles : 1) / 128.0);
                rc->acc = rc->acc_cycles = 0;
                rc->next += rc->per_sample;
            }
        }
        if (m->dac != before) rc->last_change = c1;
    }
    return 1;
}

/* What the main board does to send a sound (SNDOUT, RRS22.ASM): write $3F,
 * then the complemented number; the board sees it with the top two bits set,
 * and CB1 high while it is not $FF. */
static void send(board *m, int id)
{
    m->pb_in = 0xFF;
    pia_cb1(m, 0);
    m->pb_in = (uint8_t)(((~id) & 0x3F) | 0xC0);
    pia_cb1(m, m->pb_in != 0xFF);
}

float *wms_record(const unsigned char *rom, size_t rom_size, const wms_step *steps, int nsteps, int rate,
                  float max_seconds, int *frames)
{
    if (!rom || rom_size < 0x800 || (rom_size & (rom_size - 1))) return NULL;
    board *m = malloc(sizeof *m);
    if (!m) return NULL;
    reset(m, rom, rom_size);

    /* let the board start up (its RAM test and idle loop), unrecorded */
    recorder warm = { 0 };
    warm.per_sample = (double)WMS_CLOCK / rate;
    warm.next = warm.per_sample;
    run_until(m, &warm, WMS_CLOCK / 5);
    free(warm.buf);

    recorder rc = { 0 };
    rc.per_sample = (double)WMS_CLOCK / rate;
    rc.next = (double)m->cycles + rc.per_sample;
    long start = m->cycles;
    const long frame = WMS_CLOCK / 60;
    long t = start;
    int ok = 1;

    for (int s = 0; s < nsteps && ok; s++) {
        for (int r = 0; r < steps[s].repeat && ok; r++) {
            send(m, steps[s].id);
            rc.last_change = m->cycles;
            t += frame * steps[s].frames;
            ok = run_until(m, &rc, t);
        }
    }
    /* then until it falls silent (the DAC still for 0.12 s), or the limit */
    long limit = start + (long)(max_seconds * WMS_CLOCK);
    while (ok && m->cycles < limit && m->cycles - rc.last_change < WMS_CLOCK / 8)
        ok = run_until(m, &rc, m->cycles + WMS_CLOCK / 100);
    free(m);
    if (!ok || rc.n == 0) { free(rc.buf); return NULL; }

    /* take the DC off (the board's output is AC coupled) and fade the end */
    float dc = 0, prev_in = 0, prev_out = 0;
    const float k = 1.0f - 2 * 3.14159265f * 15.0f / rate;
    (void)dc;
    for (int i = 0; i < rc.n; i++) {
        float in = rc.buf[i];
        float out = k * (prev_out + in - prev_in);
        prev_in = in;
        prev_out = out;
        rc.buf[i] = out * 0.8f;
    }
    int fade = rate / 100;
    for (int i = 0; i < fade && i < rc.n; i++) rc.buf[rc.n - 1 - i] *= (float)i / fade;
    *frames = rc.n;
    return rc.buf;
}

void wms_run(const unsigned char *rom, size_t rom_size, long cycles, unsigned char ram[256])
{
    board *m = malloc(sizeof *m);
    if (!m) return;
    reset(m, rom, rom_size);
    while (m->cycles < cycles) step(m);
    memcpy(ram, m->ram, 256);
    free(m);
}
