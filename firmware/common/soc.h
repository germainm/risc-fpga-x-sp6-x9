#ifndef SOC_H
#define SOC_H
#include <stdint.h>

#define MMIO(o)      (*(volatile uint32_t*)(0x80000000u + (o)))
#define UART_DATA    MMIO(0x00)
#define UART_STAT    MMIO(0x04)   /* bit0 rx_avail, bit1 tx_busy */
#define SPI_DATA     MMIO(0x10)
#define SPI_STAT     MMIO(0x14)   /* bit0 busy */
#define SPI_CTRL     MMIO(0x18)   /* bit0 cs_n, bits 15:8 diviseur */
#define LED_REG      MMIO(0x20)
#define GPIO_REG     MMIO(0x30)   /* LCD: [7:0]=D0..D7 [8]=RS [9]=RW [10]=E */
#define BELL_REG     MMIO(0x38)   /* [15:0] demi-periode; 0=silence; bit31=continu */
#define DIP_REG      MMIO(0x3C)   /* brut: ON = 0 (pullups) */
#define LEDS_REG     MMIO(0x24)   /* bits 11:0 -> LEDs D3..D14 */
#define BTN_REG      MMIO(0x28)   /* brut: presse = 0 */
#define I2C_REG      MMIO(0x2C)   /* W: b0=scl_oe b1=sda_oe; R: +b2=sda_in b3=scl_in */
#define I2C2_REG     MMIO(0x60)   /* meme protocole que I2C_REG, bus libre pour capteurs */
#define GPX_DIR      MMIO(0x64)   /* 1 = broche en sortie */
#define GPX_OUT      MMIO(0x68)   /* valeur ecrite si sortie */
#define GPX_IN       MMIO(0x6C)   /* etat reel de chaque broche (lecture) */
#define SEG7(i)      MMIO(0x40 + ((i) << 2))  /* motif chiffre i, bit0=A..bit7=DP */

#define RAM_BASE     0x00010000u
#define RAM_SIZE     0x00008000u  /* 32 Ko */
#define PAYLOAD_MAX  0x00007000u  /* 28 Ko: proteger data/pile du boot */
#define F_CPU        50000000u

static inline void uart_putc(char c){
    while (UART_STAT & 2) ;
    UART_DATA = (uint8_t)c;
}
static inline void uart_puts(const char *s){
    while (*s){ if (*s=='\n') uart_putc('\r'); uart_putc(*s++); }
}
static inline int uart_getc_nb(void){
    if (!(UART_STAT & 1)) return -1;
    return (int)(UART_DATA & 0xFF);
}
static inline int uart_getc(void){
    int c; while ((c = uart_getc_nb()) < 0) ; return c;
}
static inline void put_hex32(uint32_t v){
    for (int i = 28; i >= 0; i -= 4){
        int d = (v >> i) & 0xF;
        uart_putc(d < 10 ? '0'+d : 'A'+d-10);
    }
}
static inline void put_udec(uint32_t v){
    char b[11]; int i = 0;
    do { b[i++] = '0' + v % 10; v /= 10; } while (v);
    while (i) uart_putc(b[--i]);
}
static inline uint32_t rdcycle(void){
    uint32_t v; __asm__ volatile ("rdcycle %0" : "=r"(v)); return v;
}
static inline void delay_us(uint32_t us){
    uint32_t t0 = rdcycle();
    while ((rdcycle() - t0) < us * (F_CPU/1000000)) ;
}
static inline void delay_ms(uint32_t ms){
    uint32_t t0 = rdcycle();
    while ((rdcycle() - t0) < ms * (F_CPU/1000)) ;
}
static inline uint8_t btn_read(void){   /* bit0=K1 bit1=K2 bit2=K4; 1 = presse */
    return (uint8_t)(~BTN_REG) & 0x07;
}
static inline uint8_t dip_read(void){   /* 1 = interrupteur ON */
    return (uint8_t)~DIP_REG;
}
static inline void bell_freq(uint32_t hz){
    BELL_REG = hz ? (F_CPU/2)/hz : 0;
}
static inline void gpx_mode(int pin, int is_output){
    uint32_t d = GPX_DIR;
    GPX_DIR = is_output ? (d | (1u << pin)) : (d & ~(1u << pin));
}
static inline void gpx_write(int pin, int val){
    uint32_t o = GPX_OUT;
    GPX_OUT = val ? (o | (1u << pin)) : (o & ~(1u << pin));
}
static inline int gpx_read(int pin){
    return (GPX_IN >> pin) & 1;
}
static inline void spi_ctrl(int cs_n, uint8_t div){
    SPI_CTRL = ((uint32_t)div << 8) | (cs_n & 1);
}
static inline uint8_t spi_xfer(uint8_t v){
    SPI_DATA = v;
    while (SPI_STAT & 1) ;
    return (uint8_t)SPI_DATA;
}
#endif
