#include "stdafx.h"

#include <doctest.h>

#include "RmlLayoutFixture.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Events/SiegeRegistration.h"
#include "Guild/GuildCache.h"
#include "Guild/GuildTypes.h"
#include "Core/Utilities/StringUtils.h"
#include "I18N/All.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowObject.h"
#include "UI/Combat/SiegeUpdates.h"
#include "UI/Combat/GuardWindowRmlModel.h"
#include "UI/Combat/GuardGuildLists.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/Dialogs/MessageBox.h"
#include "UI/Inventory/MyInventory.h"
#include "Guild/GuildInfoWindow.h"
#include "UI/HUD/ChatLogWindow.h"
#include "UI/Inventory/InventoryCtrl.h"
#define private public
#include "UI/Combat/GuardWindow.h"
#undef private
#include "UI/Core/WindowManager.h"
#include "UI/RmlBridge/RmlTheme.h"

#include <cwchar>
#include <filesystem>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace
{
class GuardRegistrationFixture : public UI::Tests::RmlLayoutFixture
{
public:
    explicit GuardRegistrationFixture(const char* theme)
        : m_directory(std::filesystem::current_path()), m_theme(UI::RmlBridge::GetActiveThemeName()),
          m_locale(I18N::GetCurrentLocale()), m_scene(SceneFlag), m_previousHero(Hero),
          m_previousMark(GuildMark[0]), m_previousRegistration(g_SiegeRegistration)
    {
        std::filesystem::current_path(MU_BIN_DIR);
        UI::RmlBridge::SetContextResolver([this](std::string_view) { return context; });
        UI::RmlBridge::SetActiveThemeName(theme);
        I18N::SetLocale("en");
        SceneFlag = MAIN_SCENE;
        Hero = &m_hero;
        Hero->GuildMarkIndex = 0;
        Hero->GuildStatus = G_MASTER;
        GuildMark[0] = {};
        std::wcscpy(GuildMark[0].GuildName, L"TestA");
        g_SiegeRegistration.SetRegistered(false);
        REQUIRE(window.Create(&m_manager));
        window.Show(true);
    }

    ~GuardRegistrationFixture()
    {
        window.Release();
        g_SiegeRegistration = m_previousRegistration;
        GuildMark[0] = m_previousMark;
        Hero = m_previousHero;
        SceneFlag = m_scene;
        UI::RmlBridge::SetContextResolver({});
        UI::RmlBridge::SetActiveThemeName(m_theme);
        I18N::SetLocale(m_locale.c_str());
        std::filesystem::current_path(m_directory);
    }

    void SetStatus(CASTLESIEGE_STATE phase, std::uint32_t secondsRemaining)
    {
        UI::Siege::GuardStatus status{};
        status.phase = phase;
        status.secondsRemaining = secondsRemaining;
        window.SetData(status);
        window.Update();
        Refresh();
    }

    Rml::String NextStageText()
    {
        auto* document = window.GetFillDocument();
        REQUIRE(document != nullptr);
        auto* countdown = document->GetElementById("status_next_stage");
        REQUIRE(countdown != nullptr);
        return countdown->GetInnerRML();
    }

    static Rml::String ExpectedNextStageText(std::uint32_t secondsRemaining)
    {
        wchar_t text[256] = {};
        mu_swprintf(text, I18N::Game::UUURemainedForTheNextStage, secondsRemaining / 3600,
                    (secondsRemaining % 3600) / 60, secondsRemaining % 60);
        return StringUtils::WideToNarrow(text);
    }

    static bool MatchesElapsedWindow(const Rml::String& actual, std::uint32_t startSeconds,
                                    std::int64_t elapsedLowerSeconds, std::int64_t elapsedUpperSeconds)
    {
        if (elapsedLowerSeconds < 0 || elapsedUpperSeconds < elapsedLowerSeconds)
            return false;
        const auto minimum = elapsedUpperSeconds >= startSeconds ? 0U : startSeconds - static_cast<std::uint32_t>(elapsedUpperSeconds);
        const auto maximum = elapsedLowerSeconds >= startSeconds ? 0U : startSeconds - static_cast<std::uint32_t>(elapsedLowerSeconds);
        for (auto seconds = minimum; seconds <= maximum; ++seconds)
            if (actual == ExpectedNextStageText(seconds))
                return true;
        return false;
    }

    void SelectListTabWithoutNetwork()
    {
        // Use the real GuardWindow tab state/model path without firing the click handler, whose
        // production Update() intentionally sends a server list request.
        window.m_iNumCurOpenTab = mu::ui::window::CGuardWindow::TAB_REGISTER_INFO;
        window.Update();
        Refresh();
    }

    std::vector<Rml::Element*> ActualRows(Rml::ElementDocument* document, const char* selector)
    {
        Rml::ElementList candidates;
        document->QuerySelectorAll(candidates, selector);
        std::vector<Rml::Element*> rows;
        for (auto* element : candidates)
        {
            // RmlUi retains the data-for template as a hidden element; actual cloned rows have
            // the directive removed by DataViewFor.
            if (element->GetAttribute("data-for") == nullptr)
                rows.push_back(element);
        }
        return rows;
    }

    Rml::ElementDocument* OpenDeclarationList()
    {
        window.ClearDeclareGuildList();
        window.ClearGuildList();
        std::wcscpy(GuildMark[0].UnionName, L"TestB");
        SetStatus(CASTLESIEGE_STATE_REGMARK, 0);
        window.AddDeclareGuildList(L"TestA", 3, false, 1);
        window.AddDeclareGuildList(L"TestB", 2, false, 2);
        window.Update();
        Refresh();
        auto* document = window.GetFillDocument();
        REQUIRE(document != nullptr);
        SelectListTabWithoutNetwork();
        return document;
    }

    Rml::ElementDocument* OpenSiegeList()
    {
        window.ClearDeclareGuildList();
        window.ClearGuildList();
        SetStatus(CASTLESIEGE_STATE_NOTIFY, 0);
        window.AddGuildList(L"Defender", 1, 1, 0);
        window.AddGuildList(L"Attacker", 0, 1, 0);
        window.Update();
        Refresh();
        auto* document = window.GetFillDocument();
        REQUIRE(document != nullptr);
        SelectListTabWithoutNetwork();
        return document;
    }

    Rml::Element* Announce(const wchar_t* owner, const wchar_t* alliance,
                           CASTLESIEGE_STATE phase = CASTLESIEGE_STATE_REGSIEGE)
    {
        std::wcscpy(GuildMark[0].UnionName, alliance);
        UI::Siege::GuardStatus status{};
        status.phase = phase;
        status.ownerGuild = owner;
        window.SetData(status);
        window.Update();
        Refresh();
        auto* document = window.GetFillDocument();
        REQUIRE(document != nullptr);
        auto* tab = document->QuerySelector(".gd-tab:nth-child(2)");
        REQUIRE(tab != nullptr);
        tab->Click();
        window.Update();
        Refresh();
        auto* button = document->GetElementById("btn_proclaim");
        REQUIRE(button != nullptr);
        return button;
    }

    mu::ui::window::CGuardWindow window;

private:
    mu::ui::window::CManager m_manager;
    std::filesystem::path m_directory;
    std::string m_theme;
    std::string m_locale;
    EGameScene m_scene;
    CHARACTER* m_previousHero;
    MARK_t m_previousMark;
    CSiegeRegistration m_previousRegistration;
    CHARACTER m_hero{};
};
} // namespace

