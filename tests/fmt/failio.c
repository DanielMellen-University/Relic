// Fault injector for the `relic fmt --write` regressions in
// tools/check_fmt.cmake. LD_PRELOAD this against the host `relic` binary.
//
// Derived as-is from the ee59898 acceptance audit reproduction, with one
// addition: an fclose() hook gated on FAIL_FMT_CLOSE, for the late-close
// failure the audit injector did not cover. (A close() hook cannot work
// there: libstdc++ closes the ofstream with fclose, and glibc reaches
// close through an internal alias the preload cannot see.)
//
//   FAIL_FMT_WRITE fails fwrite/write/writev on the .fmt-tmp stream.
//   FAIL_FMT_RENAME fails rename when the destination contains "victim.relic".
//   FAIL_FMT_CLOSE fails close on the .fmt-tmp file descriptor.
//
// The hooks log every open/fopen/rename to /tmp/fmt-probe/hook.log, which
// the test reads only to confirm the temp stream was found. Build with:
//   gcc -shared -fPIC -o failio.so tests/fmt/failio.c -ldl
#define _GNU_SOURCE
#include <sys/uio.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE *tmp_file;
static int tmp_fd = -1;

/* Use raw syscall via the next open only after we're not logging. */
static void append_log(const char *s) {
    int (*real_open)(const char *, int, ...) = dlsym(RTLD_NEXT, "open");
    ssize_t (*real_write)(int, const void *, size_t) = dlsym(RTLD_NEXT, "write");
    int (*real_close)(int) = dlsym(RTLD_NEXT, "close");
    int fd = real_open("/tmp/fmt-probe/hook.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { real_write(fd, s, strlen(s)); real_close(fd); }
}

int open(const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap); }
    int (*real)(const char *, int, ...) = dlsym(RTLD_NEXT, "open");
    int fd = real(path, flags, mode);
    char buf[512];
    snprintf(buf, sizeof buf, "open %s -> %d\n", path ? path : "?", fd);
    append_log(buf);
    if (fd >= 0 && path && strstr(path, ".fmt-tmp")) tmp_fd = fd;
    return fd;
}
FILE *fopen(const char *path, const char *mode) {
    FILE *(*real)(const char *, const char *) = dlsym(RTLD_NEXT, "fopen");
    FILE *f = real(path, mode);
    char buf[512];
    snprintf(buf, sizeof buf, "fopen %s %s -> %p\n", path ? path : "?", mode ? mode : "?", (void *)f);
    append_log(buf);
    if (f && path && strstr(path, ".fmt-tmp")) { tmp_file = f; tmp_fd = fileno(f); }
    return f;
}
ssize_t write(int fd, const void *buf, size_t n) {
    ssize_t (*real)(int, const void *, size_t) = dlsym(RTLD_NEXT, "write");
    if (getenv("FAIL_FMT_WRITE") && fd == tmp_fd) { errno = ENOSPC; return -1; }
    return real(fd, buf, n);
}
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t (*real)(const void *, size_t, size_t, FILE *) = dlsym(RTLD_NEXT, "fwrite");
    if (getenv("FAIL_FMT_WRITE") && stream == tmp_file) { errno = ENOSPC; return 0; }
    return real(ptr, size, nmemb, stream);
}
ssize_t writev(int fd, const struct iovec *iov, int cnt) {
    ssize_t (*real)(int, const void *, size_t) = dlsym(RTLD_NEXT, "writev");
    if (getenv("FAIL_FMT_WRITE") && fd == tmp_fd) { errno = ENOSPC; return -1; }
    return real(fd, iov, cnt);
}
int close(int fd) {
    int (*real)(int) = dlsym(RTLD_NEXT, "close");
    if (getenv("FAIL_FMT_CLOSE") && fd == tmp_fd) { errno = ENOSPC; return -1; }
    return real(fd);
}
/* The audit injector did not cover a late close failure. libstdc++ closes
   the ofstream with fclose, and glibc's fclose reaches close through its
   internal alias, so a close() hook never fires there. Interpose fclose
   instead: fail it only for the .fmt-tmp stream, and only under
   FAIL_FMT_CLOSE. Deliberately do not call the real fclose when failing,
   so a second close (the ofstream destructor) is safe. */
int fclose(FILE *stream) {
    int (*real)(FILE *) = dlsym(RTLD_NEXT, "fclose");
    if (getenv("FAIL_FMT_CLOSE") && stream == tmp_file) { errno = ENOSPC; return EOF; }
    return real(stream);
}
int rename(const char *old, const char *new) {
    int (*real)(const char *, const char *) = dlsym(RTLD_NEXT, "rename");
    char buf[512];
    snprintf(buf, sizeof buf, "rename %s -> %s\n", old ? old : "?", new ? new : "?");
    append_log(buf);
    if (getenv("FAIL_FMT_RENAME") && new && strstr(new, "victim.relic")) { errno = EPERM; return -1; }
    return real(old, new);
}
