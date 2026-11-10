#include "ppu.h"
#include "bus.h"
#include "host.h"
#include "log.h"

static const uint8_t PPU_OBJ_SIZES[8][2] = {
	{  8, 16 }, {  8, 32 }, {  8, 64 },
	{ 16, 32 }, { 16, 64 }, { 32, 64 },
	{ 16, 32 }, { 16, 32 }
};

uint16_t vram_effective_addr(uint16_t addr, uint8_t remap) {
	switch (remap) {
		default:
		case 0: return addr & 0x7fff;
		case 1: return (addr & 0xff00) | ((addr & 0xe0) >> 5) | ((addr & 0x1f) << 3);
		case 2: return (addr & 0xfe00) | ((addr & 0x1c0) >> 6) | ((addr & 0x3f) << 3);
		case 3: return (addr & 0xfc00) | ((addr & 0x380) >> 7) | ((addr & 0x7f) << 3);
	}
}

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

uint8_t ppu_bus_read(PPU *ppu, uint32_t addr) {
	switch (addr) {
		default:
			return ppu->bus->open_bus;
	}
}

void ppu_bus_write(PPU *ppu, uint32_t addr, uint8_t val) {
	switch (addr) {
		case 0x2101: ppu->obsel = val; break;
		case 0x2102:
			ppu->oamaddl = val;
			ppu->oamaddr_in = (ppu->oamadd & 0x1ff) << 1;
			break;
		case 0x2103:
			ppu->oamaddh = val;
			ppu->oamaddr_in = (ppu->oamadd & 0x1ff) << 1;
			break;
		case 0x2104: // oamdata
			if (ppu->oamaddr_in < 512) { // low table
				if (ppu->oamaddr_in & 1) {
					ppu->oam[ppu->oamaddr_in - 1] = ppu->oam_latch;
					ppu->oam[ppu->oamaddr_in] = val;
				} else {
					ppu->oam_latch = val;
				}
			} else { // high table
				addr = 0x200 | (ppu->oamaddr_in & 0x1f);
				ppu->oam[addr] = val;
				if (!(ppu->oamaddr_in & 1)) {
					ppu->oam_latch = val;
				}
			}
			ppu->oamaddr_in = (ppu->oamaddr_in + 1) & 0x3ff;
			break;
		case 0x2121:
			ppu->cgadd = val;
			ppu->cgram_toggle = false;
			break;
		case 0x2122:
			if (!ppu->cgram_toggle) {
				ppu->cgram_latch = val;
				ppu->cgram_toggle = true;
			} else {
				ppu->cgram[ppu->cgadd].raw = ppu->cgram_latch | ((val & 0x7f) << 8);
				ppu->cgadd++;
				ppu->cgram_toggle = false;
			}
			break;
		case 0x2116: ppu->vmaddl = val; break;
		case 0x2117: ppu->vmaddh = val; break;
		case 0x2118:
			ppu->vram[vram_effective_addr(ppu->vmadd, ppu->vmain.remap) << 1] = val;
			if (!ppu->vmain.inc) ppu->vmadd = (ppu->vmadd + ppu->vmain_inc) & 0x7fff;
			break;
		case 0x2119:
			ppu->vram[vram_effective_addr(ppu->vmadd, ppu->vmain.remap) << 1 | 1] = val;
			if (ppu->vmain.inc) ppu->vmadd = (ppu->vmadd + ppu->vmain_inc) & 0x7fff;
			break;
		case 0x2115:
			ppu->vmain.raw = val;
			static const uint8_t PPU_VMAIN_INC[4] = { 1, 32, 128, 128 };
			ppu->vmain_inc = PPU_VMAIN_INC[ppu->vmain.step];
			break;
		default:
			break;
	}
}

void ppu_eval_sprites(PPU *ppu) {
	ppu->line_sprites_count = 0;
	uint16_t sy = ppu->scanline;

	for (int i = 0; i < 128 || ppu->line_sprites_count == 32; i++) {
		uint8_t ht_addr = i >> 2;
		uint8_t ht_shift = (i & 3) << 1;
		uint8_t ht_bits = ppu->sprites.ht[ht_addr] >> ht_shift & 3;
		uint8_t size = PPU_OBJ_SIZES[ppu->s_size_sel][ht_bits >> 1];
		int16_t spr_y = ppu->sprites.lt[i].y_pos + 1; // one scanline delay

		if (sy >= spr_y && sy < spr_y + size) {
			ppu->line_sprites[ppu->line_sprites_count++] = i;
		}
	}
}

void ppu_render_dot(PPU *ppu) {
	// layer dots
	// top bit unused, so if set, transparent
	Color15 lay[8];
	for (int i = 0; i < 8; i++) lay[i].raw = 0x8000;

	int16_t sx = ppu->dot;
	int16_t sy = ppu->scanline;

	for (int i = 0; i < ppu->line_sprites_count; i++) {
		uint8_t s = ppu->line_sprites[i];

		uint8_t ht_addr = s >> 2;
		uint8_t ht_shift = (s & 3) << 1;
		uint8_t ht_bits = ppu->sprites.ht[ht_addr] >> ht_shift & 3;
		uint8_t size = PPU_OBJ_SIZES[ppu->s_size_sel][ht_bits >> 1];
		int16_t spr_x = ppu->sprites.lt[s].x_pos;
		if (ht_bits & 1) spr_x -= 256;
		int16_t spr_y = ppu->sprites.lt[s].y_pos + 1; // one scanline delay

		(void)size;

		if (sx >= spr_x && sx < spr_x + size
			&& sy >= spr_y && sy < spr_y + size)
			lay[7].raw = 0x7fff;
	}

	for (int i = 7; i >= 0; i--) {
		if (!lay[i].x) {
			// for now, normal mode is assumed
			uint32_t col32 =
				0xff000000
				| lay[i].r << 19
				| lay[i].g << 11
				| lay[i].b << 3;
			host_screen_buffer[sy * SNES_WIDTH + sx] = col32;
			return;
		}
	}
	host_screen_buffer[sy * SNES_WIDTH + sx] = 0xff000000;
}

void ppu_clock(PPU *ppu) {
	ppu->dot++;

	if (ppu->dot >= 340) {
		ppu->dot = 0;
		ppu->scanline++;

		if (ppu->scanline == 225) {
			// vblank start
			ppu->oamaddr_in = (ppu->oamadd & 0x1ff) << 1;
			host_swap_buffer();
			ppu->bus->vblank_started = true;
			bus_trigger_vblank(ppu->bus);
		} else if (ppu->scanline >= 262) {
			ppu->frame_count++;
			ppu->scanline = 0;
			bus_clear_vblank(ppu->bus);
		}
		if (ppu->scanline < 224) ppu_eval_sprites(ppu);
	}

	bus_check_irq(ppu->bus);

	if (ppu->dot < 256 && ppu->scanline < 224) ppu_render_dot(ppu);
}
