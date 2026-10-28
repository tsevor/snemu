#include "cart.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "host.h"

#define _DONT_SAVE_SRAM_FILE

int header_score(uint8_t *rom, uint32_t addr, uint32_t rom_size) {
	// thanks mesen for the code
	// snes header detection is hard

	if (rom_size < addr + 0x7fff) return -1; // too small

	RomHeader *header = (RomHeader *)(rom + addr + 0x8000 - sizeof(RomHeader));

	if (header->vece_reset < 0x8000) return -1;

	int score = 0;

	if (header->magic == 1) score++;

	// check layout
	if ((header->layout == ML_LOROM || header->layout == ML_S_DD1)
		&& (addr & 0x3fffff) < 0x8000) {
		score++;
	} else if ((header->layout == ML_HIROM || header->layout == ML_EXHIROM)
		&& (addr & 0x3fffff) >= 0x8000) {
		score++;
	}

	// rom type is more likely to have low chipset and no coprocessor
	if (!header->coproc && (header->chipset < 8)) score++;

	if (header->rom_size < 0x10) score++;

	if (header->sram_size < 0x08) score++;

	if ((header->csum ^ header->csum_comp) == 0xffff
		&& header->csum != 0 && header->csum_comp != 0) {
		score += 8;
	}

	// check opcode at reset vector
	uint8_t op = rom[addr + (header->vece_reset & 0x7FFF)];
	switch (op) {
		case 0x18: // clc
		case 0x78: // sei
		case 0x4c: // jmp
		case 0x5c: // jmp
		case 0x20: // jsr
		case 0x22: // jsl
		case 0x9c: // stz
			score += 8;
			break;
		case 0xc2: // rep
		case 0xe2: // sep
		case 0xa9: // lda
		case 0xa2: // ldx
		case 0xa0: // ldy
			score += 4;
			break;
		// these ones likely have little to do with opcode
		// and are more because it's a common data byte
		case 0x00: // brk
		case 0xff: // sbc
		case 0xcc: // cpy
			score -= 8;
			break;
		default:
			break;
	}

	return score;
}

bool corrupted_header(RomHeader *header) {
	// more code from mesen
	int badHeaderCounter =
		(header->sram_size  >= 0x10)
		+ (header->chipset  == 0xf && header->coproc == 0xf)
		+ (header->country  == 0xff)
		+ (header->dev_id   == 0xff)
		+ (header->rom_size == 0xff)
		+ (header->layout   == 0xf && header->fastrom == 1 && header->magic == 0x7);
	return badHeaderCounter > 2;
}

static uint32_t base_addresses[] = {
	0,
	0x8000,
	0x400000,
	0x408000,
};

bool cart_init(Cart *cart, char *filename) {

	FILE *file = fopen(filename, "rb");

	if (!file) {
		lerror("cart: file doesn't exist\n");
		return false;
	}

	fseek(file, 0, SEEK_END);
	uint32_t file_size = ftell(file);

	ldeb1("cart: file size: %d\n", (int)file_size);

	if (file_size < 0x8000) {
		// less than minimum
		lerror("cart: file too small\n");
		fclose(file);
		return false;
	}

	size_t copier_offset = (file_size % 1024 == 512) ? 512 : 0;

	file_size -= copier_offset;

	if (copier_offset > 0) {
		ldeb1("cart: removed 512 byte copier header\n");
		ldeb1("cart: new file size: %d\n", (int)file_size);
	}

	// load full rom
	cart->rom_size = 1 << (sizeof(file_size) * 8 - __builtin_clz(file_size - 1)); // next power of 2
	ldeb1("cart: rom size: %d\n", (int)cart->rom_size);

	cart->rom = calloc(cart->rom_size, 1);
	fseek(file, copier_offset, SEEK_SET);
	fread(cart->rom, 1, file_size, file);
	if (file_size == cart->rom_size) { // 1:1
		ldeb1("cart: file size is a power of 2\n");
	} else {
		// not power of 2, smaller bank mirrored
		ldeb1("cart: file size is not a power of 2\n");
		uint32_t large_bank = cart->rom_size >> 1; // half
		uint32_t small_bank = file_size - large_bank; // remaining
		ldeb1("cart: large bank size %d\n", (int)large_bank);
		ldeb1("cart: small bank size %d\n", (int)small_bank);

		// is small bank size a power of 2?
		if ((small_bank & (small_bank - 1)) == 0) {
			ldeb1("cart: smaller bank is a power of 2\n");
		} else {
			ldeb1("cart: smaller bank is not a power of 2\n");
			// pad small bank with zeros until power of 2
			// (handled by calloc above)
			small_bank = 1 << (32 - __builtin_clz(small_bank - 1));
		}
		// mirror small bank until size of large bank
		for (; small_bank < large_bank; small_bank <<= 1) {
			memcpy(
				cart->rom + large_bank + small_bank,
				cart->rom + large_bank,
				small_bank
			);
		}
	}

	fclose(file);
	file = NULL;

	// find header
	int best_score = -1;
	bool is_lorom = true;
	bool is_exrom = true;

	for (int i = 0; i < 4; i++) {
		uint32_t base = base_addresses[i];
		int32_t score = header_score(cart->rom, base, cart->rom_size);
		if (score >= 0 && score >= best_score) {
			best_score = score;
			is_lorom = (base & 0x8000) == 0;
			is_exrom = (base & 0x400000) != 0;
			cart->header_offset = base + 0x7fb0;
		}
	}

	cart->header = (RomHeader *)(cart->rom + cart->header_offset);

	cart->bad_header = corrupted_header(cart->header);

	cart->layout = is_lorom ?
		(is_exrom ? ML_EXLOROM : ML_LOROM ) :
		(is_exrom ? ML_EXHIROM : ML_HIROM );

	if (cart->header->layout == ML_EXHIROM) cart->layout = ML_EXHIROM;

	ldeb1("cart: layout mode is id %d\n", cart->layout);
	if (cart->layout != ML_LOROM && cart->layout != ML_HIROM) {
		lerror("cart: layouts other than lorom or hirom are unimplemented\n");
		free(cart->rom);
		return false;
	}

	cart->fastrom = cart->header->fastrom;

	switch (cart->header->chipset) {
		case CS_ROM_COP:
		case CS_ROM_COP_RAM:
		case CS_ROM_COP_RAM_BAT:
		case CS_ROM_COP_BAT:
			lerror("cart: rom with coprocessor is unimplemented\n");
			free(cart->rom);
			return false;
		case CS_ROM_RAM_BAT:
			cart->battery = true;
			// fallthrough
		case CS_ROM_RAM:
			cart->has_sram = true;
			break;
		default:
			break;
	}

	cart->filename = strdup(filename);

	cart->has_sram = cart->header->sram_size != 0;
	if (cart->has_sram) {
		cart->sram_size = (1 << 10) << cart->header->sram_size;
		cart->sram = calloc(cart->sram_size, 1);
		ldeb1("cart: sram size: %d\n", cart->sram_size);

		size_t len = strlen(cart->filename);
		int dot = len;
		for (dot = len - 1; dot >= 0; dot--) {
			if (filename[dot] == '.') break;
		}
		if (dot == -1) dot = len; // add to end
		cart->savefile = malloc(dot + 5);
		memcpy(cart->savefile, cart->filename, dot);
		memcpy(cart->savefile + dot, ".srm\0", 5);

		FILE *file = fopen(cart->savefile, "rb");
		if (file) {
			linfo("cart: save file found\n");
			fseek(file, 0, SEEK_END);
			size_t file_size = ftell(file);
			if (file_size != cart->sram_size)
				lerror("cart: save file is wrong size\n");
			else {
				fseek(file, 0, SEEK_SET);
				fread(cart->sram, 1, cart->sram_size, file);
			}
		}
	}

	if (cart->bad_header) lwarn("cart: header likely corrupt\n");

	linfo("cart: name: %.*s\n", 21, cart->header->title);

	return true;
}

