#ifndef PROTOCOL_DEFINITIONS_H
#define PROTOCOL_DEFINITIONS_H

#include <wayland-server.h>
#include <wayland-server-protocol.h>

// 定义Wayland接口结构
extern struct wl_compositor_interface compositor_interface;
extern struct wl_surface_interface surface_interface;
extern struct wl_buffer_interface buffer_interface;
extern struct wl_shm_interface shm_interface;
extern struct wl_shm_pool_interface shm_pool_interface;

// Wayland接口的bind函数
void wl_compositor_interface_bind(struct wl_client *client, void *data, 
                                 uint32_t version, uint32_t id)
{
    struct wl_resource *resource = wl_resource_create(client, &wl_compositor_interface, version, id);
    wl_resource_set_implementation(resource, &compositor_interface, data, NULL);
}

#endif // PROTOCOL_DEFINITIONS_H