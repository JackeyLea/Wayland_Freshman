/////////////////////
// \author JackeyLea
// \date 2023-03-19
// \note 连接wayland server
/////////////////////

#include <wayland-client.h>
#include <stdio.h>

int main()
{
    struct wl_display *display = wl_display_connect(0);

    if (!display)
    {
        fprintf(stderr, "Unable to connect to wayland compositor\n");
        return -1;
    }

    wl_display_disconnect(display);

    return 0;
}
