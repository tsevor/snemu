#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct Bus Bus;

#define lhstruct(vname, hlen) union { uint16_t vname; struct { uint8_t vname##l : 8; uint8_t vname##h : hlen; }; }

typedef union Color15 {
	uint16_t raw;
	struct {
		uint16_t r : 5;
		uint16_t g : 5;
		uint16_t b : 5;
		uint16_t x : 1;
	};
} Color15;


typedef struct BGLayer {
	uint16_t tilemap_addr;
	uint16_t chr_addr;
	uint16_t h_scroll;
	uint16_t v_scroll;
	bool double_width;
	bool double_height;
	bool large_tiles;
} BGLayer;

typedef struct PPU {
	Bus *bus;

	uint8_t vram[0x10000];
	Color15 cgram[256];
	union {
		uint8_t oam[544];
		struct {
			struct {
				uint8_t x_pos;
				uint8_t y_pos;
				uint8_t tile_idx;
				union {
					uint8_t attrs;
					struct {
						uint8_t nt : 1;
						uint8_t pa : 3;
						uint8_t pr : 2;
						uint8_t hf : 1;
						uint8_t vf : 1;
					};
				};
			} lt[128];
			uint8_t ht[32];
		} sprites;
	};

	union {
		uint8_t obsel;
		struct {
			uint8_t s_tile_base : 3;
			uint8_t s_name_sel  : 2;
			uint8_t s_size_sel  : 3;
		};
	};

	lhstruct(oamadd, 8);
	uint16_t oamaddr_in;
	uint8_t oam_latch;

	uint8_t line_sprites[32];
	int line_sprites_count;

	uint8_t cgadd;
	uint8_t cgram_latch;
	uint8_t cgram_toggle;

	lhstruct(vmadd, 7);
	union {
		uint8_t raw;
		struct {
			uint8_t step : 2;
			uint8_t remap : 2;
			uint8_t : 3;
			uint8_t inc : 1;
		};
	} vmain;
	uint8_t vmain_inc;

	BGLayer bg[4];
	uint8_t bg_mode;
	bool mode1_bg3_prio;
	uint8_t scroll_latch;
	uint8_t main_screen_en;

	uint8_t sub_screen_en;
	uint8_t cgwsel;
	uint8_t cgadsub;
	Color15 fixed_color;

	int16_t m7a;
	int16_t m7b;
	uint8_t m7_latch;

	bool forced_blank;
	uint8_t brightness;

	uint32_t frame_count;
	int scanline;
	int dot;
} PPU;

void ppu_init(PPU *ppu);
void ppu_poweron(PPU *ppu);
void ppu_reset(PPU *ppu);

void ppu_clock(PPU *ppu);

uint8_t ppu_bus_read(PPU *ppu, uint32_t addr);
void ppu_bus_write(PPU *ppu, uint32_t addr, uint8_t val);
