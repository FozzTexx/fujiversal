# MSX mapper test

One cartridge ROM per mapper: plain, Konami, Konami SCC, ASCII8, ASCII16.
Every segment is filled with its own number, and each ROM walks every window
through every segment.

```sh
make              # build/mappertest-<type>.rom, 128K mapped ROMs
make SIZE=256     # 256K
make test         # run in openMSX
```

Needs `z88dk-z80asm` and `perl`. `build/` also gets `mappertest[Konami].rom`
etc., named for the FujiNet's filename-based mapper selection.

## Checks

The tester copies itself to RAM at C000h and runs from there. For each window
and each segment it:

1. selects the segment through the usual register and reads it back at three
   addresses in the window;
2. selects it through a few other addresses that decode to the same
   register, after selecting a different segment first;
3. writes to addresses that must not switch this window (just outside the
   register's range, or another window's register) and checks the window
   didn't change.

Fixed windows (Konami 4000h, both plain pages) only get step 3.

Konami and Konami SCC also check their page 0 and page 3 mirrors:

| mirror      | Konami          | Konami SCC |
|-------------|-----------------|------------|
| 0000h-1FFFh | 4000h (fixed)   | 8000h      |
| 2000h-3FFFh | 6000h           | A000h      |
| C000h-DFFFh | 8000h           | 4000h      |
| E000h-FFFFh | A000h           | 6000h      |

Page 0 is read with the BIOS briefly switched out. Page 3 is read by
`src/page3.asm`, which sits in the last segment and runs at 8000h, since page
3 RAM is gone while it runs. Mirrors are reported separately from windows.

The ranges follow openMSX (`src/memory/Rom*.cc`). Nothing touches
BFFCh-BFFFh.

## Output

```
Mapper test: KonamiSCC 10h x 8K
WIN  SEG/LAST
4000 0F/0F OK
...
0000 0F/0F OK
C000 0F/0F OK

WINDOWS: PASS
MIRRORS: PASS
```

A failing line shows the write, the read and the expected value, in hex:
`4000 00/0F FAIL W5000=01 R4800=00 E01`.

The results are also in RAM: C000h for windows and C002h for mirrors, each
`P`, `F` or `-` (no mirrors). The failure counts are at C001h and C003h.

## openMSX

`run-openmsx.sh` runs each ROM on its own mapper (it must pass), on a wrong
one (the windows must fail), and the Konami ROMs in subslot 1 of a slot
expander. Set `OPENMSX` and `MACHINE` to override the defaults.
