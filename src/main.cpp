#include "core.hpp"
#include "fmt/fmt.hpp"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
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

static const char kCheckHelp[] =
    "usage: relic check FILE...\n"
    "       relic check -\n"
    "\n"
    "- reads the program from stdin and reports it as <stdin>.\n"
    "\n"
    "Finds mistakes without running: syntax errors, names that are never defined,\n"
    "a top-level name used before its fun or let, and break, next or return in the wrong place.\n"
    "\n"
    "exit 0  every file is clean\n"
    "exit 1  a file has a finding or a syntax error\n"
    "exit 2  a file cannot be opened, or the command is misused\n";

static const char kFmtHelp[] =
    "usage: relic fmt [--check | --write] FILE...\n"
    "\n"
    "Writes each file in canonical form. --check prints paths that would change.\n"
    "--write updates files in place. --check and --write do not combine.\n"
    "\n"
    "exit 0  stdout was written, --write finished, or --check found nothing to change\n"
    "exit 1  a file does not parse, or --check found a file that would change\n"
    "exit 2  bad usage, or a file could not be opened or written\n";

static bool is_help_flag(const std::string& s) { return s == "--help" || s == "-h"; }

static void usage(std::ostream& to) {
    to << "usage: relic [-h | --help | help | -V | --version | version] [--tokens | --ast | --bytecode | --tree] [file.relic [args...]]\n"
          "       relic check file.relic...   (finds mistakes without running, such as a name used before it is defined)\n"
          "       relic fmt [--check | --write] file.relic...   (canonical format; --check exits 1 if a file would change)\n"
          "       relic                       (no file: starts the REPL)\n";
}

// A sibling temp name that is unique per process and per call, so two
// concurrent formats never share one. The ".fmt-tmp" part stays so the
// fault-injection tests can find the temp stream.
static std::string unique_tmp_path(const std::string& path) {
    static std::atomic<unsigned long> counter{0};
    unsigned long n = counter.fetch_add(1);
#ifdef _WIN32
    unsigned long pid = static_cast<unsigned long>(GetCurrentProcessId());
#else
    unsigned long pid = static_cast<unsigned long>(getpid());
#endif
    return path + ".fmt-tmp." + std::to_string(pid) + "." + std::to_string(n);
}

// Replaces path with tmp. On POSIX rename already replaces the destination;
// on Windows it does not, so use MoveFileEx. Never deletes the destination.
static bool replace_file(const std::string& tmp, const std::string& path) {
#ifdef _WIN32
    return MoveFileExA(tmp.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return !ec;
#endif
}

// Writes text to path via a sibling temp file so a crash does not leave a
// half file. Every write, flush, close, or replace failure leaves the
// original bytes untouched and returns false; the temp is removed.
static bool write_file(const std::string& path, const std::string& text) {
    std::string tmp = unique_tmp_path(path);
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << text;
        // The stream buffers: a short write can report success here and
        // only fail when the buffer is flushed, so check the flush and
        // the close too. The destructor flush is unchecked, so never
        // rely on it.
        out.flush();
        if (!out) {
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
        out.close();
        if (out.fail()) {
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
    }
    if (!replace_file(tmp, path)) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

static int cmd_fmt(int argc, char** argv) {
    bool check = false;
    bool write = false;
    int i = 2;
    if (i < argc && is_help_flag(argv[i])) {
        if (argc != 3) {
            usage(std::cerr);
            return 2;
        }
        std::cout << kFmtHelp;
        return 0;
    }
    if (i < argc && (std::string(argv[i]) == "--check" || std::string(argv[i]) == "--write")) {
        check = std::string(argv[i]) == "--check";
        write = !check;
        ++i;
    }
    if (i < argc && std::string(argv[i]).rfind("--", 0) == 0) {
        usage(std::cerr);
        return 2;
    }
    if (i >= argc) {
        usage(std::cerr);
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
    std::string arg1 = argc >= 2 ? argv[1] : "";
    bool help_word = arg1 == "--help" || arg1 == "-h" || arg1 == "help";
    bool version_word = arg1 == "--version" || arg1 == "-V" || arg1 == "version";
    if (argc == 2 && version_word) {
        std::cout << relic::version_string() << "\n";
        return 0;
    }
    if (argc == 2 && help_word) {
        usage(std::cout);
        return 0;
    }
    if (argc > 2 && (help_word || version_word)) {
        usage(std::cerr);
        return 2;
    }
    if (argc >= 2 && std::string(argv[1]) == "check") {
        if (argc >= 3 && is_help_flag(argv[2])) {
            if (argc != 3) {
                usage(std::cerr);
                return 2;
            }
            std::cout << kCheckHelp;
            return 0;
        }
        if (argc == 2) {
            usage(std::cerr);
            return 2;
        }
    }
    if (argc >= 2 && std::string(argv[1]) == "fmt") return cmd_fmt(argc, argv);
    if (argc >= 3 && std::string(argv[1]) == "check") {
        int dashes = 0;
        for (int i = 2; i < argc; ++i) {
            std::string path = argv[i];
            if (path == "-") {
                ++dashes;
                continue;
            }
            // A flag here is a mistake, not a file named --bogus. Help was handled above.
            if (!path.empty() && path[0] == '-') {
                usage(std::cerr);
                return 2;
            }
        }
        if (dashes > 1) {
            usage(std::cerr);
            return 2;
        }
        int code = 0;
        for (int i = 2; i < argc; ++i) {
            std::string path = argv[i];
            std::string shown = path;
            std::string src;
            if (path == "-") {
                std::stringstream ss;
                ss << std::cin.rdbuf();
                src = ss.str();
                shown = "<stdin>";
            } else {
                std::ifstream f(path, std::ios::binary);
                if (!f || is_folder(path)) {
                    std::cerr << "relic: cannot open " << path << "\n";
                    code = 2;
                    continue;
                }
                std::stringstream ss;
                ss << f.rdbuf();
                src = ss.str();
            }
            relic::RunResult r = relic::check_source(shown, src);
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
        usage(std::cerr);
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
