#include "host.h"
#include "log.h"

#include <gtk/gtk.h>
#include <adwaita.h>
#include <stdlib.h>
#include <string.h>

#define NES_WIDTH 256
#define NES_HEIGHT 240
#define MAX_EVENTS 16

uint32_t *host_screen_buffer = NULL;

static struct {
	uint32_t *buffer_front;
	uint32_t *buffer_back;
	uint8_t controller;
	GtkWidget *window;
	GtkWidget *canvas;
	GtkWidget *toast;
	AdwToast *active_toast;

	struct {
		bool sprint;
	} binds;

	EmuCtlEvent event_queue[MAX_EVENTS];
	int event_head;
	int event_tail;

	uint64_t last_time;
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

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer data) {
	(void)controller;
	(void)keycode;
	(void)state;
	(void)data;

	switch (keyval) {
		// controller
		case GDK_KEY_s:     host.controller |= BUTTON_A;      break;
		case GDK_KEY_a:     host.controller |= BUTTON_B;      break;
		case GDK_KEY_q:     host.controller |= BUTTON_SELECT; break;
		case GDK_KEY_w:     host.controller |= BUTTON_START;  break;
		case GDK_KEY_Up:    host.controller |= BUTTON_UP;     break;
		case GDK_KEY_Down:  host.controller |= BUTTON_DOWN;   break;
		case GDK_KEY_Left:  host.controller |= BUTTON_LEFT;   break;
		case GDK_KEY_Right: host.controller |= BUTTON_RIGHT;  break;
		// emu control once
		case GDK_KEY_r: push_event(E_RESET, NULL); break;
		// emu control held
		case GDK_KEY_bracketright: host.binds.sprint = true; break;
		// no key
		default:
			return FALSE;
	}
	return TRUE;
}

static gboolean on_key_released(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer data) {
	(void)controller;
	(void)keycode;
	(void)state;
	(void)data;

	switch (keyval) {
		// controller
		case GDK_KEY_s:     host.controller &= ~BUTTON_A;      break;
		case GDK_KEY_a:     host.controller &= ~BUTTON_B;      break;
		case GDK_KEY_q:     host.controller &= ~BUTTON_SELECT; break;
		case GDK_KEY_w:     host.controller &= ~BUTTON_START;  break;
		case GDK_KEY_Up:    host.controller &= ~BUTTON_UP;     break;
		case GDK_KEY_Down:  host.controller &= ~BUTTON_DOWN;   break;
		case GDK_KEY_Left:  host.controller &= ~BUTTON_LEFT;   break;
		case GDK_KEY_Right: host.controller &= ~BUTTON_RIGHT;  break;
		// emu control held
		case GDK_KEY_bracketright: host.binds.sprint = false; break;
		// no key
		default:
			return FALSE;
	}
	return TRUE;
}

static void on_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
	(void)area;
	(void)data;

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_paint(cr);

	double scale_x = (double)width / NES_WIDTH;
	double scale_y = (double)height / NES_HEIGHT;
	double scale = (scale_x < scale_y) ? scale_x : scale_y;

	double offset_x = (width - (NES_WIDTH * scale)) / 2.0;
	double offset_y = (height - (NES_HEIGHT * scale)) / 2.0;

	cairo_translate(cr, offset_x, offset_y);
	cairo_scale(cr, scale, scale);

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		(unsigned char *)host.buffer_front,
		CAIRO_FORMAT_ARGB32,
		NES_WIDTH,
		NES_HEIGHT,
		NES_WIDTH * 4
	);

	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
	cairo_paint(cr);

	cairo_surface_destroy(surface);
}

static void on_window_close(GtkWindow *win, gpointer data) {
	(void)win;
	(void)data;

	host_exit(0);
}

int host_init() {
	lhost("gtk: init\n");

	host.buffer_front = malloc(NES_WIDTH * NES_HEIGHT * sizeof(uint32_t));
	host.buffer_back = malloc(NES_WIDTH * NES_HEIGHT * sizeof(uint32_t));
	host_screen_buffer = host.buffer_back;

	adw_init();

	host.window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(host.window), "snemu");
	gtk_window_set_default_size(GTK_WINDOW(host.window), NES_WIDTH * 2, NES_HEIGHT * 2);
	g_signal_connect(host.window, "close-request", G_CALLBACK(on_window_close), NULL);

	host.canvas = gtk_drawing_area_new();
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(host.canvas), on_draw, NULL, NULL);

	host.toast = adw_toast_overlay_new();
	adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(host.toast), host.canvas);
	gtk_window_set_child(GTK_WINDOW(host.window), host.toast);

	GtkEventController *key_ctrl = gtk_event_controller_key_new();
	g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_key_pressed), NULL);
	g_signal_connect(key_ctrl, "key-released", G_CALLBACK(on_key_released), NULL);
	gtk_widget_add_controller(host.window, key_ctrl);

	gtk_window_present(GTK_WINDOW(host.window));

	host.last_time = g_get_real_time();

	return 0;
}

void host_frame() {
	gtk_widget_queue_draw(host.canvas);

	int sleep_time = (1000000 / 60) - (g_get_real_time() - host.last_time);
	if (!host.binds.sprint && sleep_time > 0) g_usleep(sleep_time);
	host.last_time = g_get_real_time();

	while (g_main_context_pending(NULL)) {
		g_main_context_iteration(NULL, FALSE);
	}
}

void host_exit(int code) {
	free(host.buffer_front);
	free(host.buffer_back);
	exit(code);
}

void host_swap_buffer() {
	uint32_t *temp = host.buffer_back;
	host.buffer_back = host.buffer_front;
	host.buffer_front = temp;
	host_screen_buffer = host.buffer_back;
}

void host_queue_audio_buffer(int16_t *buffer, size_t len) {
	(void)buffer;
	(void)len;
	// how to do in gtk?
}

void host_send_info(InfoType type, char *str) {
	(void)type;

	if (host.active_toast) {
		adw_toast_dismiss(host.active_toast);
		g_object_unref(host.active_toast);
	}

	// maybe switch type to pick icon or something in the future

	host.active_toast = adw_toast_new(str);
	g_object_ref(host.active_toast);
	adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(host.toast), host.active_toast);
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
