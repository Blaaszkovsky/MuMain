#include "ItemGridGeometry.h"

#include <cmath>
#include <algorithm>
#include <limits>

namespace UI::Items
{
namespace
{
double DragCoordinate(float pointer, float origin, float pitch, float anchor, float anchorError)
{
    const double offset = static_cast<double>(anchor) * pitch;
    double cell = (static_cast<double>(pointer) - origin - offset) / pitch;
    // Layout/input are floats. Treat only their representation error as an exact cell boundary;
    // a real outside offset is not clamped into the grid.
    const double error =
        std::numeric_limits<float>::epsilon() *
            (std::abs(static_cast<double>(pointer)) + std::abs(static_cast<double>(origin)) + std::abs(offset)) /
            pitch +
        anchorError;
    const double boundary = std::round(cell);
    if (std::abs(cell - boundary) <= error)
        cell = boundary;
    return std::floor(cell);
}
} // namespace

GridGeometry::GridGeometry(float left, float top, float pitchX, float pitchY, int columns, int rows)
    : m_Left(left), m_Top(top), m_PitchX(pitchX > 0.f ? pitchX : DefaultPitch),
      m_PitchY(pitchY > 0.f ? pitchY : DefaultPitch), m_Columns(columns), m_Rows(rows)
{
}

bool GridGeometry::Contains(float x, float y) const
{
    const GridRect box = Bounds();
    return x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height;
}

bool GridGeometry::CellAt(float x, float y, int& column, int& row) const
{
    if (!Contains(x, y))
        return false;
    column = static_cast<int>((x - m_Left) / m_PitchX);
    row = static_cast<int>((y - m_Top) / m_PitchY);
    if (column >= m_Columns)
        column = m_Columns - 1;
    if (row >= m_Rows)
        row = m_Rows - 1;
    return true;
}

void GridGeometry::CellOf(float x, float y, int& column, int& row) const
{
    if (CellAt(x, y, column, row))
        return;
    const float dx = x - m_Left;
    const float dy = y - m_Top;
    column = static_cast<int>(dx / m_PitchX) - (dx < 0.f ? 1 : 0);
    row = static_cast<int>(dy / m_PitchY) - (dy < 0.f ? 1 : 0);
}

bool GridGeometry::DragCellOf(float pointerX, float pointerY, const PickupAnchor& anchor, int& column, int& row) const
{
    const double x = DragCoordinate(pointerX, m_Left, m_PitchX, anchor.column, anchor.columnError);
    const double y = DragCoordinate(pointerY, m_Top, m_PitchY, anchor.row, anchor.rowError);
    if (!std::isfinite(x) || !std::isfinite(y) || x < std::numeric_limits<int>::min() ||
        x > std::numeric_limits<int>::max() || y < std::numeric_limits<int>::min() ||
        y > std::numeric_limits<int>::max())
        return false;
    column = static_cast<int>(x);
    row = static_cast<int>(y);
    return true;
}

bool GridGeometry::DragCellAt(float pointerX, float pointerY, const PickupAnchor& anchor, int& column, int& row) const
{
    return DragCellOf(pointerX, pointerY, anchor, column, row) && column >= 0 && column < m_Columns && row >= 0 &&
           row < m_Rows;
}

PickupAnchor GridGeometry::PickupAt(float pointerX, float pointerY, const GridRect& itemBox) const
{
    if (itemBox.width <= 0.f || itemBox.height <= 0.f)
        return {};
    PickupAnchor anchor{
        std::clamp((pointerX - itemBox.x) / m_PitchX, 0.f, std::nextafter(itemBox.width / m_PitchX, 0.f)),
        std::clamp((pointerY - itemBox.y) / m_PitchY, 0.f, std::nextafter(itemBox.height / m_PitchY, 0.f))};
    const double epsilon = std::numeric_limits<float>::epsilon();
    const double errorX =
        epsilon * ((std::abs(static_cast<double>(pointerX)) + std::abs(static_cast<double>(itemBox.x))) / m_PitchX +
                   anchor.column);
    const double errorY =
        epsilon *
        ((std::abs(static_cast<double>(pointerY)) + std::abs(static_cast<double>(itemBox.y))) / m_PitchY + anchor.row);
    anchor.columnError = std::nextafter(static_cast<float>(errorX), std::numeric_limits<float>::infinity());
    anchor.rowError = std::nextafter(static_cast<float>(errorY), std::numeric_limits<float>::infinity());
    return anchor;
}

GridRect GridGeometry::CellsRect(int column, int row, int width, int height) const
{
    return {m_Left + column * m_PitchX, m_Top + row * m_PitchY, width * m_PitchX, height * m_PitchY};
}

void AnchoredTopLeft(float pointerX, float pointerY, const PickupAnchor& anchor, float pitchX, float pitchY, int& left,
                     int& top)
{
    left = static_cast<int>(std::lround(pointerX - anchor.column * pitchX));
    top = static_cast<int>(std::lround(pointerY - anchor.row * pitchY));
}
} // namespace UI::Items
