WAYLAND_FLAGS = $(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

XDG_SHELL_PROTOCOL = $(WAYLAND_PROTOCOLS_DIR)/unstable/xdg-shell/xdg-shell-unstable-v6.xml

HEADERS=xdg-shell-unstable-v6-client-protocol.h
SOURCES=xdg-shell-unstable-v6-protocol.c

all: $(HEADERS) $(SOURCES)
	gcc -o unstable unstable.c $(SOURCES) -I. -lwayland-client

$(HEADERS):
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTOCOL) $@

$(SOURCES):
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTOCOL) $@

clean:
	rm -rf unstable $(HEADERS) $(SOURCES)
