/* vt-sounds — the original sounds, recorded from the sound ROM into WAV files.
 *
 *   vt-sounds [<robotron.zip or sound ROM>] [<folder>]
 *
 * Without a ROM given, it is looked for as the game does (VT_SOUND_ROM, the
 * game's assets/, AdvanceMAME's ROM folders). The WAVs go to <folder>
 * (default: sounds/), named as the game's assets/sounds/ files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/romzip.h"
#include "../src/sound.h"
#include "../src/williams.h"

static void put16(FILE *f, int v) { fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); }
static void put32(FILE *f, long v) { put16(f, (int)(v & 0xFFFF)); put16(f, (int)((v >> 16) & 0xFFFF)); }

static int write_wav(const char *path, const float *buf, int n, int rate)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + n * 2L);
    fwrite("WAVEfmt ", 1, 8, f); put32(f, 16); put16(f, 1); put16(f, 1);
    put32(f, rate); put32(f, rate * 2L); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, n * 2L);
    for (int i = 0; i < n; i++) {
        float v = buf[i] * 32767;
        put16(f, (int)(v > 32767 ? 32767 : v < -32768 ? -32768 : v));
    }
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    static const char *const NAMES[] = { "video_sound_rom_3_std_767.ic12", "video_sound_rom_1.ic12", "robotron.snd", "vsr1.ic12", NULL };
    size_t n = 0;
    unsigned char *rom = argc > 1 ? romzip_load(argv[1], NAMES, &n) : snd_find_rom(&n);
    const char *dir = argc > 2 ? argv[2] : "sounds";
    if (!rom) {
        fprintf(stderr, "vt-sounds: no Robotron sound ROM %s%s\n", argc > 1 ? "in " : "found", argc > 1 ? argv[1] : "");
        return 1;
    }
    char cmd[1100];
#ifdef _WIN32
    /* cmd.exe has no mkdir -p and makes the parents by itself */
    snprintf(cmd, sizeof cmd, "%s", dir);
    for (char *p = cmd; *p; p++) if (*p == '/') *p = '\\';
    char mk[2300];
    snprintf(mk, sizeof mk, "if not exist \"%s\" mkdir \"%s\"", cmd, cmd);
    if (system(mk) != 0) { fprintf(stderr, "vt-sounds: cannot make %s\n", dir); return 1; }
#else
    snprintf(cmd, sizeof cmd, "mkdir -p '%s'", dir);
    if (system(cmd) != 0) { fprintf(stderr, "vt-sounds: cannot make %s\n", dir); return 1; }
#endif
    int written = 0;
    for (int i = 0; i < SND_COUNT; i++) {
        int frames = 0;
        float *buf = snd_record_original(rom, n, (snd_id)i, 44100, &frames);
        if (!buf) continue;
        char path[1200];
        snprintf(path, sizeof path, "%s/%s.wav", dir, snd_name((snd_id)i));
        if (write_wav(path, buf, frames, 44100)) {
            printf("%-16s %5.2f s\n", snd_name((snd_id)i), frames / 44100.0);
            written++;
        }
        free(buf);
    }
    free(rom);
    printf("%d sounds in %s/\n", written, dir);
    return written ? 0 : 1;
}
