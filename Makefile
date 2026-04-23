BUILD_DIR = build
BUILD_MAKE = $(BUILD_DIR)/Makefile
FIRMWARE = fujiversal.uf2
MSX_DIR = msxio
#ROM_IMAGE = $(MSX_DIR)/r2r/msxrom/disk.rom
ROM_IMAGE ?= hdbdw3bc3.rom
ROM_CFILES = $(addprefix $(MSX_DIR)/src/,main.c)
ROM_AFILES = $(addprefix $(MSX_DIR)/src/,portio.s timeout.s)
ROM_H = $(BUILD_DIR)/rom.h
# MSX_DIR = msxdisk
# ROM_IMAGE = $(MSX_DIR)/disk.rom
# ROM_CFILES = $(addprefix $(MSX_DIR)/,disk.c)
# ROM_AFILES = $(addprefix $(MSX_DIR)/,header.s jptable.s io.s)

SRC = main.cpp FujiBusPacket.cpp FujiBusPacket.h bus.pio $(ROM_H)	\
      fujiDeviceID.h fujiCommandID.h setup_sm.cpp setup_sm.h

$(BUILD_DIR)/$(FIRMWARE): $(SRC) $(BUILD_MAKE)
	defoogi make -C $(BUILD_DIR)

$(BUILD_MAKE): CMakeLists.txt
	defoogi cmake -B $(BUILD_DIR)

upload: $(BUILD_DIR)/$(FIRMWARE)
	defoogi sudo picotool load -v -x build/fujiversal.uf2 -f

$(ROM_H): $(ROM_IMAGE) | $(BUILD_DIR)
	xxd -i -n disk_rom $< > $@

$(ROM_IMAGE): $(ROM_CFILES) $(ROM_AFILES)
	defoogi make -C $(MSX_DIR)

$(BUILD_DIR):
	mkdir -p $@
