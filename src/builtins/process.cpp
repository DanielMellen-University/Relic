// host_program and host_run. Messages match runtime/c/rl_process.c.
#include "builtins/process.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace relic {
namespace {

bool has_nul(const std::string& s) { return s.find('\0') != std::string::npos; }

std::string run_error(const std::string& name) {
    return name.empty() ? "cannot run" : "cannot run " + name;
}

#ifdef _WIN32
std::wstring to_wide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), NULL, 0);
    if (n <= 0) throw std::string("bad utf-8");
    std::wstring w((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), &w[0], n);
    return w;
}

void quote_arg(std::string& out, const std::string& s) {
    bool need = s.empty();
    for (size_t i = 0; i < s.size() && !need; ++i) {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '"') need = true;
    }
    if (!need) { out += s; return; }
    out.push_back('"');
    size_t bs = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\') { ++bs; continue; }
        if (s[i] == '"') {
            out.append(bs * 2 + 1, '\\');
            out.push_back('"');
            bs = 0;
            continue;
        }
        out.append(bs, '\\');
        bs = 0;
        out.push_back(s[i]);
    }
    out.append(bs * 2, '\\');
    out.push_back('"');
}

bool key_eq(const std::wstring& entry, const std::wstring& key) {
    if (entry.empty() || entry[0] == L'=') return false;
    if (entry.size() < key.size() + 1 || entry[key.size()] != L'=') return false;
    for (size_t i = 0; i < key.size(); ++i) {
        wchar_t a = entry[i], b = key[i];
        if (a >= L'A' && a <= L'Z') a = (wchar_t)(a + 32);
        if (b >= L'A' && b <= L'Z') b = (wchar_t)(b + 32);
        if (a != b) return false;
    }
    return true;
}

std::wstring env_block(const Map& extra) {
    wchar_t* src = GetEnvironmentStringsW();
    if (!src) throw std::string("cannot run");
    std::vector<std::wstring> ents;
    for (wchar_t* p = src; *p; ) {
        ents.emplace_back(p);
        p += ents.back().size() + 1;
    }
    FreeEnvironmentStringsW(src);
    extra.each_key([&](const std::string& k) {
        const std::string& v = extra.items.at(k).as<std::string>();
        std::wstring row = to_wide(k) + L"=" + to_wide(v);
        std::wstring key = to_wide(k);
        bool found = false;
        for (auto& e : ents) {
            if (key_eq(e, key)) { e = row; found = true; break; }
        }
        if (!found) ents.push_back(row);
    });
    std::wstring block;
    for (const auto& e : ents) { block += e; block.push_back(0); }
    block.push_back(0);
    return block;
}

HANDLE stdin_handle(const std::string& bytes) {
    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof sa);
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    if (bytes.empty()) {
        HANDLE rd = NULL, wr = NULL;
        if (!CreatePipe(&rd, &wr, &sa, 0)) throw std::string("cannot run");
        SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
        CloseHandle(wr);
        return rd;
    }
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, dir) || !GetTempFileNameW(dir, L"rlr", 0, path)) throw std::string("cannot run");
    HANDLE h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, &sa,
                           OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h == INVALID_HANDLE_VALUE) { DeleteFileW(path); throw std::string("cannot run"); }
    size_t off = 0;
    while (off < bytes.size()) {
        DWORD wrote = 0;
        DWORD chunk = (DWORD)((bytes.size() - off) > (1u << 20) ? (1u << 20) : (bytes.size() - off));
        if (!WriteFile(h, bytes.data() + off, chunk, &wrote, NULL) || wrote == 0) {
            CloseHandle(h);
            throw std::string("cannot run");
        }
        off += wrote;
    }
    if (SetFilePointer(h, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        CloseHandle(h);
        throw std::string("cannot run");
    }
    return h;
}

