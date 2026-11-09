#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "dsp.h"
#include "host.h"
#include "log.h"

static const int16_t GAUSS_TABLE[512] = {
	   0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
	   1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    2,    2,    2,    2,    2,
	   2,    2,    3,    3,    3,    3,    3,    4,    4,    4,    4,    4,    5,    5,    5,    5,
	   6,    6,    6,    6,    7,    7,    7,    8,    8,    8,    9,    9,    9,   10,   10,   10,
	  11,   11,   11,   12,   12,   13,   13,   14,   14,   15,   15,   15,   16,   16,   17,   17,
	  18,   19,   19,   20,   20,   21,   21,   22,   23,   23,   24,   24,   25,   26,   27,   27,
	  28,   29,   29,   30,   31,   32,   32,   33,   34,   35,   36,   36,   37,   38,   39,   40,
	  41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51,   52,   53,   54,   55,   56,
	  58,   59,   60,   61,   62,   64,   65,   66,   67,   69,   70,   71,   73,   74,   76,   77,
	  78,   80,   81,   83,   84,   86,   87,   89,   90,   92,   94,   95,   97,   99,  100,  102,
	 104,  106,  107,  109,  111,  113,  115,  117,  118,  120,  122,  124,  126,  128,  130,  132,
	 134,  137,  139,  141,  143,  145,  147,  150,  152,  154,  156,  159,  161,  163,  166,  168,
	 171,  173,  175,  178,  180,  183,  186,  188,  191,  193,  196,  199,  201,  204,  207,  210,
	 212,  215,  218,  221,  224,  227,  230,  233,  236,  239,  242,  245,  248,  251,  254,  257,
	 260,  263,  267,  270,  273,  276,  280,  283,  286,  290,  293,  297,  300,  304,  307,  311,
	 314,  318,  321,  325,  328,  332,  336,  339,  343,  347,  351,  354,  358,  362,  366,  370,
	 374,  378,  381,  385,  389,  393,  397,  401,  405,  410,  414,  418,  422,  426,  430,  434,
	 439,  443,  447,  451,  456,  460,  464,  469,  473,  477,  482,  486,  491,  495,  499,  504,
	 508,  513,  517,  522,  527,  531,  536,  540,  545,  550,  554,  559,  563,  568,  573,  577,
	 582,  587,  592,  596,  601,  606,  611,  615,  620,  625,  630,  635,  640,  644,  649,  654,
	 659,  664,  669,  674,  678,  683,  688,  693,  698,  703,  708,  713,  718,  723,  728,  732,
	 737,  742,  747,  752,  757,  762,  767,  772,  777,  782,  787,  792,  797,  802,  806,  811,
	 816,  821,  826,  831,  836,  841,  846,  851,  855,  860,  865,  870,  875,  880,  884,  889,
	 894,  899,  904,  908,  913,  918,  923,  927,  932,  937,  941,  946,  951,  955,  960,  965,
	 969,  974,  978,  983,  988,  992,  997, 1001, 1005, 1010, 1014, 1019, 1023, 1027, 1032, 1036,
	1040, 1045, 1049, 1053, 1057, 1061, 1066, 1070, 1074, 1078, 1082, 1086, 1090, 1094, 1098, 1102,
	1106, 1109, 1113, 1117, 1121, 1125, 1128, 1132, 1136, 1139, 1143, 1146, 1150, 1153, 1157, 1160,
	1164, 1167, 1170, 1174, 1177, 1180, 1183, 1186, 1190, 1193, 1196, 1199, 1202, 1205, 1207, 1210,
	1213, 1216, 1219, 1221, 1224, 1227, 1229, 1232, 1234, 1237, 1239, 1241, 1244, 1246, 1248, 1251,
	1253, 1255, 1257, 1259, 1261, 1263, 1265, 1267, 1269, 1270, 1272, 1274, 1275, 1277, 1279, 1280,
	1282, 1283, 1284, 1286, 1287, 1288, 1290, 1291, 1292, 1293, 1294, 1295, 1296, 1297, 1297, 1298,
	1299, 1300, 1300, 1301, 1302, 1302, 1303, 1303, 1303, 1304, 1304, 1304, 1304, 1304, 1305, 1305,
};


