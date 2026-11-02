#include "cpu.h"

#include "bus.h"
#include "host.h"
#include "log.h"

typedef enum AddrMode {
	Imp,
	ImmM,
	ImmXY,
	Imm8,
	Rel,
	RelL,
	Dp,
	DpX,
	DpY,
	DpInd,
	DpXInd,
	DpIndY,
	DpIndL,
	DpIndLY,
	Abs,
	AbsX,
	AbsY,
	AbsL,
	AbsLX,
	Sr,
	SrIndY,
	AbsInd,
	AbsIndL,
	AbsXInd,
	ImpAcc,
	BlkMov,
} AddrMode;

void cpu_internal_op(CPU *cpu) {
	cpu->cycles++;
	bus_next_fast(cpu->bus, true);
	bus_clock(cpu->bus);
}

uint8_t cpu_read(CPU *cpu, uint32_t addr) {
	uint8_t val = bus_read(cpu->bus, addr);
	cpu->cycles++;
	bus_clock(cpu->bus);
	return val;
}

void cpu_write(CPU *cpu, uint32_t addr, uint8_t val) {
	bus_write(cpu->bus, addr, val);
	cpu->cycles++;
	bus_clock(cpu->bus);
}

uint16_t cpu_read16(CPU *cpu, uint32_t addr) {
	return cpu_read(cpu, addr)
		| (cpu_read(cpu, addr + 1) << 8);
}

void cpu_write16(CPU *cpu, uint32_t addr, uint16_t val) {
	cpu_write(cpu, addr, val & 0xff);
	cpu_write(cpu, addr + 1, val >> 8);
}

uint16_t cpu_read24(CPU *cpu, uint32_t addr) {
	return cpu_read(cpu, addr)
	| (cpu_read(cpu, addr + 1) << 8)
	| (cpu_read(cpu, addr + 2) << 16);
}

uint8_t cpu_pull(CPU *cpu) {
	if (cpu->e) {
		cpu->sl++;
	} else {
		cpu->s++;
	}
	return cpu_read(cpu, cpu->s);
}

void cpu_push(CPU *cpu, uint8_t val) {
	cpu_write(cpu, cpu->s, val);
	if (cpu->e) {
		cpu->sl--;
	} else {
		cpu->s--;
	}
}

void cpu_push16(CPU *cpu, uint16_t val) {
	cpu_push(cpu, val >> 8);
	cpu_push(cpu, val & 0xff);
}

uint16_t cpu_pull16(CPU *cpu) {
	return cpu_pull(cpu) | cpu_pull(cpu) << 8;
}


void cpu_set_zn8(CPU *cpu, uint8_t val) {
	cpu->p.z = (val == 0);
	cpu->p.n = (val & 0x80) != 0;
}

void cpu_set_zn16(CPU *cpu, uint16_t val) {
	cpu->p.z = (val == 0);
	cpu->p.n = (val & 0x8000) != 0;
}




