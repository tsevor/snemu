#include "ppu.h"
#include "bus.h"
#include "host.h"
#include "log.h"

static const uint8_t PPU_OBJ_SIZES[8][2] = {
	{  8, 16 }, {  8, 32 }, {  8, 64 },
	{ 16, 32 }, { 16, 64 }, { 32, 64 },
	{ 16, 32 }, { 16, 32 }
};

static const uint8_t PPU_BG_BPP[8][4] = {
	{ 2, 2, 2, 2 },
	{ 4, 4, 2, 0 },
	{ 4, 4, 0, 0 },
	{ 8, 4, 0, 0 },
	{ 8, 2, 0, 0 },
	{ 4, 2, 0, 0 },
	{ 4, 0, 0, 0 },
	{ 8, 7, 0, 0 },
};


static const uint8_t SPRITE_PRIO[8][4] = {
	{ 3, 6, 9, 12 },
	{ 2, 4, 7, 10 },
	{ 2, 4, 6,  8 },
	{ 2, 4, 6,  8 },
};

static const uint8_t BG_PRIO[8][4][2] = {
	{ {8, 11}, {7, 10}, {2, 5}, {1,  4} },
	{ {6,  9}, {5,  8}, {1, 3}, {1, 11} }, // mode1 bg3 prio uses bg4 entry
	{ {3,  7}, {1,  5}, {0, 0}, {0,  0} },
	{ {3,  7}, {1,  5}, {0, 0}, {0,  0} },
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
	int32_t prod;
	switch (addr) {

	case 0x2134: // mpyl
		prod = (int16_t)ppu->m7a * (int8_t)(ppu->m7b >> 8);
		return prod & 0xff;
	case 0x2135: // mpym
		prod = (int16_t)ppu->m7a * (int8_t)(ppu->m7b >> 8);
		return (prod >> 8) & 0xff;
	case 0x2136: // mpyh
		prod = (int16_t)ppu->m7a * (int8_t)(ppu->m7b >> 8);
		return (prod >> 16) & 0xff;
	case 0x2137: // slhv: latch beam position
		ppu->latched_h = ppu->dot;
		ppu->latched_v = ppu->scanline;
		ppu->location_latched = true;
		return ppu->bus->open_bus;
	case 0x2138: { // oamdataread
		uint16_t oam_addr = (ppu->oamaddr_in < 512) ? ppu->oamaddr_in : (0x200 | (ppu->oamaddr_in & 0x1f));
		uint8_t val = ppu->oam[oam_addr];
		ppu->oamaddr_in = (ppu->oamaddr_in + 1) & 0x3ff;
		return val;
	}
	case 0x2139: { // vmdatalread
		uint8_t val = ppu->vram_read_buf & 0xff;
		if (!ppu->vmain.inc) {
			uint16_t eff = vram_effective_addr(ppu->vmadd, ppu->vmain.remap);
			ppu->vram_read_buf = ppu->vram[eff << 1] | (ppu->vram[eff << 1 | 1] << 8);
			ppu->vmadd = (ppu->vmadd + ppu->vmain_inc) & 0x7fff;
		}
		return val;
	}
	case 0x213a: { // vmdatahread
		uint8_t val = (ppu->vram_read_buf >> 8) & 0xff;
		if (ppu->vmain.inc) {
			uint16_t eff = vram_effective_addr(ppu->vmadd, ppu->vmain.remap);
			ppu->vram_read_buf = ppu->vram[eff << 1] | (ppu->vram[eff << 1 | 1] << 8);
			ppu->vmadd = (ppu->vmadd + ppu->vmain_inc) & 0x7fff;
		}
		return val;
	}
	case 0x213b: { // cgdataread
		uint8_t val;
		if (ppu->cgram_toggle) {
			val = (ppu->cgram[ppu->cgadd].raw >> 8) & 0x7f;
			ppu->cgadd++;
		} else {
			val = ppu->cgram[ppu->cgadd].raw & 0xff;
		}
		ppu->cgram_toggle = !ppu->cgram_toggle;
		return val;
	}
	case 0x213c: { // ophct: horizontal scanline location
		uint8_t val;
		if (!ppu->h_loc_toggle) {
			val = ppu->latched_h & 0xff;
		} else {
			val = ((ppu->latched_h >> 8) & 1) | (ppu->bus->open_bus & 0xfe);
		}
		ppu->h_loc_toggle = !ppu->h_loc_toggle;
		return val;
	}
	case 0x213d: { // opvct: vertical scanline location
		uint8_t val;
		if (!ppu->v_loc_toggle) {
			val = ppu->latched_v & 0xff;
		} else {
			val = ((ppu->latched_v >> 8) & 1) | (ppu->bus->open_bus & 0xfe);
		}
		ppu->v_loc_toggle = !ppu->v_loc_toggle;
		return val;
	}
	case 0x213e: // stat77
		return 0x01; // ppu1 version 1
	case 0x213f:
		ppu->h_loc_toggle = false;
		ppu->v_loc_toggle = false;
		uint8_t res = (ppu->frame_count & 1) << 7 | (ppu->location_latched ? 0x40 : 0) | 0x01;
		ppu->location_latched = false;
		return res;
	default:
		return ppu->bus->open_bus;
	}
}

