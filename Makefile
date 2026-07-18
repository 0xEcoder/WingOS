BACKEND = gcc
CROSS = x86_64-elf-

SRC = $(shell find src -name "*.c")
OUT_DIR = out
TARGET = $(OUT_DIR)/kernel.bin
ISO_IMAGE = $(OUT_DIR)/WingOS.iso

.PHONY: all clean iso setup_limine

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p $(OUT_DIR)
	$(CROSS)$(BACKEND) -ffreestanding -nostdlib -mcmodel=kernel -mno-red-zone -T linker.ld $(SRC) -o $(TARGET)
# 1. Download and build exact Limine v12.x binaries
setup_limine:
	@if [ ! -d "limine" ]; then \
		git clone https://github.com/limine-bootloader/limine.git --branch=v12.x-binary --depth=1; \
		make -C limine; \
	fi

# 2. Build the bootable ISO image inside the out/ folder
iso: $(TARGET) setup_limine
	# Create a temporary staging area inside out/
	mkdir -p $(OUT_DIR)/iso_root
	
	# Create the Limine v12.5.0 configuration file using colons
	echo "timeout: 3" > $(OUT_DIR)/iso_root/limine.conf
	echo "/WingOS" >> $(OUT_DIR)/iso_root/limine.conf
	echo "    protocol: limine" >> $(OUT_DIR)/iso_root/limine.conf
	echo "    path: boot():/kernel.bin" >> $(OUT_DIR)/iso_root/limine.conf
	
	# Copy your kernel and Limine components into the staging area
	cp $(TARGET) $(OUT_DIR)/iso_root/
	cp limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin $(OUT_DIR)/iso_root/
	
	# Build the ISO right into the out/ directory
	xorriso -as mkisofs -b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		-eltorito-alt-boot \
		-e limine-uefi-cd.bin \
		-no-emul-boot -isohybrid-gpt-basdat \
		$(OUT_DIR)/iso_root -o $(ISO_IMAGE)
	
	# Deploy Limine via the modern binary utility
	./limine/limine bios-install $(ISO_IMAGE)
	
	# Clean up the staging folder automatically
	rm -rf $(OUT_DIR)/iso_root

clean:
	rm -rf $(OUT_DIR)