#pragma once

namespace World::Maps
{
    // The upper Devil Square levels share the original client's map state and assets.
    inline constexpr int DevilSquareMap = 9;
    inline constexpr int UpperDevilSquareMap = 32;

    constexpr int ClientMapNumber(int serverMap)
    {
        return serverMap == UpperDevilSquareMap ? DevilSquareMap : serverMap;
    }
}
