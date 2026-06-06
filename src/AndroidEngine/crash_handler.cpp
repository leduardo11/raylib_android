#ifdef __ANDROID__

#include "crash_handler.h"

#include <android/log.h>
#include <signal.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unwind.h>

namespace {

static constexpr int MAX_FRAMES = 32;
static char g_crash_msg[4096] = {};

struct unwind_state {
    uintptr_t frames[MAX_FRAMES];
    int count = 0;
};

static _Unwind_Reason_Code unwind_cb(_Unwind_Context* ctx, void* arg)
{
    auto* state = static_cast<unwind_state*>(arg);
    if (state->count >= MAX_FRAMES) return _URC_END_OF_STACK;
    state->frames[state->count++] = _Unwind_GetIP(ctx);
    return _URC_NO_REASON;
}

static void crash_handler(int sig)
{
    unwind_state state;
    _Unwind_Backtrace(unwind_cb, &state);

    const char* sig_name = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: sig_name = "SIGSEGV"; break;
        case SIGABRT: sig_name = "SIGABRT"; break;
        case SIGFPE:  sig_name = "SIGFPE";  break;
        case SIGILL:  sig_name = "SIGILL";  break;
    }

    int len = snprintf(g_crash_msg, sizeof(g_crash_msg),
        "CRASH: signal %d (%s)\nBacktrace (%d frames):\n",
        sig, sig_name, state.count);

    for (int i = 0; i < state.count && len < (int)sizeof(g_crash_msg) - 32; i++)
    {
        len += snprintf(g_crash_msg + len, sizeof(g_crash_msg) - (std::size_t)len,
            "  #%02d: 0x%016llx\n", i, (unsigned long long)state.frames[i]);
    }

    __android_log_write(ANDROID_LOG_FATAL, "raylib_android", g_crash_msg);

    // Write to file
    {
        FILE* f = fopen("/data/data/com.raylib.android/files/crash_log.txt", "w");
        if (f) { fputs(g_crash_msg, f); fclose(f); }
    }

    signal(sig, SIG_DFL);
    raise(sig);
}

} // anonymous namespace

namespace hb::android {

void install_crash_handler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crash_handler;
    sa.sa_flags = SA_RESETHAND;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
}

} // namespace hb::android

#endif
