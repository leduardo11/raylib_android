#pragma once

#include <string>

namespace Content {

// Resolves a packaged-asset path for both runtimes:
//   Android : packaged assets live at the app root  -> raw path works.
//   Desktop : raylib resolves relative to the CWD, and we run from the repo
//             root, so packed files live under assets/<path>.
// Tries `path` first, then `assets/<path>`, and returns the candidate that
// exists (falling back to `path` so loaders can report their own errors).
std::string resolveAssetPath(const std::string& path);

} // namespace Content