#include<stdio.h>
#include<stdlib.h>
#include<wayland-util.h>
#include<wayland-server.h>

// Creating a wayland server
int main(int argc, char* argv[]){
	// Get the display of the server
	struct wl_display *display = wl_display_create();
	if(!display){
		fprintf(stderr, "Unable to create a Wayland display\n");
		return 1;
	}
	
	// Create a socket to connect with it
	const char *socket = wl_display_add_socket_auto(display);
	if(!socket){
		fprintf(stderr, "Unable to add socket to Wayland display\n");
		return 1;
	}

	// Get event loop
	struct wl_event_loop *event = wl_display_get_event_loop(display);
	
	// Run wayland server
	fprintf(stderr, "Running Wayland display on %s\n", socket);
	wl_display_run(display);


	// Destroy display
	wl_display_destroy(display);
	wl_display_destroy_clients(display);
	return 0;
}