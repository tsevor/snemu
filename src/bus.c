#include "log.h"
#include "bus.h"
#include "host.h"
#include "spc.h"

#include <stdbool.h>
#include <string.h>


void bus_init(Bus *bus) {
	memset(bus->wram, 0, sizeof(bus->wram));
}

void bus_free(Bus *bus) {
	cart_free(bus->cart);
}

void bus_poweron(Bus *bus) {
	cpu_poweron(bus->cpu);
	spc_poweron(bus->spc);
}

void bus_reset(Bus *bus) {
	cpu_reset(bus->cpu);
	spc_reset(bus->spc);
}

void bus_next_fast(Bus *bus) {
	bus->last_wr_cycles = 6;
}

void bus_clock(Bus *bus) {
	// clock components
	bus->cycles += bus->last_wr_cycles;

	if (bus->cycles >= 357368) {
		// force vblank approximately every frame
		bus->vblank_started = true;
		bus->cpu->nmi_pending = true;
		bus->cycles -= 357368;
	}

	// if apu is behind, exec one instruction
	bus->spc_cycle_acc += bus->last_wr_cycles * 1024000;
	while (bus->spc_cycle_acc >= 21477270) {
		int spc_cycles = spc_step(bus->spc);
		bus->spc_cycle_acc -= 21477270 * spc_cycles;
	}
}

uint8_t bus_read(Bus *bus, uint32_t addr) {
	if ((addr & 0x408000) == 0x000000) { // bottom half q1/q3
		bus->last_wr_cycles = 12;
		uint16_t offset = addr & 0xffff;
		switch (offset) { // aux
			case 0x0000 ... 0x1fff: // wram mirror
				bus->last_wr_cycles = 8;
				bus->open_bus = bus->wram[offset];
				break;
			case 0x2140 ... 0x217f: // apu mmio
				bus->open_bus = spc_read_io_port(bus->spc, offset & 3);
				break;
			// TODO: ppu, cpu, and joypad mmio
			default: // allow cart to take over
				bus->open_bus = cart_read(bus->cart, addr);
				break;
		}
	} else if ((addr & 0xfe0000) == 0x7e0000) { // wram
		bus->open_bus = bus->wram[addr & 0x1ffff];
		bus->last_wr_cycles = 8;
	} else { // romsel
		bus->open_bus = cart_read(bus->cart, addr);
		bus->last_wr_cycles = bus->cart->last_read_fast ? 6 : 8;
	}
	return bus->open_bus;
}

void bus_write(Bus *bus, uint32_t addr, uint8_t val) {
	bus->last_wr_cycles = 8;
	bus->open_bus = val;
	if (!(addr & 0x408000)) { // bottom half q1/q3
		bus->last_wr_cycles = 12;
		uint16_t offset = addr & 0xffff;
		switch (offset) { // aux
			case 0x0000 ... 0x1fff: // wram mirror
				bus->last_wr_cycles = 8;
				bus->wram[offset] = val;
				break;
			case 0x2140 ... 0x217f: // apu mmio
				spc_write_io_port(bus->spc, offset & 3, val);
				break;
			// TODO: ppu, cpu, and joypad mmio
			default: // allow cart to take over
				cart_write(bus->cart, addr, val);
				break;
		}
	} else if (addr >= 0x7e0000 && addr < 0x800000) { // wram
		bus->wram[addr & 0x1ffff] = val;
	} else { // romsel
		cart_write(bus->cart, addr, val);
	}
}
