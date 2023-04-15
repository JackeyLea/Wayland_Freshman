/////////////////////
// \author JackeyLea
// \date 
// \note 使用stable接口
/////////////////////

#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "xdg-shell-client-protocol.h"

struct wl_display *display=NULL;
struct wl_registry *registry;
struct wl_compositor *compositor;
struct wl_region *region;
struct wl_surface *surface;

struct xdg_wm_base *xdg_shell=NULL;
struct xdg_surface *shell_surface;
struct xdg_toplevel *toplevel;

EGLDisplay egl_display;
EGLConfig egl_config;
EGLSurface egl_surface;
EGLContext egl_context;
struct wl_egl_window *egl_window;

const int WIDTH = 800;
const int HEIGHT = 600;

static void handle_configure(void *data, struct xdg_surface *surface, uint32_t serial) {
    xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener surface_listener = {
    .configure = handle_configure
};

static void registry_handler(void *data,struct wl_registry *registry, uint32_t id,
                             const char *interface,uint32_t version){
    printf("Got a registry event for %s id %d\n", interface, id);
    if (strcmp(interface, "wl_compositor") == 0)
    {
        compositor = wl_registry_bind(registry, id,&wl_compositor_interface,1);
    }else if (strcmp(interface, xdg_wm_base_interface.name) == 0){
        xdg_shell = wl_registry_bind(registry, id,&xdg_wm_base_interface,1);
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

    if (compositor == NULL || xdg_shell == NULL)
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

    shell_surface = xdg_wm_base_get_xdg_surface(xdg_shell, surface);
    toplevel = xdg_surface_get_toplevel(shell_surface);
    xdg_surface_add_listener(shell_surface, &surface_listener, NULL);

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