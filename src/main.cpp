#include "core.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void usage() {
    std::cerr << "usage: relic [--version] [--tokens] [file.relic]\n";
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << relic::version_string() << "\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--help") {
        usage();
        return 0;
    }
    bool tokens = argc == 3 && std::string(argv[1]) == "--tokens";
    if (argc != 2 && !tokens) {
        // REPL arrives in M5.
        usage();
        return 2;
    }
    std::string path = argv[tokens ? 2 : 1];
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "relic: cannot open " << path << "\n";
        return 2;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    relic::RunResult r = tokens ? relic::dump_tokens(path, ss.str())
                               : relic::run_source(path, ss.str());
    std::cout << r.out;
    std::cerr << r.err;
    return r.exit_code;
}
