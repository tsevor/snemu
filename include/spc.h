#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "dsp.h"

typedef struct SPC {
	union {
		uint16_t ya;
		struct {
			uint8_t a;
			uint8_t y;
		};
	};
	uint8_t x;

	uint8_t sp;
	uint16_t pc;

	union {
		uint8_t raw;
		struct {
			uint8_t c : 1;
			uint8_t z : 1;
			uint8_t i : 1;
			uint8_t h : 1;
			uint8_t b : 1;
			uint8_t p : 1;
			uint8_t v : 1;
			uint8_t n : 1;
		};
	} psw;

	union {
		uint8_t raw;
		struct {
			uint8_t t0  : 1;
			uint8_t t1  : 1;
			uint8_t t2  : 1;
			uint8_t u0  : 1;
			uint8_t c01 : 1;
			uint8_t c23 : 1;
			uint8_t u1  : 1;
			uint8_t brd : 1;
		};
	} ctrl;

	struct {
		uint8_t div;
		uint16_t stage;
		uint8_t count;
	} timers[3];

	uint8_t ram[0x10000];

	uint8_t port_in[4];
	uint8_t port_out[4];

	uint16_t oper_addr_ex;

	bool stopped;

	uint64_t cycles;

	DSP dsp;
} SPC;

// main bus io
void spc_write_io_port(SPC *spc, int idx, uint8_t val);
uint8_t spc_read_io_port(SPC *spc, int idx);

void spc_poweron(SPC *spc);
void spc_reset(SPC *spc);

int spc_step(SPC *spc);
