#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct Bus Bus;

typedef struct PPU {
	Bus *bus;
	uint32_t frame_count;
	int scanline;
	int dot;
} PPU;

void ppu_init(PPU *ppu);
void ppu_poweron(PPU *ppu);
void ppu_reset(PPU *ppu);

void ppu_clock(PPU *ppu);

uint8_t ppu_read(PPU *ppu, uint32_t addr);
void ppu_write(PPU *ppu, uint32_t addr, uint8_t val);
