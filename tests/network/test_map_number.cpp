#include <doctest.h>

#include "World/MapInfra/MapNumber.h"

TEST_CASE("upper Devil Square arrival uses the same map alias as world loading [control][maps]")
{
    CHECK(World::Maps::ClientMapNumber(32) == 9);
    for (int map = 0; map <= 255; ++map)
    {
        if (map != 32)
        {
            CHECK(World::Maps::ClientMapNumber(map) == map);
        }
    }
}
