/* Payload: clignote la LED sur P133 a 2 Hz. 'q' sur la console = retour bootloader */
#include "soc.h"
void main(void)
{
    uart_puts("blink: LED P133 a 2 Hz ('q' = retour bootloader)\n");
    for (;;) {
        LED_REG = 1; delay_ms(250);
        LED_REG = 0; delay_ms(250);
        int c = uart_getc_nb();
        if (c == 'q') return;     /* retour -> reset -> bootloader */
    }
}