void ppu_bus_write(PPU *ppu, uint32_t addr, uint8_t val) {
	switch (addr) {
		case 0x2100:
			ppu->forced_blank = (val & 0x80) != 0;
			ppu->brightness   = val & 0x0f;
			break;
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
		case 0x2105:
			ppu->bg_mode = val & 0x07;
			ppu->mode1_bg3_prio = (val & 0x08) != 0;
			ppu->bg[0].large_tiles = (val & 0x10) != 0;
			ppu->bg[1].large_tiles = (val & 0x20) != 0;
			ppu->bg[2].large_tiles = (val & 0x40) != 0;
			ppu->bg[3].large_tiles = (val & 0x80) != 0;
			break;
		case 0x2107 ... 0x210a: {
			int bg = addr - 0x2107;
			ppu->bg[bg].tilemap_addr = (val & 0x7c) << 9;
			ppu->bg[bg].double_width  = (val & 0x01) != 0;
			ppu->bg[bg].double_height = (val & 0x02) != 0;
			break;
		}
		case 0x210b:
			ppu->bg[0].chr_addr = (val & 0x07) << 13;
			ppu->bg[1].chr_addr = (val & 0x70) << 9;
			break;
		case 0x210c:
			ppu->bg[2].chr_addr = (val & 0x07) << 13;
			ppu->bg[3].chr_addr = (val & 0x70) << 9;
			break;
		case 0x212c:
			ppu->main_screen_en = val & 0x1f;
			break;
		case 0x210d: case 0x210f: case 0x2111: case 0x2113: {
			int bg = (addr - 0x210d) >> 1;
			ppu->bg[bg].h_scroll = ((val << 8) | ppu->scroll_latch) & 0x3ff;
			ppu->scroll_latch = val;
			break;
		}
		case 0x210e: case 0x2110: case 0x2112: case 0x2114: {
			int bg = (addr - 0x210e) >> 1;
			ppu->bg[bg].v_scroll = ((val << 8) | ppu->scroll_latch) & 0x3ff;
			ppu->scroll_latch = val;
			break;
		}
		case 0x2115:
			ppu->vmain.raw = val;
			static const uint8_t PPU_VMAIN_INC[4] = { 1, 32, 128, 128 };
			ppu->vmain_inc = PPU_VMAIN_INC[ppu->vmain.step];
			break;
		case 0x2116: {
			ppu->vmaddl = val;
			uint16_t eff = vram_effective_addr(ppu->vmadd, ppu->vmain.remap);
			ppu->vram_read_buf = ppu->vram[eff << 1] | (ppu->vram[eff << 1 | 1] << 8);
			break;
		}
		case 0x2117: {
			ppu->vmaddh = val;
			uint16_t eff = vram_effective_addr(ppu->vmadd, ppu->vmain.remap);
			ppu->vram_read_buf = ppu->vram[eff << 1] | (ppu->vram[eff << 1 | 1] << 8);
			break;
		}
		case 0x2118:
			ppu->vram[vram_effective_addr(ppu->vmadd, ppu->vmain.remap) << 1] = val;
			if (!ppu->vmain.inc) ppu->vmadd = (ppu->vmadd + ppu->vmain_inc) & 0x7fff;
			break;
		case 0x2119:
			ppu->vram[vram_effective_addr(ppu->vmadd, ppu->vmain.remap) << 1 | 1] = val;
			if (ppu->vmain.inc) ppu->vmadd = (ppu->vmadd + ppu->vmain_inc) & 0x7fff;
			break;
		case 0x211b:
			ppu->m7a = (val << 8) | ppu->m7_latch;
			ppu->m7_latch = val;
			break;
		case 0x211c:
			ppu->m7b = (val << 8) | ppu->m7_latch;
			ppu->m7_latch = val;
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
		case 0x2123: ppu->w12sel = val; break;
		case 0x2124: ppu->w34sel = val; break;
		case 0x2125: ppu->wobjsel = val; break;
		case 0x2126: ppu->wh0 = val; break;
		case 0x2127: ppu->wh1 = val; break;
		case 0x2128: ppu->wh2 = val; break;
		case 0x2129: ppu->wh3 = val; break;
		case 0x212a: ppu->wbglog = val; break;
		case 0x212b: ppu->wobjlog = val; break;
		case 0x212d: ppu->sub_screen_en = val & 0x1f; break;
		case 0x212e: ppu->tmw = val & 0x1f; break;
		case 0x212f: ppu->tsw = val & 0x1f; break;
		case 0x2130: ppu->cgwsel = val; break;
		case 0x2131: ppu->cgadsub = val; break;
		case 0x2132:
			// fixed color: bits 5-7 select R/G/B, bits 0-4 are 5-bit intensity
			if (val & 0x20) ppu->fixed_color.r = val & 0x1f;
			if (val & 0x40) ppu->fixed_color.g = val & 0x1f;
			if (val & 0x80) ppu->fixed_color.b = val & 0x1f;
			break;
		default:
			break;
	}
}

