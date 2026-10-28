#include "MapAssets.h"

#include "MapNumber.h"

#include <array>
#include <string>
#include <system_error>

namespace World::Maps
{
    namespace
    {
        constexpr int BloodCastleFirstMap = 11;
        constexpr int BloodCastleLastMap = 17;
        constexpr int BloodCastleMasterLevelMap = 52;
        constexpr int ChaosCastleFirstMap = 18;
        constexpr int ChaosCastleLastMap = 23;
        constexpr int ChaosCastleMasterLevelMap = 53;
        constexpr int HellasFirstMap = 24;
        constexpr int HellasLastMap = 29;
        constexpr int HellasHiddenMap = 36;
        constexpr int CursedTempleFirstMap = 45;
        constexpr int CursedTempleLastMap = 50;
        constexpr int StandardWorldNumberOffset = 1;
        constexpr int CursedTempleWorldNumberOffset = 2;
    }

    int AssetWorldNumber(int serverMap)
    {
        const int clientMap = ClientMapNumber(serverMap);

        if ((clientMap >= BloodCastleFirstMap && clientMap <= BloodCastleLastMap) ||
            clientMap == BloodCastleMasterLevelMap)
        {
            return BloodCastleFirstMap + StandardWorldNumberOffset;
        }
        if ((clientMap >= ChaosCastleFirstMap && clientMap <= ChaosCastleLastMap) ||
            clientMap == ChaosCastleMasterLevelMap)
        {
            return ChaosCastleFirstMap + StandardWorldNumberOffset;
        }
        if ((clientMap >= HellasFirstMap && clientMap <= HellasLastMap) || clientMap == HellasHiddenMap)
        {
            return HellasFirstMap + StandardWorldNumberOffset;
        }
        if (clientMap >= CursedTempleFirstMap && clientMap <= CursedTempleLastMap)
        {
            return CursedTempleFirstMap + CursedTempleWorldNumberOffset;
        }

        return clientMap + StandardWorldNumberOffset;
    }

    std::optional<std::filesystem::path> FindFirstMissingCoreAsset(
        int serverMap, const std::filesystem::path& dataDirectory)
    {
        const int worldNumber = AssetWorldNumber(serverMap);
        const std::string worldName = "World" + std::to_string(worldNumber);
        const std::string terrainPrefix = "EncTerrain" + std::to_string(worldNumber);
        const std::array<std::filesystem::path, 5> requiredFiles = {
            std::filesystem::path(worldName) / (terrainPrefix + ".map"),
            std::filesystem::path(worldName) / (terrainPrefix + ".att"),
            std::filesystem::path(worldName) / (terrainPrefix + ".obj"),
            std::filesystem::path(worldName) / "TerrainHeight.OZB",
            std::filesystem::path(worldName) / "TerrainLight.OZJ",
        };

        for (const auto& relativePath : requiredFiles)
        {
            std::error_code error;
            if (!std::filesystem::is_regular_file(dataDirectory / relativePath, error))
            {
                return dataDirectory / relativePath;
            }
        }

        return std::nullopt;
    }
}
