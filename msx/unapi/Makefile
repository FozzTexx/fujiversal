# ROM_HEADER = header.s
# ROM_AFILES =
# ROM_CFILES = dispatch.c
# CFLAGS = +z80 --no-crt -c
# LDFLAGS = -m=z80 -b -r=0x4000
# BINFILE = header_code_user.bin

# NORM_AFILES := $(ROM_AFILES:.asm=.s)
# OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(ROM_CFILES)) \
#         $(patsubst %.s,$(OBJ_DIR)/%.o,$(NORM_AFILES))

# unapi.rom: $(BINFILE)
# 	mv $^ $@
# 	ls -Fla unapi*
# 	hexdump -C $@

# $(BINFILE): $(ROM_HEADER) $(OBJS)
# 	z80asm $(LDFLAGS) $^

# %.o: %.c
# 	zcc $(CFLAGS) $^ -o $@

PRODUCT = unapi
PLATFORMS += msxrom

# You can run 'make <platform>' to build for a specific platform,
# or 'make <platform>/<target>' for a platform-specific target.
# Example shortcuts:
#   make coco        → build for coco
#   make apple2/disk → build the 'disk' target for apple2

# SRC_DIRS may use the literal %PLATFORM% token.
# It expands to the chosen PLATFORM plus any of its combos.
SRC_DIRS = src src/%PLATFORM%

# FUJINET_LIB is not used

# Define extra dirs ("combos") that expand with a platform.
# Format: platform+=combo1,combo2
PLATFORM_COMBOS = \
  c64+=commodore \
  atarixe+=atari \
  msxrom+=msx \
  msxdos+=msx \
  adam_cpm+=adam

include ../mekkogx/toplevel-rules.mk

# If you need to add extra platform-specific steps, do it below:
#   coco/r2r:: coco/custom-step1
#   coco/r2r:: coco/custom-step2
# or
#   apple2/disk: apple2/custom-step1 apple2/custom-step2

MSXROM_FLAGS = +msx -subtype=callext
MSX_EXTRA_DEPS = src/header/msx_rom_header.asm
LDFLAGS_EXTRA_MSXROM = -Ca-Isrc/header -create-app
