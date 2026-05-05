#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "key.hpp"

namespace thumbboard::core {

class Layout {
public:
    static Layout load_from_file(const std::filesystem::path& path);

    const std::string&      name() const { return name_; }
    const std::string&      language() const { return language_; }
    int                     cols() const { return cols_; }
    int                     rows() const { return rows_; }
    const std::vector<Key>& keys() const { return keys_; }

private:
    std::string      name_;
    std::string      language_;
    int              cols_ = 0;
    int              rows_ = 0;
    std::vector<Key> keys_;
};

} // namespace thumbboard::core
