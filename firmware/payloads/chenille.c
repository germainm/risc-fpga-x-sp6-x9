/* Exemple LEDs D3..D14: chenillard dont la vitesse suit le DIP switch.
 * LEDS_REG: bit 0 = D3 ... bit 11 = D14.
 * Sert aussi de validation: les LEDs doivent s'allumer dans l'ordre
 * D3 -> D14. Si l'ordre observe differe, ajuster les broches dans soc.ucf.
 * Si tout est inverse (11 allumees / 1 eteinte), mettre
 * LEDS_ACTIVE_LOW = 1'b1 dans top.v.
 */
#include "soc.h"

void main(void)
{
    int pos = 0, dir = 1;

    uart_puts("chenille: DIP = vitesse, 'q' = retour bootloader\n");
    for (;;) {
        LEDS_REG = 1u << pos;
        pos += dir;
        if (pos == 11 || pos == 0) dir = -dir;   /* aller-retour */

        uint32_t vitesse = dip_read();           /* 0..255 */
        delay_ms(30 + (255 - vitesse));          /* DIP haut = rapide */

        if (uart_getc_nb() == 'q') { LEDS_REG = 0; return; }
    }
}