uint32_t cpu_oper_addr(CPU *cpu, AddrMode mode) {
	uint16_t addr16;
	uint32_t addr32;

	switch (mode) {
		case Imp: // implied
		case ImpAcc: // implied accumulator
			return 0;
		case ImmM: // immediate memory flag
			addr32 = cpu->pb << 16 | cpu->pc;
			cpu->pc += cpu->p.m ? 1 : 2;
			return addr32;
		case ImmXY: // immediate index flag
			addr32 = cpu->pb << 16 | cpu->pc;
			cpu->pc += cpu->p.x ? 1 : 2;
			return addr32;
		case Imm8: // immediate 8-bit
			return cpu->pb << 16 | cpu->pc++;
		case Rel: // relative
			addr16 = ((int8_t)cpu_read(cpu, cpu->pb << 16 | cpu->pc)) + cpu->pc + 1;
			cpu->pc++;
			return cpu->pb << 16 | addr16;
		case RelL: // relative long
			addr16 = ((int16_t)cpu_read16(cpu, cpu->pb << 16 | cpu->pc)) + cpu->pc + 2;
			cpu->pc += 2;
			return cpu->pb << 16 | addr16;
		case Dp: // direct
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->d;
			return addr16;
		case DpX: // direct indexed x
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->d + cpu->x;
			return addr16;
		case DpY: // direct indexed y
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->d + cpu->y;
			return addr16;
		case DpInd: // direct indirect
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->d;
			addr16 = cpu_read16(cpu, addr16);
			return cpu->db << 16 | addr16;
		case DpXInd: // direct indexed indirect
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->d + cpu->x;
			addr16 = cpu_read16(cpu, addr16);
			return cpu->db << 16 | addr16;
		case DpIndY: // direct indirect indexed
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->d;
			addr16 = cpu_read16(cpu, addr16) + cpu->y;
			return cpu->db << 16 | addr16;
		case DpIndL: // direct indirect long
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->d;
			addr32 = cpu_read24(cpu, addr16);
			return addr32;
		case DpIndLY: // direct indirect indexed long
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->d;
			addr32 = (cpu_read24(cpu, addr16) + cpu->y) & 0xffffff;
			return addr32;
		case Abs: // absolute
			addr16 = cpu_read16(cpu, cpu->pb << 16 | cpu->pc);
			cpu->pc += 2;
			return cpu->db << 16 | addr16;
		case AbsX: // absolute indexed x
			addr16 = cpu_read16(cpu, cpu->pb << 16 | cpu->pc) + cpu->x;
			cpu->pc += 2;
			return cpu->db << 16 |addr16;
		case AbsY: // absolute indexed y
			addr16 = cpu_read16(cpu, cpu->pb << 16 | cpu->pc) + cpu->y;
			cpu->pc += 2;
			return cpu->db << 16 |addr16;
		case AbsL: // absolute long
			addr32 = cpu_read24(cpu, cpu->pb << 16 | cpu->pc);
			cpu->pc += 3;
			return addr32;
		case AbsLX: // absolute indexed long
			addr32 = cpu_read24(cpu, cpu->pb << 16 | cpu->pc) + cpu->x;
			cpu->pc += 3;
			return addr32;
		case Sr: // stack relative
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->s;
			return addr16;
		case SrIndY: // stack relative indirect indexed
			addr16 = cpu_read(cpu, cpu->pb << 16 | cpu->pc++) + cpu->s;
			addr16 = cpu_read16(cpu, addr16) + cpu->y;
			return cpu->db << 16 | addr16;
		case AbsInd: // absolute indirect
			addr16 = cpu_read16(cpu, cpu->pb << 16 | cpu->pc);
			cpu->pc += 2;
			addr16 = cpu_read16(cpu, addr16);
			return addr16;
		case AbsIndL: // absolute indirect long
			addr16 = cpu_read16(cpu, cpu->pb << 16 | cpu->pc);
			cpu->pc += 2;
			addr32 = cpu_read24(cpu, addr16);
			return addr32;
		case AbsXInd: // absolute indexed indirect
			addr16 = cpu_read16(cpu, cpu->pb << 16 | cpu->pc) + cpu->x;
			cpu->pc += 2;
			addr16 = cpu_read16(cpu, cpu->pb << 16 | addr16);
			return addr16;
		case BlkMov: // block move
			addr16 = cpu_read16(cpu, cpu->pb << 16 | cpu->pc);
			cpu->pc += 2;
			return addr16; // must be handled special in block instrs

		default:
			return 0;
	}
}


void cpu_op_adc(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("adc\n");}
void cpu_op_and(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("and\n");}
void cpu_op_asl(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("asl\n");}
void cpu_op_bcc(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("bcc\n");}
void cpu_op_bcs(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("bcs\n");}
void cpu_op_beq(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("beq\n");}
void cpu_op_bit(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("bit\n");}
void cpu_op_bmi(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("bmi\n");}
void cpu_op_bne(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("bne\n");}
void cpu_op_bpl(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("bpl\n");}
void cpu_op_bra(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("bra\n");}
void cpu_op_brk(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("brk\n");}
void cpu_op_brl(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("brl\n");}
void cpu_op_bvc(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("bvc\n");}
void cpu_op_bvs(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("bvs\n");}

void cpu_op_clc(CPU *cpu, AddrMode mode) {
	(void)mode;
	cpu->p.c = 0;
	cpu_internal_op(cpu);
}

void cpu_op_cld(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("cld\n");}
void cpu_op_cli(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("cli\n");}
void cpu_op_clv(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("clv\n");}
void cpu_op_cmp(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("cmp\n");}
void cpu_op_cop(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("cop\n");}
void cpu_op_cpx(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("cpx\n");}
void cpu_op_cpy(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("cpy\n");}
void cpu_op_dec(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("dec\n");}
void cpu_op_dex(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("dex\n");}
void cpu_op_dey(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("dey\n");}
void cpu_op_eor(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("eor\n");}
void cpu_op_inc(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("inc\n");}
void cpu_op_inx(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("inx\n");}
void cpu_op_iny(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("iny\n");}

