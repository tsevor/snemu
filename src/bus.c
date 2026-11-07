#include "log.h"
#include "bus.h"
#include "host.h"
#include "spc.h"

#include <stdbool.h>
#include <string.h>


void bus_init(Bus *bus) {
	memset(bus->wram, 0, sizeof(bus->wram));
	bus->nmitimen = 0;
	bus->rdnmi = 0;
	bus->timeup = 0;
	bus->htime = 0x1ff;
	bus->vtime = 0x1ff;
	bus->vblank_started = false;
}

void bus_free(Bus *bus) {
	cart_free(bus->cart);
}

void bus_poweron(Bus *bus) {
	cpu_poweron(bus->cpu);
	spc_poweron(bus->spc);
	ppu_poweron(bus->ppu);
}

void bus_reset(Bus *bus) {
	cpu_reset(bus->cpu);
	spc_reset(bus->spc);
	ppu_reset(bus->ppu);
	bus->nmitimen = 0;
	bus->rdnmi = 0;
	bus->timeup = 0;
	bus->vblank_started = false;
}

void bus_next_fast(Bus *bus) {
	bus->last_wr_cycles = 6;
}

void bus_clock(Bus *bus) {
	// clock components
	bus->cycles += bus->last_wr_cycles;

	// if apu is behind, exec one instruction
	bus->spc_cycle_acc += bus->last_wr_cycles * 1024000;
	while (bus->spc_cycle_acc >= 21477270) {
		bus->spc_cycle_acc -= 21477270 * spc_step(bus->spc);
	}

	// clock ppu every 4 master cycles
	bus->ppu_cycle_acc += bus->last_wr_cycles;
	while (bus->ppu_cycle_acc >= 4) {
		ppu_clock(bus->ppu);
		bus->ppu_cycle_acc -= 4;
	}
}

void bus_trigger_vblank(Bus *bus) {
	bus->vblank_flag = true;
	bus->vblank_started = true;

	if (bus->vblank_nmi_en) {
		bus->cpu->nmi_pending = true;
	}
}

void bus_clear_vblank(Bus *bus) {
	bus->vblank_started = false;
}

void bus_check_irq(Bus *bus) {
	bool match = false;
	uint16_t h = bus->ppu->dot;
	uint16_t v = bus->ppu->scanline;

	switch (bus->irq_mode) {
		case 0:
			return;
		case 1:
			if (h == bus->htime) match = true;
			break;
		case 2:
			if (v == bus->vtime && h == 0) match = true;
			break;
		case 3:
			if (v == bus->vtime && h == bus->htime) match = true;
			break;
	}

	if (match) {
		bus->timer_flag = true;
		if (bus->cpu) {
			bus->cpu->irq_pending = true;
			bus->cpu->waiting = false;
		}
	}
}

static uint8_t bus_read_cpu_io(Bus *bus, uint16_t addr) {
	uint8_t res;
	switch (addr) {
		case 0x4210: // rdnmi
			bus->cpu_ver = 0x02; // cpu 5a22 v2
			res = bus->rdnmi;
			bus->vblank_flag = false;
			return res;
		case 0x4211: { // timeup
			res = (bus->timer_flag) | (bus->open_bus & 0x7f);
			bus->timeup = 0;
			bus->cpu->irq_pending = false;
			return res;
		}
		default:
			return bus->open_bus;
	}
}

static void bus_write_cpu_io(Bus *bus, uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x4200: { // nmitimen
			bool old_nmi = bus->vblank_nmi_en;
			bus->nmitimen = val;
			if (!old_nmi && (bus->vblank_nmi_en) && (bus->vblank_flag)) {
				bus->cpu->nmi_pending = true;
			}
			if (bus->irq_mode == 0) {
				bus->timeup = 0;
				bus->cpu->irq_pending = false;
			}
			break;
		}
		case 0x4207: // htimel
			bus->htimel = val;
			break;
		case 0x4208: // htimeh
			bus->htimeh = val & 1;
			break;
		case 0x4209: // vtimel
			bus->vtimel = val;
			break;
		case 0x420a: // vtimeh
			bus->vtime = val & 1;
			break;
		default:
			break;
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
			case 0x2100 ... 0x213f: // ppu mmio
				bus->open_bus = ppu_read(bus->ppu, offset);
				break;
			case 0x2140 ... 0x217f: // spc mmio
				bus->open_bus = spc_read_io_port(bus->spc, offset & 3);
				break;
			case 0x4200 ... 0x421f: // cpu mmio
				bus->last_wr_cycles = 6;
				bus->open_bus = bus_read_cpu_io(bus, offset);
				break;
			// TODO: dma and joypad mmio
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
			case 0x2140 ... 0x217f: // spc mmio
				spc_write_io_port(bus->spc, offset & 3, val);
				break;
			case 0x2100 ... 0x213f: // ppu mmio
				ppu_write(bus->ppu, offset, val);
				break;
			case 0x4200 ... 0x421f: // cpu mmio
				bus_write_cpu_io(bus, offset, val);
				break;
			// TODO: dma and joypad mmio
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
