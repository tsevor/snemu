#include "log.h"
#include "bus.h"

void bus_init(Bus *bus) {
	for (int i = 0; i < (128 << 10); i++) {
		bus->wram[i] = 0;
	}
}

void bus_poweron(Bus *bus) {
	cpu_poweron(bus->cpu);
}

void bus_reset(Bus *bus) {
	cpu_reset(bus->cpu);
}

void bus_clock(Bus *bus) {
	// clock components
	(void)bus;
}

uint8_t bus_read(Bus *bus, uint32_t addr) {
	if (addr & 0xff000000) {
		lwarn("bus: read out of range: 0x%08x\n", addr);
		addr = addr & 0x00ffffff;
	}
	switch (addr) {
		default:
			break;
	}
	return bus->open_bus;
}

void bus_write(Bus *bus, uint32_t addr, uint8_t val) {
	if (addr & 0xff000000) {
		lwarn("bus: write out of range: 0x%08x\n", addr);
		addr = addr & 0x00ffffff;
	}
	switch (addr) {
		default:
			break;
	}
	bus->open_bus = val;
}
