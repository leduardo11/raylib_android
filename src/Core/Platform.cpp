#include "Platform.h"

#ifdef __ANDROID__
#include "AndroidEngine/crash_handler.h"
#endif

namespace Core {

void Platform::init()
{
#ifdef __ANDROID__
    installCrashHandler();
#endif
}

const char* Platform::assetPath(const char* path)
{
    // raylib on Android reads assets directly from the APK
    // On desktop, assets are relative to the working directory
    return path;
}

#ifdef __ANDROID__
void Platform::installCrashHandler()
{
    hb::android::install_crash_handler();
}
#endif

} // namespace Core
