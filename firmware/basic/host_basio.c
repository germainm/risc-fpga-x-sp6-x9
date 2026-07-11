/* Implementation basio pour tests sur PC (gcc x86). */
#include <stdio.h>
#include <stdlib.h>
#include "basio.h"

static uint32_t fakemem[1024];

void bas_putc(char c)        { putchar(c); }
void bas_puts(const char *s) { fputs(s, stdout); }
int  bas_getc(void)
{
    int c = getchar();
    if (c == EOF) { fflush(stdout); exit(0); }
    return c;
}
int  bas_inkey(void)         { return -1; }

void bas_lcd_init(void)      { printf("[LCD INIT]"); }
void bas_lcd_clear(void)     { printf("[LCD CLR]"); }
void bas_lcd_goto(int r,int c){ printf("[LCD POS %d,%d]", r, c); }
void bas_lcd_putc(char c)    { printf("[LCD:%c]", c); }
void bas_lcd_puts(const char *s){ printf("[LCD:%s]", s); }

uint32_t bas_peek(uint32_t a){ return fakemem[(a >> 2) & 1023]; }
void bas_poke(uint32_t a, uint32_t v) { fakemem[(a >> 2) & 1023] = v; }
void bas_led(uint32_t v)     { printf("[LED %03X]", (unsigned)v & 0xFFF); }
void bas_bell(uint32_t hz)   { printf("[BELL %u]", (unsigned)hz); }
void bas_seg(int d, int v)   { printf("[SEG %d=%d]", d, v); }
void bas_delay_ms(uint32_t ms){ (void)ms; }
uint32_t bas_dip(void)       { return 0xAA; }
uint32_t bas_btn(void)       { return 0; }
uint32_t bas_ticks(void)     { return 12345; }

static FILE *f;
int fio_open_read(const char *n)  { f = fopen(n, "rb"); return f ? 0 : -1; }
int fio_open_write(const char *n) { f = fopen(n, "wb"); return f ? 0 : -1; }
int fio_read(void *b, int n)      { return (int)fread(b, 1, n, f); }
int fio_write(const void *b, int n){ return fwrite(b, 1, n, f) == (size_t)n ? 0 : -1; }
void fio_close(void)              { if (f) fclose(f); f = 0; }
int fio_dir_open(void)            { return 0; }
int fio_dir_next(char *n, uint32_t *s) { (void)n; (void)s; return -1; }
