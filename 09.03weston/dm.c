#include "stdio.h"

int load_drm_backend()
{

}

int main(int argc, char* argv[])
{
    //声明
    struct wl_display *display;

    display = wl_display_create();
    if(display == NULL){
        printf("fatal: failed to create display\n");
        return;
    }

    //加载backends
    load_drm_backend();
}