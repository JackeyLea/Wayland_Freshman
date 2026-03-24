#include <wayland-server.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WIDTH 640
#define HEIGHT 480
#define MAX_CLIENTS 10

// Wayland 接口定义
struct wl_compositor_interface compositor_interface;
struct wl_shell_interface shell_interface;
struct wl_surface_interface surface_interface;
struct wl_buffer_interface buffer_interface;
struct wl_shm_interface shm_interface;
struct wl_shm_pool_interface shm_pool_interface;

// 服务器全局状态
struct server {
    struct wl_display *display;
    struct wl_global *compositor_global;
    struct wl_global *shell_global;
    struct wl_global *shm_global;
    
    // EGL/GLES状态
    EGLDisplay egl_display;
    EGLConfig egl_config;
    EGLContext egl_context;
    GLuint program;
    
    // 显示表面
    struct wl_client *clients[MAX_CLIENTS];
    struct wl_list surfaces;
    int client_count;
    
    // SHM支持
    int shm_fd;
    void *shm_data;
    uint32_t shm_format;
};

// 表面状态
struct surface {
    struct wl_resource *resource;
    struct wl_resource *buffer_resource;
    struct wl_list link;
    int width, height;
    int x, y;
    void *data;
    uint32_t format;
};

// 创建一个共享内存区域
static int create_shared_memory(off_t size) {
    char name[] = "/wayland-shm-XXXXXX";
    int fd;
    void *data;

    fd = memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0)
        return -1;

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

// 初始化EGL
static int init_egl(struct server *server) {
    static const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    
    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLint num_configs;
    
    server->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(server->egl_display, NULL, NULL);
    
    eglChooseConfig(server->egl_display, config_attribs, &server->egl_config, 1, &num_configs);
    server->egl_context = eglCreateContext(server->egl_display, server->egl_config, 
                                          EGL_NO_CONTEXT, context_attribs);
    
    if (!server->egl_context) {
        fprintf(stderr, "Failed to create EGL context\n");
        return -1;
    }
    
    eglMakeCurrent(server->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, server->egl_context);
    
    // 编译简单的GLES3着色器
    const char *vertex_shader =
        "#version 300 es\n"
        "in vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
        
    const char *fragment_shader =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(0.3, 0.3, 0.3, 1.0);\n"
        "}\n";
    
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    
    glShaderSource(vert, 1, &vertex_shader, NULL);
    glShaderSource(frag, 1, &fragment_shader, NULL);
    glCompileShader(vert);
    glCompileShader(frag);
    
    server->program = glCreateProgram();
    glAttachShader(server->program, vert);
    glAttachShader(server->program, frag);
    glLinkProgram(server->program);
    glDeleteShader(vert);
    glDeleteShader(frag);
    
    // 创建共享内存用于合成
    server->shm_fd = create_shared_memory(WIDTH * HEIGHT * 4);
    server->shm_data = mmap(NULL, WIDTH * HEIGHT * 4, PROT_READ | PROT_WRITE, MAP_SHARED, server->shm_fd, 0);
    server->shm_format = WL_SHM_FORMAT_ARGB8888;
    
    return 0;
}

// 渲染一帧
static void render_frame(struct server *server) {
    // 设置视口
    glViewport(0, 0, WIDTH, HEIGHT);
    
    // 清除屏幕
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // 使用我们的着色器
    glUseProgram(server->program);
    
    // 绘制所有表面
    struct surface *surface;
    wl_list_for_each(surface, &server->surfaces, link) {
        if (surface->data) {
            // 在实际合成器中，这里应该实现表面合成逻辑
            // 此处简化处理：在固定位置绘制一个矩形
            float x1 = (surface->x - WIDTH/2) / (float)(WIDTH/2);
            float y1 = (HEIGHT/2 - surface->y) / (float)(HEIGHT/2);
            float x2 = (surface->x + surface->width - WIDTH/2) / (float)(WIDTH/2);
            float y2 = (HEIGHT/2 - (surface->y + surface->height)) / (float)(HEIGHT/2);
            
            float vertices[] = {
                x1, y1, 0,
                x2, y1, 0,
                x1, y2, 0,
                x2, y2, 0,
            };
            
            GLuint vbo;
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            
            GLint pos_attrib = glGetAttribLocation(server->program, "position");
            glEnableVertexAttribArray(pos_attrib);
            glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
            
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            
            glDeleteBuffers(1, &vbo);
        }
    }
    
    // 读取渲染结果到共享内存
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, server->shm_data);
    
    // 实际合成器会将渲染结果显示到物理输出设备
    // 这里简化处理：将结果保存为PPM文件用于验证
    static int frame_count = 0;
    if (frame_count % 60 == 0) {
        char filename[256];
        snprintf(filename, sizeof(filename), "frame_%d.ppm", frame_count);
        FILE *f = fopen(filename, "wb");
        fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                uint8_t *pixel = (uint8_t*)server->shm_data + (y * WIDTH + x) * 4;
                fwrite(pixel, 1, 3, f); // 写入RGB
            }
        }
        fclose(f);
    }
    frame_count++;
}

