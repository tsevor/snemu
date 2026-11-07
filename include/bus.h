#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cart.h"
#include "spc.h"

typedef struct CPU CPU;
typedef struct PPU PPU;

typedef struct Bus {
	uint8_t wram[128 << 10];

	CPU *cpu;
	SPC *spc;
	Cart *cart;
	PPU *ppu;

	uint8_t open_bus;

	int last_wr_cycles;
	uint64_t cycles;
	int64_t spc_cycle_acc;
	int64_t ppu_cycle_acc;

	bool vblank_started;

	union {
		uint8_t nmitimen;
		struct {
			bool autojoy : 1;
			uint8_t : 3; // pad
			uint8_t irq_mode : 2;
			uint8_t : 1; // pad
			bool vblank_nmi_en : 1;
		};
	};
	union {
		uint8_t rdnmi;
		struct {
			uint8_t cpu_ver : 4;
			uint8_t : 3; // pad
			bool vblank_flag : 1;
		};
	};
	union {
		uint8_t timeup;
		struct {
			uint8_t : 7; // pad
			bool timer_flag : 1;
		};
	};
	union {
		uint16_t htime;
		struct {
			uint8_t htimel : 8;
			uint8_t htimeh : 1;
		};
	};
	union {
		uint16_t vtime;
		struct {
			uint8_t vtimel : 8;
			uint8_t vtimeh : 1;
		};
	};
} Bus;

void bus_init(Bus *bus);
void bus_free(Bus *bus);
void bus_poweron(Bus *bus);
void bus_reset(Bus *bus);

void bus_next_fast(Bus *bus);
void bus_clock(Bus *bus);

uint8_t bus_read(Bus *bus, uint32_t addr);
void bus_write(Bus *bus, uint32_t addr, uint8_t val);

void bus_trigger_vblank(Bus *bus);
void bus_clear_vblank(Bus *bus);
void bus_check_irq(Bus *bus);

#include "cpu.h"
#include "ppu.h"