TEST_CASE("An unowned castle permits a guild master to announce in both themes [ui][siege][registration]")
{
    for (const char* theme : {"legacy", "modern"})
    for (const wchar_t* alliance : {L"", L"Ally"})
    {
        CAPTURE(std::string(theme));
        GuardRegistrationFixture fixture(theme);
        auto* button = fixture.Announce(L"", alliance);
        CHECK(button->IsVisible(true));
        CHECK_FALSE(button->IsClassSet("locked"));
    }
}

TEST_CASE("The castle owner and its alliance remain unable to announce [ui][siege][registration]")
{
    for (const char* theme : {"legacy", "modern"})
    {
        GuardRegistrationFixture fixture(theme);
        CHECK(fixture.Announce(L"TestA", L"")->IsClassSet("locked"));
        CHECK(fixture.Announce(L"Owner", L"Owner")->IsClassSet("locked"));
        CHECK_FALSE(fixture.Announce(L"Owner", L"")->IsClassSet("locked"));
        CHECK_FALSE(fixture.Announce(L"Owner", L"Other")->IsClassSet("locked"));
    }
}

TEST_CASE("Announce still requires its registration phase and an unregistered guild master [ui][siege][registration]")
{
    for (const char* theme : {"legacy", "modern"})
    {
        GuardRegistrationFixture fixture(theme);
        CHECK_FALSE(fixture.Announce(L"", L"", CASTLESIEGE_STATE_READYSIEGE)->IsVisible(true));
        g_SiegeRegistration.SetRegistered(true);
        CHECK_FALSE(fixture.Announce(L"", L"")->IsVisible(true));
        g_SiegeRegistration.SetRegistered(false);
        Hero->GuildStatus = G_PERSON;
        CHECK_FALSE(fixture.Announce(L"", L"")->IsVisible(true));
    }
}

