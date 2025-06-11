WAYLAND_FLAGS = $(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

XDG_SHELL_PROTOCOL = $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml

HEADERS=xdg-shell-client-protocol.h
SOURCES=xdg-shell-protocol.c

all: $(HEADERS) $(SOURCES)
	gcc -o surface surface.c $(SOURCES) -I. -lwayland-client
	gcc -o surface_egl surface_egl.c $(SOURCES) -I. -lwayland-client -lwayland-egl -lEGL -lGL

$(HEADERS):
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTOCOL) $@

$(SOURCES):
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTOCOL) $@

clean:
	rm -rf surface surface_egl $(HEADERS) $(SOURCES)
