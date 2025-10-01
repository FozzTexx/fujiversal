BUILD_DIR = build
BUILD_MAKE = $(BUILD_DIR)/Makefile
FIRMWARE = fujiversal.uf2

$(BUILD_DIR)/$(FIRMWARE): main.c bus.pio $(BUILD_MAKE)
	defoogi make -C $(BUILD_DIR)

$(BUILD_MAKE): CMakeLists.txt
	defoogi cmake -B $(BUILD_DIR)

upload: $(BUILD_DIR)/$(FIRMWARE)
	defoogi sudo picotool load -v -x build/fujiversal.uf2 -f