static const uint16_t RATES_TABLE[32] = {
	65535, 2048, 1536, 1280,
	 1024,  768,  640,  512,
	  384,  320,  256,  192,
	  160,  128,   96,   80,
	   64,   48,   40,   32,
	   24,   20,   16,   12,
	   10,    8,    6,    5,
	    4,    3,    2,    1,
};

static const uint16_t OFFS_TABLE[32] = {
	  1, 0, 1040,
	536, 0, 1040,
	536, 0, 1040,
	536, 0, 1040,
	536, 0, 1040,
	536, 0, 1040,
	536, 0, 1040,
	536, 0, 1040,
	536, 0, 1040,
	536, 0, 1040,
	  0, 0
};

static inline bool dsp_check_counter(DSP *dsp, int32_t rate) {
	return (((uint16_t)dsp->counter + OFFS_TABLE[rate]) % RATES_TABLE[rate]) == 0;
}

static inline int32_t clamp16(int32_t val) {
	if (val < -32768) return -32768;
	if (val > 32767) return 32767;
	return val;
}

void dsp_init(DSP *dsp, uint8_t *ram) {
	(void)GAUSS_TABLE; // shut up gcc

	memset(dsp, 0, sizeof(DSP));
	dsp->ram = ram;
	for (int i = 0; i < 8; i++) {
		dsp->voices[i].brr_offset = 1;
		dsp->voices[i].env_mode = ENV_RELEASE;
	}
	dsp->new_kon = dsp->regs[0x4c];
	dsp->dir_table_address = dsp->regs[0x5d];
	dsp->echo_ring_buffer_addr = dsp->regs[0x6d];
	dsp_reset(dsp);
}

void dsp_reset(DSP *dsp) {
	dsp->regs[0x6c] = 0xe0;
	dsp->cycle = 0;
	dsp->counter = 0;
	dsp->echo_history_pos = 0;
	dsp->echo_offset = 0;
	dsp->every_other_sample = 1;
	dsp->noise_lfsr = 0x4000;
	dsp->sample_buf_pos = 0;
	dsp->main_out[0] = 0;
	dsp->main_out[1] = 0;
	dsp->echo_in[0] = 0;
	dsp->echo_in[1] = 0;
	dsp->echo_out[0] = 0;
	dsp->echo_out[1] = 0;
	memset(dsp->echo_history, 0, sizeof(dsp->echo_history));
	for (int i = 0; i < 8; i++) {
		dsp->voices[i].env_volume = 0;
		dsp->voices[i].prev_calc_env = 0;
		dsp->voices[i].interp_pos = 0;
		dsp->voices[i].env_mode = ENV_RELEASE;
		dsp->voices[i].brr_addr = 0;
		dsp->voices[i].brr_offset = 1;
		dsp->voices[i].key_on_delay = 0;
		dsp->voices[i].env_out = 0;
		dsp->voices[i].buffer_pos = 0;
		memset(dsp->voices[i].sample_buffer, 0, sizeof(dsp->voices[i].sample_buffer));
	}
}

uint8_t dsp_read(DSP *dsp) {
	uint8_t addr = dsp->addr & 0x7f;
	if (addr == 0x4c) return 0;
	return dsp->regs[addr];
}

void dsp_write(DSP *dsp, uint8_t val) {
	uint8_t addr = dsp->addr & 0x7f;
	dsp->regs[addr] = val;

	switch (addr & 0x0f) {
		case 0x08: dsp->env_reg_buffer = val; break;
		case 0x09: dsp->out_reg_buffer = val; break;
		case 0x0c:
			if (addr == 0x4c) {
				dsp->new_kon = val;
			} else if (addr == 0x7c) {
				dsp->voice_end_buffer = 0;
				dsp->regs[0x7c] = 0;
			}
			break;
	}
}

static inline int16_t dsp_interpolate_gauss(int32_t interp_pos, const int16_t *samples, uint8_t buf_pos) {
	uint8_t pos = (interp_pos >> 12) + buf_pos;
	uint8_t offset = (interp_pos >> 4) & 0xff;

	int32_t out = (int16_t)(
		((GAUSS_TABLE[255 - offset] * (int32_t)samples[pos % 12]) >> 11) +
		((GAUSS_TABLE[511 - offset] * (int32_t)samples[(pos + 1) % 12]) >> 11) +
		((GAUSS_TABLE[256 + offset] * (int32_t)samples[(pos + 2) % 12]) >> 11)
	) + ((GAUSS_TABLE[offset] * (int32_t)samples[(pos + 3) % 12]) >> 11);

	return clamp16(out) & ~0x01;
}

