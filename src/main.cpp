#include "core.hpp"
#include "fmt/fmt.hpp"

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
    std::cerr << "usage: relic [--help | --version] [--tokens | --ast | --bytecode | --tree] [file.relic [args...]]\n"
                 "       relic check file.relic...   (finds mistakes without running, such as a name used before it is defined)\n"
                 "       relic fmt [--check | --write] file.relic...   (canonical format; --check exits 1 if a file would change)\n"
                 "       relic                       (no file: starts the REPL)\n";
}

// Writes text to path via a sibling temp file so a crash does not leave a half file.
static bool write_file(const std::string& path, const std::string& text) {
    std::string tmp = path + ".fmt-tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << text;
        if (!out) {
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::filesystem::remove(tmp, ec);
            return false;
        }
    }
    return true;
}

static int cmd_fmt(int argc, char** argv) {
    bool check = false;
    bool write = false;
    int i = 2;
    if (i < argc && (std::string(argv[i]) == "--check" || std::string(argv[i]) == "--write")) {
        check = std::string(argv[i]) == "--check";
        write = !check;
        ++i;
    }
    if (i < argc && std::string(argv[i]).rfind("--", 0) == 0) {
        usage();
        return 2;
    }
    if (i >= argc) {
        usage();
        return 2;
    }
    int code = 0;
    for (; i < argc; ++i) {
        std::string path = argv[i];
        std::ifstream f(path, std::ios::binary);
        if (!f || is_folder(path)) {
            std::cerr << "relic: cannot open " << path << "\n";
            code = 2;
            continue;
        }
        std::stringstream ss;
        ss << f.rdbuf();
        f.close();
        std::string src = ss.str();
        relic::FmtResult r = relic::format_source(path, src);
        if (r.exit_code) {
            std::cerr << r.err;
            if (code == 0) code = r.exit_code;
            continue;
        }
        if (check) {
            if (r.out != src) {
                std::cout << path << "\n";
                if (code == 0) code = 1;
            }
        } else if (write) {
            if (r.out != src && !write_file(path, r.out)) {
                std::cerr << "relic: cannot write " << path << "\n";
                code = 2;
            }
        } else {
            std::cout << r.out;
        }
    }
    return code;
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
    if (argc >= 2 && std::string(argv[1]) == "fmt") return cmd_fmt(argc, argv);
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
