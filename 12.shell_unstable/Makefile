WAYLAND_FLAGS = $(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

XDG_SHELL_PROTOCOL = $(WAYLAND_PROTOCOLS_DIR)/unstable/xdg-shell/xdg-shell-unstable-v6.xml

HEADERS=xdg-shell-unstable-v6-protocol.h
SOURCES=xdg-shell-unstable-v6-protocol.c

all: $(HEADERS) $(SOURCES)
	gcc -o shell_unstable shell_unstable.c $(SOURCES) -I. -lwayland-client -lwayland-egl -lEGL -lGL

xdg-shell-unstable-v6-protocol.h:
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTOCOL) xdg-shell-unstable-v6-protocol.h

xdg-shell-unstable-v6-protocol.c:
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTOCOL) xdg-shell-unstable-v6-protocol.c

clean:
	rm -rf shell_stable $(HEADERS) $(SOURCES)
