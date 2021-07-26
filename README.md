# STM32F103C8 - Mecrisp-Stellaris Forth

## Prerequisite
- stlink
  - `sudo apt install stlink-tools` or download latest deb package from [here](https://github.com/stlink-org/stlink/releases)
- Mecrisp-Stellaris Forth
  - [Download Link](https://sourceforge.net/projects/mecrisp/files)
  - Note the binary file for STM32F103 at `mecrisp-stellaris-2.5.9a` is already contained in the data folder
- (optional) e4thcom
  - [Download Link](https://wiki.forth-ev.de/doku.php/en:projects:e4thcom)

## Run
- Flash
  - `st-flash erase`
  - `st-flash write mecrisp-stellaris-stm32f103.bin 0x08000000`
- Open Serial Terminal
  - `e4thcom -t mecrisp-st -d ttyUSB0 -b B115200`
  - `miniterm /dev/ttyUSB0 115200`
  - etc.

## References
- https://mecrisp-stellaris-folkdoc.sourceforge.io/flashing-mecrisp_stellaris.html
