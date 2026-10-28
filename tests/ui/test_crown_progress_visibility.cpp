#include "stdafx.h"

#include <doctest.h>

#include "RmlLayoutFixture.h"
#include "UI/Combat/CrownRegistrationMessageBox.h"
#include "Core/Text/Utf8.h"

TEST_CASE("Only crown registration hides an unsupported duration gauge in both themes [ui][siege][crown]")
{
    for (const char* theme : {"legacy", "modern"})
    {
        CAPTURE(std::string(theme));
        UI::Tests::RmlLayoutFixture fixture;
        auto* crown = fixture.Load(theme, "message_box_view");
        auto* other = fixture.Load(theme, "message_box_view");
        auto* panel = crown->GetElementById("panel");
        REQUIRE(panel != nullptr);
        auto* text = crown->GetElementById("lines");
        REQUIRE(text != nullptr);
        const auto label = Core::Text::ToUtf8(UI::Siege::CrownRegistrationMessageBox::FormatText(true, 29000).c_str());
        text->SetInnerRML(label);
        UI::Siege::CrownRegistrationMessageBox::ApplyPresentation(panel);
        fixture.Refresh();
        CHECK(panel->IsClassSet("crown-registration"));
        CHECK_FALSE(other->GetElementById("panel")->IsClassSet("crown-registration"));
        for (const char* id : {"progress_back", "progress_bar"})
        {
            CAPTURE(std::string(id));
            auto* crownBar = crown->GetElementById(id);
            auto* ordinaryBar = other->GetElementById(id);
            REQUIRE(crownBar != nullptr);
            REQUIRE(ordinaryBar != nullptr);
            CHECK(crownBar->GetComputedValues().visibility() == Rml::Style::Visibility::Hidden);
            CHECK(ordinaryBar->GetComputedValues().visibility() == Rml::Style::Visibility::Visible);
        }
        CHECK(text->GetInnerRML() == label);
        CHECK(text->GetComputedValues().visibility() == Rml::Style::Visibility::Visible);
        for (const std::uint32_t elapsed : {61000U, 90000U, UINT32_MAX})
        {
            CAPTURE(elapsed);
            text->SetInnerRML(Core::Text::ToUtf8(UI::Siege::CrownRegistrationMessageBox::FormatText(true, elapsed).c_str()));
            fixture.Refresh();
            CHECK(text->GetInnerRML().find(std::to_string(elapsed / 1000)) != std::string::npos);
        }
    }
}
