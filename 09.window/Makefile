all:
	gcc -o window window.c -lwayland-client
	gcc -o window_egl window_egl.c -lwayland-client -lwayland-egl -lEGL -lGL

clean:
	rm -rf window window_egl
