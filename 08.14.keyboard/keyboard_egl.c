/////////////////////
// \author JackeyLea
// \date 2023-04-15
// \note 以EGL方式显示一个空白窗口
/////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>

#include "xdg-shell-client-protocol.h"

static struct wl_compositor *compositor = NULL;
struct xdg_wm_base *xdg_shell = NULL;
struct wl_region *region = NULL;
struct wl_surface *surface = NULL;
struct wl_seat *seat;
struct wl_pointer *pointer;
struct xdg_toplevel *toplevel;
struct wl_keyboard *keyboard;
struct xkb_context *xkb_context;
struct xkb_keymap *keymap = NULL;
struct xkb_state *xkb_state = NULL;

static char running = 1;

const int WIDTH = 480;
const int HEIGHT = 360;

static void
handle_ping(void *data, struct xdg_wm_base *xdg_shell,
			uint32_t serial)
{
	xdg_wm_base_pong(xdg_shell, serial);
	fprintf(stderr, "Pinged and ponged\n");
}

struct xdg_wm_base_listener xdg_shell_listener = {
	handle_ping};

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
					 uint32_t serial, struct wl_surface *surface,
					 wl_fixed_t sx, wl_fixed_t sy)
{
	fprintf(stderr, "Pointer entered surface %p at %f %f\n", surface, wl_fixed_to_double(sx), wl_fixed_to_double(sy));
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
					 uint32_t serial, struct wl_surface *surface)
{
	fprintf(stderr, "Pointer left surface %p\n", surface);
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
					  uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
	printf("Pointer moved at %f %f\n", wl_fixed_to_double(sx), wl_fixed_to_double(sy));
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
					  uint32_t serial, uint32_t time, uint32_t button,
					  uint32_t state)
{
	printf("Pointer button\n");
	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED)
		xdg_toplevel_move(toplevel, seat, serial);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
					uint32_t time, uint32_t axis, wl_fixed_t value)
{
	printf("Pointer handle axis\n");
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int32_t fd, uint32_t size)
{
	char *keymap_string = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	xkb_keymap_unref(keymap);
	keymap = xkb_keymap_new_from_string(xkb_context, keymap_string, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(keymap_string, size);
	close(fd);
	xkb_state_unref(xkb_state);
	xkb_state = xkb_state_new(keymap);
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
	{
		xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkb_state, key + 8);
		uint32_t utf32 = xkb_keysym_to_utf32(keysym);
		if (utf32)
		{
			if (utf32 >= 0x21 && utf32 <= 0x7E)
			{
				printf("the key %c was pressed\n", (char)utf32);
				if (utf32 == 'q')
					running = 0;
			}
			else
			{
				printf("the key U+%04X was pressed\n", utf32);
			}
		}
		else
		{
			char name[64];
			xkb_keysym_get_name(keysym, name, 64);
			printf("the key %s was pressed\n", name);
		}
	}
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
	xkb_state_update_mask(xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static struct wl_keyboard_listener keyboard_listener = {
	keyboard_keymap,
	keyboard_enter,
	keyboard_leave,
	keyboard_key,
	keyboard_modifiers};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
						 enum wl_seat_capability caps)
{
	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer)
	{
		pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(pointer, &pointer_listener, NULL);
	}
	else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && pointer)
	{
		wl_pointer_destroy(pointer);
		pointer = NULL;
	}
	if (caps & WL_SEAT_CAPABILITY_KEYBOARD)
	{
		keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
};

static void handle_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
	xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener surface_listener = {
	.configure = handle_configure};

static void
global_registry_handler(void *data, struct wl_registry *registry,
						uint32_t id, const char *interface, uint32_t version)
{
	printf("Got a registry event for %s id %d\n", interface, id);
	if (strcmp(interface, "wl_compositor") == 0)
	{
		compositor = wl_registry_bind(registry, id, &wl_compositor_interface, version);
	}
	else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
	{
		xdg_shell = wl_registry_bind(registry, id, &xdg_wm_base_interface, version);
		xdg_wm_base_add_listener(xdg_shell, &xdg_shell_listener, NULL);
	}
	else if (strcmp(interface, "wl_seat") == 0)
	{
		seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
		wl_seat_add_listener(seat, &seat_listener, NULL);
	}
}

