#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "cart.h"
#include "spc.h"

typedef struct CPU CPU;
typedef struct PPU PPU;

#define lhstruct(vname, hlen) union { uint16_t vname; struct { uint8_t vname##l : 8; uint8_t vname##h : hlen; }; }

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
	lhstruct(htime, 1);
	lhstruct(vtime, 1);

	bool joy_strobe;
	lhstruct(joy1, 8);
	uint16_t joy1_shift;

	union {
		uint8_t dmaregs[0x80];
		struct {
			union {
				uint8_t dmap;
				struct {
					uint8_t mode : 3;
					uint8_t fixed : 1;
					uint8_t decrement : 1;
					uint8_t : 1;
					uint8_t hdma_ind : 1;
					uint8_t invert : 1;
				};
			};
			uint8_t bbad;
			lhstruct(a1t, 8);
			uint8_t a1b;
			lhstruct(das, 8);
			uint8_t dasb;
			lhstruct(a2a, 8);
			uint8_t ntrl;
			uint8_t scratch;
			uint8_t pad[4];
		} __attribute__((packed)) dmachan[8];
	};

	uint8_t hdmaen;
	uint8_t hdma_active;
	uint8_t hdma_stopped;
	bool hdma_do_transfer[8];

	uint8_t wrmpya;
	lhstruct(rdmpy, 8);
	lhstruct(rddiv, 8);
	lhstruct(wrdiv, 8);

	uint32_t wram_addr;
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

void bus_init_hdma(Bus *bus);
void bus_exec_hdma(Bus *bus);

#include "cpu.h"
#include "ppu.h"
