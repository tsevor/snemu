# snemu

## use

`./snemu rom.sfc`

or drag the rom onto the executable

## build

requirements for sdl3 host
- sdl3

then run:

`make`

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
  - [X] mmio registers
  - [X] emulation mode
  - [ ] cycle accuracy
- [ ] apu
  - [X] research differences from 6502
  - [X] official opcodes
  - [X] proper cpu interaction
  - [X] hardware timers
  - [ ] cycle accuracy
  - [X] apu dsp
    - [X] spc register mmio
    - [X] brr/adpcm decoding
    - [X] adsr & gain (envelope generation)
    - [X] pitch & frequency control
    - [X] voice mixing (8 channels)
    - [X] noise generation
    - [X] pitch modulation (pmon)
    - [X] echo ring buffer & delay
    - [X] 8-tap fir filter
- [ ] ppu
  - [X] basic registers
  - [ ] all registers
  - [X] basic state machine
  - [X] implement basic rendering for smw
  - [X] rendering modes
    - [X] mode 0
    - [X] mode 1
    - [X] mode 2
    - [X] mode 3
    - [ ] mode 4
    - [ ] mode 5
    - [ ] mode 6
    - [ ] mode 7
  - [ ] rendering features
    - [X] masking windows
    - [X] color math
    - [ ] h512
    - [ ] interlacing
    - [ ] mode 7 transforms
    - [ ] column shift
- [X] controller
  - [X] basic controller
  - [X] auto joypad
- [X] interaction features
  - [X] dma
  - [X] hdma
- [ ] coprocessors (eventually)
  - [ ] research + update todo
  - [ ] superfx
