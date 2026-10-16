/* romzip.c — reading one file out of a MAME ROM set (.zip), or a loose ROM.
 *
 * A small inflate (RFC 1951) and enough of the ZIP format to find a file by
 * name in the central directory.
 */
#include "romzip.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- inflate --------------------------------------------------------------- */

typedef struct {
    const uint8_t *in;
    size_t in_len, pos;
    uint32_t bits;
    int nbits;
    uint8_t *out;
    size_t out_len, out_pos;
    int error;
} inflater;

static int getbit(inflater *z)
{
    if (z->nbits == 0) {
        if (z->pos >= z->in_len) { z->error = 1; return 0; }
        z->bits = z->in[z->pos++];
        z->nbits = 8;
    }
    int b = z->bits & 1;
    z->bits >>= 1;
    z->nbits--;
    return b;
}

static int getbits(inflater *z, int n)
{
    int v = 0;
    for (int i = 0; i < n; i++) v |= getbit(z) << i;
    return v;
}

typedef struct { uint16_t count[16], symbol[320]; } huffman;

static void build(huffman *h, const uint8_t *lengths, int n)
{
    uint16_t offs[16];
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++) h->count[lengths[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = (uint16_t)(offs[i] + h->count[i]);
    for (int i = 0; i < n; i++) if (lengths[i]) h->symbol[offs[lengths[i]]++] = (uint16_t)i;
}

static int decode(inflater *z, const huffman *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= getbit(z);
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
        if (z->error) return -1;
    }
    z->error = 1;
    return -1;
}

static void put(inflater *z, uint8_t b)
{
    if (z->out_pos >= z->out_len) { z->error = 1; return; }
    z->out[z->out_pos++] = b;
}

