BUILD_DIR = build
BUILD_MAKE = $(BUILD_DIR)/Makefile
FIRMWARE = fujiversal.uf2
MSX_DIR = msxio
ROM_IMAGE = $(MSX_DIR)/r2r/msxrom/disk.rom
ROM_CFILES = $(addprefix $(MSX_DIR)/src/,main.c)
ROM_AFILES = $(addprefix $(MSX_DIR)/src/,portio.s timeout.s)
# MSX_DIR = msxdisk
# ROM_IMAGE = $(MSX_DIR)/disk.rom
# ROM_CFILES = $(addprefix $(MSX_DIR)/,disk.c)
# ROM_AFILES = $(addprefix $(MSX_DIR)/,header.s jptable.s io.s)

$(BUILD_DIR)/$(FIRMWARE): main.c bus.pio rom.h $(BUILD_MAKE)
	defoogi make -C $(BUILD_DIR)

$(BUILD_MAKE): CMakeLists.txt
	defoogi cmake -B $(BUILD_DIR)

upload: $(BUILD_DIR)/$(FIRMWARE)
	defoogi sudo picotool load -v -x build/fujiversal.uf2 -f

rom.h: $(ROM_IMAGE)
	xxd -i -n disk_rom $< > $@

$(ROM_IMAGE): $(ROM_CFILES) $(ROM_AFILES)
	defoogi make -C $(MSX_DIR)
