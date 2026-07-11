# Demarrage commun bootloader/payload
    .section .init
    .globl _start
_start:
    la   sp, __stack_top
    # copie .data (LMA -> VMA), no-op si identiques
    la   a0, __data_lma
    la   a1, __data_start
    la   a2, __data_end
    beq  a0, a1, 2f
1:  bgeu a1, a2, 2f
    lw   a3, 0(a0)
    sw   a3, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    j    1b
2:  # efface .bss
    la   a0, __bss_start
    la   a1, __bss_end
3:  bgeu a0, a1, 4f
    sw   zero, 0(a0)
    addi a0, a0, 4
    j    3b
4:  call main
    # retour de main -> redemarrage a froid (ROM)
    li   a0, 0
    jr   a0
