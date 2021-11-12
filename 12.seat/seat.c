#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

struct wl_display *display = NULL;
struct wl_seat *seat = NULL;

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
                         enum wl_seat_capability caps)
{
    if (caps & WL_SEAT_CAPABILITY_POINTER)
    {
        printf("Display has a pointer\n");
    }

    if (caps & WL_SEAT_CAPABILITY_KEYBOARD)
    {
        printf("Display has a keyboard\n");
    }

    if (caps & WL_SEAT_CAPABILITY_TOUCH)
    {
        printf("Display has a touch screen\n");
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id,
                             const char *interface, uint32_t version)
{
    if (strcmp(interface, "wl_seat") == 0)
    {
        seat = wl_registry_bind(registry, id,
                                &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    }
}

static void
global_registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
    printf("Got a registry losing event for %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover};

int main(int argc, char **argv)
{

    display = wl_display_connect(NULL);
    if (display == NULL)
    {
        fprintf(stderr, "Can't connect to display\n");
        exit(1);
    }
    printf("connected to display\n");

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    wl_display_disconnect(display);
    printf("disconnected from display\n");

    exit(0);
}
