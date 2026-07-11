# SoC RISC-V (PicoRV32) pour Spartan-6 XC6SLX9 — bootloader carte SD

## Architecture
- CPU PicoRV32 (RV32I), 50 MHz
- ROM boot 16 Ko @ 0x00000000 (BRAM, initialisee par firmware/build/boot.hex)
- RAM 40 Ko @ 0x00010000 (payloads charges a la base; max 36 Ko)
- MMIO @ 0x80000000 : UART 115200 (+0x00 data, +0x04 status),
  SPI SD (+0x10 data, +0x14 status, +0x18 ctrl), LED P133 (+0x20)

## Compilation firmware (VM Debian)
    apt install gcc-riscv64-unknown-elf python3
    cd firmware && make
Produit: build/boot.hex (ROM), build/BLINK.BIN, build/HELLO.BIN (payloads SD).

## Synthese ISE 14.7
1. Nouveau projet Spartan-6 XC6SLX9 TQG144 -2.
2. Ajouter rtl/picorv32.v, rtl/top.v, rtl/uart.v, rtl/spi_master.v et ise/soc.ucf.
3. IMPORTANT: copier firmware/build/boot.hex dans le repertoire de travail
   du projet ISE ($readmemh le cherche la au moment de la synthese).
4. Generer le .bit et flasher avec iMPACT comme d'habitude.

## Carte SD
- Formater FAT16 ou FAT32, noms 8.3 (ex: BLINK.BIN, DEFAULT.BIN).
- Copier des payloads .BIN a la racine.
- Si DEFAULT.BIN existe: demarrage auto apres 3 s (touche = menu).
- Payload = binaire lie a 0x00010000 (voir payloads/link_payload.ld),
  'return' de main() = retour au bootloader.

## Cablage module SD (AMS1117, sans level shifter)
  5V module <- 5V carte (ou 3.3 <- 3.3V), GND <- GND
  SDCS->P115  MOSI->P116  SCK->P117  MISO->P118

## Simulation (optionnel, deja validee)
    apt install iverilog mtools
    cd sim && cp ../firmware/build/boot.hex . && iverilog -o tb tb.v ../rtl/*.v && vvp tb
