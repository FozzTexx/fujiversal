# MSX mapper test ROMs. See README.md.

AS       = z88dk-z80asm
BUILD    = build

# Mapped ROM size in K, a power of two.
SIZE    ?= 128

TYPES    = plain konami konamiscc ascii8 ascii16

# -D takes only numbers: MAP_xxx in the source.
plain_ID         = 0
plain_BANK       = 16
konami_ID        = 1
konami_BANK      = 8
konami_SUFFIX    = [Konami]
konami_P3        = 1
konamiscc_ID     = 2
konamiscc_BANK   = 8
konamiscc_SUFFIX = [KonamiSCC]
konamiscc_P3     = 1
ascii8_ID        = 3
ascii8_BANK      = 8
ascii8_SUFFIX    = [ASCII8]
ascii16_ID       = 4
ascii16_BANK     = 16
ascii16_SUFFIX   = [ASCII16]

segs = $(if $(filter plain,$(1)),2,$(shell echo $$(($(SIZE) / $($(1)_BANK)))))

ROMS = $(TYPES:%=$(BUILD)/mappertest-%.rom)

all: $(ROMS)

# The bracketed FujiNet names can't be make targets ([] globs), so they're copies.
$(BUILD)/mappertest-%.rom: src/mappertest.asm src/probe.inc \
    $(BUILD)/page3.bin $(BUILD)/size mkrom.pl | $(BUILD)
	$(AS) -b -Isrc -O$(BUILD) -DMAPPER=$($*_ID) -DSEGS=$(call segs,$*) \
	    -o$(BUILD)/mappertest-$*.bin $<
	./mkrom.pl $(BUILD)/mappertest-$*.bin $$(($($*_BANK) * 1024)) $(call segs,$*) \
	    $(if $($*_P3),$(BUILD)/page3.bin)
	mv $(BUILD)/mappertest-$*.bin $@
	$(if $($*_SUFFIX),cp $@ '$(BUILD)/mappertest$($*_SUFFIX).rom')
	@echo "$@: $$(wc -c < $@) bytes, $(call segs,$*) x $($*_BANK)K"

$(BUILD)/page3.bin: src/page3.asm src/probe.inc | $(BUILD)
	$(AS) -b -Isrc -O$(BUILD) -o$@ $<

$(BUILD)/size: FORCE | $(BUILD)
	@echo '$(SIZE)' | cmp -s - $@ || echo '$(SIZE)' > $@

$(BUILD)/empty.rom: | $(BUILD)
	perl -e 'print "AB", "\0" x 16382' > $@

test: $(ROMS) $(BUILD)/empty.rom
	./run-openmsx.sh $(BUILD)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all test clean FORCE
FORCE:
