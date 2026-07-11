/* Pilote SD SPI pour FatFs (lecture + ecriture par secteurs de 512 o).
 * Meme sequence d'init que le bootloader (CMD0/8/ACMD41/58), plus
 * CMD17 (lecture secteur), CMD24 (ecriture secteur) et CTRL_SYNC.
 */
#include "ff.h"
#include "diskio.h"
#include "soc.h"

#define SPI_DIV_INIT 62   /* 396.8 kHz */
#define SPI_DIV_FAST 3    /* 6.25 MHz  */

#define CMD0   0
#define CMD1   1
#define CMD8   8
#define CMD16  16
#define CMD17  17
#define CMD24  24
#define CMD55  55
#define CMD58  58
#define ACMD41 41

static uint8_t card_type;   /* bit0: v2, bit1: adressage bloc (SDHC) */
static uint8_t inited;

static int wait_ready(void)
{
    uint32_t t = 2500000;                 /* ~500 ms a 6.25 MHz */
    while (t--) if (spi_xfer(0xFF) == 0xFF) return 1;
    return 0;
}

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t r, n;
    if (cmd == ACMD41) {
        r = sd_cmd(CMD55, 0);
        if (r > 1) return r;
    }
    spi_xfer(0xFF);
    spi_xfer(0x40 | (cmd & 0x3F));
    spi_xfer(arg >> 24); spi_xfer(arg >> 16);
    spi_xfer(arg >> 8);  spi_xfer(arg);
    spi_xfer(cmd == CMD0 ? 0x95 : (cmd == CMD8 ? 0x87 : 0x01));
    for (n = 10; n; n--) {
        r = spi_xfer(0xFF);
        if (!(r & 0x80)) break;
    }
    return r;
}

DSTATUS disk_status(BYTE pdrv)
{
    (void)pdrv;
    return inited ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    uint8_t n;
    uint32_t tmo;
    (void)pdrv;

    inited = 0;
    card_type = 0;
    spi_ctrl(1, SPI_DIV_INIT);
    for (n = 10; n; n--) spi_xfer(0xFF);
    spi_ctrl(0, SPI_DIV_INIT);

    if (sd_cmd(CMD0, 0) != 1) goto fail;

    if (sd_cmd(CMD8, 0x1AA) == 1) {
        uint8_t ocr[4];
        for (n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) goto fail;
        card_type = 1;
        for (tmo = 200000; tmo; tmo--)
            if (sd_cmd(ACMD41, 1UL << 30) == 0) break;
        if (!tmo) goto fail;
        if (sd_cmd(CMD58, 0)) goto fail;
        for (n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
        if (ocr[0] & 0x40) card_type |= 2;
    } else {
        for (tmo = 200000; tmo; tmo--) {
            if (sd_cmd(ACMD41, 0) == 0) break;
            if (tmo == 100000 && sd_cmd(CMD1, 0) == 0) break;
        }
        if (!tmo) goto fail;
        if (sd_cmd(CMD16, 512)) goto fail;
    }
    spi_ctrl(0, SPI_DIV_FAST);
    inited = 1;
    return 0;
fail:
    spi_ctrl(1, SPI_DIV_INIT);
    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    (void)pdrv;
    if (!inited) return RES_NOTRDY;
    while (count--) {
        uint32_t addr = (card_type & 2) ? sector : sector * 512;
        uint32_t t;
        if (sd_cmd(CMD17, addr)) return RES_ERROR;
        for (t = 400000; t; t--) {          /* attendre jeton 0xFE */
            uint8_t d = spi_xfer(0xFF);
            if (d == 0xFE) break;
            if (d != 0xFF && (d & 0xE0) == 0) return RES_ERROR;
        }
        if (!t) return RES_ERROR;
        for (t = 0; t < 512; t++) buff[t] = spi_xfer(0xFF);
        spi_xfer(0xFF); spi_xfer(0xFF);     /* CRC */
        buff += 512;
        sector++;
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    (void)pdrv;
    if (!inited) return RES_NOTRDY;
    while (count--) {
        uint32_t addr = (card_type & 2) ? sector : sector * 512;
        uint32_t t;
        uint8_t r;
        if (!wait_ready()) return RES_ERROR;
        if (sd_cmd(CMD24, addr)) return RES_ERROR;
        spi_xfer(0xFF);
        spi_xfer(0xFE);                     /* jeton de donnees */
        for (t = 0; t < 512; t++) spi_xfer(buff[t]);
        spi_xfer(0xFF); spi_xfer(0xFF);     /* CRC bidon */
        r = spi_xfer(0xFF);                 /* reponse donnees */
        if ((r & 0x1F) != 0x05) return RES_ERROR;
        if (!wait_ready()) return RES_ERROR;
        buff += 512;
        sector++;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)pdrv; (void)buff;
    if (!inited) return RES_NOTRDY;
    switch (cmd) {
    case CTRL_SYNC:
        if (!wait_ready()) return RES_ERROR;
        return RES_OK;
    }
    return RES_PARERR;
}
