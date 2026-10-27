#include "log.h"
#include "bus.h"

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

void bus_clock(Bus *bus) {
	// clock components
	bus->vblank_started = true; // force vblank for now
}

uint8_t bus_read(Bus *bus, uint32_t addr) {
	if (addr & 0xff000000) {
		lwarn("bus: read out of range: 0x%08x\n", addr);
		addr = addr & 0x00ffffff;
	}
	if (!(addr & 0x048000)) { // bottom half q1/q3
		switch (addr & 0xffff) { // aux
			case 0x0000 ... 0x1fff:
				bus->open_bus = bus->wram[addr & 0xffff];
				break;
			// TODO: ppu, cpu, and joypad mmio
			default:
				break;
		}
	} else if (addr >= 0x7e0000 && addr < 0x800000) { // wram
		bus->open_bus = bus->wram[addr & 0x1ffff];
	} else { // romsel
		bus->open_bus = cart_read(bus->cart, addr);
	}
	return bus->open_bus;
}

void bus_write(Bus *bus, uint32_t addr, uint8_t val) {
	if (addr & 0xff000000) {
		lwarn("bus: write out of range: 0x%08x\n", addr);
		addr = addr & 0x00ffffff;
	}
	bus->open_bus = val;
	switch (addr) {
		default:
			break;
	}
}
