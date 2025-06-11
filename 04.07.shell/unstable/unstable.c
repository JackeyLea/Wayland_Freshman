/////////////////////
// \author JackeyLea
// \date 2023-04-15
// \note 使用unstable协议注册部分registory
/////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

#include "xdg-shell-unstable-v6-client-protocol.h"

static struct wl_compositor *compositor = NULL;
struct zxdg_shell_v6 *xdg_shell = NULL;

static void
global_registry_handler(void *data, struct wl_registry *registry,
						uint32_t id, const char *interface, uint32_t version)
{
	printf("Got a registry event for %s id %d\n", interface, id);
	if (strcmp(interface, "wl_compositor") == 0)
	{
		compositor = wl_registry_bind(registry, id, &wl_compositor_interface, version);
	}
	else if (strcmp(interface, zxdg_shell_v6_interface.name) == 0)
	{
		xdg_shell = wl_registry_bind(registry, id, &zxdg_shell_v6_interface, 1);
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

	struct wl_surface *surface = wl_compositor_create_surface(compositor);
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
	struct zxdg_surface_v6 *shell_surface = zxdg_shell_v6_get_xdg_surface(xdg_shell, surface);
	if (shell_surface == NULL)
	{
		fprintf(stderr, "Can't create shell surface\n");
		exit(1);
	}
	else
	{
		fprintf(stderr, "Created shell surface\n");
	}

	// 此处是窗口显示
	//...
	printf("Nothing to display.\n");

	zxdg_surface_v6_destroy(shell_surface);
	zxdg_shell_v6_destroy(xdg_shell);
	wl_surface_destroy(surface);
	wl_compositor_destroy(compositor);
	wl_registry_destroy(registry);
	wl_display_disconnect(display);
	printf("disconnected from display\n");

	exit(0);
}
