# MasterBoy
A Game Boy, Game Boy Color, Sega Master System and Game Gear emulator for PSP. It features:
- Save states along with traditional on-cartridge saves
- Emulation turbo mode
- Gameshark and Gamegenie cheats [(how to)](https://github.com/splitowo/MasterBoy/wiki/Cheats)
- Screenshots
- Rich customization settings [(more details)](https://github.com/splitowo/MasterBoy/wiki/Feature-documentation-(from-psp-Doc.txt))

## Installation & usage
- Download a release from GitHub, extract it and put `MasterBoy` directory into `/PSP/GAME` directory on a PSP memory stick
- ROM files can be put in `MasterBoy/Roms GBC` and `MasterBoy/Roms SMS`, although emulator lets you pick a file from any directory on memory stick

## Building & development
- Build process depends on PSPSDK and OSlib (OldSchool Library for PSP)
- With depenencies set, run `make -f Makefile.psp` to build EBOOT.PBP
