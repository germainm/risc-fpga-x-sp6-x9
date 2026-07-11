MEMORY {
  RAM (rwx) : ORIGIN = 0x00010000, LENGTH = 32K
}
__stack_top = 0x00017F00;
SECTIONS {
  .text : { KEEP(*(.init)) *(.text*) *(.rodata*) . = ALIGN(4); } > RAM
  __data_lma = LOADADDR(.data);
  .data : { __data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); __data_end = .; } > RAM
  .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); __bss_end = .; } > RAM
}
