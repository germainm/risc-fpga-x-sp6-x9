/* Payload: echo console. 'q' = retour bootloader */
#include "soc.h"
void main(void)
{
    uart_puts("hello depuis la RAM! echo actif ('q' = retour)\n");
    for (;;) {
        int c = uart_getc();
        if (c == 'q') return;
        uart_putc(c);
        if (c == '\r') uart_putc('\n');
    }
}
