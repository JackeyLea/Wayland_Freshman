all:
	gcc -o pointer pointer.c -lwayland-client
	gcc -o pointer_egl pointer_egl.c -lwayland-client -lwayland-egl -lEGL -lGL

clean:
	rm -rf pointer pointer_egl
