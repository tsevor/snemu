#include "cpu.h"

#include <stdio.h>

#include "bus.h"
#include "host.h"
#include "log.h"

typedef enum RWType {
	RW_DATA,
	RW_PROG,
	RW_DEAD, // dummy
} RWType;

typedef enum InstrType {
	IT_READ,
	IT_WRITE,
	IT_RMW
} InstrType;

typedef enum AddrMode {
	Imp,
	ImmMF,
	ImmIF,
	Imm8B,
	Rel,
	RelL,
	Dir,
	DirIdX,
	DirIdY,
	DirIn,
	DirIdIn,
	DirInId,
	DirInL,
	DirInIdL,
	Abs,
	AbsIdX,
	AbsIdY,
	AbsL,
	AbsIdL,
	StkRel,
	StkRelInId,
	AbsIn,
	AbsInL,
	AbsIdIn,
	ImpAcc,
	BlkMv,
} AddrMode;

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



uint16_t cpu_oper_addr(CPU *cpu, AddrMode mode, InstrType type) {
	(void)cpu;
	(void)mode;
	(void)type;
	switch (mode) {
		case Imp: // implied
			return 0;
		case ImmMF: // immediate memory flag
			return 0;
		case ImmIF: // immediate index flag
			return 0;
		case Imm8B: // immediate 8-bit
			return 0;
		case Rel: // relative
			return 0;
		case RelL: // relative long
			return 0;
		case Dir: // direct
			return 0;
		case DirIdX: // direct indexed x
			return 0;
		case DirIdY: // direct indexed y
			return 0;
		case DirIn: // direct indexed
			return 0;
		case DirIdIn: // direct indexed indirect
			return 0;
		case DirInId: // direct indirect indexed
			return 0;
		case DirInL: // direct indirect long
			return 0;
		case DirInIdL: // direct indirect indexed long
			return 0;
		case Abs: // absolute
			return 0;
		case AbsIdX: // absolute indexed x
			return 0;
		case AbsIdY: // absolute indexed y
			return 0;
		case AbsL: // absolute long
			return 0;
		case AbsIdL: // absolute indexed long
			return 0;
		case StkRel: // stack relative
			return 0;
		case StkRelInId: // stack relative indirect indexed
			return 0;
		case AbsIn: // absolute indirect
			return 0;
		case AbsInL: // absolute indirect long
			return 0;
		case AbsIdIn: // absolute indexed indirect
			return 0;
		case ImpAcc: // implied accumulator
			return 0;
		case BlkMv: // block move
			return 0;
		default: // shut up compiler error
			return 0;
	}
}





void cpu_op_nop(CPU *cpu, AddrMode mode) {
	uint16_t addr = cpu_oper_addr(cpu, mode, IT_READ);
	if (mode != Imp)
		cpu_read(cpu, addr, RW_DEAD);
}








void cpu_op_switch(CPU *cpu, uint8_t opcode) {
	switch (opcode) {
		case 0xea: cpu_op_nop(cpu, Imp); break;
		default:
			break;
	}
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

void cpu_step(CPU *cpu) {
	for (int i = 0; i < 0x40000; i++) {
		bus_read(cpu->bus, cpu->cycles);
		cpu->cycles = (cpu->cycles + 1) & 0xffffff;
	}
	bus_clock(cpu->bus);
	host_copy_buffer();

	return;

	if (cpu->stopped) {
		return;
	}

	if (cpu->stall) {
		for (; cpu->stall > 0; cpu->stall--) {
			cpu->cycles++;
			bus_clock(cpu->bus);
		}
		return;
	}

	uint8_t opcode = cpu_read(cpu, cpu->pc++, RW_PROG);

	// logging for snes-test
	fflush(stdout);

	cpu_op_switch(cpu, opcode);
	printf(
		"cpu: A=$%02X X=$%02X Y=$%02X P=$%02X E=%d\n",
		cpu->a,
		cpu->x,
		cpu->y,
		cpu->p.raw,
		cpu->e
	);
}

void cpu_run(CPU *cpu) {
	// run until vblank
	do {
		cpu_step(cpu);
	} while (!cpu->stopped && !cpu->bus->vblank_started);

	cpu->bus->vblank_started = false;
	// vblank interrupt
	return; // allow main to handle calls
}
