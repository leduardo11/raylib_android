#pragma once

namespace Core {

struct Platform {
    static void init();
    static const char* assetPath(const char* path);

#ifdef __ANDROID__
    static void installCrashHandler();
#endif
};

} // namespace Core
