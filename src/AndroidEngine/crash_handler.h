#pragma once

#ifdef __ANDROID__

namespace hb::android {

// Install signal handlers for SIGSEGV/SIGABRT/SIGFPE/SIGILL.
// Writes crash backtrace to Android logcat and /data/data/.../crash_log.txt.
void install_crash_handler();

} // namespace hb::android

#endif
