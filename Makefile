BACKEND = gcc
CROSS = x86_64-elf-

SRC = $(shell find src -name "*.c") $(shell find src -name "*.S")
OUT_DIR = out
TARGET = $(OUT_DIR)/kernel.elf
ISO_IMAGE = $(OUT_DIR)/WingOS.iso
CMDLINE ?= cli

.PHONY: all clean iso setup_limine

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p $(OUT_DIR)
	$(CROSS)$(BACKEND) -ffreestanding -nostdlib -mcmodel=kernel -mno-red-zone \
		-Wl,--build-id=none -z max-page-size=0x1000 -T linker.ld $(SRC) -o $(TARGET)

# 1. Download and build exact Limine v12.x binaries
setup_limine:
	wget https://github.com/Limine-Bootloader/Limine/releases/download/v12.5.2/limine-12.5.2.tar.xz
	tar -xf limine-12.5.2.tar.xz
	cd limine-12.5.2 && make

# 2. Build the bootable ISO image inside the out/ folder
iso: $(TARGET) $(setup_limine)
	# Create a temporary staging area inside out/
	mkdir -p $(OUT_DIR)/iso_root

	cp limine.conf $(OUT_DIR)/iso_root/limine.cfg
	
	# Copy kernel and Limine binaries to staging area
	cp $(TARGET) $(OUT_DIR)/iso_root/
	cp limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin $(OUT_DIR)/iso_root/
	
	# Build bootable ISO image
	xorriso -as mkisofs -b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-eltorito-alt-boot \
		-e limine-uefi-cd.bin \
		-no-emul-boot -isohybrid-gpt-basdat \
		$(OUT_DIR)/iso_root -o $(ISO_IMAGE)
	
	# Deploy Limine BIOS bootloader stage
	./limine/limine bios-install $(ISO_IMAGE)
	
	# Clean up staging area
	rm -rf $(OUT_DIR)/iso_root

clean:
	rm -rf $(OUT_DIR)