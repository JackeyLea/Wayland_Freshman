#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <linux/input.h>

struct wl_display *display=NULL;
struct wl_registry *registry;
struct wl_compositor *compositor;
struct wl_shell *shell;
struct wl_surface *surface;
struct wl_shell_surface *shell_surface;
struct wl_region *region;
struct wl_seat *seat;
struct wl_pointer *pointer;

EGLDisplay egl_display;
EGLConfig egl_config;
EGLSurface egl_surface;
EGLContext egl_context;
struct wl_egl_window *egl_window;

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
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
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
        seat = wl_registry_bind(registry,id, &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seat_listener,NULL);
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