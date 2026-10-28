#pragma once

#include "UI/HUD/SlideTicker.h"

namespace UI::HUD::Detail
{
// Shared presentation policy; lanes own animation and expose the resulting display.
// Keeping this independent of the window system permits a headless lifecycle regression.
template <class Lane>
SlideDisplay AdvanceSlideDisplay(Lane& help, Lane& notice)
{
    if (notice.HaveText() == FALSE)
    {
        help.Advance(TRUE);
    }
    else if (notice.GetAlphaRate() <= 0)
    {
        help.Advance();
    }

    if (help.GetAlphaRate() <= 0)
    {
        notice.Advance();
        return notice.Display();
    }

    return help.Display(notice.HaveText() == FALSE);
}
} // namespace UI::HUD::Detail
