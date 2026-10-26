#include <stdio.h>

#include "host.h"

int loglevel = 3;

int main(int argc, char **argv) {

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <rom.sfc>\n", argv[0]);
		return 1;
	}

	int hecode = host_init();

	if (hecode) {
		fprintf(stderr, "Failed to init host interface: %d\n", hecode);
		return 1;
	}

	// declare components

	// init components

	while (1) {
		// vblank
		// cpu run
		host_frame();
		// controller state update
		EmuCtlEvent e;
		while (host_get_event(&e)) {
			switch (e.type) {
			case E_RESET:
				// bus reset
				host_send_info(I_INFO, "reset system");
				break;
			default:
				break;
			}
		}
	}
}
