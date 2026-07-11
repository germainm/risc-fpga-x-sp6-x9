/* Exemple boutons K1/K2/K4 avec anti-rebond logiciel.
 * K1: allume la moitie gauche des LEDs
 * K2: allume la moitie droite
 * K4: bip 880 Hz tant qu'il est enfonce
 * Chaque appui est trace sur la console. 'q' = retour bootloader.
 */
#include "soc.h"

void main(void)
{
    uint8_t prev = 0;

    uart_puts("boutons: K1/K2 -> LEDs, K4 -> bip ('q' = retour)\n");
    for (;;) {
        uint8_t b = btn_read();
        delay_ms(20);                     /* anti-rebond simple */
        if (btn_read() != b) continue;    /* etat instable: on ignore */

        if (b != prev) {                  /* front: trace des changements */
            if ((b & 1) && !(prev & 1)) uart_puts("K1 presse\n");
            if ((b & 2) && !(prev & 2)) uart_puts("K2 presse\n");
            if ((b & 4) && !(prev & 4)) uart_puts("K4 presse\n");
            prev = b;
        }

        LEDS_REG = ((b & 1) ? 0x03F : 0) | ((b & 2) ? 0xFC0 : 0);
        bell_freq((b & 4) ? 880 : 0);

        if (uart_getc_nb() == 'q') { LEDS_REG = 0; bell_freq(0); return; }
    }
}