void cpu_op_jmp(CPU *cpu, AddrMode mode) {
	uint32_t addr = cpu_oper_addr(cpu, mode);
	cpu->pc = addr;
	if (mode == AbsL || mode == AbsIndL) {
		cpu->pb = addr >> 16;
	}
}

void cpu_op_jsr(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("jsr\n");}

void cpu_op_lda(CPU *cpu, AddrMode mode) {
	uint32_t addr = cpu_oper_addr(cpu, mode);

	if (cpu->p.m) {
		cpu->al = cpu_read(cpu, addr);
		cpu_set_zn8(cpu, cpu->al);
	} else {
		cpu->a = cpu_read16(cpu, addr);
		cpu_set_zn16(cpu, cpu->a);
	}
}

void cpu_op_ldx(CPU *cpu, AddrMode mode) {
	uint32_t addr = cpu_oper_addr(cpu, mode);

	if (cpu->p.x) {
		cpu->xl = cpu_read(cpu, addr);
		cpu_set_zn8(cpu, cpu->xl);
	} else {
		cpu->x = cpu_read16(cpu, addr);
		cpu_set_zn16(cpu, cpu->x);
	}
}

void cpu_op_ldy(CPU *cpu, AddrMode mode) {
	uint32_t addr = cpu_oper_addr(cpu, mode);

	if (cpu->p.x) {
		cpu->yl = cpu_read(cpu, addr);
		cpu_set_zn8(cpu, cpu->yl);
	} else {
		cpu->y = cpu_read16(cpu, addr);
		cpu_set_zn16(cpu, cpu->y);
	}
}

void cpu_op_lsr(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("lsr\n");}
void cpu_op_mvn(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("mvn\n");}
void cpu_op_mvp(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("mvp\n");}
void cpu_op_nop(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("nop\n");}
void cpu_op_ora(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("ora\n");}
void cpu_op_pea(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("pea\n");}
void cpu_op_pei(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("pei\n");}
void cpu_op_per(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("per\n");}
void cpu_op_pha(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("pha\n");}
void cpu_op_phb(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("phb\n");}
void cpu_op_phd(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("phd\n");}
void cpu_op_phk(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("phk\n");}

void cpu_op_php(CPU *cpu, AddrMode mode) {
	(void)mode;
	cpu_push(cpu, cpu->p.raw);
	cpu_internal_op(cpu);
}

void cpu_op_phx(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("phx\n");}
void cpu_op_phy(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("phy\n");}
void cpu_op_pla(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("pla\n");}
void cpu_op_plb(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("plb\n");}
void cpu_op_pld(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("pld\n");}
void cpu_op_plp(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("plp\n");}
void cpu_op_plx(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("plx\n");}
void cpu_op_ply(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("ply\n");}

void cpu_op_rep(CPU *cpu, AddrMode mode) {
	uint32_t addr = cpu_oper_addr(cpu, mode);
	uint8_t val = cpu_read(cpu, addr);

	cpu->p.raw &= ~val;

	if (cpu->e) {
		cpu->p.m = 1;
		cpu->p.x = 1;
	}

	cpu_internal_op(cpu);
}

void cpu_op_rol(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("rol\n");}
void cpu_op_ror(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("ror\n");}
void cpu_op_rti(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("rti\n");}
void cpu_op_rtl(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("rtl\n");}
void cpu_op_rts(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("rts\n");}
void cpu_op_sbc(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("sbc\n");}
void cpu_op_sec(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("sec\n");}
void cpu_op_sed(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("sed\n");}
void cpu_op_sei(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("sei\n");}

void cpu_op_sep(CPU *cpu, AddrMode mode) {
	uint32_t addr = cpu_oper_addr(cpu, mode);
	uint8_t val = cpu_read(cpu, addr);

	bool was_16bit_x = (cpu->p.x == 0);

	cpu->p.raw |= val;

	if (was_16bit_x && cpu->p.x) {
		cpu->xh = 0x00;
		cpu->yh = 0x00;
	}

	cpu_internal_op(cpu);
}

void cpu_op_sta(CPU *cpu, AddrMode mode) {
	uint32_t addr = cpu_oper_addr(cpu, mode);
	if (cpu->p.m) {
		cpu_write(cpu, addr, cpu->al);
	} else {
		cpu_write16(cpu, addr, cpu->a);
	}
}

void cpu_op_stp(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("stp\n");}
void cpu_op_stx(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("stx\n");}
void cpu_op_sty(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("sty\n");}

