wayland_scanner_prog = find_program('wayland-scanner')

gen_c = generator(
    wayland_scanner_prog,
    output   : '@BASENAME@-protocol.c',
    arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)
gen_h = generator(
    wayland_scanner_prog,
    output   : '@BASENAME@-client-protocol.h',
    arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

wayland_protocols_dep = dependency('wayland-protocols')
wp_datadir = wayland_protocols_dep.get_variable('pkgdatadir')

layer_shell_xml = files('wlr-layer-shell-unstable-v1.xml')
xdg_shell_xml   = files(join_paths(wp_datadir, 'stable/xdg-shell/xdg-shell.xml'))

proto_src = [
    gen_c.process(layer_shell_xml),
    gen_c.process(xdg_shell_xml),
]
proto_hdr = [
    gen_h.process(layer_shell_xml),
    gen_h.process(xdg_shell_xml),
]

lib_protos = static_library(
    'thumbboard-protocols',
    proto_src + proto_hdr,
    dependencies : [wayland_client],
)

protocols_dep = declare_dependency(
    link_with           : lib_protos,
    sources             : proto_hdr,
    include_directories : include_directories('.'),
)
