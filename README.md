# tama62

**An emulator for the original Tamagotchi digital pet**

## Overview

The original Tamagotchi was an incredibly simple yet addictive toy. Its specs are very modest, even for the mid 90's:

**Processor:** 4-bit CMOS E0C6S46 CPU @ 32.768 kHz
**Program Memory:** 6,144 x 12-bit words ROM
**Data Memory:** 640 x 4-bit words RAM
**Display:** Monochrome 32x20 LCD w/ 160 x 4-bit words

## Emulator Progress

- [x] E0C6200 CPU Emulator
- [x] 32.768 kHz Program Runner
- [ ] Timers
- [ ] Interrupts
- [ ] LCD Display
- [ ] Input Ports
- [ ] Sound Generator

## Building

Requires a C++20 compiler (GCC or Clang) and `make`:

```sh
make
```

## Running

This repository does **not** include the Tamagotchi ROM. You need to supply your own dump, named `tama.bin`, in the directory you run the emulator from.

```sh
./tama            # run the emulator
./tama --trace    # also print every executed instruction (or -t)
```

Press **Ctrl-C** to stop. The emulator prints a dump of the CPU registers, flags and memory before exiting.

## References

[The Tamagotchi Project](https://cctp506m.github.io/tamagotchi/architecture/)

## License

The emulator source code is released under the [MIT License](LICENSE). This does not cover any ROM image.

## Disclaimer

Tamagotchi is a trademark of Bandai. This project is not affiliated with or endorsed by Bandai.
