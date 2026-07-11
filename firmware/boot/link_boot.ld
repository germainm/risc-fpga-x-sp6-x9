MEMORY {
  ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 16K
  RAM (rwx) : ORIGIN = 0x00017000, LENGTH = 2K
}
__stack_top = 0x00017F00;
SECTIONS {
  .text : { KEEP(*(.init)) *(.text*) *(.rodata*) . = ALIGN(4); } > ROM
  __data_lma = LOADADDR(.data);
  .data : { __data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); __data_end = .; } > RAM AT > ROM
  .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); __bss_end = .; } > RAM
}
