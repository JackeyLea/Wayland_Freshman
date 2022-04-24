/////////////////////
// \author JackeyLea
// \date 20220424
// \note 加载自定义窗口背景图
/////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

static struct wl_display *display = NULL;
static struct wl_compositor *compositor = NULL;
struct wl_surface *surface;
struct wl_shell *shell;
struct wl_shell_surface *shell_surface;
struct wl_shm *shm;
struct wl_buffer *buffer;

void *shm_data;

int WIDTH = 825;
int HEIGHT = 645;

#define BIND_WL_REG(registry, ptr, id, intf, n) \
	do                                          \
	{                                           \
		(ptr) = (typeof(ptr))wl_registry_bind(  \
			registry, id, intf, n);             \
	} while (0)

// void buffer_release(void *data, struct wl_buffer *buffer)
// {
// 	LPPAINTBUFFER lpBuffer = data;
// 	lpBuffer->busy = 0;
// }

// static const struct wl_buffer_listener buffer_listener = 
// {
// 	.release = buffer_release
// };

static struct wl_buffer *
create_buffer()
{
	struct wl_shm_pool *pool;
	int stride = WIDTH * 4; // 4 bytes per pixel
	int size = stride * HEIGHT;
	int fd;
	struct wl_buffer *buff;

	fd = open("./3.rgb",O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "creating a buffer file for %d B failed: %m\n",
				size);
		exit(1);
	}

	shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (shm_data == MAP_FAILED)
	{
		fprintf(stderr, "mmap failed: %m\n");
		close(fd);
		exit(1);
	}

	pool = wl_shm_create_pool(shm, fd, size);
	buff = wl_shm_pool_create_buffer(pool, 0,
									 WIDTH, HEIGHT,
									 stride,
									 WL_SHM_FORMAT_XRGB8888);
	//wl_buffer_add_listener(buffer, &buffer_listener, buffer);
	wl_shm_pool_destroy(pool);
	return buff;
}

static void
create_window()
{
	buffer = create_buffer();

	wl_surface_attach(surface, buffer, 0, 0);
	//wl_surface_damage(surface, 0, 0, WIDTH, HEIGHT);
	wl_surface_commit(surface);
}

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
							uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
    fprintf(stderr, "Pinged and ponged\n");
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done
};

static void
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	//struct display *d = data;
	//	d->formats |= (1 << format);
	fprintf(stderr, "Format %d\n", format);
}

struct wl_shm_listener shm_listener = {
	.format = shm_format};

static void
global_registry_handler(void *data, struct wl_registry *registry,
						uint32_t id, const char *interface, uint32_t version)
{
	if (strcmp(interface, "wl_compositor") == 0)
	{
		BIND_WL_REG(registry, compositor, id, &wl_compositor_interface, 1);
	}
	else if (strcmp(interface, "wl_shell") == 0)
	{
		BIND_WL_REG(registry, shell, id, &wl_shell_interface, 1);
	}
	else if (strcmp(interface, "wl_shm") == 0)
	{
		BIND_WL_REG(registry, shm, id, &wl_shm_interface, 1);
		wl_shm_add_listener(shm, &shm_listener, NULL);
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

	shell_surface = wl_shell_get_shell_surface(shell, surface);
	if (shell_surface == NULL)
	{
		fprintf(stderr, "Can't create shell surface\n");
		exit(1);
	}
	else
	{
		fprintf(stderr, "Created shell surface\n");
	}
	wl_shell_surface_set_toplevel(shell_surface);
	wl_shell_surface_add_listener(shell_surface,&shell_surface_listener,NULL);

	create_window();

	while(wl_display_dispatch(display)!=-1){
		;
	}

	wl_display_disconnect(display);
	printf("disconnected from display\n");

	exit(0);
}