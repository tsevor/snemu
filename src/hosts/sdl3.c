#include "host.h"
#include "log.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL3/SDL.h>

#define SNES_WIDTH 256
#define SNES_HEIGHT 240

#define PIXEL_SCALE 3

#define MAX_EVENTS 16

uint32_t *host_screen_buffer = NULL;

static struct {
	uint32_t *buffer_front;
	uint32_t *buffer_back;
	uint8_t controller;

	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;

	SDL_AudioStream *audio_stream;

	struct {
		bool sprint;
	} binds;

	EmuCtlEvent event_queue[MAX_EVENTS];
	int event_head;
	int event_tail;

	SDL_Time last_time;
} host = {0};

static void push_event(EmuCtlEventType type, void *arg) {
	int next_tail = (host.event_tail + 1) % MAX_EVENTS;
	if (next_tail == host.event_head) {
		lhost("host event queue full, dropping event type: %d\n", type);
		return;
	}
	host.event_queue[host.event_tail].type = type;
	host.event_queue[host.event_tail].arg = arg;
	host.event_tail = next_tail;
}


int host_init() {

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
		lerror("SDL_Init failed: %s\n", SDL_GetError());
		return -1;
	}

	host.buffer_front = malloc(SNES_WIDTH * SNES_HEIGHT * sizeof(uint32_t));
	host.buffer_back = malloc(SNES_WIDTH * SNES_HEIGHT * sizeof(uint32_t));
	host_screen_buffer = host.buffer_back;

	// window
	host.window = SDL_CreateWindow(
		"snemu",
		SNES_WIDTH * PIXEL_SCALE,
		SNES_HEIGHT * PIXEL_SCALE,
		SDL_WINDOW_RESIZABLE
	);
	if (!host.window) {
		lerror("sdl: window creation failed: %s\n", SDL_GetError());
		return -1;
	}

	// renderer
	host.renderer = SDL_CreateRenderer(host.window, NULL);
	if (!host.renderer) {
		lerror("sdl: renderer creation failed: %s\n", SDL_GetError());
		return -1;
	}
	SDL_SetRenderLogicalPresentation(
		host.renderer,
		SNES_WIDTH,
		SNES_HEIGHT,
		SDL_LOGICAL_PRESENTATION_LETTERBOX
	);

	// texture
	host.texture = SDL_CreateTexture(
		host.renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		SNES_WIDTH,
		SNES_HEIGHT
	);
	if (!host.texture) {
		lerror("sdl: texture creation failed: %s\n", SDL_GetError());
		return -1;
	}
	SDL_SetTextureScaleMode(host.texture, SDL_SCALEMODE_NEAREST);

	// audio
	SDL_AudioSpec spec = {
		.freq = 32000,
		.format = SDL_AUDIO_S16,
		.channels = 1,
	};

	host.audio_stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&spec,
		NULL,
		NULL
	);
	if (!host.audio_stream) {
		lerror("sdl: audio stream creation failed: %s\n", SDL_GetError());
	} else
		SDL_ResumeAudioStreamDevice(host.audio_stream);

	host.last_time = SDL_GetTicksNS();

	return 0;
}

static void capture_screenshot(void) {
	SDL_Surface *surface = SDL_CreateSurfaceFrom(
		SNES_WIDTH,
		SNES_HEIGHT,
		SDL_PIXELFORMAT_ARGB8888,
		host.buffer_front,
		SNES_WIDTH * sizeof(uint32_t)
	);
	if (!surface) {
		lerror("screenshot: failed to create surface: %s\n", SDL_GetError());
		return;
	}

	char filename[64];
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	char base[48];
	if (tm) {
		strftime(base, sizeof(base), "screenshot-%Y%m%d-%H%M%S", tm);
	} else {
		snprintf(base, sizeof(base), "screenshot");
	}
	snprintf(filename, sizeof(filename), "%s.png", base);
	int index = 1;
	FILE *f;
	while ((f = fopen(filename, "r")) != NULL) {
		fclose(f);
		snprintf(filename, sizeof(filename), "%s-%d.png", base, index++);
	}

	if (SDL_SavePNG(surface, filename)) {
		lhost("screenshot: saved to %s\n", filename);
	} else {
		lerror("screenshot: failed to save %s: %s\n", filename, SDL_GetError());
	}

	SDL_DestroySurface(surface);
}

