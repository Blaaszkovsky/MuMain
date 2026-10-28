#include "stdafx.h"

#include "UI/Combat/CrownRegistrationMessageBox.h"
#include "I18N/All.h"

#include <RmlUi/Core/Element.h>

namespace UI::Siege
{
CrownRegistrationMessageBox* CrownRegistrationMessageBox::s_active = nullptr;

CrownRegistrationMessageBox::CrownRegistrationMessageBox()
{
    s_active = this;
}

CrownRegistrationMessageBox::~CrownRegistrationMessageBox()
{
    if (s_active == this)
        s_active = nullptr;
}

bool CrownRegistrationMessageBox::Create()
{
    const bool created = CProgressMsgBox::Create();
    ApplyPresentation(this->GetPanel());
    return created;
}

bool CrownRegistrationMessageBox::Update()
{
    const bool updated = CProgressMsgBox::Update();
    ApplyPresentation(this->GetPanel());
    return updated;
}

void CrownRegistrationMessageBox::ApplyPresentation(Rml::Element* panel)
{
    if (panel)
        panel->SetClass("crown-registration", true);
}

std::wstring CrownRegistrationMessageBox::FormatText(bool registrationStarted, std::uint32_t accumulatedTimeMs)
{
    constexpr std::uint32_t millisecondsPerSecond = 1000;
    const int seconds = static_cast<int>(accumulatedTimeMs / millisecondsPerSecond);
    wchar_t text[256];
    mu_swprintf(text, I18N::Game::SAccumulatedHourDseconds,
                registrationStarted ? I18N::Game::OfficialSealRegistrationWillStart
                                    : I18N::Game::OfficialSealRegistrationIsFailed,
                seconds);
    return text;
}
} // namespace UI::Siege