static void decode_brr_group(DSP *dsp, DSPVoice *voice) {
	uint16_t next_addr = (voice->brr_addr + voice->brr_offset + 1) & 0xffff;
	uint8_t next_data = dsp->ram[next_addr];

	int16_t samples[4] = {
		(int16_t)((int16_t)((dsp->brr_data & 0xf0) << 8) >> 12),
		(int16_t)((int16_t)((dsp->brr_data & 0x0f) << 12) >> 12),
		(int16_t)((int16_t)((next_data & 0xf0) << 8) >> 12),
		(int16_t)((int16_t)((next_data & 0x0f) << 12) >> 12)
	};

	int shift = dsp->brr_header >> 4;
	int filter = dsp->brr_header & 0x0c;

	int32_t prev1 = voice->sample_buffer[voice->buffer_pos > 0 ? voice->buffer_pos - 1 : 11] >> 1;
	int32_t prev2 = voice->sample_buffer[voice->buffer_pos > 1 ? voice->buffer_pos - 2 : 10] >> 1;

	for (int i = 0; i < 4; i++) {
		int32_t s = ((int32_t)samples[i] << shift) >> 1;
		if (shift >= 0x0d) {
			s = (s < 0) ? -0x800 : 0;
		}

		switch (filter) {
			case 0x00: break;
			case 0x04: s += prev1 + (-prev1 >> 4); break;
			case 0x08: s += (prev1 << 1) + ((-((prev1 << 1) + prev1)) >> 5) - prev2 + (prev2 >> 4); break;
			case 0x0c: s += (prev1 << 1) + ((-(prev1 + (prev1 << 2) + (prev1 << 3))) >> 6) - prev2 + (((prev2 << 1) + prev2) >> 4); break;
		}

		voice->sample_buffer[voice->buffer_pos + i] = clamp16(s) * 2;
		prev2 = prev1;
		prev1 = voice->sample_buffer[voice->buffer_pos + i] >> 1;
	}

	if (voice->buffer_pos <= 4) {
		voice->buffer_pos += 4;
	} else {
		voice->buffer_pos = 0;
	}
}

static void process_envelope(DSP *dsp, DSPVoice *voice, int v) {
	int32_t env = voice->env_volume;
	if (voice->env_mode == ENV_RELEASE) {
		env -= 8;
		voice->env_volume = (env < 0) ? 0 : env;
	} else {
		int32_t rate = 0;
		uint8_t sustain = 0;

		if (dsp->adsr1 & 0x80) { // adsr mode
			uint8_t adsr2 = dsp->regs[(v << 4) | 0x06];
			sustain = adsr2;
			switch (voice->env_mode) {
				case ENV_ATTACK:
					if ((dsp->adsr1 & 0x0f) == 0x0f) {
						rate = 31;
						env += 1024;
					} else {
						rate = ((dsp->adsr1 & 0x0f) << 1) | 0x01;
						env += 32;
					}
					break;
				case ENV_DECAY:
					env -= ((env - 1) >> 8) + 1;
					rate = ((dsp->adsr1 >> 3) & 0x0e) | 0x10;
					break;
				case ENV_SUSTAIN:
					env -= ((env - 1) >> 8) + 1;
					rate = adsr2 & 0x1f;
					break;
				default: break;
			}
		} else { // gain mode
			uint8_t gain = dsp->regs[(v << 4) | 0x07];
			sustain = gain;
			if (gain & 0x80) {
				rate = gain & 0x1f;
				switch (gain & 0x60) {
					case 0x00: env -= 32; break;
					case 0x20: env -= ((env - 1) >> 8) + 1; break;
					case 0x40: env += 32; break;
					case 0x60: env += ((uint16_t)voice->prev_calc_env < 0x600 ? 32 : 8); break;
				}
			} else {
				env = (uint16_t)gain << 4;
				rate = 31;
			}
		}

		if (voice->env_mode == ENV_DECAY && (env >> 8) == (sustain >> 5)) {
			voice->env_mode = ENV_SUSTAIN;
		}

		voice->prev_calc_env = env;

		if (env < 0 || env > 0x7ff) {
			env = (env < 0) ? 0 : 0x7ff;
			if (voice->env_mode == ENV_ATTACK) {
				voice->env_mode = ENV_DECAY;
			}
		}

		if (dsp_check_counter(dsp, rate)) {
			voice->env_volume = env;
		}
	}
}

