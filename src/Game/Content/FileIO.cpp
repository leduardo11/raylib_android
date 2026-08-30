#include "FileIO.h"

#include "raylib.h"

namespace Content {

std::string resolveAssetPath(const std::string& path)
{
    if (FileExists(path.c_str()))
        return path;

    std::string candidate = "assets/" + path;
    if (FileExists(candidate.c_str()))
        return candidate;

    return path;
}

} // namespace Content