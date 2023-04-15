all:
	gcc -o keyboard keyboard.c -lwayland-client -lxkbcommon
	gcc -o keyboard_egl keyboard_egl.c -lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon

clean:
	rm -rf keyboard keyboard_egl
