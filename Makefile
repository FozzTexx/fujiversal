BOARD ?= picorom
ROM_FILE ?=
BUILD_DIR = build/$(BOARD)
BUILD_MAKE = $(BUILD_DIR)/Makefile
FIRMWARE = fujiversal_$(BOARD).uf2
MSX_DIR = msxio
ROM_IMAGE = $(MSX_DIR)/r2r/msxrom/disk.rom
ROM_CFILES = $(addprefix $(MSX_DIR)/src/,main.c)
ROM_AFILES = $(addprefix $(MSX_DIR)/src/,portio.s timeout.s)
# MSX_DIR = msxdisk
# ROM_IMAGE = $(MSX_DIR)/disk.rom
# ROM_CFILES = $(addprefix $(MSX_DIR)/,disk.c)
# ROM_AFILES = $(addprefix $(MSX_DIR)/,header.s jptable.s io.s)

SRC = main.cpp FujiBusPacket.cpp FujiBusPacket.h bus.pio fujiDeviceID.h fujiCommandID.h rom.h

$(BUILD_DIR)/$(FIRMWARE): $(SRC) $(BUILD_MAKE)
	defoogi make -C $(BUILD_DIR)

$(BUILD_MAKE): CMakeLists.txt boards/$(BOARD).pio
	defoogi cmake -B $(BUILD_DIR) -DBOARD=$(BOARD)

upload: $(BUILD_DIR)/$(FIRMWARE)
	defoogi sudo picotool load -v -x $(BUILD_DIR)/fujiversal_$(BOARD).uf2 -f

picorom:
	$(MAKE) BOARD=picorom

msxrp2350:
	$(MAKE) BOARD=msxrp2350

all: picorom msxrp2350

clean:
	rm -rf build

ifdef ROM_FILE
rom.h: $(ROM_FILE)
	xxd -i -n disk_rom $< > $@
else
rom.h: $(ROM_IMAGE)
	xxd -i -n disk_rom $< > $@

$(ROM_IMAGE): $(ROM_CFILES) $(ROM_AFILES)
	defoogi make -C $(MSX_DIR)
endif
