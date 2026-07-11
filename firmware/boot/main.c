/* Bootloader SD - PicoRV32 / Spartan-6
 * - DEFAULT.BIN present: demarrage auto apres 3 s (touche = menu)
 * - sinon: menu des *.BIN de la racine
 */
#include "soc.h"
#include "pff.h"
#include "diskio.h"
#include "eeprom.h"

#define MAX_FILES 16

static FATFS fs;
static char names[MAX_FILES][13];
static uint32_t sizes[MAX_FILES];

static int is_bin(const char *n)
{
    int l = 0;
    while (n[l]) l++;
    return l > 4 && n[l-4]=='.' && n[l-3]=='B' && n[l-2]=='I' && n[l-1]=='N';
}

static int scan_files(void)
{
    DIR dir; FILINFO fi;
    int n = 0;
    if (pf_opendir(&dir, "/") != FR_OK) return -1;
    while (pf_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        if (fi.fattrib & AM_DIR) continue;
        if (!is_bin(fi.fname)) continue;
        if (n < MAX_FILES) {
            int i = 0;
            do { names[n][i] = fi.fname[i]; } while (fi.fname[i++]);
            sizes[n] = fi.fsize;
            n++;
        }
    }
    return n;
}

static void run_payload(const char *name)
{
    UINT br; uint32_t got = 0;
    uint8_t *dst = (uint8_t *)RAM_BASE;

    if (pf_open(name) != FR_OK) { uart_puts("erreur: pf_open\n"); return; }
    uart_puts("Chargement "); uart_puts(name); uart_puts(" ");
    for (;;) {
        if (pf_read(dst + got, 512, &br) != FR_OK) { uart_puts("\nerreur lecture\n"); return; }
        got += br;
        if (got > PAYLOAD_MAX) { uart_puts("\npayload trop gros (max 36 Ko)\n"); return; }
        if ((got & 0x0FFF) == 0) uart_putc('.');
        if (br < 512) break;
    }
    uart_puts("\n"); put_udec(got); uart_puts(" octets. Saut a 0x00010000\n\n");
    ((void (*)(void))RAM_BASE)();
    /* si le payload retourne, start.S nous ramene ici via reset ROM */
}

void main(void)
{
    int nf, i, c;

    uart_puts("\n=== Bootloader RISC-V SD v1.0 - XC6SLX9 ===\n");
    LED_REG = 1;

    if (disk_initialize() & STA_NOINIT) {
        uart_puts("Carte SD absente ou init echouee.\n");
        uart_puts("Inserer une carte et appuyer une touche...\n");
        uart_getc();
        ((void (*)(void))0)();      /* redemarrage propre */
    }
    if (pf_mount(&fs) != FR_OK) {
        uart_puts("Pas de FAT valide (formater FAT16/FAT32).\n");
        uart_getc();
        ((void (*)(void))0)();
    }

    /* dernier payload memorise en EEPROM, sinon DEFAULT.BIN */
    char last[13];
    const char *auto_name = 0;
    if (eeprom_get_last(last) == 0 && pf_open(last) == FR_OK) {
        auto_name = last;
        uart_puts("EEPROM: dernier payload = "); uart_puts(last); uart_puts("\n");
    } else if (pf_open("DEFAULT.BIN") == FR_OK) {
        auto_name = "DEFAULT.BIN";
    }
    if (auto_name) {
        uart_puts("Demarrage de "); uart_puts(auto_name);
        uart_puts(" dans 3 s, touche = menu...\n");
        uint32_t t0 = rdcycle();
        int key = 0;
        while ((rdcycle() - t0) < 3 * F_CPU) {
            if (uart_getc_nb() >= 0) { key = 1; break; }
        }
        if (!key) { run_payload(auto_name); ((void (*)(void))0)(); }
    }

    /* menu */
    for (;;) {
        nf = scan_files();
        if (nf <= 0) {
            uart_puts("Aucun .BIN a la racine. Touche pour relire...\n");
            uart_getc();
            continue;
        }
        uart_puts("\nPayloads disponibles:\n");
        for (i = 0; i < nf; i++) {
            uart_putc('0' + i + 1); uart_puts(") ");
            uart_puts(names[i]); uart_puts("  (");
            put_udec(sizes[i]); uart_puts(" o)\n");
        }
        uart_puts("Choix: ");
        do { c = uart_getc(); } while (c < '1' || c >= '1' + nf);
        uart_putc(c); uart_puts("\n");
        if (eeprom_set_last(names[c - '1']) == 0)
            { uart_puts("EEPROM: memorise "); uart_puts(names[c - '1']); uart_puts("\n"); }
        else
            uart_puts("EEPROM: pas de reponse (24C04 absente?)\n");
        run_payload(names[c - '1']);
        ((void (*)(void))0)();
    }
}
