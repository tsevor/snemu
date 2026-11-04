#include "spc.h"
#include "log.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum AddrMode {
	None = 0,
	Imp,
	Imm,
	Rel,
	Dp,
	DpX,
	DpY,
	Ind,
	IndXi,
	DpDp,
	IpIp,
	ImmDp,
	DpBit,
	DpBitRel,
	AbsBit,
	Abs,
	AbsX,
	AbsY,
	IndX,
	IndY,
	PCall,
} AddrMode;

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
	if (spc->ctrl.c23 && (idx & 2)) return;
	else if (spc->ctrl.c01) return;
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
				spc->timers[0].count = (spc->timers[0].count + 1) & 0x0f;
			}
		}
		if (spc->ctrl.t1) {
			spc->timers[1].stage++;
			uint16_t target = spc->timers[1].div == 0 ? 256 : spc->timers[1].div;
			if (spc->timers[1].stage >= target) {
				spc->timers[1].stage = 0;
				spc->timers[1].count = (spc->timers[1].count + 1) & 0x0f;
			}
		}
	}

	// 64 khz timer
	if (spc->ctrl.t2) { // && (spc->cycles % 16) == 0, but guaranteed
		spc->timers[2].stage++;
		uint16_t target = spc->timers[2].div == 0 ? 256 : spc->timers[2].div;

		if (spc->timers[2].stage >= target) {
			spc->timers[2].stage = 0;
			spc->timers[2].count = (spc->timers[2].count + 1) & 0x0f;
		}
	}
}

