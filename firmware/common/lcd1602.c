/* Pilote LCD1602 (HD44780) mode 8 bits - partage entre BASIC et payloads C.
 * gpio[7:0]=D0..D7 (P51,P55,P56,P57,P58,P59,P61,P62)
 * gpio[8]=RS (P47), gpio[9]=R/W (P48, toujours a 0), gpio[10]=E (P50)
 *
 * IMPORTANT: R/W reste TOUJOURS a 0 (ecriture seule). Le LCD est alimente
 * en 5V: s'il pilotait le bus (R/W=1), il enverrait du 5V dans le
 * Spartan-6 qui ne le tolere pas. Ne jamais lire le busy flag.
 */
#include "soc.h"
#include "lcd1602.h"

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

void lcd_init(void)
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

void lcd_clear(void) { lcd_cmd(0x01); delay_ms(2); }

void lcd_goto(int row, int col)
{
    if (row < 0) row = 0; if (row > 1) row = 1;
    if (col < 0) col = 0; if (col > 15) col = 15;
    lcd_cmd(0x80 | (row ? 0x40 : 0x00) | col);
}

void lcd_putc(char c) { lcd_dat((uint8_t)c); }

void lcd_puts(const char *s) { while (*s) lcd_dat((uint8_t)*s++); }