bool drain_pipe(HANDLE h, std::string& dest, bool& open) {
    if (!open) return false;
    DWORD avail = 0, got = 0;
    if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) { open = false; return false; }
    bool any = false;
    while (avail) {
        char tmp[4096];
        DWORD chunk = avail > sizeof tmp ? (DWORD)sizeof tmp : avail;
        if (!ReadFile(h, tmp, chunk, &got, NULL) || got == 0) { open = false; break; }
        dest.append(tmp, tmp + got);
        any = true;
        avail -= got;
    }
    return any;
}
#endif

#ifndef _WIN32
int stdin_fd(const std::string& bytes) {
    if (bytes.empty()) {
        int p[2];
        if (pipe(p) != 0) return -1;
        close(p[1]);
        return p[0];
    }
    char path[4096];
    const char* tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) tmp = "/tmp";
    int nw = snprintf(path, sizeof path, "%s/relic-run-%ld-XXXXXX", tmp, (long)getpid());
    if (nw < 0 || nw >= (int)sizeof path) return -1;
    int fd = mkstemp(path);
    if (fd < 0) return -1;
    size_t off = 0;
    while (off < bytes.size()) {
        ssize_t w = write(fd, bytes.data() + off, bytes.size() - off);
        if (w < 0) { if (errno == EINTR) continue; close(fd); unlink(path); return -1; }
        if (w == 0) { close(fd); unlink(path); return -1; }
        off += (size_t)w;
    }
    if (lseek(fd, 0, SEEK_SET) < 0) { close(fd); unlink(path); return -1; }
    unlink(path);
    return fd;
}

void exec_argv(char** argv, char** envp) {
    if (strchr(argv[0], '/')) { execve(argv[0], argv, envp); return; }
    const char* path = "/usr/bin:/bin";
    for (char** e = envp; *e; ++e)
        if (strncmp(*e, "PATH=", 5) == 0) { path = *e + 5; break; }
    for (;;) {
        const char* colon = strchr(path, ':');
        size_t len = colon ? (size_t)(colon - path) : strlen(path);
        char full[4096];
        int nw = len == 0 ? snprintf(full, sizeof full, "%s", argv[0])
                          : snprintf(full, sizeof full, "%.*s/%s", (int)len, path, argv[0]);
        if (nw > 0 && nw < (int)sizeof full) {
            execve(full, argv, envp);
            if (errno != ENOENT && errno != ENOTDIR && errno != EACCES) return;
        }
        if (!colon) return;
        path = colon + 1;
        if (!*path) { execve(argv[0], argv, envp); return; }
    }
}

int pump(int out_fd, int err_fd, std::string& out, std::string& err) {
    int fl = fcntl(out_fd, F_GETFL, 0);
    if (fl >= 0) fcntl(out_fd, F_SETFL, fl | O_NONBLOCK);
    fl = fcntl(err_fd, F_GETFL, 0);
    if (fl >= 0) fcntl(err_fd, F_SETFL, fl | O_NONBLOCK);
    int rc = 0;
    for (;;) {
        pollfd pf[2];
        int np = 0, oi = -1, ei = -1;
        if (out_fd >= 0) { oi = np; pf[np].fd = out_fd; pf[np].events = POLLIN; ++np; }
        if (err_fd >= 0) { ei = np; pf[np].fd = err_fd; pf[np].events = POLLIN; ++np; }
        if (!np) break;
        int r = poll(pf, (nfds_t)np, -1);
        if (r < 0) {
            if (errno == EINTR) continue;
            rc = -1;
            break;
        }
        if (oi >= 0 && (pf[oi].revents & (POLLIN | POLLHUP | POLLERR))) {
            char tmp[4096];
            ssize_t n = read(out_fd, tmp, sizeof tmp);
            if (n > 0) out.append(tmp, tmp + n);
            else if (!(n < 0 && (errno == EINTR || errno == EAGAIN))) { close(out_fd); out_fd = -1; }
        }
        if (ei >= 0 && (pf[ei].revents & (POLLIN | POLLHUP | POLLERR))) {
            char tmp[4096];
            ssize_t n = read(err_fd, tmp, sizeof tmp);
            if (n > 0) err.append(tmp, tmp + n);
            else if (!(n < 0 && (errno == EINTR || errno == EAGAIN))) { close(err_fd); err_fd = -1; }
        }
    }
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);
    return rc;
}
#endif

