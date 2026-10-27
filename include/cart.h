#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum MemLayout {
	ML_LOROM   = 0,
	ML_HIROM   = 1,
	ML_S_DD1   = 2,
	ML_SA_1    = 3,
	ML_EXLOROM = 4,
	ML_EXHIROM = 5,
	ML_SPC7110 = 10,
} MemLayout;

typedef enum Chipset {
	CS_ROM             = 0,
	CS_ROM_RAM         = 1,
	CS_ROM_RAM_BAT     = 2,
	CS_ROM_COP         = 3,
	CS_ROM_COP_RAM     = 4,
	CS_ROM_COP_RAM_BAT = 5,
	CS_ROM_COP_BAT     = 6,
} Chipset;

typedef enum Coproc {
	CP_DSP    = 0, // DSP-1, 2, 3, or 4
	CP_GSU    = 1, // superfx
	CP_OBC1   = 2,
	CP_SA_1   = 3,
	CP_S_DD1  = 4,
	CP_S_RTC  = 5,
	CP_OTHER  = 14,
	CP_CUSTOM = 15,
} Coproc;

typedef enum CoprocCustom {
	CP_SPC7110 = 0,
	CP_ST010   = 1,
	CP_ST018   = 2,
	CP_CX4     = 3,
} CoprocCustom;

typedef enum CountryCode {
	CC_JAPAN         = 0,
	CC_NORTH_AMERICA = 1,
	CC_EUROPE        = 2,
	CC_SCANDANAVIA   = 3,
	CC_FINLAND       = 4,
	CC_DENMARK       = 5,
	CC_EUROPE_FRENCH = 6,
	CC_DUTCH         = 7,
	CC_SPANISH       = 8,
	CC_GERMAN        = 9,
	CC_ITALIAN       = 10,
	CC_CHINESE       = 11,
	CC_INDONESIA     = 12,
	CC_SOUTH_KOREA   = 13,
	CC_COMMON        = 14,
	CC_CANADA        = 15,
	CC_BRAZIL        = 16, // also Nintendo Gateway System??
	CC_AUSTRALIA     = 17,
	CC_OTHER_VAR_1   = 18,
	CC_OTHER_VAR_2   = 19,
	CC_OTHER_VAR_3   = 20,
} CountryCode;

typedef struct RomHeader {
	char maker[2];           // ffb0
	char game[4];            // ffb2
	uint8_t reserved[6];     // ffb6
	uint8_t exp_flash_size : 8; // ffbc
	uint8_t exp_ram_size : 8; // ffbd
	uint8_t special_ver : 8; // ffbe
	CoprocCustom cs_sub : 8; // ffbf
	char title[21];          // ffc0
	MemLayout layout    : 4; // ffd5
	bool fastrom        : 1; // -
	uint8_t magic       : 3; // -
	Chipset chipset     : 4; // ffd6
	Coproc coproc       : 4; // -
	uint8_t rom_size    : 8; // ffd7
	uint8_t sram_size   : 8; // ffd8
	CountryCode country : 8; // ffd9
	uint8_t dev_id      : 8; // ffda
	uint8_t rom_ver     : 8; // ffdb
	uint16_t csum_comp  : 16; // ffdc
	uint16_t csum       : 16; // ffde
	uint16_t vec_pad    : 16; // ffe0
	uint16_t vec_pad2   : 16; // ffe2
	uint16_t vec_cop    : 16; // ffe4
	uint16_t vec_brk    : 16; // ffe6
	uint16_t vec_abort  : 16; // ffe8
	uint16_t vec_nmi    : 16; // ffea
	uint16_t vec_unused : 16; // ffec
	uint16_t vec_irq    : 16; // ffee
	uint16_t vece_pad   : 16; // fff0
	uint16_t vece_pad2  : 16; // fff2
	uint16_t vece_cop   : 16; // ffe4
	uint16_t vece_none  : 16; // ffe6
	uint16_t vece_abort : 16; // fff8
	uint16_t vece_nmi   : 16; // fffa
	uint16_t vece_reset : 16; // fffc
	uint16_t vece_irq   : 16; // fffe
} RomHeader;


typedef struct Cart {

	RomHeader header;
	uint32_t header_offset;

	bool bad_header;

	MemLayout layout;
	bool fastrom;
	bool battery;
	bool has_sram;

	uint32_t rom_size;
	uint32_t sram_size;

	uint8_t *rom;
	uint8_t *sram;

} Cart;

bool cart_init(Cart *cart, char *filename);
void cart_free(Cart *cart);

uint8_t cart_read(Cart *cart);
void cart_write(Cart *cart);
