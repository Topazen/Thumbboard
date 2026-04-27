thumbboard_sources = files(
    'main.cpp',
    'app/daemon.cpp',
    'wayland/wayland_client.cpp',
    'wayland/layer_shell_surface.cpp',
    'render/egl_context.cpp',
)

executable(
    'thumbboard',
    sources             : thumbboard_sources,
    dependencies        : [wayland_client, wayland_egl, egl, glesv2, protocols_dep],
    include_directories : include_directories('.'),
    install             : true,
)