void voice_s1(DSP *dsp, int v) {
	dsp->sample_address = (dsp->dir_table_address << 8) + (dsp->source_number << 2);
	dsp->source_number = dsp->regs[(v << 4) | 0x04];
}

void voice_s2(DSP *dsp, int v) {
	uint16_t addr = dsp->sample_address;
	if (dsp->voices[v].key_on_delay == 0) {
		addr += 2; // loop address
	}

	dsp->brr_next_address = dsp->ram[addr & 0xffff] | (dsp->ram[(addr + 1) & 0xffff] << 8);
	dsp->adsr1 = dsp->regs[(v << 4) | 0x05];
	dsp->pitch = dsp->regs[(v << 4) | 0x02];
}

void voice_s3a(DSP *dsp, int v) {
	dsp->pitch |= (dsp->regs[(v << 4) | 0x03] & 0x3f) << 8;
}

void voice_s3b(DSP *dsp, int v) {
	dsp->brr_header = dsp->ram[dsp->voices[v].brr_addr & 0xffff];
	dsp->brr_data = dsp->ram[(dsp->voices[v].brr_addr + dsp->voices[v].brr_offset) & 0xffff];
}

void voice_s3c(DSP *dsp, int v) {
	DSPVoice *voice = &dsp->voices[v];

	// pitch modulation
	if (dsp->pitch_mod_on & (1 << v)) {
		dsp->pitch += ((dsp->voice_output >> 5) * dsp->pitch) >> 10;
	}

	if (voice->key_on_delay) {
		if (voice->key_on_delay == 5) {
			voice->brr_addr = dsp->brr_next_address;
			voice->brr_offset = 1;
			voice->buffer_pos = 0;
			dsp->brr_header = 0;
		}

		voice->env_volume = 0;
		voice->prev_calc_env = 0;
		voice->key_on_delay--;
		voice->interp_pos = (voice->key_on_delay & 0x03) ? 0x4000 : 0;
		dsp->pitch = 0;
	}

	int32_t output = dsp_interpolate_gauss(voice->interp_pos, voice->sample_buffer, voice->buffer_pos);
	if (dsp->noise_on & (1 << v)) {
		output = (int16_t)(dsp->noise_lfsr * 2);
	}

	dsp->voice_output = ((output * voice->env_volume) >> 11) & ~0x01;
	voice->env_out = (uint8_t)(voice->env_volume >> 4);

	if ((dsp->regs[0x6c] & 0x80) || (dsp->brr_header & 0x03) == 0x01) {
		voice->env_mode = ENV_RELEASE;
		voice->env_volume = 0;
	}

	if (dsp->every_other_sample) {
		if (dsp->key_off & (1 << v)) {
			voice->env_mode = ENV_RELEASE;
		}
		if (dsp->key_on & (1 << v)) {
			voice->key_on_delay = 5;
			voice->env_mode = ENV_ATTACK;
		}
	}

	if (!voice->key_on_delay) {
		process_envelope(dsp, voice, v);
	}
}

void voice_s3(DSP *dsp, int v) {
	voice_s3a(dsp, v);
	voice_s3b(dsp, v);
	voice_s3c(dsp, v);
}

void voice_s4(DSP *dsp, int v) {
	DSPVoice *voice = &dsp->voices[v];
	dsp->looped = 0;

	if (voice->interp_pos >= 0x4000) {
		decode_brr_group(dsp, voice);

		if (voice->brr_offset >= 7) {
			if (dsp->brr_header & 0x01) {
				voice->brr_addr = dsp->brr_next_address;
				dsp->looped = (1 << v);
			} else {
				voice->brr_addr += 9;
			}
			voice->brr_offset = 1;
		} else {
			voice->brr_offset += 2;
		}
	}

	voice->interp_pos = (voice->interp_pos & 0x3fff) + dsp->pitch;
	if (voice->interp_pos > 0x7fff) {
		voice->interp_pos = 0x7fff;
	}

	// voll
	int8_t voll = (int8_t)dsp->regs[(v << 4) | 0x00];
	int32_t voice_out = ((int32_t)dsp->voice_output * voll) >> 7;
	dsp->main_out[0] = clamp16(dsp->main_out[0] + voice_out);
	if (dsp->echo_on & (1 << v)) {
		dsp->echo_out[0] = clamp16(dsp->echo_out[0] + voice_out);
	}
}

