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
	DpXRel,
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
			if (spc->ctrl.brd) {
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

		case DpXRel:
			op8 = spc_read(spc, spc->pc++) + spc->x;
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
			op16 = spc_read16(spc, spc->pc);
			spc->pc += 2;
			return op16;

		case AbsBit:
			op16 = spc_read16(spc, spc->pc);
			spc->pc += 2;
			spc->oper_addr_ex = op16 >> 13;
			return op16 & 0x1fff;

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

void spc_op_jmp(SPC *spc, uint16_t addr);

void spc_branch(SPC *spc, bool condition, uint16_t addr) {
	if (condition) {
		spc_op_jmp(spc, addr);
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

uint8_t spc_op_and(SPC *spc, uint8_t a, uint8_t b) {
	uint8_t res = a & b;
	spc_set_zn(spc, res);
	return res;
}

void spc_op_and1(SPC *spc, uint8_t bit) {
	spc->p.c &= bit;
}

uint8_t spc_op_asl(SPC *spc, uint8_t val) {
	spc->p.c = (val & 0x80) != 0;
	val <<= 1;
	spc_set_zn(spc, val);
	return val;
}

void spc_op_brk(SPC *spc) {
	spc_push16(spc, spc->pc);
	spc_push(spc, spc->p.raw);
	spc->p.b = 1;
	spc->p.i = 0;
	spc_op_jmp(spc, spc_read16(spc, 0xffde));
}

void spc_op_call(SPC *spc, uint16_t addr) {
	spc_push16(spc, spc->pc);
	spc_op_jmp(spc, addr);
}

void spc_op_cmp(SPC *spc, uint8_t a, uint8_t b) {
	spc->p.c = (a >= b);
	spc_set_zn(spc, a - b);
}

void spc_op_cmpw(SPC *spc, uint16_t a, uint16_t b) {
	spc->p.c = (a >= b);
	spc_set_zn16(spc, a - b);
}

void spc_op_daa(SPC *spc) {
	uint8_t src = spc->a;
	if (((src & 0x0f) > 9) || spc->p.h) {
		spc->a += 6;
		if (spc->a < 6) {
			spc->p.c = 1;
		}
	}
	if ((src > 0x99) || spc->p.c) {
		spc->a += 0x60;
		spc->p.c = 1;
	}
	spc_set_zn(spc, spc->a);
}

void spc_op_das(SPC *spc) {
	uint8_t src = spc->a;
	if (!spc->p.h || ((src & 0x0f) > 9)) {
		spc->a -= 6;
	}
	if (!spc->p.c || (src > 0x99)) {
		spc->a -= 0x60;
		spc->p.c = 0;
	}
	spc_set_zn(spc, spc->a);
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

void spc_op_div(SPC *spc) {
	uint32_t ya = spc->ya;
	uint32_t x = (uint32_t)spc->x << 9;
	spc->p.h = ((spc->y & 0x0f) >= (spc->x & 0x0f));
	for (int i = 0; i < 9; i++) {
		ya <<= 1;
		if (ya & 0x20000) ya = (ya & 0x1ffff) | 1;
		if (ya >= x) ya ^= 1;
		if (ya & 1) ya = (ya - x) & 0x1ffff;
	}
	spc->p.v = (ya & 0x100) != 0;
	spc->a = ya & 0xff;
	spc->y = (ya >> 9) & 0xff;
	spc_set_zn(spc, spc->a);
}

uint8_t spc_op_eor(SPC *spc, uint8_t a, uint8_t b) {
	uint8_t res = a ^ b;
	spc_set_zn(spc, res);
	return res;
}

void spc_op_eor1(SPC *spc, uint8_t bit) {
	spc->p.c ^= bit;
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

void spc_op_jmp(SPC *spc, uint16_t addr) {
	spc->pc = addr;
}

uint8_t spc_op_lsr(SPC *spc, uint8_t val) {
	spc->p.c = (val & 0x01) != 0;
	val >>= 1;
	spc_set_zn(spc, val);
	return val;
}

uint8_t spc_op_mov(SPC *spc, uint8_t val) {
	spc_set_zn(spc, val);
	return val;
}

uint8_t spc_op_mov1(SPC *spc, uint8_t val, uint8_t bit) {
	if (spc->p.c) return val | (1 << bit);
	else          return val & ~(1 << bit);
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

void spc_op_mul(SPC *spc) {
    spc->ya = (uint16_t)spc->y * (uint16_t)spc->a;
    spc_set_zn(spc, spc->y); // N and Z are set based on Y only
}

uint8_t spc_op_not1(SPC *spc, uint8_t val, uint8_t bit) {
	(void)spc;
	return val ^ (1 << bit);
}

uint8_t spc_op_or(SPC *spc, uint8_t a, uint8_t b) {
	uint8_t res = a | b;
	spc_set_zn(spc, res);
	return res;
}

void spc_op_or1(SPC *spc, uint8_t bit) {
	spc->p.c |= bit;
}

uint8_t spc_op_rol(SPC *spc, uint8_t val) {
	uint8_t old_c = spc->p.c;
	spc->p.c = (val & 0x80) != 0;
	val = (val << 1) | old_c;
	spc_set_zn(spc, val);
	return val;
}

uint8_t spc_op_ror(SPC *spc, uint8_t val) {
	uint8_t old_c = spc->p.c;
	spc->p.c = (val & 0x01) != 0;
	val = (val >> 1) | (old_c << 7);
	spc_set_zn(spc, val);
	return val;
}

uint8_t spc_op_sbc(SPC *spc, uint8_t a, uint8_t b) {
	int result = a - b - !spc->p.c;

	spc->p.v = (((a ^ b) & (a ^ result) & 0x80)) != 0;
	spc->p.h = (((a & 0x0f) - (b & 0x0f) - !spc->p.c) >= 0);
	spc->p.c = (result >= 0);

	uint8_t res8 = (uint8_t)result;
	spc_set_zn(spc, res8);
	return res8;
}

uint16_t spc_op_subw(SPC *spc, uint16_t a, uint16_t b) {
	int32_t result = (int32_t)a - (int32_t)b;

	spc->p.v = (((a ^ b) & (a ^ result) & 0x8000)) != 0;
	spc->p.h = (((int)(a & 0x0fff) - (int)(b & 0x0fff)) >= 0);
	spc->p.c = (result >= 0);

	uint16_t res16 = (uint16_t)result;
	spc_set_zn16(spc, res16);
	return res16;
}

void spc_op_xcn(SPC *spc) {
	spc->a = (spc->a >> 4) | (spc->a << 4);
	spc_set_zn(spc, spc->a);
}

AddrMode ADDR_MODES[256] = {
	Imp, Imp, Dp, DpBitRel, Dp,  Abs,  Ind,  IndX, Imm,   DpDp, AbsBit, Dp,  Abs, Imp, Abs,      Imp,
	Rel, Imp, Dp, DpBitRel, DpX, AbsX, AbsY, IndY, ImmDp, IpIp, Dp,     DpX, Imp, Imp, Abs,      AbsX,
	Imp, Imp, Dp, DpBitRel, Dp,  Abs,  Ind,  IndX, Imm,   DpDp, AbsBit, Dp,  Abs, Imp, DpBitRel, Rel,
	Rel, Imp, Dp, DpBitRel, DpX, AbsX, AbsY, IndY, ImmDp, IpIp, Dp,     DpX, Imp, Imp, Dp,       Abs,
	Imp, Imp, Dp, DpBitRel, Dp,  Abs,  Ind,  IndX, Imm,   DpDp, AbsBit, Dp,  Abs, Imp, Abs,      PCall,
	Rel, Imp, Dp, DpBitRel, DpX, AbsX, AbsY, IndY, ImmDp, IpIp, Dp,     DpX, Imp, Imp, Abs,      Abs,
	Imp, Imp, Dp, DpBitRel, Dp,  Abs,  Ind,  IndX, Imm,   DpDp, AbsBit, Dp,  Abs, Imp, DpBitRel, Imp,
	Rel, Imp, Dp, DpBitRel, DpX, AbsX, AbsY, IndY, ImmDp, IpIp, Dp,     DpX, Imp, Imp, Dp,       Imp,
	Imp, Imp, Dp, DpBitRel, Dp,  Abs,  Ind,  IndX, Imm,   DpDp, AbsBit, Dp,  Abs, Imm, Imp,      ImmDp,
	Rel, Imp, Dp, DpBitRel, DpX, AbsX, AbsY, IndY, ImmDp, IpIp, Dp,     DpX, Imp, Imp, Imp,      Imp,
	Imp, Imp, Dp, DpBitRel, Dp,  Abs,  Ind,  IndX, Imm,   DpDp, AbsBit, Dp,  Abs, Imm, Imp,      IndXi,
	Rel, Imp, Dp, DpBitRel, DpX, AbsX, AbsY, IndY, ImmDp, IpIp, Dp,     DpX, Imp, Imp, Imp,      IndXi,
	Imp, Imp, Dp, DpBitRel, Dp,  Abs,  Ind,  IndX, Imm,   Abs,  AbsBit, Dp,  Abs, Imm, Imp,      Imp,
	Rel, Imp, Dp, DpBitRel, DpX, AbsX, AbsY, IndY, Dp,    DpY,  Dp,     DpX, Imp, Imp, DpXRel,   Imp,
	Imp, Imp, Dp, DpBitRel, Dp,  Abs,  Ind,  IndX, Imm,   Abs,  AbsBit, Dp,  Abs, Imp, Imp,      Imp,
	Rel, Imp, Dp, DpBitRel, DpX, AbsX, AbsY, IndY, Dp,    DpY,  DpDp,   DpX, Imp, Imp, Rel,      Imp,
};

void spc_op_switch(SPC *spc, uint8_t opcode) {
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

		case 0x7a:
			spc->ya = spc_op_addw(spc, spc->ya, spc_read16wrap(spc, addr));
			break;

		case 0x89:
		case 0x98:
		case 0x99:
			src = spc_read(spc, spc->oper_addr_ex);
			dst = spc_read(spc, addr);
			spc_write(spc, addr, spc_op_adc(spc, dst, src));
			break;

		// and
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x28:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
			spc->a = spc_op_and(spc, spc->a, spc_read(spc, addr));
			break;

		case 0x29:
		case 0x38:
		case 0x39:
			src = spc_read(spc, spc->oper_addr_ex);
			dst = spc_read(spc, addr);
			spc_write(spc, addr, spc_op_and(spc, dst, src));
			break;

		// asl
		case 0x1c:
			spc->a = spc_op_asl(spc, spc->a);
			break;

		case 0x0b:
		case 0x1b:
		case 0x0c:
			spc_write(spc, addr, spc_op_asl(spc, spc_read(spc, addr)));
			break;

		// bbc
		case 0x13:
		case 0x33:
		case 0x53:
		case 0x73:
		case 0x93:
		case 0xb3:
		case 0xd3:
		case 0xf3:
			spc_branch(spc, (spc_read(spc, addr) & (1 << (opcode >> 5))) == 0, spc->oper_addr_ex);
			break;

		// bbs
		case 0x03:
		case 0x23:
		case 0x43:
		case 0x63:
		case 0x83:
		case 0xa3:
		case 0xc3:
		case 0xe3:
			spc_branch(spc, (spc_read(spc, addr) & (1 << (opcode >> 5))) != 0, spc->oper_addr_ex);
			break;

		// bit logic
		case 0x0a:
			spc_op_or1(spc, (spc_read(spc, addr) >> spc->oper_addr_ex) & 1);
			break;

		case 0x2a:
			spc_op_or1(spc, !((spc_read(spc, addr) >> spc->oper_addr_ex) & 1));
			break;

		case 0x4a:
			spc_op_and1(spc, (spc_read(spc, addr) >> spc->oper_addr_ex) & 1);
			break;

		case 0x6a:
			spc_op_and1(spc, !((spc_read(spc, addr) >> spc->oper_addr_ex) & 1));
			break;

		case 0x8a:
			spc_op_eor1(spc, (spc_read(spc, addr) >> spc->oper_addr_ex) & 1);
			break;

		case 0xaa:
			spc->p.c = (spc_read(spc, addr) >> spc->oper_addr_ex) & 1;
			break;

		case 0xca:
			spc_write(spc, addr, spc_op_mov1(spc, spc_read(spc, addr), spc->oper_addr_ex));
			break;

		case 0xea:
			spc_write(spc, addr, spc_op_not1(spc, spc_read(spc, addr), spc->oper_addr_ex));
			break;

		// branches
		case 0x10:
			spc_branch(spc, !spc->p.n, addr);
			break;

		case 0x2f:
			spc_branch(spc, true, addr);
			break;

		case 0x30:
			spc_branch(spc, spc->p.n, addr);
			break;

		case 0x50:
			spc_branch(spc, !spc->p.v, addr);
			break;

		case 0x70:
			spc_branch(spc, spc->p.v, addr);
			break;

		case 0x90:
			spc_branch(spc, !spc->p.c, addr);
			break;

		case 0xb0:
			spc_branch(spc, spc->p.c, addr);
			break;

		case 0xd0:
			spc_branch(spc, !spc->p.z, addr);
			break;

		case 0xf0:
			spc_branch(spc, spc->p.z, addr);
			break;

		// call
		case 0x3f:
		case 0x4f:
			spc_op_call(spc, addr);
			break;

		case 0x01:
		case 0x11:
		case 0x21:
		case 0x31:
		case 0x41:
		case 0x51:
		case 0x61:
		case 0x71:
		case 0x81:
		case 0x91:
		case 0xa1:
		case 0xb1:
		case 0xc1:
		case 0xd1:
		case 0xe1:
		case 0xf1:
			spc_op_call(spc, spc_read16(spc, 0xffde - (opcode >> 3)));
			break;

		// cbne
		case 0x2e:
		case 0xde:
			spc_branch(spc, spc->a != spc_read(spc, addr), spc->oper_addr_ex);
			break;

		// clr1
		case 0x12:
		case 0x32:
		case 0x52:
		case 0x72:
		case 0x92:
		case 0xb2:
		case 0xd2:
		case 0xf2:
			dst = spc_read(spc, addr);
			dst &= ~(1 << (opcode >> 5));
			spc_write(spc, addr, dst);
			break;

		// cmp
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
			spc_op_cmp(spc, spc->a, spc_read(spc, addr));
			break;

		case 0xc8:
		case 0x3e:
		case 0x1e:
			spc_op_cmp(spc, spc->x, spc_read(spc, addr));
			break;

		case 0xad:
		case 0x7e:
		case 0x5e:
			spc_op_cmp(spc, spc->y, spc_read(spc, addr));
			break;

		case 0x78:
		case 0x69:
		case 0x79:
			src = spc_read(spc, spc->oper_addr_ex);
			dst = spc_read(spc, addr);
			spc_op_cmp(spc, dst, src);
			break;

		case 0x5a:
			spc_op_cmpw(spc, spc->ya, spc_read16wrap(spc, addr));
			break;

		// daa
		case 0xdf:
			spc_op_daa(spc);
			break;

		// das
		case 0xbe:
			spc_op_das(spc);
			break;

		// dbnz
		case 0x6e:
			src = spc_read(spc, addr) - 1;
			spc_write(spc, addr, src);
			spc_branch(spc, src != 0, spc->oper_addr_ex);
			break;

		case 0xfe:
			spc->y--;
			spc_branch(spc, spc->y != 0, addr);
			break;

		// dec
		case 0x8b:
		case 0x8c:
		case 0x9b:
			spc_write(spc, addr, spc_op_dec(spc, spc_read(spc, addr)));
			break;

		case 0x1a:
			spc_write16wrap(spc, addr, spc_op_decw(spc, spc_read16wrap(spc, addr)));
			break;

		case 0x9c:
			spc->a = spc_op_dec(spc, spc->a);
			break;

		case 0x1d:
			spc->x = spc_op_dec(spc, spc->x);
			break;

		case 0xdc:
			spc->y = spc_op_dec(spc, spc->y);
			break;

		// div
		case 0x9e:
			spc_op_div(spc);
			break;

		// eor
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
		case 0x48:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
			spc->a = spc_op_eor(spc, spc->a, spc_read(spc, addr));
			break;

		case 0x49:
		case 0x58:
		case 0x59:
			src = spc_read(spc, spc->oper_addr_ex);
			dst = spc_read(spc, addr);
			spc_write(spc, addr, spc_op_eor(spc, dst, src));
			break;

		// flags
		case 0x20:
			spc->p.p = 0;
			break;

		case 0x40:
			spc->p.p = 1;
			break;

		case 0x60:
			spc->p.c = 0;
			break;

		case 0x80:
			spc->p.c = 1;
			break;

		case 0xed:
			spc->p.c = !spc->p.c;
			break;

		case 0xe0:
			spc->p.v = 0;
			spc->p.h = 0;
			break;

		case 0xa0:
			spc->p.i = 1;
			break;

		case 0xc0:
			spc->p.i = 0;
			break;

		// inc
		case 0xab:
		case 0xac:
		case 0xbb:
			spc_write(spc, addr, spc_op_inc(spc, spc_read(spc, addr)));
			break;

		case 0x3a:
			spc_write16wrap(spc, addr, spc_op_incw(spc, spc_read16wrap(spc, addr)));
			break;

		case 0xbc:
			spc->a = spc_op_inc(spc, spc->a);
			break;

		case 0x3d:
			spc->x = spc_op_inc(spc, spc->x);
			break;

		case 0xfc:
			spc->y = spc_op_inc(spc, spc->y);
			break;

		// jmp
		case 0x1f:
			spc_op_jmp(spc, spc_read16(spc, addr));
			break;

		case 0x5f:
			spc_op_jmp(spc, addr);
			break;

		// lsr
		case 0x5c:
			spc->a = spc_op_lsr(spc, spc->a);
			break;

		case 0x4b:
		case 0x5b:
		case 0x4c:
			spc_write(spc, addr, spc_op_lsr(spc, spc_read(spc, addr)));
			break;

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

		case 0xda:
			spc_write16wrap(spc, addr, spc_op_movw_nf(spc, spc->ya));
			break;

		case 0xba:
			spc->ya = spc_op_movw(spc, spc_read16wrap(spc, addr));
			break;

		case 0x7d:
			spc->a = spc_op_mov(spc, spc->x);
			break;

		case 0x5d:
			spc->x = spc_op_mov(spc, spc->a);
			break;

		case 0xdd:
			spc->a = spc_op_mov(spc, spc->y);
			break;

		case 0xfd:
			spc->y = spc_op_mov(spc, spc->a);
			break;

		case 0x9d:
			spc->x = spc_op_mov(spc, spc->sp);
			break;

		case 0xbd:
			spc->sp = spc_op_mov_nf(spc, spc->x);
			break;

		// mul
		case 0xcf:
			spc_op_mul(spc);
			break;

		// or
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			spc->a = spc_op_or(spc, spc->a, spc_read(spc, addr));
			break;

		case 0x09:
		case 0x18:
		case 0x19:
			src = spc_read(spc, spc->oper_addr_ex);
			dst = spc_read(spc, addr);
			spc_write(spc, addr, spc_op_or(spc, dst, src));
			break;

		// pop
		case 0x8e:
			spc->p.raw = spc_pull(spc);
			break;

		case 0xae:
			spc->a = spc_pull(spc);
			break;

		case 0xce:
			spc->x = spc_pull(spc);
			break;

		case 0xee:
			spc->y = spc_pull(spc);
			break;

		// push
		case 0x0d:
			spc_push(spc, spc->p.raw);
			break;

		case 0x2d:
			spc_push(spc, spc->a);
			break;

		case 0x4d:
			spc_push(spc, spc->x);
			break;

		case 0x6d:
			spc_push(spc, spc->y);
			break;

		// ret
		case 0x7f:
			spc->p.raw = spc_pull(spc);
			// fallthrough
		case 0x6f:
			spc->pc = spc_pull16(spc);
			break;

		// rol
		case 0x3c:
			spc->a = spc_op_rol(spc, spc->a);
			break;

		case 0x2b:
		case 0x3b:
		case 0x2c:
			spc_write(spc, addr, spc_op_rol(spc, spc_read(spc, addr)));
			break;

		// ror
		case 0x7c:
			spc->a = spc_op_ror(spc, spc->a);
			break;

		case 0x6b:
		case 0x7b:
		case 0x6c:
			spc_write(spc, addr, spc_op_ror(spc, spc_read(spc, addr)));
			break;

		// sbc
		case 0xa4:
		case 0xa5:
		case 0xa6:
		case 0xa7:
		case 0xa8:
		case 0xb4:
		case 0xb5:
		case 0xb6:
		case 0xb7:
			spc->a = spc_op_sbc(spc, spc->a, spc_read(spc, addr));
			break;

		case 0x9a:
			spc->ya = spc_op_subw(spc, spc->ya, spc_read16wrap(spc, addr));
			break;

		case 0xa9:
		case 0xb8:
		case 0xb9:
			src = spc_read(spc, spc->oper_addr_ex);
			dst = spc_read(spc, addr);
			spc_write(spc, addr, spc_op_sbc(spc, dst, src));
			break;

		// set1
		case 0x02:
		case 0x22:
		case 0x42:
		case 0x62:
		case 0x82:
		case 0xa2:
		case 0xc2:
		case 0xe2:
			dst = spc_read(spc, addr);
			dst |= (1 << (opcode >> 5));
			spc_write(spc, addr, dst);
			break;

		// tclr1
		case 0x4e:
			dst = spc_read(spc, addr);
			spc_set_zn(spc, spc->a - dst);
			spc_write(spc, addr, dst & ~spc->a);
			break;

		// tset1
		case 0x0e:
			dst = spc_read(spc, addr);
			spc_set_zn(spc, spc->a - dst);
			spc_write(spc, addr, dst | spc->a);
			break;

		// xcn
		case 0x9f:
			spc_op_xcn(spc);
			break;

		// system
		case 0x00:
			break;

		case 0x0f:
			spc_op_brk(spc);
			break;

		case 0xef: // wait, but this spc700 has no hardware interrupts to resume it
		case 0xff:
			spc->stopped = true;
			break;

		default:
			exit(1);
	}
}

void spc_reset(SPC *spc) {
	spc->stopped = false;

	spc->ctrl.raw = 0x80;

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
		// printf("spc: %02x\n", opcode);
		spc_op_switch(spc, opcode);
	}


	return spc->cycles - cycles_before;
}
