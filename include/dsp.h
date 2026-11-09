#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum EnvelopeMode {
	ENV_RELEASE,
	ENV_ATTACK,
	ENV_DECAY,
	ENV_SUSTAIN
} EnvelopeMode;

typedef struct DSPVoice {
	EnvelopeMode env_mode;
	int32_t env_volume;
	int32_t prev_calc_env;
	int32_t interp_pos;

	uint16_t brr_addr;
	uint16_t brr_offset;
	uint8_t key_on_delay;
	uint8_t env_out;
	uint8_t buffer_pos;

	int16_t sample_buffer[12];
} DSPVoice;

typedef struct DSP {
	uint8_t *ram; // 64kb spc ram
	uint8_t regs[128];
	uint8_t addr;

	DSPVoice voices[8];

	uint8_t new_kon;
	uint8_t key_on;
	uint8_t key_off;
	uint8_t every_other_sample;
	uint8_t looped;
	uint8_t pitch_mod_on;
	uint8_t noise_on;
	uint8_t echo_on;

	uint8_t out_reg_buffer;
	uint8_t env_reg_buffer;
	uint8_t voice_end_buffer;

	int32_t main_out[2];
	int32_t echo_in[2];
	int32_t echo_out[2];

	int16_t echo_history[8][2];
	uint8_t echo_history_pos;
	uint16_t echo_pointer;
	uint16_t echo_offset;
	uint16_t echo_length;
	uint8_t echo_ring_buffer_addr;
	bool echo_enabled;

	int16_t noise_lfsr;

	uint16_t counter;
	uint64_t cycle;

	// latched pipeline registers
	int32_t pitch;
	int32_t voice_output;
	uint16_t sample_address;
	uint16_t brr_next_address;
	uint8_t dir_table_address;
	uint8_t source_number;
	uint8_t brr_header;
	uint8_t brr_data;
	uint8_t adsr1;

	int16_t sample_buf[2048];
	size_t sample_buf_pos;
} DSP;

void dsp_init(DSP *dsp, uint8_t *ram);
void dsp_reset(DSP *dsp);
uint8_t dsp_read(DSP *dsp);
void dsp_write(DSP *dsp, uint8_t val);
void dsp_step(DSP *dsp);
void dsp_flush(DSP *dsp);
