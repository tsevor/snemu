#include "log.h"
#include "bus.h"
#include "host.h"
#include "spc.h"
#include "cpu.h"

#include <stdbool.h>
#include <string.h>

static const uint8_t DMA_OFFS[8][4] = {
	{0, 0, 0, 0},
	{0, 1, 0, 1},
	{0, 0, 0, 0},
	{0, 0, 1, 1},
	{0, 1, 2, 3},
	{0, 1, 0, 1},
	{0, 0, 0, 0},
	{0, 0, 1, 1},
};

void bus_init(Bus *bus) {
	(void)DMA_OFFS;
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

void bus_exec_dma(Bus *bus, int ch) {
	uint8_t i = 0;
	while (bus->dmachan[ch].das != 0) {
		uint16_t b_addr = 0x2100 | (bus->dmachan[ch].bbad + DMA_OFFS[bus->dmachan[ch].mode][i++ & 3]);
		uint32_t a_addr = (bus->dmachan[ch].a1b << 16) | bus->dmachan[ch].a1t;
		// using cpu funcs because they auto-clock
		if (bus->dmachan[ch].invert) {
			cpu_write(bus->cpu, a_addr, cpu_read(bus->cpu, b_addr));
		} else {
			cpu_write(bus->cpu, b_addr, cpu_read(bus->cpu, a_addr));
		}
		if (!bus->dmachan[ch].fixed) bus->dmachan[ch].a1t += bus->dmachan[ch].decrement ? -1 : 1;
		bus->dmachan[ch].das--;
	}
}

void bus_trigger_vblank(Bus *bus) {
	bus->vblank_flag = true;
	bus->vblank_started = true;

	if (bus->vblank_nmi_en) {
		bus->cpu->nmi_pending = true;
	}
	if (bus->autojoy) {
		bus->joy1 = host_get_controller();
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
		case 0x4016: // legacy controller 1
			res = bus->joy1_shift >> 15;
			if (!bus->joy_strobe) {
				bus->joy1_shift = (bus->joy1_shift << 1) | 1;
			}
			return (bus->open_bus & ~1) | res;
		case 0x4017: // legacy controller 2
			return 0;
		case 0x4210: // rdnmi
			bus->cpu_ver = 0x02; // cpu 5a22 v2
			res = bus->rdnmi;
			bus->vblank_flag = false;
			return res;
		case 0x4211: // timeup
			res = (bus->timer_flag) | (bus->open_bus & 0x7f);
			bus->timeup = 0;
			bus->cpu->irq_pending = false;
			return res;
		case 0x4212: // auto-poll done
			return bus->open_bus & ~1;
		case 0x4218: // controller 1 low
			return bus->joy1l;
		case 0x4219: // controller 1 high
			return bus->joy1h;
		case 0x421a ... 0x421b: // controller 2
			return 0;
		case 0x4300 ... 0x430c:
		case 0x4310 ... 0x431c:
		case 0x4320 ... 0x432c:
		case 0x4330 ... 0x433c:
		case 0x4340 ... 0x434c:
		case 0x4350 ... 0x435c:
		case 0x4360 ... 0x436c:
		case 0x4370 ... 0x437c:
			return bus->dmaregs[addr & 0x7f];
		case 0x430f:
		case 0x431f:
		case 0x432f:
		case 0x433f:
		case 0x434f:
		case 0x435f:
		case 0x436f:
		case 0x437f:
			return bus->dmaregs[addr & 0x7b];
		default:
			return bus->open_bus;
	}
}

static void bus_write_cpu_io(Bus *bus, uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x4016: // legacy controller 1
			if (val & 1) {
				bus->joy_strobe = true;
				bus->joy1_shift = host_get_controller();
			} else {
				bus->joy_strobe = false;
			}
			break;
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
		case 0x420b:
			for (int ch = 0; ch < 8; ch++) {
				if (val & (1 << ch)) bus_exec_dma(bus, ch);
			}
			break;
		case 0x4300 ... 0x430a:
		case 0x4310 ... 0x431a:
		case 0x4320 ... 0x432a:
		case 0x4330 ... 0x433a:
		case 0x4340 ... 0x434a:
		case 0x4350 ... 0x435a:
		case 0x4360 ... 0x436a:
		case 0x4370 ... 0x437a:
			bus->dmaregs[addr & 0x7f] = val;
			break;
		case 0x430b: case 0x430f:
		case 0x431b: case 0x431f:
		case 0x432b: case 0x432f:
		case 0x433b: case 0x433f:
		case 0x434b: case 0x434f:
		case 0x435b: case 0x435f:
		case 0x436b: case 0x436f:
		case 0x437b: case 0x437f:
			bus->dmaregs[addr & 0x7b] = val;
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
				bus->open_bus = ppu_bus_read(bus->ppu, offset);
				break;
			case 0x2140 ... 0x217f: // spc mmio
				bus->open_bus = spc_read_io_port(bus->spc, offset & 3);
				break;
			case 0x4016 ... 0x4017: // legacy controller
			case 0x4200 ... 0x421f: // dma
			case 0x4300 ... 0x437f: // cpu mmio
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
				ppu_bus_write(bus->ppu, offset, val);
				break;
			case 0x4016 ... 0x4017: // legacy controller
			case 0x4200 ... 0x421f: // dma
			case 0x4300 ... 0x437f: // cpu mmio
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