TEST_CASE("Guard guild rows keep theme-specific computed foregrounds [ui][siege][rml]")
{
    for (const char* theme : {"legacy", "modern"})
    {
        CAPTURE(std::string(theme));
        GuardRegistrationFixture fixture(theme);
        const bool modern = std::string(theme) == "modern";
        const Rml::Colourb selectedText = modern ? Rml::Colourb(0xd6, 0xd2, 0xc6) : Rml::Colourb(230, 220, 200);
        const Rml::Colourb defendingOnlyText = modern ? Rml::Colourb(0xaa, 0xa7, 0x9d) : Rml::Colourb(230, 220, 200);

        auto* document = fixture.OpenDeclarationList();
        auto declarationRows = fixture.ActualRows(document, ".gd-list-declare .gd-row");
        REQUIRE(declarationRows.size() == 2);
        REQUIRE(declarationRows[0]->IsClassSet("selected"));
        Rml::ElementList selectedDeclarationCells;
        declarationRows[0]->QuerySelectorAll(selectedDeclarationCells, ".gd-cell");
        REQUIRE(selectedDeclarationCells.size() == 4);
        for (auto* cell : selectedDeclarationCells)
            CHECK(cell->GetComputedValues().color() == selectedText);

        declarationRows[1]->Click();
        fixture.window.Update();
        fixture.Refresh();
        declarationRows = fixture.ActualRows(document, ".gd-list-declare .gd-row");
        REQUIRE(declarationRows.size() == 2);
        CHECK_FALSE(declarationRows[0]->IsClassSet("selected"));
        CHECK(declarationRows[1]->IsClassSet("selected"));
        Rml::ElementList selectedAllianceCells;
        declarationRows[1]->QuerySelectorAll(selectedAllianceCells, ".gd-cell");
        REQUIRE(selectedAllianceCells.size() == 4);
        for (auto* cell : selectedAllianceCells)
            CHECK(cell->GetComputedValues().color() == selectedText);

        document = fixture.OpenSiegeList();
        auto siegeRows = fixture.ActualRows(document, ".gd-list-siege .gd-row");
        REQUIRE(siegeRows.size() == 2);
        REQUIRE(siegeRows[0]->IsClassSet("selected"));
        REQUIRE(siegeRows[0]->IsClassSet("defending"));
        Rml::ElementList selectedDefenderCells;
        siegeRows[0]->QuerySelectorAll(selectedDefenderCells, ".gd-cell");
        REQUIRE(selectedDefenderCells.size() == 3);
        for (auto* cell : selectedDefenderCells)
            CHECK(cell->GetComputedValues().color() == selectedText);

        siegeRows[1]->Click();
        fixture.window.Update();
        fixture.Refresh();
        siegeRows = fixture.ActualRows(document, ".gd-list-siege .gd-row");
        REQUIRE(siegeRows.size() == 2);
        CHECK(siegeRows[1]->IsClassSet("selected"));
        CHECK_FALSE(siegeRows[1]->IsClassSet("defending"));
        CHECK_FALSE(siegeRows[0]->IsClassSet("selected"));
        CHECK(siegeRows[0]->IsClassSet("defending"));
        Rml::ElementList defendingOnlyCells;
        siegeRows[0]->QuerySelectorAll(defendingOnlyCells, ".gd-cell");
        REQUIRE(defendingOnlyCells.size() == 3);
        for (auto* cell : defendingOnlyCells)
            CHECK(cell->GetComputedValues().color() == defendingOnlyText);
        Rml::ElementList selectedAttackerCells;
        siegeRows[1]->QuerySelectorAll(selectedAttackerCells, ".gd-cell");
        REQUIRE(selectedAttackerCells.size() == 3);
        for (auto* cell : selectedAttackerCells)
            CHECK(cell->GetComputedValues().color() == selectedText);
    }
}

