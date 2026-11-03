#include "log.h"
#include "bus.h"
#include "host.h"


void bus_init(Bus *bus) {
	for (int i = 0; i < (128 << 10); i++) {
		bus->wram[i] = 0;
	}
}

void bus_free(Bus *bus) {
	cart_free(bus->cart);
}

void bus_poweron(Bus *bus) {
	cpu_poweron(bus->cpu);
}

void bus_reset(Bus *bus) {
	cpu_reset(bus->cpu);
}

void bus_next_fast(Bus *bus, bool fast) {
	bus->cart->last_read_fast = fast;
}

void bus_clock(Bus *bus) {
	// clock components
	bus->vblank_started = (bus->cpu->cycles & 0xffff) == 0; // force vblank for now
	bus->cycles += bus->last_wr_cycles;
	// if apu is behind, exec one instruction
	// clock ppu every 4 cycles
}

uint8_t bus_read(Bus *bus, uint32_t addr) {
	if ((addr & 0x408000) == 0x000000) { // bottom half q1/q3
		bus->last_wr_cycles = 12;
		switch (addr & 0xffff) { // aux
			case 0x0000 ... 0x1fff: // wram mirror
				bus->last_wr_cycles = 8;
				bus->open_bus = bus->wram[addr & 0xffff];
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
		switch (addr & 0xffff) { // aux
			case 0x0000 ... 0x1fff:
				bus->last_wr_cycles = 8;
				bus->wram[addr & 0xffff] = val;
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
