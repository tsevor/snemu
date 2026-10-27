#include "cart.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

bool cart_init(Cart *cart, char *filename) {

	FILE *file = fopen(filename, "rb");

	if (!file) {
		lerror("cart: file doesn't exist\n");
		return false;
	}

	fseek(file, 0, SEEK_END);
	size_t file_size = ftell(file);

	ldeb1("cart: file size: %d\n", (int)file_size);

	if (file_size < 256 << 10) {
		// less than minimum
		lerror("cart: file too small\n");
		fclose(file);
		return false;
	}

	size_t copier_offset = (file_size % 1024 == 512) ? 512 : 0;

	file_size -= copier_offset;

	if (copier_offset > 0) {
		linfo("cart: detected 512-byte copier header\n");
	}

	uint16_t checksum_buf[4];
	// try lorom
	fseek(file, 0x7fdc + copier_offset, SEEK_SET);
	fread(checksum_buf, 2, 2, file);
	// try hirom
	fseek(file, 0xffdc + copier_offset, SEEK_SET);
	fread(checksum_buf + 2, 2, 2, file);

	bool is_hirom;
	if ((checksum_buf[0] ^ checksum_buf[1]) == 0xffff) { // lorom
		ldeb1("cart: valid checksum proves lorom\n");
		is_hirom = false;
	} else if ((checksum_buf[2] ^ checksum_buf[3]) == 0xffff) { // hirom
		ldeb1("cart: valid checksum proves hirom\n");
		is_hirom = true;
	} else {
		lerror("cart: file checksum not found\n");
		fclose(file);
		return false;
	}

	fseek(file, (is_hirom ? 0xffc0 : 0x7fc0) + copier_offset, SEEK_SET);
	fread(cart->header.raw, 1, 64, file);

	if (cart->header.dev_id == 0xee) {
		ldeb1("cart: has expanded header");
		fseek(file, (is_hirom ? 0xffb0 : 0x7fb0) + copier_offset, SEEK_SET);
		fread(cart->header_expanded.raw, 1, 16, file);
	}

	size_t rom_size = (1 << 10) << cart->header.rom_size;
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
		// copy small bank until size of large bank
		for (; small_bank < large_bank; small_bank <<= 1) {
			memcpy(
				cart->rom + large_bank + small_bank,
				cart->rom + large_bank,
				small_bank
			);
		}
	}

	fclose(file);

	linfo("cart: name: %.*s\n", 21, cart->header.title);

	return true;
}

void cart_free(Cart *cart);

uint8_t cart_read(Cart *cart);
void cart_write(Cart *cart);
