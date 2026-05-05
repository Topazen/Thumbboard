thumbboard_sources = files(
    'main.cpp',
    'app/daemon.cpp',
    'core/layout.cpp',
    'input/gamepad_manager.cpp',
    'input/input_mapper.cpp',
    'input/cursor_controller.cpp',
    'wayland/wayland_client.cpp',
    'wayland/layer_shell_surface.cpp',
    'wayland/xkb_context.cpp',
    'wayland/virtual_keyboard.cpp',
    'render/egl_context.cpp',
    'render/font_atlas.cpp',
    'render/mesh.cpp',
    'render/renderer.cpp',
    'render/shader_program.cpp',
)

executable(
    'thumbboard',
    sources             : thumbboard_sources,
    dependencies        : [
        wayland_client, wayland_egl, egl, glesv2, xkbcommon,
        freetype, nlohmann_json, protocols_dep,
        sdl3,
    ],
    include_directories : include_directories('.'),
    install             : true,
)