void spc_clock(SPC *spc) {
	spc->cycles++;
	if (spc->cycles % 16 == 0) spc_tick_timers(spc);
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

uint16_t spc_read16wrap(SPC *spc, uint16_t addr) {
	uint8_t lo = spc_read(spc, addr);
	uint8_t hi = spc_read(spc, (addr & 0xff00) | ((addr + 1) & 0xff));
	return hi << 8 | lo;
}

void spc_write16(SPC *spc, uint16_t addr, uint16_t val) {
	spc_write(spc, addr, (uint8_t)val);
	spc_write(spc, addr + 1, (uint8_t)(val >> 8));
}

void spc_write16wrap(SPC *spc, uint16_t addr, uint16_t val) {
	spc_write(spc, addr, (uint8_t)val);
	spc_write(spc, (addr & 0xff00) | ((addr + 1) & 0xff), (uint8_t)(val >> 8));
}

uint8_t spc_pull(SPC *spc) {
	return spc_read(spc, 0x0100 | ++spc->sp);
}

void spc_push(SPC *spc, uint8_t val) {
	spc_write(spc, 0x0100 | spc->sp--, val);
}

uint16_t spc_pull16(SPC *spc) {
	uint16_t val = spc_pull(spc);
	val |= spc_pull(spc) << 8;
	return val;
}

void spc_push16(SPC *spc, uint16_t val) {
	spc_push(spc, val >> 8);
	spc_push(spc, val);
}

void spc_set_zn(SPC *spc, uint8_t val) {
	spc->p.z = (val == 0);
	spc->p.n = (val & 0x80) != 0;
}

void spc_set_zn16(SPC *spc, uint16_t val) {
	spc->p.z = (val == 0);
	spc->p.n = (val & 0x8000) != 0;
}

uint16_t spc_oper_addr(SPC *spc, AddrMode mode) {
	uint16_t dp_base = spc->p.p ? 0x0100 : 0x0000;
	uint8_t op8;
	uint16_t op16;
	int8_t offs;

	switch (mode) {
		case Dp:
		case DpBit:
			op8 = spc_read(spc, spc->pc++);
			return dp_base | op8;

		case DpBitRel:
			op8 = spc_read(spc, spc->pc++);
			offs = (int8_t)spc_read(spc, spc->pc);
			spc->oper_addr_ex = ++spc->pc + offs;
			return dp_base | op8;

		case DpX:
			op8 = spc_read(spc, spc->pc++) + spc->x;
			return dp_base | op8;

		case DpY:
			op8 = spc_read(spc, spc->pc++) + spc->y;
			return dp_base | op8;

		case Ind:
			return dp_base | spc->x;

		case IndX:
			op8 = spc_read(spc, spc->pc++) + spc->x;
			return spc_read16wrap(spc, dp_base | op8);

		case IndXi:
			return dp_base | spc->x++;

		case IndY:
			op8 = spc_read(spc, spc->pc++);
			return spc_read16wrap(spc, dp_base | op8) + spc->y;

		case Abs:
		case AbsBit:
			op16 = spc_read16(spc, spc->pc);
			spc->pc += 2;
			return op16;

		case AbsX:
			op16 = spc_read16(spc, spc->pc);
			spc->pc += 2;
			return op16 + spc->x;

		case AbsY:
			op16 = spc_read16(spc, spc->pc);
			spc->pc += 2;
			return op16 + spc->y;

		case Imm:
			return spc->pc++;

		case Rel:
			offs = (int8_t)spc_read(spc, spc->pc);
			return ++spc->pc + offs;

		case DpDp:
			spc->oper_addr_ex = dp_base | spc_read(spc, spc->pc++);
			return dp_base | spc_read(spc, spc->pc++);

		case IpIp:
			spc->oper_addr_ex = dp_base | spc->y;
			return dp_base | spc->x;

		case ImmDp:
			spc->oper_addr_ex = spc->pc++;
			return dp_base | spc_read(spc, spc->pc++);

		case PCall:
			return 0xff00 | spc_read(spc, spc->pc++);

		case None:
		case Imp:
		default:
			return 0;
	}
}

uint8_t spc_op_adc(SPC *spc, uint8_t a, uint8_t b) {
	int result = a + b + spc->p.c;

	spc->p.v = ((~(a ^ b) & (a ^ result) & 0x80)) != 0;
	spc->p.h = (((a & 0x0f) + (b & 0x0f) + spc->p.c) > 0x0f) != 0;
	spc->p.c = (result > 0xff) != 0;

	uint8_t res8 = (uint8_t)result;
	spc_set_zn(spc, res8);
	return res8;
}

uint16_t spc_op_addw(SPC *spc, uint16_t a, uint16_t b) {
	uint32_t result = a + b;

	spc->p.v = ((~(a ^ b) & (a ^ result) & 0x8000)) != 0;
	spc->p.h = (((a & 0x0fff) + (b & 0x0fff)) > 0x0fff) != 0;
	spc->p.c = (result > 0xffff) != 0;

	uint16_t res16 = (uint16_t)result;
	spc_set_zn16(spc, res16);
	return res16;
}

uint8_t spc_op_inc(SPC *spc, uint8_t val) {
	val++;
	spc_set_zn(spc, val);
	return val;
}

uint16_t spc_op_incw(SPC *spc, uint16_t val) {
	val++;
	spc_set_zn16(spc, val);
	return val;
}

uint8_t spc_op_dec(SPC *spc, uint8_t val) {
	val--;
	spc_set_zn(spc, val);
	return val;
}

uint16_t spc_op_decw(SPC *spc, uint16_t val) {
	val--;
	spc_set_zn16(spc, val);
	return val;
}

uint8_t spc_op_mov(SPC *spc, uint8_t val) {
	spc_set_zn(spc, val);
	return val;
}

uint8_t spc_op_mov_nf(SPC *spc, uint8_t val) {
	(void)spc;
	return val;
}

uint16_t spc_op_movw(SPC *spc, uint16_t val) {
	spc_set_zn16(spc, val);
	return val;
}

uint16_t spc_op_movw_nf(SPC *spc, uint16_t val) {
	(void)spc;
	return val;
}

AddrMode ADDR_MODES[256] = {
	[0x1a] = Dp,
	[0x1d] = Imp,
	[0x3a] = Dp,
	[0x3d] = Imp,
	[0x5d] = Imp,
	[0x7a] = Dp,
	[0x7d] = Imp,
	[0x84] = Dp,
	[0x85] = Abs,
	[0x86] = Ind,
	[0x87] = IndX,
	[0x88] = Imm,
	[0x89] = DpDp,
	[0x8b] = Dp,
	[0x8c] = Abs,
	[0x8d] = Imm,
	[0x8f] = ImmDp,
	[0x94] = DpX,
	[0x95] = AbsX,
	[0x96] = AbsY,
	[0x97] = IndY,
	[0x98] = ImmDp,
	[0x99] = IpIp,
	[0x9b] = DpX,
	[0x9c] = Imp,
	[0x9d] = Imp,
	[0xab] = Dp,
	[0xac] = Abs,
	[0xaf] = IndXi,
	[0xba] = Dp,
	[0xbb] = DpX,
	[0xbc] = Imp,
	[0xbd] = Imp,
	[0xbf] = IndXi,
	[0xc4] = Dp,
	[0xc5] = Abs,
	[0xc6] = Ind,
	[0xc7] = IndX,
	[0xc9] = Abs,
	[0xcb] = Dp,
	[0xcc] = Abs,
	[0xcd] = Imm,
	[0xd4] = DpX,
	[0xd5] = AbsX,
	[0xd6] = AbsY,
	[0xd7] = IndY,
	[0xd8] = Dp,
	[0xd9] = DpY,
	[0xda] = Dp,
	[0xdb] = DpX,
	[0xdc] = Imp,
	[0xdd] = Imp,
	[0xe4] = Dp,
	[0xe5] = Abs,
	[0xe6] = Ind,
	[0xe7] = IndX,
	[0xe8] = Imm,
	[0xe9] = Abs,
	[0xeb] = Dp,
	[0xec] = Abs,
	[0xf4] = DpX,
	[0xf5] = AbsX,
	[0xf6] = AbsY,
	[0xf7] = IndY,
	[0xf8] = Dp,
	[0xf9] = DpY,
	[0xfa] = DpDp,
	[0xfb] = DpX,
	[0xfc] = Imp,
	[0xfd] = Imp,
};

void spc_op_switch(SPC *spc, uint8_t opcode) {
	ldeb1("spc: %02x\n", opcode);
	uint16_t addr = spc_oper_addr(spc, ADDR_MODES[opcode]);
	uint8_t dst;
	uint8_t src;
	switch (opcode) {
		// adc
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		case 0x88:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
			spc->a = spc_op_adc(spc, spc->a, spc_read(spc, addr));
			break;

		case 0x7a: spc->ya = spc_op_addw(spc, spc->ya, spc_read16wrap(spc, addr));
			break;

		case 0x89:
		case 0x98:
		case 0x99:
			src = spc_read(spc, spc->oper_addr_ex);
			dst = spc_read(spc, addr);
			spc_write(spc, addr, spc_op_adc(spc, dst, src));
			break;

		// inc
		case 0xab:
		case 0xac:
		case 0xbb:
			spc_write(spc, addr, spc_op_inc(spc, spc_read(spc, addr)));
			break;

		case 0x3a: spc_write16wrap(spc, addr, spc_op_incw(spc, spc_read16wrap(spc, addr)));
			break;

		case 0xbc: spc->a = spc_op_inc(spc, spc->a); break;
		case 0x3d: spc->x = spc_op_inc(spc, spc->x); break;
		case 0xfc: spc->y = spc_op_inc(spc, spc->y); break;

		// dec
		case 0x8b:
		case 0x8c:
		case 0x9b:
			spc_write(spc, addr, spc_op_dec(spc, spc_read(spc, addr)));
			break;

		case 0x1a: spc_write16wrap(spc, addr, spc_op_decw(spc, spc_read16wrap(spc, addr))); break;

		case 0x9c: spc->a = spc_op_dec(spc, spc->a); break;
		case 0x1d: spc->x = spc_op_dec(spc, spc->x); break;
		case 0xdc: spc->y = spc_op_dec(spc, spc->y); break;

		// mov
		case 0xe4:
		case 0xe5:
		case 0xe6:
		case 0xe7:
		case 0xe8:
		case 0xf4:
		case 0xf5:
		case 0xf6:
		case 0xf7:
		case 0xbf:
			spc->a = spc_op_mov(spc, spc_read(spc, addr));
			break;

		case 0xcd:
		case 0xf8:
		case 0xe9:
		case 0xf9:
			spc->x = spc_op_mov(spc, spc_read(spc, addr));
			break;

		case 0x8d:
		case 0xeb:
		case 0xfb:
		case 0xec:
			spc->y = spc_op_mov(spc, spc_read(spc, addr));
			break;

		case 0xc4:
		case 0xc5:
		case 0xc6:
		case 0xc7:
		case 0xd4:
		case 0xd5:
		case 0xd6:
		case 0xd7:
		case 0xaf:
			spc_write(spc, addr, spc_op_mov_nf(spc, spc->a));
			break;

		case 0xd8:
		case 0xc9:
		case 0xd9:
			spc_write(spc, addr, spc_op_mov_nf(spc, spc->x));
			break;

		case 0xcb:
		case 0xdb:
		case 0xcc:
			spc_write(spc, addr, spc_op_mov_nf(spc, spc->y));
			break;

		case 0xfa:
		case 0x8f:
			spc_write(spc, addr, spc_op_mov_nf(spc, spc_read(spc, spc->oper_addr_ex)));
			break;

		case 0xda: spc_write16wrap(spc, addr, spc_op_movw_nf(spc, spc->ya)); break;

		case 0xba: spc->ya = spc_op_movw(spc, spc_read16wrap(spc, addr)); break;
		case 0x7d: spc->a = spc_op_mov(spc, spc->x); break;
		case 0x5d: spc->x = spc_op_mov(spc, spc->a); break;
		case 0xdd: spc->a = spc_op_mov(spc, spc->y); break;
		case 0xfd: spc->y = spc_op_mov(spc, spc->a); break;
		case 0x9d: spc->x = spc_op_mov(spc, spc->sp); break;
		case 0xbd: spc->sp = spc_op_mov_nf(spc, spc->x); break;

		default:
			exit(1);
	}
}

void spc_reset(SPC *spc) {
	spc->stopped = false;

	spc->ctrl.raw = 0;

	for (int i = 0; i < 3; i++) {
		spc->timers[i].stage = 0;
		spc->timers[i].count = 0;
	}

	spc->pc = spc_read16(spc, 0xfffe);
}

void spc_poweron(SPC *spc) {
	spc->sp = 0xff;
	spc_reset(spc);
}

int spc_step(SPC *spc) {
	uint64_t cycles_before = spc->cycles;

	if (spc->stopped) {
		spc_clock(spc);
	} else {
		uint8_t opcode = spc_read(spc, spc->pc++);
		spc_op_switch(spc, opcode);
	}


	return spc->cycles - cycles_before;
}
