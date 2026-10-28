#include "doctest.h"
#include "UI/Inventory/ItemGridGeometry.h"

#include <cmath>
#include <initializer_list>
#include <limits>

using UI::Items::AnchoredTopLeft;
using UI::Items::GridGeometry;
using UI::Items::PickupAnchor;

namespace
{
bool DragCellAt(const GridGeometry& grid, float pointerX, float pointerY, const PickupAnchor& anchor, int& column,
                int& row)
{
    return grid.DragCellAt(pointerX, pointerY, anchor, column, row);
}
} // namespace

TEST_CASE("an item grid finds the cell under a point at the original pitch")
{
    const GridGeometry grid{100.f, 200.f, 20.f, 20.f, 8, 4};
    int column = -1;
    int row = -1;

    REQUIRE(grid.CellAt(100.f, 200.f, column, row));
    CHECK(column == 0);
    CHECK(row == 0);
    REQUIRE(grid.CellAt(119.f, 219.f, column, row));
    CHECK(column == 0);
    CHECK(row == 0);
    REQUIRE(grid.CellAt(120.f, 220.f, column, row));
    CHECK(column == 1);
    CHECK(row == 1);
    REQUIRE(grid.CellAt(259.f, 279.f, column, row));
    CHECK(column == 7);
    CHECK(row == 3);

    CHECK_FALSE(grid.CellAt(260.f, 210.f, column, row));
    CHECK_FALSE(grid.CellAt(99.f, 210.f, column, row));
    CHECK_FALSE(grid.CellAt(110.f, 280.f, column, row));
}

TEST_CASE("an item grid counts cells off its edges as the original did")
{
    const GridGeometry grid{100.f, 200.f, 20.f, 20.f, 8, 4};
    // For every offset the original's (d / 20) - (d < 0 ? 1 : 0), in integer arithmetic.
    for (int d = -45; d <= 200; ++d)
    {
        int column = 0;
        int row = 0;
        grid.CellOf(100.f + d, 200.f + d, column, row);
        const int expected = d / 20 - (d < 0 ? 1 : 0);
        CHECK(column == (d >= 0 && d < 160 ? d / 20 : expected));
        CHECK(row == (d >= 0 && d < 80 ? d / 20 : expected));
    }
}

TEST_CASE("an item grid follows the theme's pitch")
{
    const GridGeometry grid{10.f, 10.f, 24.f, 26.f, 8, 8};
    int column = -1;
    int row = -1;
    REQUIRE(grid.CellAt(10.f + 24.f * 3 + 23.f, 10.f + 26.f * 5, column, row));
    CHECK(column == 3);
    CHECK(row == 5);

    const auto box = grid.CellsRect(2, 1, 2, 3);
    CHECK(box.x == doctest::Approx(58.f));
    CHECK(box.y == doctest::Approx(36.f));
    CHECK(box.width == doctest::Approx(48.f));
    CHECK(box.height == doctest::Approx(78.f));

    const auto bounds = grid.Bounds();
    CHECK(bounds.width == doctest::Approx(192.f));
    CHECK(bounds.height == doctest::Approx(208.f));
}

TEST_CASE("an item grid without a laid-out pitch keeps the original's")
{
    const GridGeometry grid{0.f, 0.f, 0.f, -1.f, 8, 8};
    CHECK(grid.PitchX() == GridGeometry::DefaultPitch);
    CHECK(grid.PitchY() == GridGeometry::DefaultPitch);
}

TEST_CASE("a dragged item keeps its hold across grid pitches")
{
    // Held 37 x 13 px into a 2 x 2 item on a 20 pitch.
    const PickupAnchor anchor{37.f / 20.f, 13.f / 20.f};
    int left = 0;
    int top = 0;

    AnchoredTopLeft(500.f, 300.f, anchor, 20.f, 20.f, left, top);
    CHECK(left == 463);
    CHECK(top == 287);

    // The same point of the item on a 40 pitch: 74 x 26 in.
    AnchoredTopLeft(500.f, 300.f, anchor, 40.f, 40.f, left, top);
    CHECK(left == 426);
    CHECK(top == 274);

    // Every whole-pixel hold on a 20 pitch round-trips exactly.
    for (int offset = 0; offset < 80; ++offset)
    {
        const PickupAnchor hold{offset / 20.f, offset / 20.f};
        AnchoredTopLeft(1000.f, 1000.f, hold, 20.f, 20.f, left, top);
        CHECK(left == 1000 - offset);
        CHECK(top == 1000 - offset);
    }
}

TEST_CASE("a dragged item reaches the first cell of fractional trade and shop grids")
{
    for (const float top : {433.6000671386719f, 139.2000274658203f})
    {
        const GridGeometry grid{441.5999755859375f, top, 32.f, 32.f, 8, 4};
        int column = -1;
        int row = -1;
        REQUIRE(DragCellAt(grid, 457.5999755859375f, top + 16.f, {0.5f, 0.5f}, column, row));
        CHECK(column == 0);
        CHECK(row == 0);
    }
}

