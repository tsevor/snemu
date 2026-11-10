#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// everything in here should be a blind interface

// TODO: double these without performance hit
#define SNES_WIDTH 256
#define SNES_HEIGHT 224

typedef enum ControllerButton {
	BUTTON_B      = 0x8000,
	BUTTON_Y      = 0x4000,
	BUTTON_SELECT = 0x2000,
	BUTTON_START  = 0x1000,
	BUTTON_UP     = 0x0800,
	BUTTON_DOWN   = 0x0400,
	BUTTON_LEFT   = 0x0200,
	BUTTON_RIGHT  = 0x0100,
	BUTTON_A      = 0x0080,
	BUTTON_X      = 0x0040,
	BUTTON_L      = 0x0020,
	BUTTON_R      = 0x0010,
} ControllerButton;

typedef enum EmuCtlEventType {
	E_NONE,
	E_LOADROM,
	E_RESET,
	E_EXIT,
} EmuCtlEventType;

typedef struct EmuCtlEvent {
	EmuCtlEventType type;
	void* arg;
} EmuCtlEvent;

typedef enum InfoType {
	I_NONE,
	I_FAIL,
	I_INFO,
	I_MISC,
} InfoType;

extern uint32_t *host_screen_buffer;

// called on emulator start
int host_init();

// called on vblank for host event processing
void host_frame();

// called on emulator exit to free resources
void host_exit();

// swap internal buffer with exposed one
void host_swap_buffer();

// copy exposed buffer to internal one
void host_copy_buffer();

// queue 'len' stereo interlaced samples (l, r, l, r...) to play
void host_queue_audio_buffer(int16_t *buffer, size_t len);

// clears pending audio buffers
void host_clear_audio();

// sends info for host to display
void host_send_info(InfoType type, char *str);

// gets emulator event, returns true if there is one
bool host_get_event(EmuCtlEvent *event);

// returns controller state
uint16_t host_get_controller();