void cart_free(Cart *cart) {
	free(cart->rom);
#ifndef _DONT_SAVE_SRAM_FILE // less clutter during development
	if (cart->has_sram) {
		FILE *file = fopen(cart->savefile, "wb");
		if (file) {
			fwrite(cart->sram, 1, cart->sram_size, file);
			linfo("cart: save file written\n");
			fclose(file);
		} else {
			lerror("cart: save file failed to write\n");
		}
		free(cart->sram);
	}
#endif
}

uint8_t cart_read(Cart *cart, uint32_t addr) {
	static const uint16_t width = 256;
	static const uint16_t height = 240;
	uint16_t x = (width - 1) - (addr >> 16);
	uint16_t y = 239 - ((addr >> 8) & 0xff) * height / 256;
	uint16_t idx = y * width + x;
	uint32_t a;
	uint8_t p;
	switch (cart->layout) {
		case ML_LOROM:
			if (addr & 0x8000) { // rom
				a = (((addr & 0x7f0000) >> 1) | (addr & 0x7fff)) & (cart->rom_size - 1);
				p = a * 255 / cart->rom_size;
				host_screen_buffer[idx] = 0xffff0000 | (p << 8);
				return cart->rom[a];
			} else if (addr & 0x400000 && cart->has_sram) { // sram
				a = (((addr & 0x7f0000) >> 1) | (addr & 0x7fff)) & (cart->sram_size - 1);
				p = a * 255 / cart->sram_size;
				host_screen_buffer[idx] = 0xff00ff00 | p;
				return cart->sram[a];
			}
			return 0;
		case ML_HIROM:
			if (addr & 0x408000) { // rom
				a = addr & (cart->rom_size - 1);
				p = a * 255 / cart->rom_size;
				host_screen_buffer[idx] = 0xffff0000 | (p << 8);
				return cart->rom[a];
			} else if ((addr & 0x40e000) == 0x006000 && cart->has_sram) { // sram
				a = (((addr & 0xff0000) >> 3) | (addr & 0x1fff)) & (cart->sram_size - 1);
				p = a * 255 / cart->sram_size;
				host_screen_buffer[idx] = 0xff00ff00 | p;
				return cart->sram[a];
			}
			return 0;
		default:
			lerror("cart: read called on unsupported layout\n");
			return 0;
	}
}

void cart_write(Cart *cart, uint32_t addr, uint8_t val) {
	if (!cart->has_sram) return;
	switch (cart->layout) {
		case ML_LOROM:
			if (!(addr & 0x8000)) {
				uint32_t a = (((addr & 0x7f0000) >> 1) | (addr & 0x7fff)) & (cart->sram_size - 1);
				cart->sram[a] = val;
			}
			break;
		case ML_HIROM:
			if ((addr & 0x40e000) == 0x006000) {
				uint32_t a = (((addr & 0x0f0000) >> 3) | (addr & 0x1fff)) & (cart->sram_size - 1);
				cart->sram[a] = val;
			}
			break;
		default:
			break;
	}
}