void ppu_eval_sprites(PPU *ppu) {
	ppu->line_sprites_count = 0;
	uint16_t sy = ppu->scanline - 1;

	for (int i = 0; i < 128 && ppu->line_sprites_count < 32; i++) {
		uint8_t ht_addr = i >> 2;
		uint8_t ht_shift = (i & 3) << 1;
		uint8_t ht_bits = ppu->sprites.ht[ht_addr] >> ht_shift & 3;
		uint8_t size = PPU_OBJ_SIZES[ppu->s_size_sel][ht_bits >> 1];
		int16_t spr_y = ppu->sprites.lt[i].y_pos + 1; // one scanline delay
		uint8_t row = (sy - spr_y) & 0xff; // allow wrapping to top
		if (row < size) {
			ppu->line_sprites[ppu->line_sprites_count++] = i;
		}
	}
}

static bool ppu_eval_window(PPU *ppu, int layer, int x) {
	// extract the 4 setting bits for this layer
	uint8_t sel = (layer < 2) ?
		ppu->w12sel : ((layer < 4) ? ppu->w34sel : ppu->wobjsel);
	uint8_t cfg = (sel >> ((layer & 1) ? 4 : 0)) & 0x0f;

	bool w1_inv = (cfg & 0x01) != 0;
	bool w1_en  = (cfg & 0x02) != 0;
	bool w2_inv = (cfg & 0x04) != 0;
	bool w2_en  = (cfg & 0x08) != 0;

	if (!w1_en && !w2_en) return false;

	bool w1 = (ppu->wh0 <= ppu->wh1) ? (x >= ppu->wh0 && x <= ppu->wh1) : false;
	if (w1_inv) w1 = !w1;

	bool w2 = (ppu->wh2 <= ppu->wh3) ? (x >= ppu->wh2 && x <= ppu->wh3) : false;
	if (w2_inv) w2 = !w2;

	if (w1_en && w2_en) {
		uint8_t log = (layer < 4) ? (ppu->wbglog >> (layer * 2)) :
		(ppu->wobjlog >> ((layer - 4) * 2));
		switch (log & 3) {
			case 0: return w1 || w2;   // or
			case 1: return w1 && w2;   // and
			case 2: return w1 ^ w2;    // xor
			case 3: return !(w1 ^ w2); // xnor
		}
	}
	return w1_en ? w1 : w2;
}

