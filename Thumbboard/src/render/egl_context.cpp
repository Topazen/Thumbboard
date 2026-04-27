#include "egl_context.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <cstdio>
#include <cstdlib>

namespace thumbboard::render {

namespace {

void fatal_egl(const char* msg) {
    std::fprintf(stderr, "thumbboard: EGL error 0x%x — %s\n", eglGetError(), msg);
    std::exit(1);
}

EGLConfig choose_config(EGLDisplay display) {
    // Try GLES 3 first; fall back to GLES 2 if unavailable.
    const EGLint gles3_attribs[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_NONE,
    };
    EGLConfig config{};
    EGLint num_configs = 0;

    if (eglChooseConfig(display, gles3_attribs, &config, 1, &num_configs) == EGL_TRUE &&
        num_configs > 0) {
        return config;
    }

    const EGLint gles2_attribs[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_NONE,
    };
    if (eglChooseConfig(display, gles2_attribs, &config, 1, &num_configs) == EGL_TRUE &&
        num_configs > 0) {
        return config;
    }

    fatal_egl("no suitable EGL config found");
    return nullptr; // unreachable
}

} // namespace

EglContext::EglContext(wl_display* display, wl_surface* surface, int width, int height) {
    egl_display_ = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, display, nullptr);
    if (egl_display_ == EGL_NO_DISPLAY) {
        fatal_egl("eglGetPlatformDisplay failed");
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (eglInitialize(egl_display_, &major, &minor) == EGL_FALSE) {
        fatal_egl("eglInitialize failed");
    }

    eglBindAPI(EGL_OPENGL_ES_API);

    EGLConfig config = choose_config(egl_display_);

    const EGLint ctx_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION,
        3,
        EGL_CONTEXT_MINOR_VERSION,
        0,
        EGL_NONE,
    };
    egl_context_ = eglCreateContext(egl_display_, config, EGL_NO_CONTEXT, ctx_attribs);
    if (egl_context_ == EGL_NO_CONTEXT) {
        // GLES 3 context failed; fall back to GLES 2.
        const EGLint ctx2_attribs[] = {
            EGL_CONTEXT_MAJOR_VERSION,
            2,
            EGL_CONTEXT_MINOR_VERSION,
            0,
            EGL_NONE,
        };
        egl_context_ = eglCreateContext(egl_display_, config, EGL_NO_CONTEXT, ctx2_attribs);
        if (egl_context_ == EGL_NO_CONTEXT) {
            fatal_egl("eglCreateContext failed");
        }
    }

    egl_window_ = wl_egl_window_create(surface, width, height);
    if (egl_window_ == nullptr) {
        std::fprintf(stderr, "thumbboard: wl_egl_window_create failed\n");
        std::exit(1);
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    egl_surface_ = eglCreateWindowSurface(
        egl_display_, config, reinterpret_cast<EGLNativeWindowType>(egl_window_), nullptr
    );
    if (egl_surface_ == EGL_NO_SURFACE) {
        fatal_egl("eglCreateWindowSurface failed");
    }

    if (eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_) == EGL_FALSE) {
        fatal_egl("eglMakeCurrent failed");
    }
}

EglContext::~EglContext() {
    eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_display_, egl_surface_);
    eglDestroyContext(egl_display_, egl_context_);
    wl_egl_window_destroy(egl_window_);
    eglReleaseThread();
    eglTerminate(egl_display_);
}

void EglContext::resize(int width, int height) {
    wl_egl_window_resize(egl_window_, width, height, 0, 0);
}

void EglContext::render_frame() {
    glClearColor(0.05F, 0.08F, 0.12F, 0.9F);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(egl_display_, egl_surface_);
}

} // namespace thumbboard::render
