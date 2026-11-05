#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cart.h"
#include "spc.h"

typedef struct CPU CPU;

typedef struct Bus {
	uint8_t wram[128 << 10];

	CPU *cpu;
	SPC *spc;
	Cart *cart;

	uint8_t open_bus;

	int last_wr_cycles;
	uint64_t cycles;
	int64_t spc_cycle_acc;

	bool vblank_started;
} Bus;

void bus_init(Bus *bus);

void bus_free(Bus *bus);

void bus_poweron(Bus *bus);
void bus_reset(Bus *bus);

void bus_next_fast(Bus *bus);
void bus_clock(Bus *bus);

uint8_t bus_read(Bus *bus, uint32_t addr);
void bus_write(Bus *bus, uint32_t addr, uint8_t val);

#include "cpu.h"