void ppu_render_dot(PPU *ppu) {
	// layer dots
	// top bit unused, so if set, transparent
	Color15 main_lay[13];
	Color15 sub_lay[13];
	uint8_t main_src[13];

	for (int i = 0; i < 13; i++) main_lay[i].raw = sub_lay[i].raw = 0x8000;

	int16_t sx = ppu->dot;
	int16_t sy = ppu->scanline - 1;

	if (!ppu->forced_blank) {
		for (int i = 0; i < ppu->line_sprites_count; i++) {
			uint8_t s = ppu->line_sprites[i];

			uint8_t ht_addr = s >> 2;
			uint8_t ht_shift = (s & 3) << 1;
			uint8_t ht_bits = ppu->sprites.ht[ht_addr] >> ht_shift & 3;
			uint8_t size = PPU_OBJ_SIZES[ppu->s_size_sel][ht_bits >> 1];
			int16_t spr_x = ppu->sprites.lt[s].x_pos;
			if (ht_bits & 1) spr_x -= 256;
			if (sx < spr_x || sx >= spr_x + size) continue;
			int16_t spr_y = ppu->sprites.lt[s].y_pos + 1; // one scanline delay

			uint8_t px = sx - spr_x;
			uint8_t py = sy - spr_y;
			if (ppu->sprites.lt[s].hf) px = (size - 1) - px;
			if (ppu->sprites.lt[s].vf) py = (size - 1) - py;

			int tile_row = ((ppu->sprites.lt[s].tile_idx >> 4) + (py / 8)) & 0x0f;
			int tile_col = ((ppu->sprites.lt[s].tile_idx & 0x0f) + (px / 8)) & 0x0f;
			int tile_num = (tile_row << 4) | tile_col;

			int tile_px = px & 7;
			int tile_py = py & 7;

			int bit = 7 - tile_px;

			uint16_t tile_addr =
				(ppu->s_tile_base << 14)
				+ (ppu->sprites.lt[s].nt ? ((ppu->s_name_sel + 1) << 13) : 0)
				+ (tile_num << 5);

			uint8_t bp0 = (ppu->vram[tile_addr + tile_py * 2] >> bit) & 1;
			uint8_t bp1 = (ppu->vram[tile_addr + tile_py * 2 + 1] >> bit) & 1;
			uint8_t bp2 = (ppu->vram[tile_addr + 16 + tile_py * 2] >> bit) & 1;
			uint8_t bp3 = (ppu->vram[tile_addr + 16 + tile_py * 2 + 1] >> bit) & 1;

			uint8_t color_idx = bp0 | (bp1 << 1) | (bp2 << 2) | (bp3 << 3);

			if (color_idx == 0) continue;

			uint8_t prio_lay = SPRITE_PRIO[ppu->bg_mode][ppu->sprites.lt[s].pr];
			uint16_t cgram_idx = 128 + (ppu->sprites.lt[s].pa * 16) + color_idx;

			bool mask = ppu_eval_window(ppu, 4, sx);
			bool on_main = (ppu->main_screen_en & 0x10) && !((ppu->tmw & 0x10) && mask);
			bool on_sub  = (ppu->sub_screen_en & 0x10)  && !((ppu->tsw & 0x10) && mask);

			if (on_main) {
				main_lay[prio_lay] = ppu->cgram[cgram_idx];
				main_src[prio_lay] = (ppu->sprites.lt[s].pa >= 4) ? 4 : 6;
			}
			if (on_sub) {
				sub_lay[prio_lay] = ppu->cgram[cgram_idx];
			}

			break;
		}
	}

	for (int bg = 0; bg < 4; bg++) {
		uint8_t layer_bpp = PPU_BG_BPP[ppu->bg_mode][bg];
		bool mask = ppu_eval_window(ppu, bg, sx);
		bool on_main = ((ppu->main_screen_en >> bg) & 1) && !(((ppu->tmw >> bg) & 1) && mask);
		bool on_sub  = ((ppu->sub_screen_en >> bg) & 1)  && !(((ppu->tsw >> bg) & 1) && mask);

		// if not enabled or masked on both, skip
		if (layer_bpp == 0 || (!on_main && !on_sub) || ppu->forced_blank) continue;
		if (((ppu->main_screen_en >> bg) & 1) == 0 && ((ppu->sub_screen_en >> bg) & 1) == 0) continue;

		int px = (sx + ppu->bg[bg].h_scroll) & 0x3ff;
		int py = (sy + ppu->bg[bg].v_scroll) & 0x3ff;

		int tx = px / 8;
		int ty = py / 8;

		int screen_x = (tx / 32) & (ppu->bg[bg].double_width ? 1 : 0);
		int screen_y = (ty / 32) & (ppu->bg[bg].double_height ? 1 : 0);
		int screen_idx = screen_x + (screen_y * (ppu->bg[bg].double_width ? 2 : 1));

		uint16_t screen_base = ppu->bg[bg].tilemap_addr + (screen_idx * 2048);

		int cell_x = tx & 31;
		int cell_y = ty & 31;
		uint16_t map_addr = (screen_base + ((cell_y * 32 + cell_x) * 2)) & 0xffff;

		uint16_t entry = ppu->vram[map_addr] | (ppu->vram[map_addr + 1] << 8);
		uint16_t tile_idx = entry & 0x3ff;
		uint8_t pal_num   = (entry >> 10) & 7;
		bool prio     = (entry & 0x2000) != 0;
		bool h_flip       = (entry & 0x4000) != 0;
		bool v_flip       = (entry & 0x8000) != 0;

		int bg_idx = (bg == 2 && ppu->mode1_bg3_prio && ppu->bg_mode == 1) ? 3 : bg;
		uint8_t prio_lay = BG_PRIO[ppu->bg_mode][bg_idx][prio];

		int tile_px = px & 7;
		int tile_py = py & 7;
		if (h_flip) tile_px = 7 - tile_px;
		if (v_flip) tile_py = 7 - tile_py;
		int bit = 7 - tile_px;

		Color15 bg_col = { 0x8000 };

		switch (layer_bpp) {
			case 2: {
				uint16_t chr = (ppu->bg[bg].chr_addr + (tile_idx * 16)) & 0xffff;
				uint8_t bp0 = (ppu->vram[chr + tile_py * 2] >> bit) & 1;
				uint8_t bp1 = (ppu->vram[chr + tile_py * 2 + 1] >> bit) & 1;
				uint8_t color_idx = bp0 | (bp1 << 1);
				if (color_idx != 0) {
					uint8_t base_pal = (ppu->bg_mode == 0) ? (bg * 32) : 0;
					uint8_t cgram_idx = base_pal + (pal_num * 4) + color_idx;
					bg_col = ppu->cgram[cgram_idx];
				}
				break;
			}
			case 4: {
				uint16_t chr = (ppu->bg[bg].chr_addr + (tile_idx * 32)) & 0xffff;
				uint8_t bp0 = (ppu->vram[chr + tile_py * 2] >> bit) & 1;
				uint8_t bp1 = (ppu->vram[chr + tile_py * 2 + 1] >> bit) & 1;
				uint8_t bp2 = (ppu->vram[chr + 16 + tile_py * 2] >> bit) & 1;
				uint8_t bp3 = (ppu->vram[chr + 16 + tile_py * 2 + 1] >> bit) & 1;
				uint8_t color_idx = bp0 | (bp1 << 1) | (bp2 << 2) | (bp3 << 3);
				if (color_idx != 0) {
					uint8_t cgram_idx = (pal_num * 16) + color_idx;
					bg_col = ppu->cgram[cgram_idx];
				}
				break;
			}
			case 8: {
				uint16_t chr = (ppu->bg[bg].chr_addr + (tile_idx * 64)) & 0xffff;
				uint8_t bp0 = (ppu->vram[chr + tile_py * 2] >> bit) & 1;
				uint8_t bp1 = (ppu->vram[chr + tile_py * 2 + 1] >> bit) & 1;
				uint8_t bp2 = (ppu->vram[chr + 16 + tile_py * 2] >> bit) & 1;
				uint8_t bp3 = (ppu->vram[chr + 16 + tile_py * 2 + 1] >> bit) & 1;
				uint8_t bp4 = (ppu->vram[chr + 32 + tile_py * 2] >> bit) & 1;
				uint8_t bp5 = (ppu->vram[chr + 32 + tile_py * 2 + 1] >> bit) & 1;
				uint8_t bp6 = (ppu->vram[chr + 48 + tile_py * 2] >> bit) & 1;
				uint8_t bp7 = (ppu->vram[chr + 48 + tile_py * 2 + 1] >> bit) & 1;
				uint8_t color_idx = bp0 | (bp1<<1) | (bp2<<2) | (bp3<<3) | (bp4<<4) | (bp5<<5) | (bp6<<6) | (bp7<<7);
        		if (color_idx != 0) {
					uint8_t cgram_idx = (pal_num) + color_idx;
					bg_col = ppu->cgram[cgram_idx];
				}
				break;
			}
		}

		if (on_main) {
			main_lay[prio_lay] = bg_col;
			main_src[prio_lay] = bg;
		}
		if (on_sub) {
			sub_lay[prio_lay] = bg_col;
		}
	}


	// main screen pixel
	Color15 main_col = ppu->cgram[0];
	uint8_t main_source = 5; // 5 = backdrop
	for (int i = 12; i >= 0; i--) {
		if (!main_lay[i].x) {
			main_col = main_lay[i];
			main_source = main_src[i];
			break;
		}
	}

	// sub screen pixel
	Color15 sub_col = { 0 };
	bool has_sub = false;
	for (int i = 12; i >= 0; i--) {
		if (!sub_lay[i].x) {
			sub_col = sub_lay[i];
			has_sub = true;
			break;
		}
	}

	bool math_win = ppu_eval_window(ppu, 5, sx);

	uint8_t clip_mode = (ppu->cgwsel >> 6) & 3;
	if ((clip_mode == 1 && !math_win) || (clip_mode == 2 && math_win) || (clip_mode == 3)) {
		main_col.raw = 0;
	}

	// prevent mode skips color math
	uint8_t prev_mode = (ppu->cgwsel >> 4) & 3;
	bool prevent_math = (prev_mode == 1 && !math_win) || (prev_mode == 2 && math_win) || (prev_mode == 3);

	Color15 final_col = main_col;
	if (!prevent_math && (ppu->cgadsub & (1 << main_source))) {
		// subscreen vs fixed color
		Color15 col_b = (ppu->cgwsel & 0x02 && has_sub) ? sub_col : ppu->fixed_color;

		bool sub = (ppu->cgadsub & 0x80) != 0;
		bool halve = (ppu->cgadsub & 0x40) != 0;

		int r = sub ? (main_col.r - col_b.r) : (main_col.r + col_b.r);
		int g = sub ? (main_col.g - col_b.g) : (main_col.g + col_b.g);
		int b = sub ? (main_col.b - col_b.b) : (main_col.b + col_b.b);

		if (halve && (has_sub || !(ppu->cgwsel & 0x02))) {
			r >>= 1; g >>= 1; b >>= 1;
		}

		final_col.r = r < 0 ? 0 : (r > 31 ? 31 : r);
		final_col.g = g < 0 ? 0 : (g > 31 ? 31 : g);
		final_col.b = b < 0 ? 0 : (b > 31 ? 31 : b);
	}

	uint32_t col32 = ppu->forced_blank ?
		0xff000000
		: 0xff000000
		| (final_col.r * ppu->brightness / 15) << 19
		| (final_col.g * ppu->brightness / 15) << 11
		| (final_col.b * ppu->brightness / 15) << 3;
	host_screen_buffer[sy * SNES_WIDTH + sx] = col32;
}

void ppu_clock(PPU *ppu) {
	ppu->dot++;

	if (ppu->dot == 340) {
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
			bus_init_hdma(ppu->bus);
		}
		if (ppu->scanline > 0 && ppu->scanline <= 224) ppu_eval_sprites(ppu);
	}

	bus_check_irq(ppu->bus);

	if (ppu->scanline < 224 && ppu->dot == 278) {
		bus_exec_hdma(ppu->bus);
	}
	if (ppu->scanline > 0 && ppu->scanline <= 224 && ppu->dot < 256) {
		ppu_render_dot(ppu);
	}
}
