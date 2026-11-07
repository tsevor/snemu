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
	// TODO: fill in
} DSPVoice;

typedef struct DSP {
	uint8_t *ram; // 64kb spc ram

	uint8_t regs[128];
	uint8_t addr;

	DSPVoice voices[8];

	int16_t sample_buf[2048];
	size_t sample_buf_pos;
} DSP;

void dsp_init(DSP *dsp, uint8_t *ram);
void dsp_reset(DSP *dsp);
uint8_t dsp_read(DSP *dsp, uint8_t reg);
void dsp_write(DSP *dsp, uint8_t reg, uint8_t val);
void dsp_step(DSP *dsp);
void dsp_flush(DSP *dsp);
