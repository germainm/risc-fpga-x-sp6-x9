#ifndef LCD1602_H
#define LCD1602_H
void lcd_init(void);
void lcd_clear(void);
void lcd_goto(int row, int col);     /* row 0-1, col 0-15 */
void lcd_putc(char c);
void lcd_puts(const char *s);
#endif
