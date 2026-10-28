#pragma once

#include <filesystem>
#include <optional>

namespace World::Maps
{
    int AssetWorldNumber(int serverMap);

    // dataDirectory is the client's Data folder. Returns the first missing
    // required file as a path under that folder, or no value when complete.
    std::optional<std::filesystem::path> FindFirstMissingCoreAsset(
        int serverMap, const std::filesystem::path& dataDirectory);
}