bool directory_ok(const std::string& cwd) {
    if (cwd.empty()) return true;
#ifdef _WIN32
    try {
        std::wstring w = to_wide(cwd);
        DWORD a = GetFileAttributesW(w.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    } catch (const std::string&) {
        return false;
    }
#else
    struct stat st;
    return stat(cwd.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

}  // namespace

Value host_process_program() {
#ifdef _WIN32
    wchar_t w[32768];
    DWORD n = GetModuleFileNameW(NULL, w, 32768);
    if (n == 0 || n >= 32768) throw std::string("cannot find this program");
    int bytes = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    std::string s((size_t)(bytes > 0 ? bytes : 1), 0);
    if (bytes > 0) WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], bytes, NULL, NULL);
    if (!s.empty() && s.back() == 0) s.pop_back();
    return Value(s);
#elif defined(__APPLE__)
    char small[4096];
    uint32_t size = (uint32_t)sizeof small;
    if (_NSGetExecutablePath(small, &size) == 0) return Value(std::string(small));
    std::string buf((size_t)size, 0);
    if (_NSGetExecutablePath(&buf[0], &size) != 0) throw std::string("cannot find this program");
    buf.resize(strlen(buf.c_str()));
    return Value(buf);
#else
    std::string buf(256, 0);
    for (;;) {
        ssize_t n = readlink("/proc/self/exe", &buf[0], buf.size());
        if (n < 0) throw std::string("cannot find this program");
        if ((size_t)n < buf.size()) { buf.resize((size_t)n); return Value(buf); }
        buf.resize(buf.size() * 2);
    }
#endif
}