void voice_s5(DSP *dsp, int v) {
	DSPVoice *voice = &dsp->voices[v];

	// volr
	int8_t volr = (int8_t)dsp->regs[(v << 4) | 0x01];
	int32_t voice_out = ((int32_t)dsp->voice_output * volr) >> 7;
	dsp->main_out[1] = clamp16(dsp->main_out[1] + voice_out);
	if (dsp->echo_on & (1 << v)) {
		dsp->echo_out[1] = clamp16(dsp->echo_out[1] + voice_out);
	}

	// update endx
	uint8_t voice_end = dsp->regs[0x7c] | dsp->looped;
	if (voice->key_on_delay == 5) {
		voice_end &= ~(1 << v);
	}
	dsp->voice_end_buffer = voice_end;
}

void voice_s6(DSP *dsp, int v) {
	(void)v;
	dsp->out_reg_buffer = (uint8_t)(dsp->voice_output >> 8);
}

void voice_s7(DSP *dsp, int v) {
	dsp->regs[0x7c] = dsp->voice_end_buffer;
	dsp->env_reg_buffer = dsp->voices[v].env_out;
}

void voice_s8(DSP *dsp, int v) {
	dsp->regs[(v << 4) | 0x09] = dsp->out_reg_buffer;
}

void voice_s9(DSP *dsp, int v) {
	dsp->regs[(v << 4) | 0x08] = dsp->env_reg_buffer;
}

static inline int32_t dsp_calculate_fir(DSP *dsp, int index, int ch) {
	int16_t echo_history = dsp->echo_history[(dsp->echo_history_pos + index + 1) & 0x07][ch];
	int8_t coeff = (int8_t)dsp->regs[0x0f + (index << 4)];
	return (echo_history * coeff) >> 6;
}

static void dsp_echo_step22(DSP *dsp) {
	dsp->echo_history_pos = (dsp->echo_history_pos + 1) & 0x07;
	dsp->echo_pointer = (dsp->echo_ring_buffer_addr << 8) + dsp->echo_offset;

	int16_t s = dsp->ram[dsp->echo_pointer & 0xffff] | (dsp->ram[(dsp->echo_pointer + 1) & 0xffff] << 8);
	dsp->echo_history[dsp->echo_history_pos][0] = s >> 1;

	dsp->echo_in[0] = dsp_calculate_fir(dsp, 0, 0);
	dsp->echo_in[1] = dsp_calculate_fir(dsp, 0, 1);
}

static void dsp_echo_step23(DSP *dsp) {
	int16_t s = dsp->ram[(dsp->echo_pointer + 2) & 0xffff] | (dsp->ram[(dsp->echo_pointer + 3) & 0xffff] << 8);
	dsp->echo_history[dsp->echo_history_pos][1] = s >> 1;

	dsp->echo_in[0] += dsp_calculate_fir(dsp, 1, 0) + dsp_calculate_fir(dsp, 2, 0);
	dsp->echo_in[1] += dsp_calculate_fir(dsp, 1, 1) + dsp_calculate_fir(dsp, 2, 1);
}

static void dsp_echo_step24(DSP *dsp) {
	dsp->echo_in[0] += dsp_calculate_fir(dsp, 3, 0) + dsp_calculate_fir(dsp, 4, 0) + dsp_calculate_fir(dsp, 5, 0);
	dsp->echo_in[1] += dsp_calculate_fir(dsp, 3, 1) + dsp_calculate_fir(dsp, 4, 1) + dsp_calculate_fir(dsp, 5, 1);
}

