#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "bus.h"

#define lhstruct(vname, hlen) union { uint16_t vname; struct { uint8_t vname##l : 8; uint8_t vname##h : hlen; }; }

typedef struct CPU {
	lhstruct(a, 8);
	lhstruct(x, 8);
	lhstruct(y, 8);

	union {
		uint32_t pcf : 24;
		struct {
			uint16_t pc;
			uint8_t pb;
		};
	};

	union {
		uint16_t s;
		struct {
			uint8_t sl;
			uint8_t sh;
		};
	};

	uint16_t d;

	uint8_t db;

	bool e;

	union {
		uint8_t raw;
		struct {
			uint8_t c : 1;
			uint8_t z : 1;
			uint8_t i : 1;
			uint8_t d : 1;
			uint8_t x : 1;
			uint8_t m : 1;
			uint8_t v : 1;
			uint8_t n : 1;
		};
	} p;

	uint64_t cycles;
	uint64_t stall;

	bool stopped;
	bool waiting;
	bool nmi_pending;
	bool irq_pending;

	Bus *bus;

} CPU;

void cpu_init(CPU *cpu);

void cpu_poweron(CPU *cpu);
void cpu_reset(CPU *cpu);

void cpu_run(CPU *cpu);
void cpu_step(CPU *cpu);

uint8_t cpu_read(CPU *cpu, uint32_t addr);
void cpu_write(CPU *cpu, uint32_t addr, uint8_t val);
