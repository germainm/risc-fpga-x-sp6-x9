CROSS  ?= riscv64-unknown-elf-
ARCH   ?= $(shell $(CROSS)gcc -march=rv32i_zicsr -mabi=ilp32 -E -x c /dev/null >/dev/null 2>&1 && echo rv32i_zicsr || echo rv32i)
CFLAGS  = -march=$(ARCH) -mabi=ilp32 -Os -Wall -ffreestanding -fno-builtin \
          -Icommon -Iboot
LDFLAGS = -nostdlib -Wl,--gc-sections
CC      = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy

BOOT_SRC = common/start.S boot/main.c boot/diskio.c boot/eeprom.c boot/pff.c

all: build/boot.hex build/BLINK.BIN build/HELLO.BIN build/LCD.BIN build/DEMO.BIN build/MUSIC.BIN build/CHENILLE.BIN build/BOUTONS.BIN build/I2CSCAN.BIN build/BASIC.BIN build/GPIOTEST.BIN

build:
	mkdir -p build

build/boot.elf: $(BOOT_SRC) boot/link_boot.ld | build
	$(CC) $(CFLAGS) $(LDFLAGS) -T boot/link_boot.ld -o $@ $(BOOT_SRC) -lgcc
	$(CROSS)size $@

build/boot.hex: build/boot.elf
	$(OBJCOPY) -O binary $< build/boot.bin
	python3 bin2hex.py build/boot.bin $@ 16384

build/%.BIN: payloads/%.elf
	$(OBJCOPY) -O binary $< $@

.PRECIOUS: payloads/%.elf
payloads/%.elf: payloads/%.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S $< -lgcc

payloads/BLINK.elf: payloads/blink.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S payloads/blink.c -lgcc
build/BLINK.BIN: payloads/BLINK.elf | build
	$(OBJCOPY) -O binary $< $@
payloads/HELLO.elf: payloads/hello.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S payloads/hello.c -lgcc
build/HELLO.BIN: payloads/HELLO.elf | build
	$(OBJCOPY) -O binary $< $@

payloads/LCD.elf: payloads/lcd.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S payloads/lcd.c -lgcc
build/LCD.BIN: payloads/LCD.elf | build
	$(OBJCOPY) -O binary $< $@

payloads/DEMO.elf: payloads/demo.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S payloads/demo.c -lgcc
build/DEMO.BIN: payloads/DEMO.elf | build
	$(OBJCOPY) -O binary $< $@

payloads/MUSIC.elf: payloads/music.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S payloads/music.c -lgcc
build/MUSIC.BIN: payloads/MUSIC.elf | build
	$(OBJCOPY) -O binary $< $@

payloads/CHENILLE.elf: payloads/chenille.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S payloads/chenille.c -lgcc
build/CHENILLE.BIN: payloads/CHENILLE.elf | build
	$(OBJCOPY) -O binary $< $@

payloads/BOUTONS.elf: payloads/boutons.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S payloads/boutons.c -lgcc
build/BOUTONS.BIN: payloads/BOUTONS.elf | build
	$(OBJCOPY) -O binary $< $@

payloads/I2CSCAN.elf: payloads/i2cscan.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S payloads/i2cscan.c -lgcc
build/I2CSCAN.BIN: payloads/I2CSCAN.elf | build
	$(OBJCOPY) -O binary $< $@

BASIC_SRC = common/start.S basic/basic.c basic/basio_soc.c basic/libmin.c \
            common/lcd1602.c basic/ff.c basic/diskio.c
basic/BASIC.elf: $(BASIC_SRC) payloads/link_payload.ld basic/basio.h basic/ffconf.h
	$(CC) $(CFLAGS) -Ibasic $(LDFLAGS) -T payloads/link_payload.ld -o $@ $(BASIC_SRC) -lgcc
build/BASIC.BIN: basic/BASIC.elf | build
	$(OBJCOPY) -O binary $< $@

payloads/GPIOTEST.elf: payloads/gpiotest.c common/start.S payloads/link_payload.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -T payloads/link_payload.ld -o $@ common/start.S payloads/gpiotest.c -lgcc
build/GPIOTEST.BIN: payloads/GPIOTEST.elf | build
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf build payloads/*.elf
