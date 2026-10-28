#include "stdafx.h"

#include <doctest.h>

#include "RmlLayoutFixture.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Events/MatchEvent.h"
#include "I18N/All.h"
#include "UI/Dialogs/CustomMessageBox.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <filesystem>
#include <string>

namespace
{
class ResultFixture : public UI::Tests::RmlLayoutFixture
{
public:
    explicit ResultFixture(const char* theme)
        : m_directory(std::filesystem::current_path()), m_theme(UI::RmlBridge::GetActiveThemeName()),
          m_locale(I18N::GetCurrentLocale()), m_previousMatch(matchEvent::g_csMatchInfo), m_previousHero(Hero)
    {
        std::filesystem::current_path(MU_BIN_DIR);
        UI::RmlBridge::SetContextResolver([this](std::string_view) { return context; });
        UI::RmlBridge::SetActiveThemeName(theme);
        I18N::SetLocale("en");
        matchEvent::g_csMatchInfo = nullptr;
        Hero = &m_hero;
    }

    ~ResultFixture()
    {
        matchEvent::DeleteEventMatch();
        matchEvent::g_csMatchInfo = m_previousMatch;
        Hero = m_previousHero;
        UI::RmlBridge::SetContextResolver({});
        UI::RmlBridge::SetActiveThemeName(m_theme);
        I18N::SetLocale(m_locale.c_str());
        std::filesystem::current_path(m_directory);
    }

    void ReceiveResult(bool success, DWORD experience, DWORD score)
    {
        matchEvent::DeleteEventMatch();
        auto* match = new SEASON3B::CNewBloodCastleSystem;
        matchEvent::g_csMatchInfo = match;
        MatchResult result{};
        result.m_dwExp = experience;
        result.m_iScore = score;
        result.m_iZen = 120000;
        match->SetMatchResult(255, 0, &result, success);
    }

    void ReceiveDevilSquareResult()
    {
        matchEvent::DeleteEventMatch();
        auto* match = new CSDevilSquareMatch;
        matchEvent::g_csMatchInfo = match;
        MatchResult result{};
        result.m_dwExp = 4567;
        result.m_iScore = 123;
        match->SetMatchResult(1, 1, &result);
    }

    void ReceiveChaosCastleResult()
    {
        matchEvent::DeleteEventMatch();
        auto* match = new SEASON3B::CNewChaosCastleSystem;
        matchEvent::g_csMatchInfo = match;
        MatchResult result{};
        result.m_dwExp = 4567;
        result.m_iScore = 123;
        match->SetMatchResult(254, 0, &result, false);
    }

private:
    std::filesystem::path m_directory;
    std::string m_theme;
    std::string m_locale;
    CSBaseMatch* m_previousMatch;
    CHARACTER* m_previousHero;
    CHARACTER m_hero{};
};

template <typename ResultBox>
std::string RenderResult(ResultBox& box, ResultFixture& fixture)
{
    box.Update();
    fixture.Refresh();
    auto* panel = box.GetPanel();
    REQUIRE(panel != nullptr);
    return panel->GetInnerRML();
}
} // namespace

TEST_CASE("Blood Castle result survives disposal of its event on natural map exit [ui][result][blood-castle]")
{
    for (const char* theme : {"legacy", "modern"})
    for (const int percent : {75, 150})
    {
        CAPTURE(std::string(theme));
        CAPTURE(percent);
        ResultFixture fixture(theme);
        fixture.Configure({1280, 720}, percent);
        fixture.ReceiveResult(false, 4567, 123);
        mu::ui::window::CBloodCastleResultMsgBox box;
        REQUIRE(box.Create());
        const auto before = RenderResult(box, fixture);
        REQUIRE(before.find("4567") != std::string::npos);
        REQUIRE(before.find("123") != std::string::npos);

        // ReceiveTeleport calls CreateEventMatch, which deletes the BC match on return to Devias.
        matchEvent::DeleteEventMatch();
        const auto after = RenderResult(box, fixture);
        CHECK(after == before);
        CHECK(after.find("4567") != std::string::npos);
    }
}

