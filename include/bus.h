#pragma once

#include <stdint.h>

#include "cart.h"

typedef struct CPU CPU;

typedef struct Bus {
	uint8_t wram[128 << 10];

	CPU *cpu;
	Cart *cart;

	uint8_t open_bus;

	bool vblank_started;
} Bus;

void bus_init(Bus *bus);

void bus_free(Bus *bus);

void bus_poweron(Bus *bus);
void bus_reset(Bus *bus);

void bus_clock(Bus *bus);

uint8_t bus_read(Bus *bus, uint32_t addr);
void bus_write(Bus *bus, uint32_t addr, uint8_t val);

#include "cpu.h"
