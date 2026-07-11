/* Diagnostic I2C v2: scanne avec le mappage normal PUIS avec SDA/SCL
 * inverses (en logiciel, sans resynthese). Si l'inverse repond, il
 * faudra permuter P101/P102 dans soc.ucf.
 * I2C_REG: b0=drive P101, b1=drive P102, b2=lecture P102, b3=lecture P101
 * 'r' = rescanner, 'q' = retour bootloader
 */
#include "soc.h"

static uint32_t sh;
static uint32_t scl_m, sda_m;   /* masques de drive */
static int      sda_sh;         /* bit de lecture SDA */

static void hd(void)    { delay_us(5); }
static void scl(int v)  { sh = v ? (sh & ~scl_m) : (sh | scl_m); I2C_REG = sh; }
static void sda(int v)  { sh = v ? (sh & ~sda_m) : (sh | sda_m); I2C_REG = sh; }
static int  sda_r(void) { return (I2C_REG >> sda_sh) & 1; }

static void i2c_start(void) { sda(1); scl(1); hd(); sda(0); hd(); scl(0); hd(); }
static void i2c_stop(void)  { sda(0); hd(); scl(1); hd(); sda(1); hd(); }
static int  i2c_wr(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        sda((b >> i) & 1); hd();
        scl(1); hd(); scl(0);
    }
    sda(1); hd();
    scl(1); hd();
    int ack = !sda_r();
    scl(0); hd();
    return ack;
}

static void put_hex8(uint8_t v)
{
    const char *h = "0123456789ABCDEF";
    uart_putc(h[v >> 4]); uart_putc(h[v & 15]);
}

static int scan_map(const char *nom, uint32_t sm, uint32_t dm, int dsh)
{
    scl_m = sm; sda_m = dm; sda_sh = dsh;
    sh = 0; I2C_REG = 0; delay_ms(1);
    uart_puts(nom);
    int n = 0;
    for (int a = 0x02; a <= 0xFE; a += 2) {
        i2c_start();
        int ack = i2c_wr((uint8_t)a);
        i2c_stop();
        if (ack) {
            uart_puts("  ACK a 0x"); put_hex8((uint8_t)a); uart_puts("\n");
            n++;
        }
    }
    if (!n) uart_puts("  rien\n");
    return n;
}

static void scan(void)
{
    int a = scan_map("Mappage normal (SCL=P101, SDA=P102):\n", 1u, 2u, 2);
    int b = scan_map("Mappage inverse (SCL=P102, SDA=P101):\n", 2u, 1u, 3);
    if (b && !a)
        uart_puts("=> lignes croisees: permuter P101/P102 dans soc.ucf\n");
    else if (!a && !b)
        uart_puts("=> rien sur les 2 mappages: verifier alim/continuite 24C04\n");
}

void main(void)
{
    uart_puts("i2cscan v2 ('r' = rescanner, 'q' = retour)\n");
    scan();
    for (;;) {
        int c = uart_getc();
        if (c == 'q') return;
        if (c == 'r') scan();
    }
}
