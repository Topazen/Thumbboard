#include "xkb_context.hpp"

#include <sys/mman.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace thumbboard::wayland {

XkbContext::XkbContext(const std::string& layout) {
    ctx_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (ctx_ == nullptr) {
        std::fprintf(stderr, "thumbboard: xkb_context_new failed\n");
        std::exit(1);
    }

    const xkb_rule_names names{
        .rules = "evdev",
        .model = "pc105",
        .layout = layout.c_str(),
        .variant = nullptr,
        .options = nullptr,
    };
    keymap_ = xkb_keymap_new_from_names(ctx_, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (keymap_ == nullptr) {
        std::fprintf(
            stderr, "thumbboard: xkb_keymap_new_from_names failed for layout '%s'\n", layout.c_str()
        );
        std::exit(1);
    }

    char* str = xkb_keymap_get_as_string(keymap_, XKB_KEYMAP_FORMAT_TEXT_V1);
    if (str == nullptr) {
        std::fprintf(stderr, "thumbboard: xkb_keymap_get_as_string failed\n");
        std::exit(1);
    }

    size_ = static_cast<uint32_t>(std::strlen(str) + 1); // include NUL

    fd_ = memfd_create("thumbboard-xkb-keymap", MFD_CLOEXEC);
    if (fd_ < 0) {
        std::fprintf(stderr, "thumbboard: memfd_create failed: %s\n", std::strerror(errno));
        free(str);
        std::exit(1);
    }

    if (ftruncate(fd_, static_cast<off_t>(size_)) != 0) {
        std::fprintf(stderr, "thumbboard: ftruncate failed: %s\n", std::strerror(errno));
        free(str);
        std::exit(1);
    }

    void* map = mmap(nullptr, size_, PROT_WRITE, MAP_SHARED, fd_, 0);
    if (map == MAP_FAILED) {
        std::fprintf(stderr, "thumbboard: mmap keymap failed: %s\n", std::strerror(errno));
        free(str);
        std::exit(1);
    }
    std::memcpy(map, str, size_);
    munmap(map, size_);
    free(str);
}

XkbContext::~XkbContext() {
    if (fd_ >= 0) {
        close(fd_);
    }
    if (keymap_ != nullptr) {
        xkb_keymap_unref(keymap_);
    }
    if (ctx_ != nullptr) {
        xkb_context_unref(ctx_);
    }
}

uint32_t XkbContext::keysym_to_keycode(std::string_view name) const {
    if (name.empty() || keymap_ == nullptr) {
        return 0;
    }

    const std::string name_str(name);
    const xkb_keysym_t target = xkb_keysym_from_name(name_str.c_str(), XKB_KEYSYM_NO_FLAGS);
    if (target == XKB_KEY_NoSymbol) {
        return 0;
    }

    for (xkb_keycode_t kc = xkb_keymap_min_keycode(keymap_); kc <= xkb_keymap_max_keycode(keymap_);
         ++kc) {
        const xkb_layout_index_t num_layouts = xkb_keymap_num_layouts_for_key(keymap_, kc);
        for (xkb_layout_index_t li = 0; li < num_layouts; ++li) {
            const xkb_level_index_t num_levels = xkb_keymap_num_levels_for_key(keymap_, kc, li);
            for (xkb_level_index_t lv = 0; lv < num_levels; ++lv) {
                const xkb_keysym_t* syms = nullptr;
                const int n = xkb_keymap_key_get_syms_by_level(keymap_, kc, li, lv, &syms);
                for (int i = 0; i < n; ++i) {
                    if (syms[i] == target) {
                        return static_cast<uint32_t>(kc) - 8u;
                    }
                }
            }
        }
    }

    return 0;
}

} // namespace thumbboard::wayland
