#include <stdio.h>

#include "cart.h"
#include "cpu.h"
#include "spc.h"
#include "bus.h"
#include "ppu.h"
#include "host.h"

int loglevel = 3;

int main(int argc, char **argv) {

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <rom.sfc>\n", argv[0]);
		return 1;
	}

	Cart cart = {0};

	if (!cart_init(&cart, argv[1])) {
		fprintf(stderr, "Failed to load cart: %s\n", argv[1]);
		return 1;
	}

	int hecode = host_init();

	if (hecode) {
		fprintf(stderr, "Failed to init host interface: %d\n", hecode);
		return 1;
	}

	// declare components
	SPC spc = {0};
	PPU ppu = {0};
	Bus bus = {
		.spc = &spc,
		.ppu = &ppu,
		.cart = &cart
	};
	CPU cpu = { .bus = &bus };

	bus.cpu = &cpu;
	ppu.bus = &bus;

	bus_poweron(&bus);

	// init components
	bus_init(&bus);
	cpu_init(&cpu);
	ppu_init(&ppu);

	while (1) {
		// vblank
		cpu_run(&cpu);
		dsp_flush(&spc.dsp);
		host_frame();
		// controller state update
		EmuCtlEvent e;
		while (host_get_event(&e)) {
			switch (e.type) {
			case E_EXIT:
				bus_free(&bus);
				return 0;
			case E_RESET:
				bus_reset(&bus);
				host_send_info(I_INFO, "reset system");
				break;
			default:
				break;
			}
		}
	}
}
