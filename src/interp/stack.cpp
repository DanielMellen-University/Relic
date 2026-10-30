// How big the native stack of the running thread is, for the tree-walker's
// stack guard (Interpreter::eval). Kept apart so <windows.h> stays out of the
// interpreter.
#include "interp/stack.hpp"

#if defined(_WIN32)
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0602
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602  // GetCurrentThreadStackLimits (Windows 8)
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>
#else
#include <sys/resource.h>
#endif

namespace relic {

std::size_t native_stack_size() {
    constexpr std::size_t kFallback = std::size_t(1) << 20;  // 1 MB: the smallest common default
    constexpr std::size_t kCap = std::size_t(256) << 20;     // an unlimited stack still has neighbours
    std::size_t size = 0;
#if defined(_WIN32)
    ULONG_PTR low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    size = static_cast<std::size_t>(high - low);
#elif defined(__APPLE__)
    size = pthread_get_stacksize_np(pthread_self());
#else
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) == 0) {
        size = rl.rlim_cur == RLIM_INFINITY ? kCap : static_cast<std::size_t>(rl.rlim_cur);
    }
#endif
    if (size == 0) size = kFallback;
    return size < kCap ? size : kCap;
}

}  // namespace relic
