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

void bus_clock(Bus *bus) {
	// clock components
	bus->vblank_started = true; // force vblank for now
}

uint8_t bus_read(Bus *bus, uint32_t addr) {
	if (addr & 0xff000000) {
		lwarn("bus: read out of range: 0x%08x\n", addr);
		addr = addr & 0x00ffffff;
	}

	static const uint16_t width = 256;
	static const uint16_t height = 240;
	uint16_t x = (width - 1) - (addr >> 16);
	uint16_t y = 239 - ((addr >> 8) & 0xff) * height / 256;
	uint16_t idx = y * width + x;
	uint32_t a;
	uint8_t p;
	if (!(addr & 0x408000)) { // bottom half q1/q3
		switch (addr & 0xffff) { // aux
			case 0x0000 ... 0x1fff:
				a = addr & 0xffff;
				p = a * 255 / 0x20000;
				host_screen_buffer[idx] = 0xff0000ff | (p << 16);
				bus->open_bus = bus->wram[addr & 0xffff];
				break;
			// TODO: ppu, cpu, and joypad mmio
			default: // allow cart to take over
				bus->open_bus = cart_read(bus->cart, addr);
				break;
		}
	} else if (addr >= 0x7e0000 && addr < 0x800000) { // wram
		a = addr & 0x1ffff;
		p = a * 255 / 0x20000;
		host_screen_buffer[idx] = 0xff0000ff | (p << 16);
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
	if (!(addr & 0x408000)) { // bottom half q1/q3
		switch (addr & 0xffff) { // aux
			case 0x0000 ... 0x1fff:
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
