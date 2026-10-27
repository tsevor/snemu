#include "cpu.h"

#include <stdio.h>

#include "bus.h"
#include "log.h"

typedef enum RWType {
	RW_DATA = 0,
	RW_PROG = 1,
} RWType;

uint8_t cpu_read(CPU *cpu, uint16_t addr, RWType type) {
	cpu->cycles++;
	uint32_t upper = (type ? cpu->db : cpu->pb) << 16;
	uint8_t val = bus_read(cpu->bus, upper | addr);
	bus_clock(cpu->bus);
	return val;
}

void cpu_write(CPU *cpu, uint16_t addr, uint8_t val, RWType type) {
	cpu->cycles++;
	uint32_t upper = (type ? cpu->db : cpu->pb) << 16;
	bus_write(cpu->bus, upper | addr, val);
	bus_clock(cpu->bus);
}

uint16_t cpu_read16(CPU *cpu, uint16_t addr, RWType type) {
	return cpu_read(cpu, addr, type) | (cpu_read(cpu, addr + 1, type) << 8);
}

void cpu_write16(CPU *cpu, uint16_t addr, uint16_t val, RWType type) {
	cpu_write(cpu, addr, val & 0xff, type);
	cpu_write(cpu, addr + 1, val >> 8, type);
}










void cpu_init(CPU *cpu) {
	cpu_poweron(cpu);
	cpu->cycles = 0;
}

void cpu_poweron(CPU *cpu) {
	cpu->a = 0;
	cpu->x = 0;
	cpu->y = 0;
	cpu->db = 0;
	cpu->pb = 0;
	cpu_reset(cpu);
}

void cpu_reset(CPU *cpu) {
	cpu->p.raw = 0b00110100;
	cpu->e = true;
	cpu->s = 0xfd;
	cpu->pc = cpu_read16(cpu, 0xfffc, RW_PROG);
	cpu->cycles = 7;
}

void cpu_run(CPU *cpu) {
	(void)cpu;
}