void host_frame() {

	SDL_UpdateTexture(host.texture, NULL, host.buffer_front, SNES_WIDTH * sizeof(uint32_t));

    SDL_RenderClear(host.renderer);
    SDL_RenderTexture(host.renderer, host.texture, NULL, NULL);
    SDL_RenderPresent(host.renderer);

	int sleep_time = (1000000000 / 60) - (SDL_GetTicksNS() - host.last_time);
	if (!host.binds.sprint && sleep_time > 0)
		SDL_DelayNS(sleep_time);
	else
		SDL_ClearAudioStream(host.audio_stream);
	host.last_time = SDL_GetTicksNS();

	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_EVENT_QUIT:
				host_exit();
				push_event(E_EXIT, NULL);
				return;
			case SDL_EVENT_KEY_DOWN:
				if (e.key.repeat) continue;
				switch (e.key.key) {
					case SDLK_R:
						push_event(E_RESET, NULL);
						SDL_ClearAudioStream(host.audio_stream);
						break;
					case SDLK_P:
						capture_screenshot();
						break;
					default:
						break;
				}
			default:
				break;
		}
	}

	const bool* kb_state = SDL_GetKeyboardState(NULL);

	host.controller = (
		  (kb_state[SDL_SCANCODE_S]     ? BUTTON_A      : 0)
		| (kb_state[SDL_SCANCODE_A]     ? BUTTON_B      : 0)
		| (kb_state[SDL_SCANCODE_Q]     ? BUTTON_SELECT : 0)
		| (kb_state[SDL_SCANCODE_W]     ? BUTTON_START  : 0)
		| (kb_state[SDL_SCANCODE_UP]    ? BUTTON_UP     : 0)
		| (kb_state[SDL_SCANCODE_DOWN]  ? BUTTON_DOWN   : 0)
		| (kb_state[SDL_SCANCODE_LEFT]  ? BUTTON_LEFT   : 0)
		| (kb_state[SDL_SCANCODE_RIGHT] ? BUTTON_RIGHT  : 0)
	);

	host.binds.sprint = kb_state[SDL_SCANCODE_RIGHTBRACKET];
}

void host_exit() {
	if (host.audio_stream) {
		SDL_DestroyAudioStream(host.audio_stream);
		host.audio_stream = NULL;
	}
	if (host.texture) {
		SDL_DestroyTexture(host.texture);
		host.texture = NULL;
	}
	if (host.renderer) {
		SDL_DestroyRenderer(host.renderer);
		host.renderer = NULL;
	}
	if (host.window) {
		SDL_DestroyWindow(host.window);
		host.window = NULL;
	}
	if (host.buffer_front) {
		free(host.buffer_front);
		host.buffer_front = NULL;
	}
	if (host.buffer_back) {
		free(host.buffer_back);
		host.buffer_back = NULL;
	}
	SDL_Quit();
}

void host_swap_buffer() {
	uint32_t *temp = host.buffer_back;
	host.buffer_back = host.buffer_front;
	host.buffer_front = temp;
	host_screen_buffer = host.buffer_back;
}

void host_copy_buffer() {
	mempcpy(host.buffer_front, host.buffer_back, SNES_WIDTH * SNES_HEIGHT * sizeof(uint32_t));
}

void host_queue_audio_buffer(int16_t *buffer, size_t len) {
	if (host.audio_stream) {
		SDL_PutAudioStreamData(host.audio_stream, buffer, len * sizeof(uint16_t));
	}
}

void host_send_info(InfoType type, char *str) {
	(void)type;
	linfo("%s\n", str);

	// display toast
}

bool host_get_event(EmuCtlEvent *event) {
	if (host.event_head == host.event_tail) {
		event->type = E_NONE;
		event->arg = NULL;
		return false;
	}

	*event = host.event_queue[host.event_head];
	host.event_head = (host.event_head + 1) % MAX_EVENTS;
	return true;
}

uint8_t host_get_controller() {
	return host.controller;
}
