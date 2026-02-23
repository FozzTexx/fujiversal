This repository holds code for Raspberry Pi pico/pico2 to interface between your retro battlestation and Fujinet.  Fujinet is a modern multi io and internet gateway for retro hardware.

Fujiversal emulates the signal of a ROM chip and talks on the CPU bus with the pico gpio and connects to Fujinet by USB serial.

There are currently two targets PicoROM and MSXWaveshare2350b.  We hope to maybe support OneROM as well in the future.

The build commands are:

make ROM_FILE=path_to_rom_file BOARD=picorom

or 

make ROM_FILE=path_to_rom_file BOARD=msxwaveshare2350b

For Fujinet, most likely you will want to build fujinet-config to obtain the rom image for your retro battlestation before building this firmware.

Then you'll write the .uf2 files to your pico with picotool or by holding the button while plugging in the board and copying files to the drive that appears.

Don't forget to check the Fujinet discord channel for the current happenings