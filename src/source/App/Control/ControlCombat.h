#pragma once

#include "Core/Globals/_define.h"

#include <array>
#include <cstddef>
#include <cstdint>

class CHARACTER;

namespace App::Control
{
void StopWalking();
void DisarmCombatRepeat();

// A control command may cancel its own approach, never a later manual walk.
class OwnedApproach
{
public:
    explicit OwnedApproach(int targetKey);
    ~OwnedApproach();
    OwnedApproach(const OwnedApproach&) = delete;
    OwnedApproach& operator=(const OwnedApproach&) = delete;

    void Remember();
    void Forget();
    [[nodiscard]] bool WasRelocated() const;

private:
    void Stop();

    int m_targetKey;
    int m_fromMap;
    std::uint64_t m_teleportsBefore;
    CHARACTER* m_hero = nullptr;
    int m_heroKey = -1;
    int m_map = -1;
    int m_targetIndex = -1;
    int m_x = -1;
    int m_y = -1;
    std::size_t m_pathSize = 0;
    std::array<unsigned char, MAX_PATH_FIND> m_pathX{};
    std::array<unsigned char, MAX_PATH_FIND> m_pathY{};
};
} // namespace App::Control
