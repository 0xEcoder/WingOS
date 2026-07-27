BACKEND = gcc
CROSS = x86_64-elf-

SRC = $(shell find src -name "*.c") $(shell find src -name "*.S")
OUT_DIR = out
TARGET = $(OUT_DIR)/kernel.elf
ISO_IMAGE = $(OUT_DIR)/WingOS.iso
CMDLINE ?= cli

# QEMU configuration
QEMU = qemu-system-x86_64
QEMU_FLAGS = -M q35 -m 2G -cdrom $(ISO_IMAGE) -serial stdio

.PHONY: all clean distclean iso setup_limine run run-uefi debug

# Make building the ISO the default action when you run just `make`
all: iso

$(TARGET): $(SRC)
	mkdir -p $(OUT_DIR)
	$(CROSS)$(BACKEND) -ffreestanding -nostdlib -mcmodel=kernel -mno-red-zone \
		-Wl,--build-id=none -z max-page-size=0x1000 -T linker.ld $(SRC) -o $(TARGET)

setup_limine:
	@if [ ! -f "limine/limine-bios.sys" ]; then \
		echo "Downloading Limine v12.5.2 pre-built binary release..."; \
		rm -rf limine limine-*; \
		mkdir -p limine; \
		wget -q https://github.com/Limine-Bootloader/Limine/releases/download/v12.5.2/limine-binary.tar.xz; \
		tar -xf limine-binary.tar.xz -C limine --strip-components=1; \
		rm -f limine-binary.tar.xz; \
		echo "Compiling Limine host deployment tool..."; \
		make -C limine; \
	fi

# 2. Build the bootable ISO image inside the out/ folder
iso: $(TARGET) setup_limine
	# Create a temporary staging area inside out/
	mkdir -p $(OUT_DIR)/iso_root

	# Keep the exact limine.conf filename!
	cp limine.conf $(OUT_DIR)/iso_root/limine.conf
	
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

# --- Execution & Debugging Targets ---

# Test in QEMU (BIOS Mode)
run: iso
	$(QEMU) $(QEMU_FLAGS)

# Test in QEMU (UEFI Mode - requires OVMF installed on your host)
run-uefi: iso
	$(QEMU) $(QEMU_FLAGS) -bios /usr/share/ovmf/OVMF.fd

# Launch QEMU paused for GDB debugging (connect with: target remote localhost:1234)
debug: iso
	$(QEMU) $(QEMU_FLAGS) -s -S

# --- Cleanup Targets ---

# Remove kernel artifacts and ISO
clean:
	rm -rf $(OUT_DIR)

# Remove build artifacts AND the compiled Limine bootloader directory
distclean: clean
	rm -rf limine