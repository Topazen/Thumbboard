#pragma once

// EGL/GLES headers stay out of this header to avoid polluting every TU.
using EGLDisplay = void*;
using EGLSurface = void*;
using EGLContext = void*;

struct wl_display;
struct wl_surface;
struct wl_egl_window;

namespace thumbboard::render {

class EglContext {
public:
    EglContext(wl_display* display, wl_surface* surface, int width, int height);
    ~EglContext();

    EglContext(const EglContext&) = delete;
    EglContext& operator=(const EglContext&) = delete;

    void resize(int width, int height);
    void render_frame();

private:
    wl_egl_window* egl_window_ = nullptr;
    EGLDisplay egl_display_ = nullptr;
    EGLSurface egl_surface_ = nullptr;
    EGLContext egl_context_ = nullptr;
};

} // namespace thumbboard::render