TEST_CASE("a dragged four-cell weapon reaches its requested fractional inventory row")
{
    const GridGeometry grid{744.0000610351562f, 315.2000732421875f, 32.f, 32.f, 8, 8};
    int column = -1;
    int row = -1;
    // Center of the intended 1x4 rectangle: target column3,row4; its last row is7.
    REQUIRE(DragCellAt(grid, 856.0000610351562f, 507.2000274658203f, {0.5f, 2.f}, column, row));
    CHECK(column == 3);
    CHECK(row == 4);
}

TEST_CASE("a dragged item retains an off-center multi-cell hold at different pitches")
{
    struct Placement
    {
        float pitchX;
        float pitchY;
        float pointerX;
        float pointerY;
    };
    // Held 1.85 columns and0.65 rows into a2x2 item; target column2,row1.
    const PickupAnchor anchor{1.85f, 0.65f};
    for (const Placement placement : {Placement{20.f, 20.f, 87.25f, 85.75f}, Placement{32.f, 32.f, 133.45f, 105.55f},
                                      Placement{24.f, 26.f, 102.65f, 95.65f}})
    {
        const GridGeometry grid{10.25f, 52.75f, placement.pitchX, placement.pitchY, 8, 8};
        int column = -1;
        int row = -1;
        REQUIRE(DragCellAt(grid, placement.pointerX, placement.pointerY, anchor, column, row));
        CHECK(column == 2);
        CHECK(row == 1);
    }
}

TEST_CASE("drag mapping rejects outside cells without wrapping a negative column into another row")
{
    const GridGeometry grid{100.25f, 200.75f, 20.f, 20.f, 8, 8};
    int column = -1;
    int row = -1;
    const PickupAnchor anchor{0.5f, 0.5f};
    CHECK_FALSE(DragCellAt(grid, 110.24f, 230.75f, anchor, column, row));
    CHECK_FALSE(DragCellAt(grid, 270.25f, 230.75f, anchor, column, row));
    CHECK_FALSE(DragCellAt(grid, 130.25f, 210.74f, anchor, column, row));
    CHECK_FALSE(DragCellAt(grid, 130.25f, 370.75f, anchor, column, row));
    REQUIRE(grid.DragCellOf(110.24f, 230.75f, anchor, column, row));
    CHECK(column == -1);
    CHECK(row == 1);
    REQUIRE(grid.DragCellOf(270.25f, 230.75f, anchor, column, row));
    CHECK(column == 8);
    CHECK(row == 1);
}

TEST_CASE("a fractional pickup preserves the held point across source and destination pitches")
{
    const GridGeometry source{744.0000610351562f, 315.2000732421875f, 32.f, 32.f, 8, 8};
    const auto itemBox = source.CellsRect(3, 4, 2, 2);
    const PickupAnchor anchor = source.PickupAt(899.2000732421875f, 464.0000610351562f, itemBox);
    CHECK(anchor.column == doctest::Approx(1.85f));
    CHECK(anchor.row == doctest::Approx(0.65f));

    const GridGeometry destination{10.25f, 52.75f, 20.f, 20.f, 8, 8};
    int column = -1;
    int row = -1;
    REQUIRE(destination.DragCellAt(87.25f, 85.75f, anchor, column, row));
    CHECK(column == 2);
    CHECK(row == 1);
    REQUIRE(source.DragCellAt(899.2000732421875f, 464.0000610351562f, anchor, column, row));
    CHECK(column == 3);
    CHECK(row == 4);
}

TEST_CASE("drag mapping keeps real sub-cell motion and rejects non-finite projections")
{
    const GridGeometry grid{100.25f, 200.75f, 20.f, 20.f, 8, 8};
    int column = -1;
    int row = -1;
    // Continuous hold: just left/right of the top-left boundary stays in distinct cells.
    REQUIRE(grid.DragCellAt(157.24f, 233.75f, {1.85f, 0.65f}, column, row));
    CHECK(column == 0);
    CHECK(row == 1);
    REQUIRE(grid.DragCellAt(157.26f, 233.75f, {1.85f, 0.65f}, column, row));
    CHECK(column == 1);
    CHECK(row == 1);
    CHECK_FALSE(grid.DragCellAt(std::numeric_limits<float>::infinity(), 230.f, {}, column, row));
    CHECK_FALSE(grid.DragCellOf(130.f, std::numeric_limits<float>::quiet_NaN(), {}, column, row));
}

TEST_CASE("a drag carries pickup precision from a large source origin into a smaller destination")
{
    const GridGeometry source{1800.2f, 1200.2f, 32.f, 32.f, 8, 8};
    const auto anchor = source.PickupAt(1859.4f, 1221.f, source.CellsRect(0, 0, 2, 2));
    const GridGeometry destination{10.25f, 52.75f, 20.f, 20.f, 8, 8};
    int column = -1;
    int row = -1;
    REQUIRE(destination.DragCellAt(87.25f, 85.75f, anchor, column, row));
    CHECK(column == 2);
    CHECK(row == 1);
    // A genuine displacement larger than the propagated precision still crosses the boundary.
    REQUIRE(destination.DragCellAt(87.24f, 85.74f, anchor, column, row));
    CHECK(column == 1);
    CHECK(row == 0);
}
