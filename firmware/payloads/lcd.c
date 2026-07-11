/* Payload: LCD1602 (HD44780) en mode 8 bits
 * gpio[7:0]=D0..D7 (P51,P55,P56,P57,P58,P59,P61,P62)
 * gpio[8]=RS (P47), gpio[9]=R/W (P48), gpio[10]=E (P50)
 *
 * IMPORTANT: R/W reste TOUJOURS a 0 (ecriture seule). Le LCD est alimente
 * en 5V: s'il pilotait le bus (R/W=1), il enverrait du 5V dans le
 * Spartan-6 qui ne le tolere pas. Ne jamais lire le busy flag.
 */
#include "soc.h"

#define LCD_RS (1u << 8)
#define LCD_EN (1u << 10)   /* R/W = bit 9, jamais leve */

static void lcd_wr(uint8_t b, int rs)
{
    uint32_t v = b | (rs ? LCD_RS : 0);
    GPIO_REG = v;          delay_us(1);
    GPIO_REG = v | LCD_EN; delay_us(2);   /* impulsion E >= 450 ns */
    GPIO_REG = v;          delay_us(1);
}

static void lcd_cmd(uint8_t c) { lcd_wr(c, 0); delay_us(50); }
static void lcd_dat(uint8_t c) { lcd_wr(c, 1); delay_us(50); }

static void lcd_init(void)
{
    delay_ms(50);                       /* apres mise sous tension */
    lcd_wr(0x30, 0); delay_ms(5);       /* sequence de reveil x3 */
    lcd_wr(0x30, 0); delay_us(150);
    lcd_wr(0x30, 0); delay_us(150);
    lcd_cmd(0x38);                      /* 8 bits, 2 lignes, 5x8 */
    lcd_cmd(0x08);                      /* affichage off */
    lcd_cmd(0x01); delay_ms(2);         /* clear */
    lcd_cmd(0x06);                      /* curseur -> droite */
    lcd_cmd(0x0C);                      /* affichage on, curseur off */
}

static void lcd_puts(const char *s) { while (*s) lcd_dat((uint8_t)*s++); }

void main(void)
{
    uart_puts("lcd: init 1602 mode 8 bits\n");
    lcd_init();
    lcd_puts("bonjour");
    lcd_cmd(0x80 | 0x40);               /* debut ligne 2 */
    lcd_puts("RISC-V @ 50MHz");
    uart_puts("lcd: bonjour affiche. 'q' = retour bootloader\n");
    while (uart_getc() != 'q') ;
}
