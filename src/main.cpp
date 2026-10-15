#include "core.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
static bool stdin_is_terminal() { return _isatty(_fileno(stdin)) != 0; }
#else
#include <unistd.h>
static bool stdin_is_terminal() { return isatty(0) != 0; }
#endif

static void usage() {
    std::cerr << "usage: relic [--version] [--tokens | --ast] [file.relic]\n"
                 "       relic            (no file: start the REPL)\n";
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
    bool ast = argc == 3 && std::string(argv[1]) == "--ast";
    if (argc == 1) return relic::repl(std::cin, std::cout, std::cerr, stdin_is_terminal());
    if (argc != 2 && !tokens && !ast) {
        usage();
        return 2;
    }
    std::string path = argv[(tokens || ast) ? 2 : 1];
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "relic: cannot open " << path << "\n";
        return 2;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    relic::RunResult r = tokens ? relic::dump_tokens(path, ss.str())
                       : ast  ? relic::dump_tree(path, ss.str())
                              : relic::run_source(path, ss.str());
    std::cout << r.out;
    std::cerr << r.err;
    return r.exit_code;
}
