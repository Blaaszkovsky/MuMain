#include <doctest.h>

#include "World/MapInfra/MapAssets.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{
    class TemporaryDataDirectory
    {
    public:
        TemporaryDataDirectory()
        {
            static std::atomic<unsigned int> nextId = 0;
            constexpr unsigned int MaxCreateAttempts = 16;
            const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto baseName = "mu-map-assets-test-" + std::to_string(timestamp) + "-";
            std::error_code error;
            for (unsigned int attempt = 0; attempt < MaxCreateAttempts; ++attempt)
            {
                m_path = std::filesystem::temp_directory_path() /
                    (baseName + std::to_string(nextId.fetch_add(1)));
                error.clear();
                if (std::filesystem::create_directory(m_path, error))
                {
                    return;
                }
                if (error == std::errc::file_exists)
                {
                    continue;
                }
                if (error)
                {
                    throw std::filesystem::filesystem_error(
                        "Unable to create temporary map-assets fixture", m_path, error);
                }
                const bool pathExists = std::filesystem::exists(m_path, error);
                if (error)
                {
                    throw std::filesystem::filesystem_error(
                        "Unable to inspect temporary map-assets fixture", m_path, error);
                }
                if (pathExists)
                {
                    continue;
                }
                throw std::filesystem::filesystem_error(
                    "Unable to create temporary map-assets fixture", m_path, std::make_error_code(std::errc::io_error));
            }
            throw std::filesystem::filesystem_error(
                "Unable to allocate a unique temporary map-assets fixture", m_path,
                std::make_error_code(std::errc::file_exists));
        }

        ~TemporaryDataDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(m_path, error);
        }

        const std::filesystem::path& Path() const { return m_path; }

    private:
        std::filesystem::path m_path;
    };

    std::array<std::filesystem::path, 5> CoreAssetPaths(int worldNumber)
    {
        const std::string world = "World" + std::to_string(worldNumber);
        const std::string terrain = "EncTerrain" + std::to_string(worldNumber);
        return {
            std::filesystem::path(world) / (terrain + ".map"),
            std::filesystem::path(world) / (terrain + ".att"),
            std::filesystem::path(world) / (terrain + ".obj"),
            std::filesystem::path(world) / "TerrainHeight.OZB",
            std::filesystem::path(world) / "TerrainLight.OZJ",
        };
    }

    void CreateCoreAssets(const std::filesystem::path& dataDirectory, int worldNumber)
    {
        for (const auto& relativePath : CoreAssetPaths(worldNumber))
        {
            const auto fullPath = dataDirectory / relativePath;
            std::filesystem::create_directories(fullPath.parent_path());
            std::ofstream(fullPath, std::ios::binary).put('\0');
        }
    }
}

TEST_CASE("AssetWorldNumber follows the client map loader aliases [network][maps]")
{
    CHECK(World::Maps::AssetWorldNumber(32) == 10);

    for (int map = 11; map <= 17; ++map)
    {
        CHECK(World::Maps::AssetWorldNumber(map) == 12);
    }
    CHECK(World::Maps::AssetWorldNumber(52) == 12);

    for (int map = 18; map <= 23; ++map)
    {
        CHECK(World::Maps::AssetWorldNumber(map) == 19);
    }
    CHECK(World::Maps::AssetWorldNumber(53) == 19);

    for (int map = 24; map <= 29; ++map)
    {
        CHECK(World::Maps::AssetWorldNumber(map) == 25);
    }
    CHECK(World::Maps::AssetWorldNumber(36) == 25);

    for (int map = 45; map <= 50; ++map)
    {
        CHECK(World::Maps::AssetWorldNumber(map) == 47);
    }

    CHECK(World::Maps::AssetWorldNumber(112) == 113);
    CHECK(World::Maps::AssetWorldNumber(5) == 6);
}

TEST_CASE("core asset preflight accepts a complete map and reports missing paths [network][maps]")
{
    TemporaryDataDirectory dataDirectory;
    CreateCoreAssets(dataDirectory.Path(), 113);
    CHECK_FALSE(World::Maps::FindFirstMissingCoreAsset(112, dataDirectory.Path()).has_value());

    TemporaryDataDirectory missingData;
    const auto absentDataDirectory = missingData.Path() / "Data";
    const auto missingMap = World::Maps::FindFirstMissingCoreAsset(5, absentDataDirectory);
    REQUIRE(missingMap.has_value());
    CHECK(*missingMap == absentDataDirectory / "World6" / "EncTerrain6.map");
}

TEST_CASE("core asset preflight identifies missing leaf files in loader order [network][maps]")
{
    TemporaryDataDirectory dataDirectory;
    const auto assets = CoreAssetPaths(10);
    for (std::size_t index = 0; index + 1 < assets.size(); ++index)
    {
        const auto fullPath = dataDirectory.Path() / assets[index];
        std::filesystem::create_directories(fullPath.parent_path());
        std::ofstream(fullPath, std::ios::binary).put('\0');
    }

    const auto missingLight = World::Maps::FindFirstMissingCoreAsset(32, dataDirectory.Path());
    REQUIRE(missingLight.has_value());
    CHECK(*missingLight == dataDirectory.Path() / assets.back());

    std::filesystem::remove(dataDirectory.Path() / assets[2]);
    const auto missingObjects = World::Maps::FindFirstMissingCoreAsset(32, dataDirectory.Path());
    REQUIRE(missingObjects.has_value());
    CHECK(*missingObjects == dataDirectory.Path() / assets[2]);
}
