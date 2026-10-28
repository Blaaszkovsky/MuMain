#pragma once

// On-screen notice banner: the scrolling system messages drawn over the HUD
// (events, server announcements, etc.). Extracted from ZzzInterface.cpp.
namespace UI::Notices
{
    // Push a gameplay notice line. Ignored outside the world; long text is split automatically.
    void Create(const wchar_t* text, int color);

    // Drop all current notices and hide their presentation.
    void Clear();

    // Per-frame timing: expires the oldest notice over time.
    void Move();

    // Draw the active notices.
    void Render();
}
