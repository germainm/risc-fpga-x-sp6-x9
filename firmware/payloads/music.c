/* Exemple cloche: "Au clair de la lune" (traditionnel)
 * bell_freq(hz) demarre une tonalite continue; bell_freq(0) l'arrete.
 * Le generateur est materiel: le CPU est libre pendant que ca sonne.
 */
#include "soc.h"

#define DO5 523
#define RE5 587
#define MI5 659
#define SOL4 392
#define LA4 440

struct note { uint16_t hz; uint16_t ms; };

static const struct note melodie[] = {
    {DO5,400},{DO5,400},{DO5,400},{RE5,400},{MI5,800},{RE5,800},
    {DO5,400},{MI5,400},{RE5,400},{RE5,400},{DO5,1600},
    {0,0}
};

void main(void)
{
    uart_puts("music: Au clair de la lune ('q' pour arreter)\n");
    for (;;) {
        for (int i = 0; melodie[i].ms; i++) {
            bell_freq(melodie[i].hz);
            delay_ms(melodie[i].ms - 40);
            bell_freq(0);                 /* petit silence entre les notes */
            delay_ms(40);
            if (uart_getc_nb() == 'q') { bell_freq(0); return; }
        }
        delay_ms(1200);
    }
}