static void
global_registry_remover(void *data, struct wl_registry *registry,
						uint32_t id)
{
	printf("Got a registry losing event for %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
	global_registry_handler,
	global_registry_remover,
};

int main(int argc, char **argv)
{
	fprintf(stderr, "XDG_RUNTIME_DIR= %s\n", getenv("XDG_RUNTIME_DIR"));

	struct wl_display *display = display = wl_display_connect(NULL);
	if (display == NULL)
	{
		fprintf(stderr, "Can't connect to display\n");
		exit(1);
	}
	printf("connected to display\n");

	struct wl_registry *registry = wl_display_get_registry(display);
	if (!registry)
	{
		printf("Cannot get registry.\n");
		exit(1);
	}

	wl_registry_add_listener(registry, &registry_listener, NULL);

	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	if (compositor == NULL)
	{
		fprintf(stderr, "Can't find compositor\n");
		exit(1);
	}
	else
	{
		fprintf(stderr, "Found compositor\n");
	}

	surface = wl_compositor_create_surface(compositor);
	if (surface == NULL)
	{
		fprintf(stderr, "Can't create surface\n");
		exit(1);
	}
	else
	{
		fprintf(stderr, "Created surface\n");
	}

	if (!xdg_shell)
	{
		fprintf(stderr, "Cannot bind to shell\n");
		exit(1);
	}
	else
	{
		fprintf(stderr, "Found valid shell.\n");
	}
	struct xdg_surface *shell_surface = xdg_wm_base_get_xdg_surface(xdg_shell, surface);
	if (shell_surface == NULL)
	{
		fprintf(stderr, "Can't create shell surface\n");
		exit(1);
	}
	else
	{
		fprintf(stderr, "Created shell surface\n");
	}
	// 窗口处理
	toplevel = xdg_surface_get_toplevel(shell_surface);
	xdg_surface_add_listener(shell_surface, &surface_listener, NULL);

	region = wl_compositor_create_region(compositor);
	wl_region_add(region, 0, 0, WIDTH, HEIGHT);
	wl_surface_set_opaque_region(surface, region);

	xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	// 此处是窗口显示
	// init egl
	EGLint major, minor, config_count, tmp_n;
	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE};
	EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE};

	EGLConfig egl_config;

	EGLDisplay egl_display = eglGetDisplay((EGLNativeDisplayType)display);
	eglInitialize(egl_display, &major, &minor);
	printf("EGL major: %d, minor %d\n", major, minor);
	eglGetConfigs(egl_display, 0, 0, &config_count);
	printf("EGL has %d configs\n", config_count);
	eglChooseConfig(egl_display, config_attribs, &(egl_config), 1, &tmp_n);
	EGLContext egl_context = eglCreateContext(
		egl_display,
		egl_config,
		EGL_NO_CONTEXT,
		context_attribs);

	// init window
	struct wl_egl_window *egl_window = wl_egl_window_create(surface, WIDTH, HEIGHT);
	EGLSurface egl_surface = eglCreateWindowSurface(
		egl_display,
		egl_config,
		egl_window,
		0);
	eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

	glClearColor(0.5f, 0.5f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	eglSwapBuffers(egl_display, egl_surface);

	// 窗口循环直到关闭
	while (wl_display_dispatch(display) != -1)
	{
		;
	}

	xdg_surface_destroy(shell_surface);
	xdg_wm_base_destroy(xdg_shell);
	wl_surface_destroy(surface);
	wl_compositor_destroy(compositor);
	wl_registry_destroy(registry);
	wl_display_disconnect(display);
	printf("disconnected from display\n");

	exit(0);
}
