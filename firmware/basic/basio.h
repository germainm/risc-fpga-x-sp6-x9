/* basio.h - couche d'E/S du Tiny BASIC.
 * SEUL point de contact entre l'interpreteur et le monde exterieur.
 * Pour rediriger la console (VGA, LCD, ...) ou changer de support de
 * stockage, ne modifier QUE l'implementation (basio_soc.c ou hote).
 */
#ifndef BASIO_H
#define BASIO_H
#include <stdint.h>

/* console */
void bas_putc(char c);
void bas_puts(const char *s);
int  bas_getc(void);       /* bloquant */
int  bas_inkey(void);      /* -1 si rien */

/* materiel */
uint32_t bas_peek(uint32_t addr);
void bas_poke(uint32_t addr, uint32_t val);
void bas_led(uint32_t v);
void bas_bell(uint32_t hz);
void bas_seg(int digit, int val);      /* val -1 = eteint, 0..15 = hexa */
void bas_delay_ms(uint32_t ms);
uint32_t bas_dip(void);
uint32_t bas_btn(void);
uint32_t bas_ticks(void);              /* pour graine RND */

/* LCD1602 */
void bas_lcd_init(void);
void bas_lcd_clear(void);
void bas_lcd_goto(int row, int col);   /* row 0-1, col 0-15 */
void bas_lcd_putc(char c);
void bas_lcd_puts(const char *s);

/* fichiers (retour 0 = ok) */
int  fio_open_read(const char *name);
int  fio_open_write(const char *name);
int  fio_read(void *buf, int n);       /* octets lus, 0 = fin, <0 = erreur */
int  fio_write(const void *buf, int n);
void fio_close(void);
int  fio_dir_open(void);
int  fio_dir_next(char *name13, uint32_t *size);  /* 0 = ok, <0 = fin */

#endif
