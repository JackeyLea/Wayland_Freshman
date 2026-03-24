#include <wayland-client.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

struct wl_display *display = NULL;
struct wl_compositor *compositor = NULL;
struct wl_surface *surface = NULL;
struct wl_buffer *buffer = NULL;

static void registry_handle_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = NULL,
};

int main(int argc, char **argv) {
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }
    
    // 获取注册表
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    
    // 等待全局对象注册完成
    wl_display_roundtrip(display);
    
    if (!compositor) {
        fprintf(stderr, "wl_compositor not found\n");
        wl_display_disconnect(display);
        return 1;
    }
    
    // 创建表面
    surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "Failed to create surface\n");
        wl_display_disconnect(display);
        return 1;
    }
    
    // 创建共享内存缓冲区
    const int width = 200;
    const int height = 200;
    const int stride = width * 4;
    const int size = stride * height;
    
    // 创建共享内存文件描述符
    int fd = memfd_create("client-buffer", MFD_CLOEXEC);
    ftruncate(fd, size);
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    // 填充缓冲区 - 创建一个简单的蓝色渐变
    uint32_t *pixels = data;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t b = (x * 255) / width;
            pixels[y * width + x] = (0xFF << 24) | (b << 16) | (0x00 << 8) | 0x00;
        }
    }
    
    // 创建Wayland缓冲区
    struct wl_shm_pool *pool = wl_shm_create_pool(display, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    
    // 提交到服务器
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
    
    // 等待帧完成 (简化处理)
    wl_display_roundtrip(display);
    
    // 清理资源
    wl_display_roundtrip(display); // 确保所有事件处理完成
    
    munmap(data, size);
    close(fd);
    
    wl_buffer_destroy(buffer);
    wl_surface_destroy(surface);
    wl_compositor_destroy(compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    
    return 0;
}