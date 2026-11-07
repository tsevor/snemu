#include "ppu.h"
#include "bus.h"
#include "host.h"

void ppu_init(PPU *ppu) {
	ppu->frame_count = 0;
	ppu->scanline = 0;
	ppu->dot = 0;
}

void ppu_poweron(PPU *ppu) {
	ppu_init(ppu);
}

void ppu_reset(PPU *ppu) {
	ppu_init(ppu);
}

uint8_t ppu_read(PPU *ppu, uint32_t addr) {
	(void)addr;
	return ppu->bus->open_bus;
}

void ppu_write(PPU *ppu, uint32_t addr, uint8_t val) {
	(void)ppu;
	(void)addr;
	(void)val;
}

void ppu_clock(PPU *ppu) {
	ppu->dot++;

	if (ppu->dot >= 340) {
		ppu->dot = 0;
		ppu->scanline++;

		if (ppu->scanline == 225) {
			// vblank start
			host_copy_buffer();
			ppu->bus->vblank_started = true;
			bus_trigger_vblank(ppu->bus);
		} else if (ppu->scanline >= 262) {
			ppu->frame_count++;
			ppu->scanline = 0;
			bus_clear_vblank(ppu->bus);
		}
	}

	bus_check_irq(ppu->bus);
}
