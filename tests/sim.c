/* sim — the rules without a screen: the demo player plays waves 1..N many
 * times over; checks that nothing breaks and prints what happened.
 *   vt-sim [games] [waves]
 */
#include <stdio.h>
#include <stdlib.h>

#include "../src/game.h"
#include "../src/render.h"
#include "../src/sound.h"

int main(int argc, char **argv)
{
    int games = argc > 1 ? atoi(argv[1]) : 20;
    int waves = argc > 2 ? atoi(argv[2]) : 45;
    static world w;
    long frames = 0;
    int cleared = 0, deaths = 0, maxwave = 0, maxents = 0;
    for (int g = 0; g < games; g++) {
        world_new_game(&w, 50, 25000, 5, (unsigned)g + 1);
        w.wave = 1 + g % waves;
        world_start_wave(&w);
        for (int f = 0; f < 60 * 60 * 10; f++) {
            controls c;
            demo_controls(&w, &c);
            world_step(&w, &c);
            fx_step();
            frames++;
            if (w.n > maxents) maxents = w.n;
            if (w.n < 0 || w.n > MAX_ENTS) { printf("bad entity count %d\n", w.n); return 1; }
            for (int i = 0; i < w.n; i++) {
                const ent *e = &w.e[i];
                if (e->type == T_NONE) continue;
                if (e->x < ARENA_X0 - 2 || e->x > ARENA_X1 + 2 || e->z < ARENA_Z0 - 2 || e->z > ARENA_Z1 + 2) {
                    printf("wave %d: type %d out of the arena at %.1f %.1f\n", w.wave, e->type, e->x, e->z);
                    return 1;
                }
            }
            if (w.ph == PH_CLEARED && w.ph_time > 50) {
                cleared++;
                w.wave++;
                if (w.wave > maxwave) maxwave = w.wave;
                world_start_wave(&w);
            } else if (w.ph == PH_RESTART) {
                deaths++;
                world_resume(&w);
            } else if (w.ph == PH_OVER) {
                break;
            }
        }
        printf("game %2d: wave %2d  score %7d  men %d\n", g, w.wave, w.score, w.lives);
    }
    printf("%ld frames, %d waves cleared, %d deaths, highest wave %d, most entities %d\n",
           frames, cleared, deaths, maxwave, maxents);
    return 0;
}
