#include "cart.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"


int header_score(uint8_t *rom, uint32_t addr, uint32_t rom_size) {
	// thanks mesen for the code
	// snes header detection is hard

	if (rom_size < addr + 0x7fff) {
		return -1; // too small
	}

	RomHeader header;

	memcpy(&header, rom + addr + 0x7fb0, sizeof(header));

	// check reset vector
	if (header.vece_reset < 0x8000) {
		return -1;
	}

	int score = 0;

	// check magic
	if (header.magic == 1)
		score++;

	// check layout
	if ((header.layout == ML_LOROM || header.layout == ML_S_DD1)
		&& (addr & 0x3fffff) < 0x8000) {
		score++;
	} else if ((header.layout == ML_HIROM || header.layout == ML_EXHIROM)
		&& (addr & 0x3fffff) >= 0x8000) {
		score++;
	}

	// rom type is more likely to have low chipset and no coprocessor
	if (!header.coproc && (header.chipset < 8)) {
		score++;
	}

	// romsize should be < 10
	if (header.rom_size < 0x10) {
		score++;
	}

	// ramsize should be < 8
	if (header.sram_size < 0x08) {
		score++;
	}

	// checksum validation
	if ((header.csum ^ header.csum_comp) == 0xffff
		&& header.csum != 0 && header.csum_comp != 0) {
		score += 8;
	}

	// check opcode at reset vector
	uint8_t op = rom[addr + (header.vece_reset & 0x7FFF)];
	switch (op) {
		case 0x18:
		case 0x78:
		case 0x4c:
		case 0x5c:
		case 0x20:
		case 0x22:
		case 0x9c:
			score += 8;
			break;
		case 0xc2:
		case 0xe2:
		case 0xa9:
		case 0xa2:
		case 0xa0:
			score += 4;
			break;
		case 0x00:
		case 0xff:
		case 0xcc:
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
		((header->sram_size & 0xf0) != 0)
		+ (header->chipset == 0xf && header->coproc == 0xf)
		+ (header->country == 0xff)
		+ (header->dev_id == 0xff)
		+ (header->rom_size == 0xff)
		+ (header->layout == 0xf && header->fastrom == 1 && header->magic == 0x7);
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
	size_t file_size = ftell(file);

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
	size_t rom_size = 1 << (32 - __builtin_clz(file_size - 1)); // next power of 2
	ldeb1("cart: rom size: %d\n", (int)rom_size);

	cart->rom = malloc(rom_size);
	fseek(file, copier_offset, SEEK_SET);
	if (file_size == rom_size) { // power of 2, 1:1
		ldeb1("cart: file size is a power of 2\n");
		fread(cart->rom, 1, rom_size, file);
	} else { // not power of 2, smaller bank mirrored
		ldeb1("cart: file size is not a power of 2\n");
		size_t large_bank = rom_size >> 1; // half
		fread(cart->rom, 1, large_bank, file);
		ldeb1("cart: large bank size %d\n", (int)large_bank);
		size_t small_bank = file_size - large_bank;
		ldeb1("cart: small bank size %d\n", (int)small_bank);
		// is other half power of 2?
		if ((small_bank & (small_bank - 1)) == 0) {
			ldeb1("cart: smaller bank is a power of 2\n");
			fread(cart->rom + large_bank, 1, small_bank, file);
		} else {
			ldeb1("cart: smaller bank is not a power of 2\n");
			fread(cart->rom + large_bank, 1, small_bank, file);
			size_t i = small_bank;
			small_bank = 1 << (32 - __builtin_clz(small_bank - 1));
			for (; i < large_bank; i++)
				cart->rom[large_bank + i] = 0;
		}
		// double small bank until size of large bank
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
		int32_t score = header_score(cart->rom, base, rom_size);
		if (score >= 0 && score >= best_score) {
			best_score = score;
			is_lorom = (base & 0x8000) == 0;
			is_exrom = (base & 0x400000) != 0;
			cart->header_offset = base + 0x7fb0;
			memcpy(&cart->header, cart->rom + cart->header_offset, sizeof(RomHeader));
		}
	}

	cart->rom_size = rom_size;

	cart->bad_header = corrupted_header(&cart->header);

	cart->layout = (
		is_lorom ?
		(is_exrom ? ML_EXLOROM : ML_LOROM ) :
		(is_exrom ? ML_EXHIROM : ML_HIROM )
	);

	if ((cart->header.layout & 0x7) == ML_EXHIROM)
		cart->layout = ML_EXHIROM;

	cart->fastrom = cart->header.fastrom;

	switch (cart->header.chipset) {
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

	cart->has_sram = cart->header.sram_size != 0;
	if (cart->has_sram) {
		cart->sram_size = (1 << 10) << cart->header.sram_size;
		cart->sram = malloc(cart->sram_size);

		size_t len = strlen(cart->filename);
		int dot = len;
		for (dot = len - 1; dot >= 0; dot--) {
			if (filename[dot] == '.')
				break;
		}
		if (dot == -1) // not found
			dot = len; // add to end
		cart->savefile = malloc(dot + 5);
		for (int i = 0; i < dot; i++)
			cart->savefile[i] = cart->filename[i];
		for (int i = 0; i < 5; i++)
			cart->savefile[dot + i] = ".srm\0"[i];

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
		} else {
			// init sram
			for (size_t i = 0; i < cart->sram_size; i++)
				cart->sram = 0;
		}
	}

	if (cart->bad_header) lwarn("cart: header likely corrupt\n");

	linfo("cart: name: %.*s\n", 21, cart->header.title);

	return true;
}

void cart_free(Cart *cart) {
	free(cart->rom);
	if (cart->has_sram) {
		FILE *file = fopen(cart->savefile, "wb");
		if (file) {
			fwrite(cart->sram, 1, cart->sram_size, file);
			linfo("cart: save file written\n");
		} else {
			lerror("cart: save file failed to write\n");
		}
		free(cart->sram);
	}
}

uint8_t cart_read(Cart *cart, uint32_t addr) {
	(void)cart;
	(void)addr;
	return 0;
}

void cart_write(Cart *cart, uint32_t addr, uint8_t val) {
	(void)cart;
	(void)addr;
	(void)val;
}
