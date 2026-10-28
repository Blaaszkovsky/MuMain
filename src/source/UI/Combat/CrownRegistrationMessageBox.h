#pragma once

#include "UI/Dialogs/CustomMessageBox.h"

#include <cstdint>
#include <string>

namespace UI::Siege
{
// Both crown-access success and crown-state success describe the same visible notice.
// Its lifetime, including manager pop-all and scene cleanup, defines when another may be created.
class CrownRegistrationMessageBox final : public mu::ui::window::CProgressMsgBox
{
public:
    CrownRegistrationMessageBox();
    ~CrownRegistrationMessageBox() override;

    bool Create();
    bool Update() override;
    static void ApplyPresentation(Rml::Element* panel);
    static std::wstring FormatText(bool registrationStarted, std::uint32_t accumulatedTimeMs);

    // A safety timeout since the latest authoritative update, not the server's capture target.
    static constexpr DWORD StartedNoticeTtlMs = 60'000;

    template <typename Factory>
    static CrownRegistrationMessageBox* CreateOnce(Factory& factory, bool succeeded = true)
    {
        if (s_active)
        {
            if (succeeded && s_active->m_succeeded)
                return nullptr;
            factory.DeleteMessageBox(s_active);
        }
        const mu::ui::window::CMessageBoxFactory::TContainer<CrownRegistrationMessageBox> container;
        auto* message = factory.NewMessageBox(container);
        if (message)
            message->m_succeeded = succeeded;
        return message;
    }

private:
    static CrownRegistrationMessageBox* s_active;
    bool m_succeeded = false;
};
} // namespace UI::Siege
