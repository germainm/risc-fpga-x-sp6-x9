/* basio_soc.c - implementation cible: console UART, registres du SoC,
 * fichiers sur carte SD via FatFs. */
#include "soc.h"
#include "basio.h"
#include "ff.h"
#include "lcd1602.h"

static const uint8_t seg_font[16] = {
    0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,
    0x7F,0x6F,0x77,0x7C,0x39,0x5E,0x79,0x71
};

void bas_putc(char c)         { if (c == '\n') uart_putc('\r'); uart_putc(c); }
void bas_puts(const char *s)  { while (*s) bas_putc(*s++); }
int  bas_getc(void)           { return uart_getc(); }
int  bas_inkey(void)          { return uart_getc_nb(); }

uint32_t bas_peek(uint32_t a) { return *(volatile uint32_t *)(a & ~3u); }
void bas_poke(uint32_t a, uint32_t v) { *(volatile uint32_t *)(a & ~3u) = v; }
void bas_led(uint32_t v)      { LEDS_REG = v; }
void bas_bell(uint32_t hz)    { bell_freq(hz); }
void bas_seg(int d, int v)
{
    SEG7(d & 7) = (v >= 0 && v <= 15) ? seg_font[v] : 0;
}
void bas_delay_ms(uint32_t ms){ delay_ms(ms); }
uint32_t bas_dip(void)        { return dip_read(); }
uint32_t bas_btn(void)        { return btn_read(); }
uint32_t bas_ticks(void)      { return rdcycle(); }

void bas_lcd_init(void)              { lcd_init(); }
void bas_lcd_clear(void)             { lcd_clear(); }
void bas_lcd_goto(int r, int c)      { lcd_goto(r, c); }
void bas_lcd_putc(char c)            { lcd_putc(c); }
void bas_lcd_puts(const char *s)     { lcd_puts(s); }

/* ---------- fichiers: FatFs, montage paresseux ---------- */
static FATFS fs;
static FIL   fil;
static DIR   dir;
static int   mounted, file_open;

static int ensure_mount(void)
{
    if (mounted) return 0;
    if (f_mount(&fs, "", 1) != FR_OK) return -1;
    mounted = 1;
    return 0;
}

int fio_open_read(const char *name)
{
    if (ensure_mount()) return -1;
    if (f_open(&fil, name, FA_READ) != FR_OK) return -1;
    file_open = 1;
    return 0;
}

int fio_open_write(const char *name)
{
    if (ensure_mount()) return -1;
    if (f_open(&fil, name, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return -1;
    file_open = 1;
    return 0;
}

int fio_read(void *buf, int n)
{
    UINT br;
    if (!file_open) return -1;
    if (f_read(&fil, buf, (UINT)n, &br) != FR_OK) return -1;
    return (int)br;
}

int fio_write(const void *buf, int n)
{
    UINT bw;
    if (!file_open) return -1;
    if (f_write(&fil, buf, (UINT)n, &bw) != FR_OK || bw != (UINT)n) return -1;
    return 0;
}

void fio_close(void)
{
    if (file_open) { f_close(&fil); file_open = 0; }
}

int fio_dir_open(void)
{
    if (ensure_mount()) return -1;
    return (f_opendir(&dir, "/") == FR_OK) ? 0 : -1;
}

int fio_dir_next(char *name13, uint32_t *size)
{
    FILINFO fi;
    for (;;) {
        if (f_readdir(&dir, &fi) != FR_OK || fi.fname[0] == 0) {
            f_closedir(&dir);
            return -1;
        }
        if (fi.fattrib & AM_DIR) continue;
        int i;
        for (i = 0; i < 12 && fi.fname[i]; i++) name13[i] = fi.fname[i];
        name13[i] = 0;
        *size = (uint32_t)fi.fsize;
        return 0;
    }
}