static void dsp_echo_step25(DSP *dsp) {
	int32_t left = (int16_t)(dsp->echo_in[0] + dsp_calculate_fir(dsp, 6, 0));
	int32_t right = (int16_t)(dsp->echo_in[1] + dsp_calculate_fir(dsp, 6, 1));

	dsp->echo_in[0] = clamp16(left + (int16_t)dsp_calculate_fir(dsp, 7, 0)) & ~0x01;
	dsp->echo_in[1] = clamp16(right + (int16_t)dsp_calculate_fir(dsp, 7, 1)) & ~0x01;
}

static void dsp_echo_step26(DSP *dsp) {
	dsp->main_out[0] = clamp16(
		((dsp->main_out[0] * (int8_t)dsp->regs[0x0c]) >> 7) +
		((dsp->echo_in[0] * (int8_t)dsp->regs[0x2c]) >> 7));

	int32_t left_echo = dsp->echo_out[0] + (int16_t)((dsp->echo_in[0] * (int8_t)dsp->regs[0x0d]) >> 7);
	int32_t right_echo = dsp->echo_out[1] + (int16_t)((dsp->echo_in[1] * (int8_t)dsp->regs[0x0d]) >> 7);
	dsp->echo_out[0] = clamp16(left_echo) & ~0x01;
	dsp->echo_out[1] = clamp16(right_echo) & ~0x01;
}

static void dsp_echo_step27(DSP *dsp) {
	dsp->main_out[1] = clamp16(
		((dsp->main_out[1] * (int8_t)dsp->regs[0x1c]) >> 7) +
		((dsp->echo_in[1] * (int8_t)dsp->regs[0x3c]) >> 7));
}

static void dsp_echo_step28(DSP *dsp) {
	dsp->echo_enabled = (dsp->regs[0x6c] & 0x20) == 0;
}

static void dsp_echo_step29(DSP *dsp) {
	if (dsp->echo_offset == 0) {
		dsp->echo_length = (dsp->regs[0x7d] & 0x0f) << 11;
	}

	dsp->echo_offset += 4;
	if (dsp->echo_offset >= dsp->echo_length) {
		dsp->echo_offset = 0;
	}

	if (dsp->echo_enabled) {
		dsp->ram[dsp->echo_pointer & 0xffff] = (uint8_t)dsp->echo_out[0];
		dsp->ram[(dsp->echo_pointer + 1) & 0xffff] = (uint8_t)(dsp->echo_out[0] >> 8);
	}
	dsp->echo_out[0] = 0;

	dsp->echo_ring_buffer_addr = dsp->regs[0x6d];
	dsp->echo_enabled = (dsp->regs[0x6c] & 0x20) == 0;
}

static void dsp_echo_step30(DSP *dsp) {
	if (dsp->echo_enabled) {
		dsp->ram[(dsp->echo_pointer + 2) & 0xffff] = (uint8_t)dsp->echo_out[1];
		dsp->ram[(dsp->echo_pointer + 3) & 0xffff] = (uint8_t)(dsp->echo_out[1] >> 8);
	}
	dsp->echo_out[1] = 0;
}

