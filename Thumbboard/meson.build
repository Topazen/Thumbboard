# Thumbboard - top-level build file.
# This is a placeholder; real targets land in M0 (see docs/ROADMAP.md).
#
# Why Meson? wlroots, Sway, Hyprland, GNOME and most of the Wayland
# ecosystem use Meson. Learning it is more transferable than CMake here.

project(
    'thumbboard',
    ['c', 'cpp'],
    version : '0.0.1',
    license : 'GPL-3.0-or-later',
    default_options : [
        'cpp_std=c++20',
        'warning_level=3',
        'werror=true',
        'b_ndebug=if-release',
    ],
    meson_version : '>=1.0.0',
)

wayland_client = dependency('wayland-client')
wayland_egl    = dependency('wayland-egl')
egl            = dependency('egl')
glesv2         = dependency('glesv2')

# xkbcommon, sdl3, freetype2 land in M1+
# xkbcommon = dependency('xkbcommon')
# sdl3      = dependency('sdl3')
# freetype  = dependency('freetype2')

subdir('protocols')
subdir('src')
# subdir('tests')  # no tests yet

summary({
    'version'      : meson.project_version(),
    'C++ standard' : get_option('cpp_std'),
}, section : 'Thumbboard')
