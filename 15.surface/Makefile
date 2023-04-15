all:
	gcc -o surface surface.c -lwayland-client
	gcc -o surface_egl surface_egl.c -lwayland-client -lwayland-egl -lEGL -lGL

clean:
	rm -rf surface surface_egl
