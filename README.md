# snemu

## why

for fun

## use

`./snemu rom.sfc`

## build

requirements for sdl3 host
- sdl3

then run:

`make`

or use

`make run ARGS="path/to/rom.sfc"`

## todo

todo:
- [ ] checklist is accurate
- [X] bus
- [ ] cart
  - [X] loading
  - [X] lorom
  - [X] hirom
  - [ ] exhirom
  - [ ] exlorom
- [ ] cpu
  - [X] research differences from 6502
  - [X] official opcodes
  - [X] interrupts
  - [ ] mmio registers
  - [X] emulation mode
- [ ] apu
  - [ ] research differences from 6502
  - [ ] official opcodes
  - [ ] proper cpu interaction
  - [ ] hardware timers
  - [ ] apu dsp
    - [ ] research + update todo
- [ ] ppu
  - [ ] basic registers
  - [ ] all registers
  - [ ] basic state machine
  - [ ] implement basic rendering for smw
  - [ ] rendering modes
    - [ ] mode 0
    - [ ] mode 1
    - [ ] mode 2
    - [ ] mode 3
    - [ ] mode 4
    - [ ] mode 5
    - [ ] mode 6
    - [ ] mode 7
  - [ ] rendering features
    - [ ] masking windows
    - [ ] color math
- [ ] controller
  - [ ] basic controller
  - [ ] auto joypad
- [ ] interaction features
  - [ ] dma
  - [ ] hdma
- [ ] coprocessors (eventually)
  - [ ] research + update todo
  - [ ] superfx
