#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "bus.h"

typedef struct CPU {
	union {
		uint16_t a;
		struct {
			uint8_t al;
			uint8_t ah;
		};
	};
	union {
		uint16_t x;
		struct {
			uint8_t xl;
			uint8_t xh;
		};
	};
	union {
		uint16_t y;
		struct {
			uint8_t yl;
			uint8_t yh;
		};
	};

	uint16_t pc;

	union {
		uint16_t s;
		struct {
			uint8_t sl;
			uint8_t sh;
		};
	};

	uint16_t d;

	uint8_t db;
	uint8_t pb;

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

	Bus *bus;

} CPU;

void cpu_init(CPU *cpu);

void cpu_poweron(CPU *cpu);
void cpu_reset(CPU *cpu);

void cpu_run(CPU *cpu);
