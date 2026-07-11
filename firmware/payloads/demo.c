/* Demo: afficheur 7 segments + DIP switch + cloche
 * - chiffres 6-7: valeur hexa du DIP switch (bouge en direct)
 * - chiffres 0-3: compteur de secondes
 * - console: 'b' = bip 1 kHz, 'm' = petite melodie, 'q' = retour bootloader
 * Sert aussi de validation: basculer chaque interrupteur doit changer
 * un bit affiche; sinon la broche correspondante du UCF est a corriger.
 */
#include "soc.h"

static const uint8_t font[16] = {
    0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,
    0x7F,0x6F,0x77,0x7C,0x39,0x5E,0x79,0x71
};

static void print_bin8(uint8_t v)
{
    for (int i = 7; i >= 0; i--) uart_putc((v >> i) & 1 ? '1' : '0');
}

void main(void)
{
    uint8_t  last = 0xAA;
    uint32_t t0 = rdcycle(), sec = 0;

    uart_puts("demo: 7seg + DIP + cloche ('b'=bip, 'm'=melodie, 'q'=retour)\n");
    for (int i = 0; i < 8; i++) SEG7(i) = 0;

    for (;;) {
        uint8_t sw = dip_read();
        if (sw != last) {
            last = sw;
            uart_puts("DIP = "); print_bin8(sw); uart_puts("\n");
            SEG7(7) = font[sw >> 4];
            SEG7(6) = font[sw & 0xF];
        }
        if (rdcycle() - t0 >= F_CPU) {           /* 1 s */
            t0 += F_CPU; sec++;
            SEG7(0) = font[sec % 10];
            SEG7(1) = font[(sec / 10) % 10];
            SEG7(2) = font[(sec / 100) % 10];
            SEG7(3) = font[(sec / 1000) % 10];
        }
        int c = uart_getc_nb();
        if (c == 'b') { bell_freq(1000); delay_ms(120); bell_freq(0); }
        if (c == 'm') {
            static const uint16_t notes[] = {523, 659, 784, 1047};
            for (int i = 0; i < 4; i++) { bell_freq(notes[i]); delay_ms(160); }
            bell_freq(0);
        }
        if (c == 'q') { bell_freq(0); return; }
    }
}
