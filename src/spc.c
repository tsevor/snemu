#include "spc.h"
#include "log.h"

#include <stdint.h>
#include <stdlib.h>

static const uint8_t SPC_BOOTROM[64] = {
	0xcd, 0xef, 0xbd, 0xe8, 0x00, 0xc6, 0x1d, 0xd0,
	0xfc, 0x8f, 0xaa, 0xf4, 0x8f, 0xbb, 0xf5, 0x78,
	0xcc, 0xf4, 0xd0, 0xfb, 0x2f, 0x19, 0xeb, 0xf4,
	0xd0, 0xfc, 0x7e, 0xf4, 0xd0, 0x0b, 0xe4, 0xf5,
	0xcb, 0xf4, 0xd7, 0x00, 0xfc, 0xd0, 0xf3, 0xab,
	0x01, 0x10, 0xef, 0x7e, 0xf4, 0x10, 0xeb, 0xba,
	0xf6, 0xda, 0x00, 0xba, 0xf4, 0xc4, 0xf4, 0xdd,
	0x5d, 0xd0, 0xdb, 0x1f, 0x00, 0x00, 0xc0, 0xff,
};

uint8_t spc_read_io_port(SPC *spc, int idx) {
	return spc->port_out[idx];
};

void spc_write_io_port(SPC *spc, int idx, uint8_t val) {
	if (spc->ctrl.c01 && (idx & 2)) return;
	else if (spc->ctrl.c23) return;
	spc->port_in[idx] = val;
};

void spc_tick_timers(SPC *spc) {
	// called every 16 cycles

	// 8 khz timers
	if ((spc->cycles % 128) == 0) {
		if (spc->ctrl.t0) {
			spc->timers[0].stage++;
			uint16_t target = spc->timers[0].div == 0 ? 256 : spc->timers[0].div;
			if (spc->timers[0].stage >= target) {
				spc->timers[0].stage = 0;
				spc->timers[0].count = (spc->timers[0].count + 1) & 0x0F;
			}
		}
		if (spc->ctrl.t1) {
			spc->timers[1].stage++;
			uint16_t target = spc->timers[1].div == 0 ? 256 : spc->timers[1].div;
			if (spc->timers[1].stage >= target) {
				spc->timers[1].stage = 0;
				spc->timers[1].count = (spc->timers[1].count + 1) & 0x0F;
			}
		}
	}

	// 64 khz timer
	if (spc->ctrl.t2) { // && (spc->cycles % 16) == 0, but guaranteed
		spc->timers[2].stage++;
		uint16_t target = spc->timers[2].div == 0 ? 256 : spc->timers[2].div;

		if (spc->timers[2].stage >= target) {
			spc->timers[2].stage = 0;
			spc->timers[2].count = (spc->timers[2].count + 1) & 0x0F;
		}
	}
}

void spc_clock(SPC *spc) {
	spc->cycles++;
	if (spc->cycles % 16) spc_tick_timers(spc);
}

uint8_t spc_read(SPC *spc, uint16_t addr) {
	uint8_t val;
	switch (addr) {
		case 0x00f4 ... 0x00f7: // cpuio0-3
			val = spc->port_in[addr & 3];
			break;
		case 0x00fd:
			val = spc->timers[0].count;
			spc->timers[0].count = 0;
			break;
		case 0x00fe:
			val = spc->timers[1].count;
			spc->timers[1].count = 0;
			break;
		case 0x00ff:
			val = spc->timers[2].count;
			spc->timers[2].count = 0;
			break;
		case 0xffc0 ... 0xffff:
			if (!spc->ctrl.brd) {
				val = SPC_BOOTROM[addr & 0x3f];
				break;
			}
			// fallthrough
		default: // ram
			val = spc->ram[addr];
	}
	spc_clock(spc);

	return val;
}

void spc_write(SPC *spc, uint16_t addr, uint8_t val) {
	switch (addr) {
		case 0x00f0:
			// TODO: test
			break;
		case 0x00f1: // control
			spc->ctrl.raw = val;
			if (spc->ctrl.c01) {
				spc->port_in[0] = 0;
				spc->port_in[1] = 0;
			}
			if (spc->ctrl.c23) {
				spc->port_in[2] = 0;
				spc->port_in[3] = 0;
			}
			if (!spc->ctrl.t0) spc->timers[0].count = 0;
			if (!spc->ctrl.t1) spc->timers[1].count = 0;
			if (!spc->ctrl.t2) spc->timers[2].count = 0;
			break;
		case 0x00f2:
			// TODO: dspaddr
			break;
		case 0x00f3:
			// TODO: dspdata
			break;
		case 0x00f4 ... 0x00f7: // cpuio0-3
			spc->port_out[addr & 3] = val;
			break;
		case 0x00fa:
			spc->timers[0].div = val;
			break;
		case 0x00fb:
			spc->timers[1].div = val;
			break;
		case 0x00fc:
			spc->timers[2].div = val;
			break;
	}
	spc->ram[addr] = val;

	spc_clock(spc);
}

uint16_t spc_read16(SPC *spc, uint16_t addr) {
	uint8_t lo = spc_read(spc, addr);
	uint8_t hi = spc_read(spc, addr + 1);
	return hi << 8 | lo;
}







void spc_op_switch(SPC *spc, uint8_t opcode) {
	(void)spc;
	switch (opcode) {
		default:
			lerror("spc: unimplemented opcode: %02x\n", opcode);
			exit(1);
	}
}

void spc_reset(SPC *spc) {
	spc->ctrl.raw = 0;
	spc->pc = spc_read16(spc, 0xfffe);
}

void spc_poweron(SPC *spc) {
	spc->sp = 0xff;
	spc_reset(spc);
}

void spc_step(SPC *spc) {

	uint8_t opcode = spc_read(spc, spc->pc++);

	spc_op_switch(spc, opcode);
}