void dsp_step(DSP *dsp) {
	// based on anomie's s-dsp doc
	switch (dsp->cycle & 31) {
		case  0: voice_s5(dsp, 0); voice_s2(dsp, 1); break;
		case  1: voice_s6(dsp, 0); voice_s3(dsp, 1); break;
		case  2: voice_s7(dsp, 0); voice_s4(dsp, 1); voice_s1(dsp, 3); break;
		case  3: voice_s8(dsp, 0); voice_s5(dsp, 1); voice_s2(dsp, 2); break;
		case  4: voice_s9(dsp, 0); voice_s6(dsp, 1); voice_s3(dsp, 2); break;
		case  5: voice_s7(dsp, 1); voice_s4(dsp, 2); voice_s1(dsp, 4); break;
		case  6: voice_s8(dsp, 1); voice_s5(dsp, 2); voice_s2(dsp, 3); break;
		case  7: voice_s9(dsp, 1); voice_s6(dsp, 2); voice_s3(dsp, 3); break;
		case  8: voice_s7(dsp, 2); voice_s4(dsp, 3); voice_s1(dsp, 5); break;
		case  9: voice_s8(dsp, 2); voice_s5(dsp, 3); voice_s2(dsp, 4); break;
		case 10: voice_s9(dsp, 2); voice_s6(dsp, 3); voice_s3(dsp, 4); break;
		case 11: voice_s7(dsp, 3); voice_s4(dsp, 4); voice_s1(dsp, 6); break;
		case 12: voice_s8(dsp, 3); voice_s5(dsp, 4); voice_s2(dsp, 5); break;
		case 13: voice_s9(dsp, 3); voice_s6(dsp, 4); voice_s3(dsp, 5); break;
		case 14: voice_s7(dsp, 4); voice_s4(dsp, 5); voice_s1(dsp, 7); break;
		case 15: voice_s8(dsp, 4); voice_s5(dsp, 5); voice_s2(dsp, 6); break;
		case 16: voice_s9(dsp, 4); voice_s6(dsp, 5); voice_s3(dsp, 6); break;
		case 17: voice_s1(dsp, 0); voice_s7(dsp, 5); voice_s4(dsp, 6); break;
		case 18: voice_s8(dsp, 5); voice_s5(dsp, 6); voice_s2(dsp, 7); break;
		case 19: voice_s9(dsp, 5); voice_s6(dsp, 6); voice_s3(dsp, 7); break;
		case 20: voice_s1(dsp, 1); voice_s7(dsp, 6); voice_s4(dsp, 7); break;
		case 21: voice_s8(dsp, 6); voice_s5(dsp, 7); voice_s2(dsp, 0); break;
		case 22: voice_s3a(dsp, 0); voice_s9(dsp, 6); voice_s6(dsp, 7); dsp_echo_step22(dsp); break;
		case 23: voice_s7(dsp, 7); dsp_echo_step23(dsp); break;
		case 24: voice_s8(dsp, 7); dsp_echo_step24(dsp); break;
		case 25: voice_s3b(dsp, 0); voice_s9(dsp, 7); dsp_echo_step25(dsp); break;
		case 26: dsp_echo_step26(dsp); break;
		case 27:
			dsp->pitch_mod_on = dsp->regs[0x2d] & 0xfe;
			dsp_echo_step27(dsp);

			int16_t sample_l, sample_r;
			if (dsp->regs[0x6c] & 0x40) {
				sample_l = 0;
				sample_r = 0;
			} else {
				sample_l = (int16_t)dsp->main_out[0];
				sample_r = (int16_t)dsp->main_out[1];
			}
			dsp->main_out[0] = 0;
			dsp->main_out[1] = 0;

			if (dsp->sample_buf_pos + 2 <= 2048) {
				dsp->sample_buf[dsp->sample_buf_pos++] = sample_l;
				dsp->sample_buf[dsp->sample_buf_pos++] = sample_r;
			}
			if (dsp->sample_buf_pos >= 2048) {
				dsp_flush(dsp);
			}
			break;

		case 28:
			dsp->dir_table_address = dsp->regs[0x5d];
			dsp->noise_on = dsp->regs[0x3d];
			dsp->echo_on = dsp->regs[0x4d];
			dsp_echo_step28(dsp);
			break;

		case 29:
			dsp->every_other_sample ^= 1;
			if (dsp->every_other_sample) {
				dsp->new_kon &= ~dsp->key_on;
			}
			dsp_echo_step29(dsp);
			break;

		case 30:
			if (dsp->every_other_sample) {
				dsp->key_on = dsp->new_kon;
				dsp->key_off = dsp->regs[0x5c];
			}

			if (dsp->counter == 0) {
				dsp->counter = 0x77ff;
			} else {
				dsp->counter--;
			}

			if (dsp_check_counter(dsp, dsp->regs[0x6c] & 0x1f)) {
				int16_t feedback = ((dsp->noise_lfsr << 14) ^ (dsp->noise_lfsr << 13)) & 0x4000;
				dsp->noise_lfsr = feedback ^ (dsp->noise_lfsr >> 1);
			}

			voice_s3c(dsp, 0);
			dsp_echo_step30(dsp);
			break;

		case 31:
			voice_s4(dsp, 0);
			voice_s1(dsp, 2);
			break;
	}
	dsp->cycle++;
}

void dsp_flush(DSP *dsp) {
	if (dsp->sample_buf_pos > 0) {
		host_queue_audio_buffer(dsp->sample_buf, dsp->sample_buf_pos);
		dsp->sample_buf_pos = 0;
	}
}
