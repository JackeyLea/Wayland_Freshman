WAYLAND_SERVER_FLAGS=$(shell pkg-config wayland-server --cflags --libs)
WAYLAND_CLIENT_FLAGS=$(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

all:
	gcc -o dm dm.c -I. -lwayland-client -lxkbcommon $(WAYLAND_SERVER_FLAGS) $(WAYLAND_CLIENT_FLAGS)

clean:
	rm -rf dm
