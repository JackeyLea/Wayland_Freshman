/////////////////////
// \author JackeyLea
// \date 
// \note 测试窗口中捕获键盘操作
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

struct wl_display *display=NULL;
struct wl_registry *registry;
struct wl_compositor *compositor;
struct wl_shell *shell;
struct wl_surface *surface;
struct wl_shell_surface *shell_surface;
struct wl_region *region;

// input device
struct wl_seat *seat;
struct wl_pointer *pointer;
struct wl_keyboard *keyboard;
struct xkb_context *xkb_context;
struct xkb_keymap *keymap = NULL;
struct xkb_state *xkb_state = NULL;

EGLDisplay egl_display;
EGLConfig egl_config;
EGLSurface egl_surface;
EGLContext egl_context;
struct wl_egl_window *egl_window;

static char running = 1;

const int WIDTH = 800;
const int HEIGHT = 600;


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
		wl_shell_surface_move(shell_surface, seat, serial);
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
	keyboard_modifiers
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !pointer)
	{
		pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(pointer, &pointer_listener, NULL);
	}
	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
	{
		keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_capabilities,
};

static void registry_handler(void *data,struct wl_registry *registry, uint32_t id,
                             const char *interface,uint32_t version){
    printf("Got a registry event for %s id %d\n", interface, id);
    if (strcmp(interface, "wl_compositor") == 0)
    {
        compositor = wl_registry_bind(registry, id,&wl_compositor_interface,1);
    }else if (strcmp(interface, "wl_shell") == 0){
        shell = wl_registry_bind(registry, id,&wl_shell_interface,1);
    }else if(strcmp(interface, "wl_seat")==0){
        seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    }
}

static void registry_remover(void *data, struct wl_registry *registry, uint32_t id){}

static const struct wl_registry_listener registry_listener= {
    registry_handler,
    registry_remover
};

int main()
{
    display = wl_display_connect(0);
    if(!display){
        printf("Cannot connect to wayland compositor.\n");
        return -1;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry,&registry_listener,NULL);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (compositor == NULL || shell == NULL)
    {
        fprintf(stderr, "Can't find compositor or shell\n");
        return -1;
    }else{
        fprintf(stderr, "Found compositor and shell\n");
    }

    surface = wl_compositor_create_surface(compositor);
    if (surface == NULL)
    {
        fprintf(stderr, "Can't create surface\n");
        return -1;
    }else{
        fprintf(stderr, "Created surface\n");
    }

    shell_surface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_set_toplevel(shell_surface);

    xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    region = wl_compositor_create_region(compositor);
    wl_region_add(region, 0, 0, WIDTH, HEIGHT);
    wl_surface_set_opaque_region(surface, region);

    // init egl
    EGLint major, minor, config_count, tmp_n;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    egl_display = eglGetDisplay((EGLNativeDisplayType)display);
    eglInitialize(egl_display, &major, &minor);
    printf("EGL major: %d, minor %d\n", major, minor);
    eglGetConfigs(egl_display, 0, 0, &config_count);
    printf("EGL has %d configs\n", config_count);
    eglChooseConfig(egl_display, config_attribs, &(egl_config), 1, &tmp_n);
    egl_context = eglCreateContext(
            egl_display,
            egl_config,
            EGL_NO_CONTEXT,
            context_attribs);

    // init window
    egl_window = wl_egl_window_create(surface, WIDTH, HEIGHT);
    egl_surface = eglCreateWindowSurface(
            egl_display,
            egl_config,
            egl_window,
            0);
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    glClearColor(0.5f, 0.5f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    eglSwapBuffers(egl_display, egl_surface);

    while (wl_display_dispatch(display) != -1) {
    }

    // disconnect
    wl_display_disconnect(display);

    return 0;
}