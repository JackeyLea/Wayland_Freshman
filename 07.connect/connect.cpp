/////////////////////
// \author JackeyLea
// \date 
// \note 测试能否正常链接wayland server
/////////////////////

#include <wayland-client.h>
#include <iostream>

using namespace std;

int main() {
    wl_display *display = wl_display_connect(0);

    if (!display) {
        std::cout << "Unable to connect to wayland compositor" << std::endl;
        return -1;
    }

    wl_display_disconnect(display);

    return 0;
}
