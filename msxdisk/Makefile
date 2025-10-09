ROM_CFILES = disk.c
ROM_AFILES = header.s jptable.s io.s

# header.s MUST be first in list of files!
disk.rom: $(ROM_AFILES) $(ROM_CFILES)
	zcc +embedded --no-crt $^ -o $@ -m