void cpu_op_stz(CPU *cpu, AddrMode mode) {
	uint32_t addr = cpu_oper_addr(cpu, mode);
	if (cpu->p.m) {
		cpu_write(cpu, addr, 0x00);
	} else {
		cpu_write16(cpu, addr, 0x0000);
	}
}

void cpu_op_tax(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tax\n");}
void cpu_op_tay(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tay\n");}
void cpu_op_tcd(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tcd\n");}
void cpu_op_tcs(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tcs\n");}
void cpu_op_tdc(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tdc\n");}
void cpu_op_trb(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("trb\n");}
void cpu_op_tsb(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tsb\n");}
void cpu_op_tsc(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tsc\n");}
void cpu_op_tsx(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tsx\n");}
void cpu_op_txa(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("txa\n");}
void cpu_op_txs(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("txs\n");}
void cpu_op_txy(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("txy\n");}
void cpu_op_tya(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tya\n");}
void cpu_op_tyx(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("tyx\n");}
void cpu_op_wai(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("wai\n");}
void cpu_op_wdm(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("wdm\n");}
void cpu_op_xba(CPU *cpu, AddrMode mode) {(void)cpu; (void)mode; ldeb1("xba\n");}

void cpu_op_xce(CPU *cpu, AddrMode mode) {
	(void)mode;
	bool temp;

	temp = cpu->e;
	cpu->e = cpu->p.c;
	cpu->p.c = temp;

	if (cpu->e) {
		cpu->p.m = 1;
		cpu->p.x = 1;

		cpu->s = 0x0100 | cpu->sl;

		cpu->x = cpu->xl;
		cpu->y = cpu->yl;
	}
	cpu_internal_op(cpu);
}





void cpu_op_switch(CPU *cpu, uint8_t opcode) {
	switch (opcode) {
		// add with carry
		case 0x61: cpu_op_adc(cpu, DpXInd);  break;
		case 0x63: cpu_op_adc(cpu, Sr);      break;
		case 0x65: cpu_op_adc(cpu, Dp);      break;
		case 0x67: cpu_op_adc(cpu, DpIndL);  break;
		case 0x69: cpu_op_adc(cpu, ImmM);    break;
		case 0x6d: cpu_op_adc(cpu, Abs);     break;
		case 0x6f: cpu_op_adc(cpu, AbsL);    break;
		case 0x71: cpu_op_adc(cpu, DpIndY);  break;
		case 0x72: cpu_op_adc(cpu, DpInd);   break;
		case 0x73: cpu_op_adc(cpu, SrIndY);  break;
		case 0x75: cpu_op_adc(cpu, DpX);     break;
		case 0x77: cpu_op_adc(cpu, DpIndLY); break;
		case 0x79: cpu_op_adc(cpu, AbsY);    break;
		case 0x7d: cpu_op_adc(cpu, AbsX);    break;
		case 0x7f: cpu_op_adc(cpu, AbsLX);   break;

		// bitwise and
		case 0x21: cpu_op_and(cpu, DpXInd);  break;
		case 0x23: cpu_op_and(cpu, Sr);      break;
		case 0x25: cpu_op_and(cpu, Dp);      break;
		case 0x27: cpu_op_and(cpu, DpIndL);  break;
		case 0x29: cpu_op_and(cpu, ImmM);    break;
		case 0x2d: cpu_op_and(cpu, Abs);     break;
		case 0x2f: cpu_op_and(cpu, AbsL);    break;
		case 0x31: cpu_op_and(cpu, DpIndY);  break;
		case 0x32: cpu_op_and(cpu, DpInd);   break;
		case 0x33: cpu_op_and(cpu, SrIndY);  break;
		case 0x35: cpu_op_and(cpu, DpX);     break;
		case 0x37: cpu_op_and(cpu, DpIndLY); break;
		case 0x39: cpu_op_and(cpu, AbsY);    break;
		case 0x3d: cpu_op_and(cpu, AbsX);    break;
		case 0x3f: cpu_op_and(cpu, AbsLX);   break;

		// bitwise or
		case 0x01: cpu_op_ora(cpu, DpXInd);  break;
		case 0x03: cpu_op_ora(cpu, Sr);      break;
		case 0x05: cpu_op_ora(cpu, Dp);      break;
		case 0x07: cpu_op_ora(cpu, DpIndL);  break;
		case 0x09: cpu_op_ora(cpu, ImmM);    break;
		case 0x0d: cpu_op_ora(cpu, Abs);     break;
		case 0x0f: cpu_op_ora(cpu, AbsL);    break;
		case 0x11: cpu_op_ora(cpu, DpIndY);  break;
		case 0x12: cpu_op_ora(cpu, DpInd);   break;
		case 0x13: cpu_op_ora(cpu, SrIndY);  break;
		case 0x15: cpu_op_ora(cpu, DpX);     break;
		case 0x17: cpu_op_ora(cpu, DpIndLY); break;
		case 0x19: cpu_op_ora(cpu, AbsY);    break;
		case 0x1d: cpu_op_ora(cpu, AbsX);    break;
		case 0x1f: cpu_op_ora(cpu, AbsLX);   break;

		// exclusive or
		case 0x41: cpu_op_eor(cpu, DpXInd);  break;
		case 0x43: cpu_op_eor(cpu, Sr);      break;
		case 0x45: cpu_op_eor(cpu, Dp);      break;
		case 0x47: cpu_op_eor(cpu, DpIndL);  break;
		case 0x49: cpu_op_eor(cpu, ImmM);    break;
		case 0x4d: cpu_op_eor(cpu, Abs);     break;
		case 0x4f: cpu_op_eor(cpu, AbsL);    break;
		case 0x51: cpu_op_eor(cpu, DpIndY);  break;
		case 0x52: cpu_op_eor(cpu, DpInd);   break;
		case 0x53: cpu_op_eor(cpu, SrIndY);  break;
		case 0x55: cpu_op_eor(cpu, DpX);     break;
		case 0x57: cpu_op_eor(cpu, DpIndLY); break;
		case 0x59: cpu_op_eor(cpu, AbsY);    break;
		case 0x5d: cpu_op_eor(cpu, AbsX);    break;
		case 0x5f: cpu_op_eor(cpu, AbsLX);   break;

		// subtract with carry
		case 0xe1: cpu_op_sbc(cpu, DpXInd);  break;
		case 0xe3: cpu_op_sbc(cpu, Sr);      break;
		case 0xe5: cpu_op_sbc(cpu, Dp);      break;
		case 0xe7: cpu_op_sbc(cpu, DpIndL);  break;
		case 0xe9: cpu_op_sbc(cpu, ImmM);    break;
		case 0xed: cpu_op_sbc(cpu, Abs);     break;
		case 0xef: cpu_op_sbc(cpu, AbsL);    break;
		case 0xf1: cpu_op_sbc(cpu, DpIndY);  break;
		case 0xf2: cpu_op_sbc(cpu, DpInd);   break;
		case 0xf3: cpu_op_sbc(cpu, SrIndY);  break;
		case 0xf5: cpu_op_sbc(cpu, DpX);     break;
		case 0xf7: cpu_op_sbc(cpu, DpIndLY); break;
		case 0xf9: cpu_op_sbc(cpu, AbsY);    break;
		case 0xfd: cpu_op_sbc(cpu, AbsX);    break;
		case 0xff: cpu_op_sbc(cpu, AbsLX);   break;

		// compare accumulator
		case 0xc1: cpu_op_cmp(cpu, DpXInd);  break;
		case 0xc3: cpu_op_cmp(cpu, Sr);      break;
		case 0xc5: cpu_op_cmp(cpu, Dp);      break;
		case 0xc7: cpu_op_cmp(cpu, DpIndL);  break;
		case 0xc9: cpu_op_cmp(cpu, ImmM);    break;
		case 0xcd: cpu_op_cmp(cpu, Abs);     break;
		case 0xcf: cpu_op_cmp(cpu, AbsL);    break;
		case 0xd1: cpu_op_cmp(cpu, DpIndY);  break;
		case 0xd2: cpu_op_cmp(cpu, DpInd);   break;
		case 0xd3: cpu_op_cmp(cpu, SrIndY);  break;
		case 0xd5: cpu_op_cmp(cpu, DpX);     break;
		case 0xd7: cpu_op_cmp(cpu, DpIndLY); break;
		case 0xd9: cpu_op_cmp(cpu, AbsY);    break;
		case 0xdd: cpu_op_cmp(cpu, AbsX);    break;
		case 0xdf: cpu_op_cmp(cpu, AbsLX);   break;

		// load accumulator
		case 0xa1: cpu_op_lda(cpu, DpXInd);  break;
		case 0xa3: cpu_op_lda(cpu, Sr);      break;
		case 0xa5: cpu_op_lda(cpu, Dp);      break;
		case 0xa7: cpu_op_lda(cpu, DpIndL);  break;
		case 0xa9: cpu_op_lda(cpu, ImmM);    break;
		case 0xad: cpu_op_lda(cpu, Abs);     break;
		case 0xaf: cpu_op_lda(cpu, AbsL);    break;
		case 0xb1: cpu_op_lda(cpu, DpIndY);  break;
		case 0xb2: cpu_op_lda(cpu, DpInd);   break;
		case 0xb3: cpu_op_lda(cpu, SrIndY);  break;
		case 0xb5: cpu_op_lda(cpu, DpX);     break;
		case 0xb7: cpu_op_lda(cpu, DpIndLY); break;
		case 0xb9: cpu_op_lda(cpu, AbsY);    break;
		case 0xbd: cpu_op_lda(cpu, AbsX);    break;
		case 0xbf: cpu_op_lda(cpu, AbsLX);   break;

		// store accumulator
		case 0x81: cpu_op_sta(cpu, DpXInd);  break;
		case 0x83: cpu_op_sta(cpu, Sr);      break;
		case 0x85: cpu_op_sta(cpu, Dp);      break;
		case 0x87: cpu_op_sta(cpu, DpIndL);  break;
		case 0x8d: cpu_op_sta(cpu, Abs);     break;
		case 0x8f: cpu_op_sta(cpu, AbsL);    break;
		case 0x91: cpu_op_sta(cpu, DpIndY);  break;
		case 0x92: cpu_op_sta(cpu, DpInd);   break;
		case 0x93: cpu_op_sta(cpu, SrIndY);  break;
		case 0x95: cpu_op_sta(cpu, DpX);     break;
		case 0x97: cpu_op_sta(cpu, DpIndLY); break;
		case 0x99: cpu_op_sta(cpu, AbsY);    break;
		case 0x9d: cpu_op_sta(cpu, AbsX);    break;
		case 0x9f: cpu_op_sta(cpu, AbsLX);   break;

		// load x & y
		case 0xa2: cpu_op_ldx(cpu, ImmXY);   break;
		case 0xa6: cpu_op_ldx(cpu, Dp);      break;
		case 0xae: cpu_op_ldx(cpu, Abs);     break;
		case 0xb6: cpu_op_ldx(cpu, DpY);     break;
		case 0xbe: cpu_op_ldx(cpu, AbsY);    break;
		case 0xa0: cpu_op_ldy(cpu, ImmXY);   break;
		case 0xa4: cpu_op_ldy(cpu, Dp);      break;
		case 0xac: cpu_op_ldy(cpu, Abs);     break;
		case 0xb4: cpu_op_ldy(cpu, DpX);     break;
		case 0xbc: cpu_op_ldy(cpu, AbsX);    break;

		// store x, y, & zero
		case 0x86: cpu_op_stx(cpu, Dp);      break;
		case 0x8e: cpu_op_stx(cpu, Abs);     break;
		case 0x96: cpu_op_stx(cpu, DpY);     break;
		case 0x84: cpu_op_sty(cpu, Dp);      break;
		case 0x8c: cpu_op_sty(cpu, Abs);     break;
		case 0x94: cpu_op_sty(cpu, DpX);     break;
		case 0x64: cpu_op_stz(cpu, Dp);      break;
		case 0x74: cpu_op_stz(cpu, DpX);     break;
		case 0x9c: cpu_op_stz(cpu, Abs);     break;
		case 0x9e: cpu_op_stz(cpu, AbsX);    break;

		// compare x & y
		case 0xe0: cpu_op_cpx(cpu, ImmXY);   break;
		case 0xe4: cpu_op_cpx(cpu, Dp);      break;
		case 0xec: cpu_op_cpx(cpu, Abs);     break;
		case 0xc0: cpu_op_cpy(cpu, ImmXY);   break;
		case 0xc4: cpu_op_cpy(cpu, Dp);      break;
		case 0xcc: cpu_op_cpy(cpu, Abs);     break;

		// bit test
		case 0x24: cpu_op_bit(cpu, Dp);      break;
		case 0x2c: cpu_op_bit(cpu, Abs);     break;
		case 0x34: cpu_op_bit(cpu, DpX);     break;
		case 0x3c: cpu_op_bit(cpu, AbsX);    break;
		case 0x89: cpu_op_bit(cpu, ImmM);    break;

		// arithmetic shift left
		case 0x06: cpu_op_asl(cpu, Dp);      break;
		case 0x0a: cpu_op_asl(cpu, ImpAcc);  break;
		case 0x0e: cpu_op_asl(cpu, Abs);     break;
		case 0x16: cpu_op_asl(cpu, DpX);     break;
		case 0x1e: cpu_op_asl(cpu, AbsX);    break;

		// logical shift right
		case 0x46: cpu_op_lsr(cpu, Dp);      break;
		case 0x4a: cpu_op_lsr(cpu, ImpAcc);  break;
		case 0x4e: cpu_op_lsr(cpu, Abs);     break;
		case 0x56: cpu_op_lsr(cpu, DpX);     break;
		case 0x5e: cpu_op_lsr(cpu, AbsX);    break;

		// rotate left
		case 0x26: cpu_op_rol(cpu, Dp);      break;
		case 0x2a: cpu_op_rol(cpu, ImpAcc);  break;
		case 0x2e: cpu_op_rol(cpu, Abs);     break;
		case 0x36: cpu_op_rol(cpu, DpX);     break;
		case 0x3e: cpu_op_rol(cpu, AbsX);    break;

		// rotate right
		case 0x66: cpu_op_ror(cpu, Dp);      break;
		case 0x6a: cpu_op_ror(cpu, ImpAcc);  break;
		case 0x6e: cpu_op_ror(cpu, Abs);     break;
		case 0x76: cpu_op_ror(cpu, DpX);     break;
		case 0x7e: cpu_op_ror(cpu, AbsX);    break;

		// branches
		case 0x90: cpu_op_bcc(cpu, Rel);     break;
		case 0xb0: cpu_op_bcs(cpu, Rel);     break;
		case 0xf0: cpu_op_beq(cpu, Rel);     break;
		case 0x30: cpu_op_bmi(cpu, Rel);     break;
		case 0xd0: cpu_op_bne(cpu, Rel);     break;
		case 0x10: cpu_op_bpl(cpu, Rel);     break;
		case 0x80: cpu_op_bra(cpu, Rel);     break;
		case 0x82: cpu_op_brl(cpu, RelL);    break;
		case 0x50: cpu_op_bvc(cpu, Rel);     break;
		case 0x70: cpu_op_bvs(cpu, Rel);     break;

		// jumps and calls
		case 0x4c: cpu_op_jmp(cpu, Abs);     break;
		case 0x5c: cpu_op_jmp(cpu, AbsL);    break;
		case 0x6c: cpu_op_jmp(cpu, AbsInd);  break;
		case 0x7c: cpu_op_jmp(cpu, AbsXInd); break;
		case 0xdc: cpu_op_jmp(cpu, AbsIndL); break;
		case 0x20: cpu_op_jsr(cpu, Abs);     break;
		case 0x22: cpu_op_jsr(cpu, AbsL);    break;
		case 0xfc: cpu_op_jsr(cpu, AbsXInd); break;

		// returns
		case 0x40: cpu_op_rti(cpu, Imp);     break;
		case 0x60: cpu_op_rts(cpu, Imp);     break;
		case 0x6b: cpu_op_rtl(cpu, Imp);     break;

		// flag operations
		case 0x18: cpu_op_clc(cpu, Imp);     break;
		case 0x38: cpu_op_sec(cpu, Imp);     break;
		case 0x58: cpu_op_cli(cpu, Imp);     break;
		case 0x78: cpu_op_sei(cpu, Imp);     break;
		case 0xb8: cpu_op_clv(cpu, Imp);     break;
		case 0xd8: cpu_op_cld(cpu, Imp);     break;
		case 0xf8: cpu_op_sed(cpu, Imp);     break;
		case 0xc2: cpu_op_rep(cpu, Imm8);    break;
		case 0xe2: cpu_op_sep(cpu, Imm8);    break;

		// pushes
		case 0x08: cpu_op_php(cpu, Imp);     break;
		case 0x0b: cpu_op_phd(cpu, Imp);     break;
		case 0x48: cpu_op_pha(cpu, Imp);     break;
		case 0x4b: cpu_op_phk(cpu, Imp);     break;
		case 0x5a: cpu_op_phy(cpu, Imp);     break;
		case 0x8b: cpu_op_phb(cpu, Imp);     break;
		case 0xda: cpu_op_phx(cpu, Imp);     break;
		case 0xd4: cpu_op_pei(cpu, DpInd);   break;
		case 0xf4: cpu_op_pea(cpu, Abs);     break;
		case 0x62: cpu_op_per(cpu, RelL);    break;

		// pulls
		case 0x28: cpu_op_plp(cpu, Imp);     break;
		case 0x2b: cpu_op_pld(cpu, Imp);     break;
		case 0x68: cpu_op_pla(cpu, Imp);     break;
		case 0x7a: cpu_op_ply(cpu, Imp);     break;
		case 0xab: cpu_op_plb(cpu, Imp);     break;
		case 0xfa: cpu_op_plx(cpu, Imp);     break;

		// transfers
		case 0xaa: cpu_op_tax(cpu, Imp);     break;
		case 0xa8: cpu_op_tay(cpu, Imp);     break;
		case 0x8a: cpu_op_txa(cpu, Imp);     break;
		case 0x98: cpu_op_tya(cpu, Imp);     break;
		case 0xba: cpu_op_tsx(cpu, Imp);     break;
		case 0x9a: cpu_op_txs(cpu, Imp);     break;
		case 0x9b: cpu_op_txy(cpu, Imp);     break;
		case 0xbb: cpu_op_tyx(cpu, Imp);     break;
		case 0x5b: cpu_op_tcd(cpu, Imp);     break;
		case 0x7b: cpu_op_tdc(cpu, Imp);     break;
		case 0x1b: cpu_op_tcs(cpu, Imp);     break;
		case 0x3b: cpu_op_tsc(cpu, Imp);     break;

		// increment & decrement
		case 0x1a: cpu_op_inc(cpu, ImpAcc);  break;
		case 0xe6: cpu_op_inc(cpu, Dp);      break;
		case 0xee: cpu_op_inc(cpu, Abs);     break;
		case 0xf6: cpu_op_inc(cpu, DpX);     break;
		case 0xfe: cpu_op_inc(cpu, AbsX);    break;
		case 0xe8: cpu_op_inx(cpu, Imp);     break;
		case 0xc8: cpu_op_iny(cpu, Imp);     break;
		case 0x3a: cpu_op_dec(cpu, ImpAcc);  break;
		case 0xc6: cpu_op_dec(cpu, Dp);      break;
		case 0xce: cpu_op_dec(cpu, Abs);     break;
		case 0xd6: cpu_op_dec(cpu, DpX);     break;
		case 0xde: cpu_op_dec(cpu, AbsX);    break;
		case 0xca: cpu_op_dex(cpu, Imp);     break;
		case 0x88: cpu_op_dey(cpu, Imp);     break;

		// block moves
		case 0x44: cpu_op_mvp(cpu, BlkMov);  break;
		case 0x54: cpu_op_mvn(cpu, BlkMov);  break;

		// test and reset/set bits
		case 0x14: cpu_op_trb(cpu, Dp);      break;
		case 0x1c: cpu_op_trb(cpu, Abs);     break;
		case 0x04: cpu_op_tsb(cpu, Dp);      break;
		case 0x0c: cpu_op_tsb(cpu, Abs);     break;

		// misc
		case 0x00: cpu_op_brk(cpu, Imp);     break;
		case 0x02: cpu_op_cop(cpu, Imp);     break;
		case 0x42: cpu_op_wdm(cpu, Imp);     break;
		case 0xdb: cpu_op_stp(cpu, Imp);     break;
		case 0xcb: cpu_op_wai(cpu, Imp);     break;
		case 0xeb: cpu_op_xba(cpu, Imp);     break;
		case 0xfb: cpu_op_xce(cpu, Imp);     break;
		case 0xea: cpu_op_nop(cpu, Imp);     break;
	}
}

void cpu_init(CPU *cpu) {
	cpu_poweron(cpu);
	cpu->cycles = 0;
}

void cpu_poweron(CPU *cpu) {
	cpu->a = 0;
	cpu->x = 0;
	cpu->y = 0;
	cpu->d = 0;
	cpu->db = 0;
	cpu->pb = 0;
	cpu_reset(cpu);
}

void cpu_reset(CPU *cpu) {
	cpu->p.raw = 0b00110100;
	cpu->d = 0;
	cpu->db = 0;
	cpu->pb = 0;
	cpu->e = true;
	cpu->s = 0xfd;
	cpu->pc = cpu_read16(cpu, 0xfffc);
	cpu->cycles = 7;
}

void cpu_step(CPU *cpu) {

	if (cpu->stopped) {
		return;
	}

	if (cpu->stall) {
		for (; cpu->stall > 0; cpu->stall--) {
			cpu->cycles++;
			bus_clock(cpu->bus);
		}
		return;
	}

	uint8_t opcode = cpu_read(cpu, cpu->pb << 16 | cpu->pc++);

	cpu_op_switch(cpu, opcode);
}

void cpu_run(CPU *cpu) {
	// run until vblank
	do {
		cpu_step(cpu);
	} while (!cpu->stopped && !cpu->bus->vblank_started);

	cpu->bus->vblank_started = false;
	// vblank interrupt
	return; // allow main to handle calls
}
