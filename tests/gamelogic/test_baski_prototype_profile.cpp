#include "doctest.h"
#include "GameLogic/Monsters/BaskiPrototypeProfile.h"

#include <array>

namespace Profile = GameLogic::Monsters::BaskiPrototypeProfile;

TEST_CASE("Baski prototype rejects incomplete animation layouts")
{
    const std::array<short, 7> keys{12, 12, 8, 9, 9, 6, 11};
    CHECK(Profile::HasRequiredActions(keys));
    CHECK_FALSE(Profile::HasRequiredActions(std::span(keys).first(6)));
    auto missingWalk = keys;
    missingWalk[2] = 0;
    CHECK_FALSE(Profile::HasRequiredActions(missingWalk));
    const std::array<short, 8> extraAction{12, 12, 8, 9, 9, 6, 11, 1};
    CHECK_FALSE(Profile::HasRequiredActions(extraAction));
}

TEST_CASE("Baski prototype uses its own playback instead of the Spider override")
{
    CHECK(Profile::MonsterNumber == 900);
    CHECK(Profile::ModelOffset != 9);
    CHECK(Profile::Scale == doctest::Approx(1.0f));
    CHECK(Profile::PlaySpeeds[2] == doctest::Approx(0.34f));
    CHECK(Profile::PlaySpeeds[2] != doctest::Approx(1.2f));
    CHECK(Profile::DeathAction == 6);
    for (const float speed : Profile::PlaySpeeds)
        CHECK(speed > 0.0f);
}
