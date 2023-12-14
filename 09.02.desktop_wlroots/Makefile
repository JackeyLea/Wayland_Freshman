WAYLAND_SERVER_FLAGS=$(shell pkg-config wayland-server --cflags --libs)
WAYLAND_CLIENT_FLAGS=$(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)
WLROOTS=$(shell pkg-config wlroots --cflags --libs)

CFLAGS=-DWLR_USE_UNSTABLE

all:
	gcc -o desktop desktop.c -I. -lwayland-client -lxkbcommon $(CFLAGS) $(WLROOTS) $(WAYLAND_SERVER_FLAGS) $(WAYLAND_CLIENT_FLAGS)

clean:
	rm -rf desktop
