#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>

static struct wl_display *display = NULL;
static struct wl_compositor *compositor = NULL;
struct wl_surface *surface;
struct wl_shell *shell;
struct wl_shell_surface *shell_surface;
struct wl_shm *shm;
struct wl_buffer *buffer;
// input device
struct wl_seat *seat;
struct wl_pointer *pointer;
struct wl_keyboard *keyboard;
struct xkb_context *xkb_context;
struct xkb_keymap *keymap = NULL;
struct xkb_state *xkb_state = NULL;

static char running = 1;

void *shm_data;

int WIDTH = 480;
int HEIGHT = 360;

#define BIND_WL_REG(registry, ptr, id, intf, n) \
	do                                          \
	{                                           \
		(ptr) = (typeof(ptr))wl_registry_bind(  \
			registry, id, intf, n);             \
	} while (0)

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

static int
set_cloexec_or_close(int fd)
{
	long flags;

	if (fd == -1)
		return -1;

	flags = fcntl(fd, F_GETFD);
	if (flags == -1)
		goto err;

	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
		goto err;

	return fd;

err:
	close(fd);
	return -1;
}

static int
create_tmpfile_cloexec(char *tmpname)
{
	int fd;

#ifdef HAVE_MKOSTEMP
	fd = mkostemp(tmpname, O_CLOEXEC);
	if (fd >= 0)
		unlink(tmpname);
#else
	fd = mkstemp(tmpname);
	if (fd >= 0)
	{
		fd = set_cloexec_or_close(fd);
		unlink(tmpname);
	}
#endif
	return fd;
}

/*
 * Create a new, unique, anonymous file of the given size, and
 * return the file descriptor for it. The file descriptor is set
 * CLOEXEC. The file is immediately suitable for mmap()'ing
 * the given size at offset zero.
 *
 * The file should not have a permanent backing store like a disk,
 * but may have if XDG_RUNTIME_DIR is not properly implemented in OS.
 *
 * The file name is deleted from the file system.
 *
 * The file is suitable for buffer sharing between processes by
 * transmitting the file descriptor over Unix sockets using the
 * SCM_RIGHTS methods.

 * 创建一个给定大小的新的、唯一的、匿名的文件，
 * 并为其返回文件描述符。文件描述符被设置为cloexec。
 * 该文件立即适合于mmap()的给定大小的偏移量为零。
 * 
 * 该文件不应该像磁盘那样有永久备份存储，
 * 但如果XDG_RUNTIME_DIR在操作系统中没有正确实现，可能有。
 * 
 * 该文件名将从文件系统中删除。
 * 
 * 该文件适用于通过使用SCM_RIGHTS方法通过Unix套接字传输文件描述符来实现进程之间的缓冲区共享。
 */
int os_create_anonymous_file(off_t size)
{
	static const char template[] = "/weston-shared-XXXXXX";
	const char *path;
	char *name;
	int fd;

	path = getenv("XDG_RUNTIME_DIR");
	if (!path)
	{
		errno = ENOENT;
		return -1;
	}

	name = malloc(strlen(path) + sizeof(template));
	if (!name)
		return -1;

	strcpy(name, path);
	strcat(name, template);
	printf("%s\n", name);

	fd = create_tmpfile_cloexec(name);

	free(name);

	if (fd < 0)
		return -1;

	if (ftruncate(fd, size) < 0)
	{
		close(fd);
		return -1;
	}

	return fd; //其中：fd是临时文件，大小为size，用于mmap用。
}

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

	fd = os_create_anonymous_file(size);
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
	else if (strcmp(interface, "wl_seat") == 0)
	{
		BIND_WL_REG(registry, seat, id, &wl_seat_interface, 1);
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

static void
paint_pixels()
{
	int n;
	uint32_t *pixel = shm_data;

	fprintf(stderr, "Painting pixels\n");
	for (n = 0; n < WIDTH * HEIGHT; n++)
	{
		*pixel++ = 0xff0000; //红色
	}
}

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
	//wl_shell_suface_add_listener(shell_surface,&shell_surface_listener,NULL);
	
	xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	create_window();
	paint_pixels();

	while (wl_display_dispatch(display) != -1)
	{
		;
	}

	wl_display_disconnect(display);
	printf("disconnected from display\n");

	exit(0);
}
