#include "core.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
static bool stdin_is_terminal() { return _isatty(_fileno(stdin)) != 0; }
static bool stdout_is_terminal() { return _isatty(_fileno(stdout)) != 0; }
#else
#include <unistd.h>
static bool stdin_is_terminal() { return isatty(0) != 0; }
static bool stdout_is_terminal() { return isatty(1) != 0; }
#endif

static bool is_folder(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

static void usage() {
    std::cerr << "usage: relic [--version] [--tokens | --ast | --bytecode | --tree] [file.relic [args...]]\n"
                 "       relic check file.relic...   (find mistakes without running (incl. use before definition))\n"
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
    if (argc == 2 && std::string(argv[1]) == "check") {
        usage();
        return 2;
    }
    if (argc >= 3 && std::string(argv[1]) == "check") {
        int code = 0;
        for (int i = 2; i < argc; ++i) {
            std::ifstream f(argv[i], std::ios::binary);
            if (!f || is_folder(argv[i])) {
                std::cerr << "relic: cannot open " << argv[i] << "\n";
                code = 2;
                continue;
            }
            std::stringstream ss;
            ss << f.rdbuf();
            relic::RunResult r = relic::check_source(argv[i], ss.str());
            std::cerr << r.err;
            if (r.exit_code && !code) code = r.exit_code;
        }
        return code;
    }
    bool tokens = argc == 3 && std::string(argv[1]) == "--tokens";
    bool ast = argc == 3 && std::string(argv[1]) == "--ast";
    // The VM is the default engine; --tree runs the tree-walker. --vm is kept as an alias.
    bool tree = argc >= 3 && std::string(argv[1]) == "--tree";
    bool vm = argc >= 3 && std::string(argv[1]) == "--vm";
    bool bytecode = argc == 3 && std::string(argv[1]) == "--bytecode";
    if (argc == 1) return relic::repl(std::cin, std::cout, std::cerr, stdin_is_terminal());
    if (argc < 2 || (!tokens && !ast && !vm && !tree && !bytecode && std::string(argv[1]).rfind("--", 0) == 0)) {
        usage();
        return 2;
    }
    bool flag = tokens || ast || vm || tree || bytecode;
    if (!tokens && !ast && !bytecode)
        for (int i = (vm || tree) ? 3 : 2; i < argc; ++i) relic::script_args().push_back(argv[i]);
    std::string path = argv[flag ? 2 : 1];
    std::ifstream f(path, std::ios::binary);
    if (!f || is_folder(path)) {  // a folder opens fine and reads as nothing, so ask it
        std::cerr << "relic: cannot open " << path << "\n";
        return 2;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    // Program output streams as it is printed. On a terminal every write is
    // flushed at once; into a pipe or file it is buffered and flushed at exit.
    std::ios::sync_with_stdio(false);
    if (stdout_is_terminal()) std::cout << std::unitbuf;
    relic::RunResult r = tokens ? relic::dump_tokens(path, ss.str())
                       : ast  ? relic::dump_tree(path, ss.str())
                       : bytecode ? relic::dump_bytecode(path, ss.str())
                       : tree ? relic::run_source(path, ss.str(), std::cout)
                              : relic::run_source_vm(path, ss.str(), std::cout);
    std::cout << r.out << std::flush;
    std::cerr << r.err;
    return r.exit_code;
}
