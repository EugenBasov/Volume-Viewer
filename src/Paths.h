#pragma once

#include <filesystem>
#include <string>

// Asset path: next to the executable, or in the source tree as a fallback
inline std::string assetPath(const std::string& relativePath)
{
    if (std::filesystem::exists(relativePath))
        return relativePath;
#ifdef ASSETS_DIR
    return std::string(ASSETS_DIR) + "/" + relativePath;
#else
    return relativePath;
#endif
}
