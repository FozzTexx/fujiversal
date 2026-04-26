BOARD ?= picorom
ROM_FILE ?=
BUILD_DIR = build/$(BOARD)
BUILD_MAKE = $(BUILD_DIR)/Makefile
FIRMWARE = fujiversal_$(BOARD).uf2
MSX_DIR = msxio
ROM_IMAGE = $(MSX_DIR)/r2r/msxrom/disk.rom
ROM_CFILES = $(addprefix $(MSX_DIR)/src/,main.c)
ROM_AFILES = $(addprefix $(MSX_DIR)/src/,portio.s timeout.s)
ROM_H = $(BUILD_DIR)/rom.h
UF2_BINARY = $(BUILD_DIR)/fujiversal_$(BOARD).uf2

SRC = main.cpp board_defs.h setup_sm.cpp setup_sm.h FujiBusPacket.cpp	\
      FujiBusPacket.h bus.pio fujiDeviceID.h fujiCommandID.h $(ROM_H)

$(BUILD_DIR)/$(FIRMWARE): $(SRC) $(BUILD_MAKE)
	defoogi make -C $(BUILD_DIR)

$(BUILD_MAKE): CMakeLists.txt boards/$(BOARD).pio
	defoogi cmake -B $(BUILD_DIR) -DBOARD=$(BOARD)

upload: $(BUILD_DIR)/$(FIRMWARE)
	defoogi sudo picotool load -v -x $(UF2_BINARY) -f

picorom msxrp2350:
	$(MAKE) BOARD=$@

all: $(BOARD)

clean:
	rm -rf build

$(ROM_H): $(ROM_IMAGE) | $(BUILD_DIR)
	xxd -i -n disk_rom $< > $@

$(ROM_IMAGE): $(ROM_CFILES) $(ROM_AFILES)
	defoogi make -C $(MSX_DIR)

$(BUILD_DIR):
	mkdir -p $@