TEST_CASE("Closing and reopening Blood Castle result replaces its owned contents [ui][result][blood-castle]")
{
    for (const char* theme : {"legacy", "modern"})
    for (const int percent : {75, 150})
    {
        CAPTURE(std::string(theme));
        CAPTURE(percent);
        ResultFixture fixture(theme);
        fixture.Configure({1280, 720}, percent);
        fixture.ReceiveResult(false, 4567, 123);
        mu::ui::window::CBloodCastleResultMsgBox box;
        REQUIRE(box.Create());
        REQUIRE(RenderResult(box, fixture).find("4567") != std::string::npos);
        box.Release();
        fixture.Refresh();
        CHECK(box.GetPanel() == nullptr);

        fixture.ReceiveResult(true, 9876, 321);
        REQUIRE(box.Create());
        const auto second = RenderResult(box, fixture);
        CHECK(second.find("9876") != std::string::npos);
        CHECK(second.find("4567") == std::string::npos);
        matchEvent::DeleteEventMatch();
        CHECK(RenderResult(box, fixture) == second);
        box.Release();
        CHECK(box.GetPanel() == nullptr);
    }
}

TEST_CASE("Message box safely retries creation after no context is available [ui][result]")
{
    ResultFixture fixture("legacy");
    UI::RmlBridge::SetContextResolver([](std::string_view) -> Rml::Context* { return nullptr; });
    mu::ui::window::MessageBoxView view;
    view.Create(0, "blood-castle-result");
    view.Sync({{"message", {{L"not shown without context"}}}}, {{L"OK"}});
    CHECK_FALSE(view.IsShown());
    CHECK(view.Panel() == nullptr);
    view.Destroy();

    UI::RmlBridge::SetContextResolver([&fixture](std::string_view) { return fixture.context; });
    view.Create(0, "blood-castle-result");
    view.Sync({{"message", {{L"context retry"}}}}, {{L"OK"}});
    fixture.Refresh();
    REQUIRE(view.Panel() != nullptr);
    CHECK(view.Panel()->GetInnerRML().find("context retry") != std::string::npos);
    view.Destroy();
    CHECK(view.Panel() == nullptr);
}

TEST_CASE("Devil Square ranking survives disposal of its event on natural map exit [ui][result][devil-square]")
{
    for (const char* theme : {"legacy", "modern"})
    for (const int percent : {75, 150})
    {
        CAPTURE(std::string(theme));
        CAPTURE(percent);
        ResultFixture fixture(theme);
        fixture.Configure({1280, 720}, percent);
        fixture.ReceiveDevilSquareResult();
        mu::ui::window::CDevilSquareRankMsgBox box;
        REQUIRE(box.Create());
        const auto before = RenderResult(box, fixture);
        REQUIRE(before.find("4567") != std::string::npos);
        matchEvent::DeleteEventMatch();
        CHECK(RenderResult(box, fixture) == before);
        box.Release();
        CHECK(box.GetPanel() == nullptr);
    }
}

TEST_CASE("Chaos Castle result survives disposal of its event on natural map exit [ui][result][chaos-castle]")
{
    for (const char* theme : {"legacy", "modern"})
    for (const int percent : {75, 150})
    {
        CAPTURE(std::string(theme));
        CAPTURE(percent);
        ResultFixture fixture(theme);
        fixture.Configure({1280, 720}, percent);
        fixture.ReceiveChaosCastleResult();
        mu::ui::window::CChaosCastleResultMsgBox box;
        REQUIRE(box.Create());
        const auto before = RenderResult(box, fixture);
        REQUIRE(before.find("4567") != std::string::npos);
        matchEvent::DeleteEventMatch();
        CHECK(RenderResult(box, fixture) == before);
        box.Release();
        CHECK(box.GetPanel() == nullptr);
    }
}
