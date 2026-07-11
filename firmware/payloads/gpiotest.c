/* Test du port GPIO generique 8 bits (P9,P10,P124,P132,P134,P138,P140,P142).
 * Broche 0 (P9) configuree en SORTIE: on la fait clignoter -> a mesurer
 *   au multimetre/oscillo, ou relier a une LED externe + resistance.
 * Broches 1-7 configurees en ENTREE: leur etat est affiche en continu ->
 *   relier un capteur tout-ou-rien (interrupteur, reed, PIR...) dessus
 *   et observer le bit changer.
 * 'q' = retour bootloader.
 */
#include "soc.h"

static void put_hex8(uint8_t v)
{
    const char *h = "0123456789ABCDEF";
    uart_putc(h[v >> 4]); uart_putc(h[v & 15]);
}

void main(void)
{
    uart_puts("gpiotest: broche0=sortie (clignote), broches1-7=entrees\n");
    uart_puts("Branche un capteur sur xgpio1..7 et regarde le bit changer.\n");

    gpx_mode(0, 1);                  /* P9 en sortie */
    for (int i = 1; i < 8; i++) gpx_mode(i, 0);  /* les autres en entree */

    uint8_t prev = 0xFF, out = 0;
    uint32_t t0 = rdcycle();

    for (;;) {
        if (rdcycle() - t0 >= F_CPU / 2) {   /* clignote la sortie a 1 Hz */
            t0 += F_CPU / 2;
            out ^= 1;
            gpx_write(0, out);
        }
        uint8_t in = (uint8_t)(GPX_IN & 0xFE);   /* bits 1-7 */
        if (in != prev) {
            prev = in;
            uart_puts("XGPIO(1..7) = ");
            put_hex8(in);
            uart_puts(" (bit1=P10 bit2=P124 bit3=P132 bit4=P134 bit5=P138 bit6=P140 bit7=P142)\n");
        }
        if (uart_getc_nb() == 'q') { gpx_write(0, 0); return; }
    }
}