// 处理客户端请求的回调
static void compositor_create_surface(struct wl_client *client,
        struct wl_resource *resource, uint32_t id) {
    struct server *server = wl_resource_get_user_data(resource);
    
    struct surface *surface = calloc(1, sizeof(*surface));
    surface->resource = wl_resource_create(client, &wl_surface_interface, 1, id);
    wl_resource_set_implementation(surface->resource, &surface_interface, surface, NULL);
    
    surface->x = 100;
    surface->y = 100;
    surface->width = 200;
    surface->height = 200;
    
    wl_list_insert(&server->surfaces, &surface->link);
}

static void surface_attach(struct wl_client *client, struct wl_resource *resource,
        struct wl_resource *buffer_resource, int32_t x, int32_t y) {
    struct surface *surface = wl_resource_get_user_data(resource);
    surface->buffer_resource = buffer_resource;
    // 简化处理：假设buffer是SHM类型
}

static void surface_commit(struct wl_client *client, struct wl_resource *resource) {
    struct surface *surface = wl_resource_get_user_data(resource);
    if (surface->buffer_resource) {
        // 获取客户端缓冲区数据
        // 在实际应用中，这里应该映射共享内存
        // 此处简化处理：设置一个虚拟数据
        surface->data = malloc(200 * 200 * 4);
        memset(surface->data, 0xFFFF0000, 200 * 200 * 4); // ARGB红色
    }
}

// 定义Wayland接口方法
static struct wl_compositor_interface compositor_interface = {
    .create_surface = compositor_create_surface,
    .create_region = NULL, // 简化处理
};

static struct wl_surface_interface surface_interface = {
    .destroy = NULL, // 由资源销毁时自动处理
    .attach = surface_attach,
    .damage = NULL,
    .frame = NULL,
    .set_opaque_region = NULL,
    .set_input_region = NULL,
    .commit = surface_commit,
    /* ... 其他方法省略 ... */
};

int main(int argc, char *argv[]) {
    struct server server = {0};
    
    // 初始化Wayland显示
    server.display = wl_display_create();
    if (!server.display) {
        fprintf(stderr, "Failed to create Wayland display\n");
        return 1;
    }
    
    // 初始化EGL和GLES
    if (init_egl(&server) < 0) {
        wl_display_destroy(server.display);
        return 1;
    }
    
    // 初始化表面列表
    wl_list_init(&server.surfaces);
    
    // 创建全局对象
    server.compositor_global = wl_global_create(server.display, &wl_compositor_interface, 4, &server, 
        (wl_global_bind_func_t)wl_compositor_interface.bin);
    
    const char *socket = wl_display_add_socket_auto(server.display);
    if (!socket) {
        fprintf(stderr, "Failed to create Wayland socket\n");
        wl_display_destroy(server.display);
        return 1;
    }
    
    printf("Wayland display running on socket: %s\n", socket);
    
    // 主事件循环
    wl_display_run(server.display);
    
    // 清理资源
    wl_display_destroy_clients(server.display);
    wl_display_destroy(server.display);
    
    if (server.shm_data) 
        munmap(server.shm_data, WIDTH * HEIGHT * 4);
    if (server.shm_fd >= 0)
        close(server.shm_fd);
    
    eglDestroyContext(server.egl_display, server.egl_context);
    eglTerminate(server.egl_display);
    
    return 0;
}