Value host_process_run(std::vector<Value>& a) {
    if (!a[0].is<std::shared_ptr<List>>())
        throw std::string("argv must be a list, got ") + type_name(a[0]);
    const List& argv = *a[0].as<std::shared_ptr<List>>();
    if (argv.empty()) throw std::string("argv must not be empty");
    std::vector<std::string> args;
    args.reserve(argv.size());
    for (size_t i = 0; i < argv.size(); ++i) {
        if (!argv[i].is<std::string>())
            throw std::string("argv item ") + std::to_string(i) + " must be a string, got " + type_name(argv[i]);
        const std::string& s = argv[i].as<std::string>();
        if (has_nul(s)) throw std::string("argv item ") + std::to_string(i) + " must not contain a NUL byte";
        args.push_back(s);
    }
    if (args[0].empty()) throw std::string("argv item 0 must not be empty");
    if (!a[1].is<std::string>()) throw std::string("cwd must be a string, got ") + type_name(a[1]);
    const std::string& cwd = a[1].as<std::string>();
    if (has_nul(cwd)) throw std::string("cwd must not contain a NUL byte");
    if (!a[2].is<std::string>()) throw std::string("stdin must be a string, got ") + type_name(a[2]);
    const std::string& input = a[2].as<std::string>();
    if (!a[3].is<std::shared_ptr<Map>>())
        throw std::string("env must be a map, got ") + type_name(a[3]);
    const Map& env = *a[3].as<std::shared_ptr<Map>>();
    std::vector<std::pair<std::string, std::string>> extra;
    env.each_key([&](const std::string& k) {
        if (k.empty()) throw std::string("env key must not be empty");
        if (has_nul(k)) throw std::string("env key must not contain a NUL byte");
        if (k.find('=') != std::string::npos) throw std::string("env key must not contain '='");
        const Value& v = env.items.at(k);
        if (!v.is<std::string>())
            throw std::string("env value of ") + k + " must be a string, got " + type_name(v);
        const std::string& text = v.as<std::string>();
        if (has_nul(text)) throw std::string("env value of ") + k + " must not contain a NUL byte";
        extra.emplace_back(k, text);
    });
    if (!directory_ok(cwd)) throw std::string("cannot use folder ") + cwd;
    const std::string& prog = args[0];

#ifdef _WIN32
    std::string cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) cmd.push_back(' ');
        quote_arg(cmd, args[i]);
    }
    if (cmd.size() > 32766) throw run_error(prog);
    std::wstring cline, app, cdir, block;
    try { cline = to_wide(cmd); }
    catch (const std::string&) { throw run_error(prog); }
    bool has_sep = prog.find('/') != std::string::npos || prog.find('\\') != std::string::npos;
    try {
        if (has_sep) app = to_wide(prog);
        if (!cwd.empty()) cdir = to_wide(cwd);
        if (!extra.empty()) block = env_block(env);
    } catch (const std::string&) {
        throw run_error(prog);
    }
    HANDLE in_h = NULL, out_r = NULL, out_w = NULL, err_r = NULL, err_w = NULL;
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof pi);
    try {
        SECURITY_ATTRIBUTES sa;
        memset(&sa, 0, sizeof sa);
        sa.nLength = sizeof sa;
        sa.bInheritHandle = TRUE;
        in_h = stdin_handle(input);
        if (!CreatePipe(&out_r, &out_w, &sa, 0) || !CreatePipe(&err_r, &err_w, &sa, 0))
            throw run_error(prog);
        SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOW si;
        memset(&si, 0, sizeof si);
        si.cb = sizeof si;
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = in_h;
        si.hStdOutput = out_w;
        si.hStdError = err_w;
        std::vector<wchar_t> mutable_cmd(cline.begin(), cline.end());
        mutable_cmd.push_back(0);
        std::vector<wchar_t> mutable_block;
        if (!block.empty()) mutable_block.assign(block.begin(), block.end());
        if (!CreateProcessW(has_sep ? app.c_str() : NULL, mutable_cmd.data(), NULL, NULL, TRUE,
                            CREATE_UNICODE_ENVIRONMENT, block.empty() ? NULL : mutable_block.data(),
                            cwd.empty() ? NULL : cdir.c_str(), &si, &pi))
            throw run_error(prog);
        CloseHandle(in_h); in_h = NULL;
        CloseHandle(out_w); out_w = NULL;
        CloseHandle(err_w); err_w = NULL;
        std::string out, err;
        bool out_open = true, err_open = true;
        for (;;) {
            bool got = false;
            if (out_open) got = drain_pipe(out_r, out, out_open) || got;
            if (err_open) got = drain_pipe(err_r, err, err_open) || got;
            if (!out_open && !err_open) break;
            if (WaitForSingleObject(pi.hProcess, got ? 0 : 15) == WAIT_OBJECT_0 && !got) {
                bool more = false;
                if (out_open) more = drain_pipe(out_r, out, out_open) || more;
                if (err_open) more = drain_pipe(err_r, err, err_open) || more;
                if (!more) break;
            }
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 1;
        if (!GetExitCodeProcess(pi.hProcess, &code)) code = 1;
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        CloseHandle(out_r); CloseHandle(err_r);
        auto m = std::make_shared<Map>();
        m->set("code", Value(static_cast<long long>(static_cast<int>(code))));
        m->set("out", Value(std::move(out)));
        m->set("err", Value(std::move(err)));
        return Value(m);
    } catch (...) {
        if (in_h) CloseHandle(in_h);
        if (out_r) CloseHandle(out_r);
        if (out_w) CloseHandle(out_w);
        if (err_r) CloseHandle(err_r);
        if (err_w) CloseHandle(err_w);
        if (pi.hProcess) { WaitForSingleObject(pi.hProcess, INFINITE); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
        throw;
    }
#else
    int op[2] = {-1, -1}, ep[2] = {-1, -1}, errp[2] = {-1, -1}, in_fd = -1;
    if (pipe(op) != 0 || pipe(ep) != 0 || pipe(errp) != 0) {
        if (op[0] >= 0) close(op[0]);
        if (op[1] >= 0) close(op[1]);
        if (ep[0] >= 0) close(ep[0]);
        if (ep[1] >= 0) close(ep[1]);
        throw run_error(prog);
    }
    fcntl(errp[1], F_SETFD, FD_CLOEXEC);
    in_fd = stdin_fd(input);
    if (in_fd < 0) {
        close(op[0]); close(op[1]); close(ep[0]); close(ep[1]); close(errp[0]); close(errp[1]);
        throw run_error(prog);
    }
    std::vector<std::string> owned;
    std::vector<char*> envp;
    if (!extra.empty()) {
        size_t nenv = 0;
        for (; environ[nenv]; ++nenv) {}
        envp.reserve(nenv + extra.size() + 1);
        for (size_t i = 0; i < nenv; ++i) envp.push_back(environ[i]);
        for (const auto& kv : extra) {
            owned.push_back(kv.first + "=" + kv.second);
        }
        for (size_t e = 0; e < extra.size(); ++e) {
            char* s = const_cast<char*>(owned[e].c_str());
            bool found = false;
            for (size_t j = 0; j < envp.size(); ++j) {
                const char* eq = strchr(envp[j], '=');
                size_t kl = eq ? (size_t)(eq - envp[j]) : strlen(envp[j]);
                if (kl == extra[e].first.size() && memcmp(envp[j], extra[e].first.data(), kl) == 0) {
                    envp[j] = s;
                    found = true;
                    break;
                }
            }
            if (!found) envp.push_back(s);
        }
        envp.push_back(nullptr);
    }
    std::vector<char*> av;
    av.reserve(args.size() + 1);
    for (auto& s : args) av.push_back(const_cast<char*>(s.c_str()));
    av.push_back(nullptr);
    char** use_env = extra.empty() ? environ : envp.data();
    pid_t pid = fork();
    if (pid < 0) {
        close(op[0]); close(op[1]); close(ep[0]); close(ep[1]); close(errp[0]); close(errp[1]); close(in_fd);
        throw run_error(prog);
    }
    if (pid == 0) {
        close(errp[0]); close(op[0]); close(ep[0]);
        if (in_fd != 0) { dup2(in_fd, 0); if (in_fd > 2) close(in_fd); }
        if (op[1] != 1) { dup2(op[1], 1); if (op[1] > 2) close(op[1]); }
        if (ep[1] != 2) { dup2(ep[1], 2); if (ep[1] > 2) close(ep[1]); }
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
            char c = 'c';
            if (write(errp[1], &c, 1) < 0) {}
            _exit(127);
        }
        exec_argv(av.data(), use_env);
        char c = 'e';
        if (write(errp[1], &c, 1) < 0) {}
        _exit(127);
    }
    close(errp[1]); close(op[1]); close(ep[1]); close(in_fd);
    std::string out, err;
    int pumped = pump(op[0], ep[0], out, err);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) { if (errno != EINTR) break; }
    char why = 0;
    ssize_t nr = read(errp[0], &why, 1);
    close(errp[0]);
    if (pumped != 0) throw run_error(prog);
    if (nr == 1) {
        if (why == 'c') throw std::string("cannot use folder ") + cwd;
        throw run_error(prog);
    }
    int code = 1;
    if (WIFEXITED(status)) code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) code = 128 + WTERMSIG(status);
    auto m = std::make_shared<Map>();
    m->set("code", Value(static_cast<long long>(code)));
    m->set("out", Value(std::move(out)));
    m->set("err", Value(std::move(err)));
    return Value(m);
#endif
}

}  // namespace relic
