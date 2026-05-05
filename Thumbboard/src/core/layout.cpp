#include "layout.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace thumbboard::core {

Layout Layout::load_from_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("layout: cannot open " + path.string());
    }

    nlohmann::json doc;
    in >> doc;

    Layout layout;
    layout.name_     = doc.value("name", "");
    layout.language_ = doc.value("language", "");

    if (doc.contains("grid")) {
        layout.cols_ = doc["grid"].value("cols", 0);
        layout.rows_ = doc["grid"].value("rows", 0);
    }

    for (const auto& k : doc.at("keys")) {
        Key key;
        key.label     = k.value("label", "");
        key.alt_label = k.value("alt", "");
        key.keysym    = k.value("keysym", "");
        key.x         = k.value("x", 0.0F);
        key.y         = k.value("y", 0.0F);
        key.w         = k.value("w", 1.0F);
        key.h         = k.value("h", 1.0F);
        layout.keys_.push_back(std::move(key));
    }

    return layout;
}

} // namespace thumbboard::core