static void codes(inflater *z, const huffman *lit, const huffman *dist)
{
    static const uint16_t LB[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
    static const uint8_t  LE[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
    static const uint16_t DB[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,
                                     3073,4097,6145,8193,12289,16385,24577 };
    static const uint8_t  DE[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
    for (;;) {
        int sym = decode(z, lit);
        if (z->error || sym == 256) return;
        if (sym < 256) { put(z, (uint8_t)sym); continue; }
        sym -= 257;
        if (sym >= 29) { z->error = 1; return; }
        int len = LB[sym] + getbits(z, LE[sym]);
        int ds = decode(z, dist);
        if (ds < 0 || ds >= 30) { z->error = 1; return; }
        size_t d = DB[ds] + (size_t)getbits(z, DE[ds]);
        if (d > z->out_pos) { z->error = 1; return; }
        for (int i = 0; i < len && !z->error; i++) put(z, z->out[z->out_pos - d]);
    }
}

static int inflate_raw(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len)
{
    inflater z = { in, in_len, 0, 0, 0, out, out_len, 0, 0 };
    huffman lit, dist;
    int last;
    do {
        last = getbit(&z);
        int type = getbits(&z, 2);
        if (type == 0) {
            z.nbits = 0;
            if (z.pos + 4 > z.in_len) return 0;
            unsigned len = z.in[z.pos] | z.in[z.pos + 1] << 8;
            z.pos += 4;
            if (z.pos + len > z.in_len) return 0;
            for (unsigned i = 0; i < len; i++) put(&z, z.in[z.pos++]);
        } else if (type == 1) {
            uint8_t l[288];
            int i;
            for (i = 0; i < 144; i++) l[i] = 8;
            for (; i < 256; i++) l[i] = 9;
            for (; i < 280; i++) l[i] = 7;
            for (; i < 288; i++) l[i] = 8;
            build(&lit, l, 288);
            for (i = 0; i < 30; i++) l[i] = 5;
            build(&dist, l, 30);
            codes(&z, &lit, &dist);
        } else if (type == 2) {
            static const uint8_t ORDER[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
            int hlit = getbits(&z, 5) + 257, hdist = getbits(&z, 5) + 1, hclen = getbits(&z, 4) + 4;
            uint8_t l[320] = { 0 };
            for (int i = 0; i < hclen; i++) l[ORDER[i]] = (uint8_t)getbits(&z, 3);
            huffman lens;
            build(&lens, l, 19);
            memset(l, 0, sizeof l);
            for (int i = 0; i < hlit + hdist && !z.error; ) {
                int sym = decode(&z, &lens);
                if (sym < 16) { l[i++] = (uint8_t)sym; continue; }
                int rep, val = 0;
                if (sym == 16) { if (!i) return 0; val = l[i - 1]; rep = 3 + getbits(&z, 2); }
                else if (sym == 17) rep = 3 + getbits(&z, 3);
                else rep = 11 + getbits(&z, 7);
                while (rep-- && i < hlit + hdist) l[i++] = (uint8_t)val;
            }
            build(&lit, l, hlit);
            build(&dist, l + hlit, hdist);
            codes(&z, &lit, &dist);
        } else {
            return 0;
        }
        if (z.error) return 0;
    } while (!last);
    return z.out_pos == out_len;
}

/* ---- files and zips --------------------------------------------------------- */

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 64 * 1024 * 1024) { fclose(f); return NULL; }
    unsigned char *buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf) *size = (size_t)n;
    return buf;
}

static unsigned u16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static int name_is(const char *name, size_t len, const char *want)
{
    /* the file part of the name, any case */
    const char *base = name;
    for (size_t i = 0; i < len; i++) if (name[i] == '/') base = name + i + 1;
    size_t bl = len - (size_t)(base - name);
    if (bl != strlen(want)) return 0;
    for (size_t i = 0; i < bl; i++) if (tolower((unsigned char)base[i]) != tolower((unsigned char)want[i])) return 0;
    return 1;
}

unsigned char *romzip_extract(const char *zip_path, const char *const *names, size_t *size)
{
    size_t zlen;
    uint8_t *z = read_file(zip_path, &zlen);
    if (!z) return NULL;
    unsigned char *result = NULL;
    /* the end of central directory record */
    long eocd = -1;
    for (long i = (long)zlen - 22; i >= 0 && i >= (long)zlen - 22 - 65535; i--) {
        if (u32(z + i) == 0x06054b50) { eocd = i; break; }
    }
    if (eocd < 0) goto done;
    unsigned entries = u16(z + eocd + 10);
    size_t cd = u32(z + eocd + 16);
    for (int pass = 0; names[pass] && !result; pass++) {
        size_t p = cd;
        for (unsigned e = 0; e < entries && p + 46 <= zlen; e++) {
            if (u32(z + p) != 0x02014b50) break;
            unsigned method = u16(z + p + 10);
            size_t csize = u32(z + p + 20), usize = u32(z + p + 24);
            unsigned nlen = u16(z + p + 28), xlen = u16(z + p + 30), clen = u16(z + p + 32);
            size_t local = u32(z + p + 42);
            const char *name = (const char *)z + p + 46;
            if (p + 46 + nlen <= zlen && name_is(name, nlen, names[pass]) && local + 30 <= zlen) {
                size_t data = local + 30 + u16(z + local + 26) + u16(z + local + 28);
                if (data + csize <= zlen && usize > 0 && usize <= 1 << 20) {
                    unsigned char *out = malloc(usize);
                    int ok = 0;
                    if (out && method == 0 && csize == usize) { memcpy(out, z + data, usize); ok = 1; }
                    else if (out && method == 8) ok = inflate_raw(z + data, csize, out, usize);
                    if (ok) { result = out; *size = usize; }
                    else free(out);
                }
                break;
            }
            p += 46 + nlen + xlen + clen;
        }
    }
done:
    free(z);
    return result;
}

unsigned char *romzip_load(const char *path, const char *const *names, size_t *size)
{
    size_t n = strlen(path);
    if (n > 4 && (!strcmp(path + n - 4, ".zip") || !strcmp(path + n - 4, ".ZIP")))
        return romzip_extract(path, names, size);
    return read_file(path, size);
}
