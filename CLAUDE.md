# Contexte projet pour Claude Code

SoC RISC-V sur Spartan-6 XC6SLX9 (TQG144, carte chinoise, horloge 50 MHz P126,
reset bouton P114 actif bas, UART P1/P2, outil: ISE 14.7 dans VM CentOS 6,
flash via iMPACT). Proprietaire: hobbyiste francophone, prefere solutions
concretes testees.

## Structure
- rtl/ : picorv32.v (upstream, ne pas modifier), top.v (SoC: decodage memoire,
  MMIO), uart.v, spi_master.v
- firmware/ : Makefile croise riscv64-unknown-elf (-march=rv32i_zicsr -mabi=ilp32)
  - boot/ : bootloader SD (Petit FatFs R0.03, lecture seule) -> boot.hex (ROM)
  - payloads/ : programmes charges depuis SD a 0x00010000
- sim/ : banc iverilog avec modele SD comportemental (image mtools sd.img)

## Regles importantes
- ROM 16 Ko max pour le boot; payloads 36 Ko max (proteger 0x19000-0x1A000 =
  data/pile du bootloader pendant le chargement).
- XST/ISE: Verilog-2001 seulement, $readmemh OK, RAM a byte-enable = 4 tableaux
  8 bits separes (pattern deja en place dans top.v).
- Toute modification RTL: valider avec `cd sim && vvp tb` avant synthese.
- MMIO: voir firmware/common/soc.h (source de verite des adresses).
