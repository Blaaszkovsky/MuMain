#pragma once

#ifdef _EDITOR
namespace MuEditor::BmdRuntimeProbe
{
bool IsEnabled();
// Opt-in local asset fixture. Returns true when it rendered instead of the login scene.
bool Render();
}
#endif
