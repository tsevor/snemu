#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "bus.h"

typedef struct CPU {
	uint16_t a;
	uint16_t x;
	uint16_t y;

	uint16_t pc;
	uint16_t s;

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

	Bus *bus;

} CPU;

void cpu_init(CPU *cpu);

void cpu_poweron(CPU *cpu);
void cpu_reset(CPU *cpu);

void cpu_run(CPU *cpu);
