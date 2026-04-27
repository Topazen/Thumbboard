#include <cstdio>
#include <exception>

#include "app/daemon.hpp"

int main() {
    try {
        thumbboard::app::Daemon daemon;
        daemon.run();
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "thumbboard: fatal: %s\n", ex.what());
        return 1;
    }
    return 0;
}
