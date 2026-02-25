This repository holds code for Raspberry Pi pico/pico2 to interface between your retro battlestation and Fujinet.  Fujinet is a modern multi io and internet gateway for retro hardware.

Fujiversal emulates the signals of a ROM chip and talks on the CPU bus with the pico gpio and connects to Fujinet by USB serial.

There are currently two targets picorom and msxrp2350.  We hope to maybe support OneROM as well in the future.

The build commands are:

make ROM_FILE=path_to_rom_file BOARD=picorom

or 

make ROM_FILE=path_to_rom_file BOARD=msxrp2350

For Fujinet, most likely you will want to build fujinet-config to obtain the rom image for your retro battlestation before building this firmware.

Then you'll write the appropriate .uf2 in build/boards/{BOARD}/ file to your pico with picotool or by holding the button while plugging in the board and copying files to the drive that appears.

Don't forget to check the Fujinet discord channel for the current happenings