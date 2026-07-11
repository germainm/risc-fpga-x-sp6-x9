/* Pilote SD SPI pour Petit FatFs - module SD externe sur SPI */
#include "diskio.h"
#include "soc.h"

#define SPI_DIV_INIT 62   /* 396.8 kHz */
#define SPI_DIV_FAST 3    /* 6.25 MHz  */

#define CMD0   0
#define CMD1   1
#define CMD8   8
#define CMD16  16
#define CMD17  17
#define CMD55  55
#define CMD58  58
#define ACMD41 41

static uint8_t card_type;   /* bit0: v2, bit1: bloc (SDHC) */

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t r, n;
    if (cmd == ACMD41) {
        r = sd_cmd(CMD55, 0);
        if (r > 1) return r;
        cmd = ACMD41;
    }
    spi_xfer(0xFF);
    spi_xfer(0x40 | (cmd & 0x3F));
    spi_xfer(arg >> 24); spi_xfer(arg >> 16);
    spi_xfer(arg >> 8);  spi_xfer(arg);
    spi_xfer(cmd == CMD0 ? 0x95 : (cmd == CMD8 ? 0x87 : 0x01)); /* CRC */
    for (n = 10; n; n--) {
        r = spi_xfer(0xFF);
        if (!(r & 0x80)) break;
    }
    return r;
}

DSTATUS disk_initialize(void)
{
    uint8_t n;
    uint32_t tmo;

    card_type = 0;
    spi_ctrl(1, SPI_DIV_INIT);
    for (n = 10; n; n--) spi_xfer(0xFF);   /* 80 cycles, CS haut */
    spi_ctrl(0, SPI_DIV_INIT);

    if (sd_cmd(CMD0, 0) != 1) goto fail;   /* idle */

    if (sd_cmd(CMD8, 0x1AA) == 1) {        /* v2 */
        uint8_t ocr[4];
        for (n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) goto fail;
        card_type = 1;
        for (tmo = 200000; tmo; tmo--) {   /* ACMD41 HCS */
            if (sd_cmd(ACMD41, 1UL << 30) == 0) break;
        }
        if (!tmo) goto fail;
        if (sd_cmd(CMD58, 0)) goto fail;   /* OCR: CCS? */
        for (n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
        if (ocr[0] & 0x40) card_type |= 2; /* SDHC: adressage bloc */
    } else {                               /* v1 / MMC */
        for (tmo = 200000; tmo; tmo--) {
            if (sd_cmd(ACMD41, 0) == 0) break;
            if (tmo == 100000 && sd_cmd(CMD1, 0) == 0) break; /* MMC */
        }
        if (!tmo) goto fail;
        if (sd_cmd(CMD16, 512)) goto fail;
    }
    spi_ctrl(0, SPI_DIV_FAST);
    return 0;
fail:
    spi_ctrl(1, SPI_DIV_INIT);
    return STA_NOINIT;
}

DRESULT disk_readp(BYTE *buff, DWORD sector, UINT offset, UINT count)
{
    DRESULT res = RES_ERROR;
    uint32_t tmo;
    UINT i;

    if (!(card_type & 2)) sector *= 512;   /* SDSC: adresse octet */

    if (sd_cmd(CMD17, sector) == 0) {
        for (tmo = 400000; tmo; tmo--) {   /* attendre jeton 0xFE */
            if (spi_xfer(0xFF) == 0xFE) break;
        }
        if (tmo) {
            for (i = 0; i < offset; i++) spi_xfer(0xFF);
            for (i = 0; i < count; i++)  *buff++ = spi_xfer(0xFF);
            for (i = offset + count; i < 514; i++) spi_xfer(0xFF); /* reste + CRC */
            res = RES_OK;
        }
    }
    spi_xfer(0xFF);
    return res;
}

DRESULT disk_writep(const BYTE *buff, DWORD sc)
{
    (void)buff; (void)sc;
    return RES_NOTRDY;  /* lecture seule */
}
