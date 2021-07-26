# STM32F103C8 - uLisp

## Step by Step Setup
- Download `ulisp-stm32.ino` from https://github.com/technoblogy/ulisp-stm32
- Move all the definitions into `ulisp.h` and the rest into `ulisp.cpp`
- `ulisp.h`
  - Add `#define _VARIANT_ARDUINO_STM32_` and `#include <Arduino.h>` to the beginning of the file
  - Replace `object *read ();` with `object *read (gfun_t gfun);`
  - Add the missing function declarations
- `ulisp.cpp`
  - Comment out SD Card related stuffs as suggest in https://github.com/technoblogy/ulisp-stm32/issues/12
  - Replace `WiringPinMode` with `uint32_t`

## References
- http://www.ulisp.com/
- http://forum.ulisp.com/t/ulisp-for-the-stm32-blue-pill-board/185
