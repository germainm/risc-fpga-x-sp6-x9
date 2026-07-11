/* Pilote I2C bit-bang + 24C04.
 * I2C_REG: ecrire b0=1 tire SCL a 0, b1=1 tire SDA a 0 (collecteur ouvert).
 * Lire: b2 = etat reel de SDA. ~100 kHz.
 * Enregistrement a l'offset 0 (page de 16 o): 'R','V', nom[13] nul-termine.
 */
#include "soc.h"
#include "eeprom.h"

static uint32_t shadow;

static void i2c_hd(void) { delay_us(5); }   /* demi-periode */
static void scl_w(int v) { shadow = v ? (shadow & ~1u) : (shadow | 1u); I2C_REG = shadow; }
static void sda_w(int v) { shadow = v ? (shadow & ~2u) : (shadow | 2u); I2C_REG = shadow; }
static int  sda_r(void)  { return (I2C_REG >> 2) & 1; }

static void i2c_start(void)
{
    sda_w(1); scl_w(1); i2c_hd();
    sda_w(0); i2c_hd();
    scl_w(0); i2c_hd();
}
static void i2c_stop(void)
{
    sda_w(0); i2c_hd();
    scl_w(1); i2c_hd();
    sda_w(1); i2c_hd();
}
static int i2c_wr(uint8_t b)                 /* retourne 1 si ACK */
{
    for (int i = 7; i >= 0; i--) {
        sda_w((b >> i) & 1); i2c_hd();
        scl_w(1); i2c_hd();
        scl_w(0);
    }
    sda_w(1); i2c_hd();                      /* relacher pour l'ACK */
    scl_w(1); i2c_hd();
    int ack = !sda_r();
    scl_w(0); i2c_hd();
    return ack;
}
static uint8_t i2c_rd(int ack)
{
    uint8_t b = 0;
    sda_w(1);
    for (int i = 7; i >= 0; i--) {
        scl_w(1); i2c_hd();
        b = (b << 1) | sda_r();
        scl_w(0); i2c_hd();
    }
    sda_w(ack ? 0 : 1); i2c_hd();
    scl_w(1); i2c_hd();
    scl_w(0); sda_w(1); i2c_hd();
    return b;
}

static int ee_read(uint8_t addr, uint8_t *buf, int n)
{
    i2c_start();
    if (!i2c_wr(0xA0)) { i2c_stop(); return -1; }
    if (!i2c_wr(addr)) { i2c_stop(); return -1; }
    i2c_start();
    if (!i2c_wr(0xA1)) { i2c_stop(); return -1; }
    for (int i = 0; i < n; i++) buf[i] = i2c_rd(i < n - 1);
    i2c_stop();
    return 0;
}
static int ee_write_page(uint8_t addr, const uint8_t *buf, int n)  /* n <= 16, meme page */
{
    i2c_start();
    if (!i2c_wr(0xA0)) { i2c_stop(); return -1; }
    i2c_wr(addr);
    for (int i = 0; i < n; i++) i2c_wr(buf[i]);
    i2c_stop();
    for (int t = 0; t < 60; t++) {           /* ACK polling, ~6 ms max */
        i2c_start();
        int a = i2c_wr(0xA0);
        i2c_stop();
        if (a) return 0;
        delay_us(100);
    }
    return -1;
}

int eeprom_get_last(char *name13)
{
    uint8_t rec[15];
    if (ee_read(0, rec, 15)) return -1;
    if (rec[0] != 'R' || rec[1] != 'V') return -1;
    int ok = 0;
    for (int i = 2; i < 15; i++) {
        if (rec[i] == 0) { ok = 1; break; }
        if (rec[i] < ' ' || rec[i] > 'z') return -1;   /* contenu suspect */
    }
    if (!ok) return -1;
    for (int i = 0; i < 13; i++) name13[i] = (char)rec[2 + i];
    return 0;
}

int eeprom_set_last(const char *name)
{
    uint8_t rec[15];
    char cur[13];
    rec[0] = 'R'; rec[1] = 'V';
    int i = 0;
    for (; i < 12 && name[i]; i++) rec[2 + i] = (uint8_t)name[i];
    for (; i < 13; i++) rec[2 + i] = 0;
    if (eeprom_get_last(cur) == 0) {         /* eviter les ecritures inutiles */
        int same = 1;
        for (int j = 0; j < 13; j++) if (cur[j] != (char)rec[2 + j]) { same = 0; break; }
        if (same) return 0;
    }
    return ee_write_page(0, rec, 15);
}