TEST_CASE("Guard Ready countdown advances to zero, resynchronizes, and continues while hidden [ui][siege][timer]")
{
    for (const char* theme : {"legacy", "modern"})
    {
        CAPTURE(std::string(theme));
        GuardRegistrationFixture fixture(theme);

        const auto requestBefore = std::chrono::steady_clock::now();
        fixture.SetStatus(CASTLESIEGE_STATE_READYSIEGE, 2);
        const auto requestAfter = std::chrono::steady_clock::now();
        const auto initial = fixture.NextStageText();
        const auto initialAt = std::chrono::steady_clock::now();
        REQUIRE_FALSE(initial.empty());
        const auto initialElapsedLower = std::chrono::duration_cast<std::chrono::seconds>(initialAt - requestAfter).count();
        const auto initialElapsedUpper = std::chrono::duration_cast<std::chrono::seconds>(initialAt - requestBefore).count();
        CAPTURE(initialElapsedLower);
        CAPTURE(initialElapsedUpper);
        CHECK(initialElapsedLower >= 0);
        CHECK(initialElapsedUpper >= initialElapsedLower);
        CHECK(GuardRegistrationFixture::MatchesElapsedWindow(initial, 2, initialElapsedLower, initialElapsedUpper));

        std::this_thread::sleep_until(requestAfter + std::chrono::milliseconds(1100));
        fixture.window.Update();
        fixture.Refresh();
        const auto elapsedAt = std::chrono::steady_clock::now();
        const auto elapsedLowerBound = std::chrono::duration_cast<std::chrono::seconds>(elapsedAt - requestAfter).count();
        const auto elapsedUpperBound = std::chrono::duration_cast<std::chrono::seconds>(elapsedAt - requestBefore).count();
        CAPTURE(elapsedLowerBound);
        CAPTURE(elapsedUpperBound);
        REQUIRE(elapsedLowerBound >= 1);
        REQUIRE(elapsedUpperBound >= elapsedLowerBound);
        const auto elapsed = fixture.NextStageText();
        CHECK(GuardRegistrationFixture::MatchesElapsedWindow(elapsed, 2, elapsedLowerBound, elapsedUpperBound));

        // The countdown is anchored when SetData receives the status. After this bound it must
        // render the localized zero value regardless of CI scheduling between intermediate frames.
        std::this_thread::sleep_until(requestAfter + std::chrono::milliseconds(2200));
        fixture.window.Update();
        fixture.Refresh();
        const auto expired = fixture.NextStageText();
        CHECK(expired == GuardRegistrationFixture::ExpectedNextStageText(0));

        const auto resyncBefore = std::chrono::steady_clock::now();
        fixture.SetStatus(CASTLESIEGE_STATE_READYSIEGE, 12);
        const auto resyncAfter = std::chrono::steady_clock::now();
        const auto resynchronized = fixture.NextStageText();
        const auto resyncAt = std::chrono::steady_clock::now();
        const auto resyncElapsedLower = std::chrono::duration_cast<std::chrono::seconds>(resyncAt - resyncAfter).count();
        const auto resyncElapsedUpper = std::chrono::duration_cast<std::chrono::seconds>(resyncAt - resyncBefore).count();
        CAPTURE(resyncElapsedLower);
        CAPTURE(resyncElapsedUpper);
        REQUIRE(resyncElapsedLower >= 0);
        REQUIRE(resyncElapsedUpper >= resyncElapsedLower);
        CHECK(GuardRegistrationFixture::MatchesElapsedWindow(resynchronized, 12,
                                                              resyncElapsedLower, resyncElapsedUpper));

        fixture.SetStatus(CASTLESIEGE_STATE_READYSIEGE, 0);
        const auto zeroValue = fixture.NextStageText();
        CHECK(zeroValue == GuardRegistrationFixture::ExpectedNextStageText(0));
        std::this_thread::sleep_until(std::chrono::steady_clock::now() + std::chrono::milliseconds(1100));
        fixture.window.Update();
        fixture.Refresh();
        CHECK(fixture.NextStageText() == GuardRegistrationFixture::ExpectedNextStageText(0));

        const auto hideBefore = std::chrono::steady_clock::now();
        fixture.SetStatus(CASTLESIEGE_STATE_READYSIEGE, 2);
        const auto hideAfter = std::chrono::steady_clock::now();
        fixture.window.Show(false);
        std::this_thread::sleep_until(hideAfter + std::chrono::milliseconds(2200));
        fixture.window.Show(true);
        fixture.window.Update();
        fixture.Refresh();
        const auto shownAt = std::chrono::steady_clock::now();
        CAPTURE(std::chrono::duration_cast<std::chrono::milliseconds>(shownAt - hideBefore).count());
        CHECK(fixture.NextStageText() == GuardRegistrationFixture::ExpectedNextStageText(0));
    }
}
