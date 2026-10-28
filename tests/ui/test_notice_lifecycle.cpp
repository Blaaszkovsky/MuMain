#include "stdafx.h"

#include <doctest.h>

#include "RmlLayoutFixture.h"
#include "Network/Server/WSclient.h"
#include "Scenes/SceneCore.h"
#include "UI/HUD/Notices.h"
#include "UI/HUD/SlideTicker.h"
#include "UI/HUD/SlideTickerPresentation.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "UI/Windows/MsgWin.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>

void ReceiveNotice(const BYTE* buffer);

namespace
{
class NoticeFixture : public UI::Tests::RmlLayoutFixture
{
public:
    NoticeFixture()
        : m_directory(std::filesystem::current_path()), m_scene(SceneFlag),
          m_theme(UI::RmlBridge::GetActiveThemeName())
    {
        std::filesystem::current_path(MU_BIN_DIR);
        UI::RmlBridge::SetContextResolver([this](std::string_view) { return context; });
        SceneFlag = MAIN_SCENE;
        UI::Notices::Clear();
    }

    ~NoticeFixture()
    {
        UI::Notices::Clear();
        g_MsgWin.Release();
        UI::RmlBridge::SetContextResolver({});
        UI::RmlBridge::SetActiveThemeName(m_theme);
        SceneFlag = m_scene;
        std::filesystem::current_path(m_directory);
    }

    Rml::ElementDocument* RenderNotices()
    {
        UI::Notices::Render();
        Refresh();
        for (int index = 0; index < context->GetNumDocuments(); ++index)
        {
            auto* document = context->GetDocument(index);
            if (document->GetSourceURL().find("notices.rml") != std::string::npos)
                return document;
        }
        return nullptr;
    }

private:
    std::filesystem::path m_directory;
    EGameScene m_scene;
    std::string m_theme;
};

void ReceiveAnnouncement(const char* text)
{
    PRECEIVE_NOTICE packet{};
    std::strncpy(packet.Notice, text, sizeof(packet.Notice) - 1);
    ReceiveNotice(reinterpret_cast<const BYTE*>(&packet));
}

} // namespace

TEST_CASE("Gameplay announcements are ignored before entering the world [ui][notices]")
{
    NoticeFixture fixture;
    for (const auto scene : {LOG_IN_SCENE, CHARACTER_SCENE, LOADING_SCENE})
    {
        CAPTURE(scene);
        UI::Notices::Clear();
        SceneFlag = scene;
        ReceiveAnnouncement("Golden invasion outside gameplay");
        SceneFlag = MAIN_SCENE;
        auto* document = fixture.RenderNotices();
        REQUIRE(document != nullptr);
        CHECK(document->GetInnerRML().find("Golden invasion outside gameplay") == std::string::npos);
    }
}

TEST_CASE("Leaving gameplay purges queued notices without dismissing login errors [ui][notices]")
{
    NoticeFixture fixture;
    for (const char* theme : {"legacy", "modern"})
    {
        CAPTURE(theme);
        UI::RmlBridge::SetActiveThemeName(theme);
        UI::RmlBridge::ReloadAllThemedDocuments();
        SceneFlag = MAIN_SCENE;
        ReceiveAnnouncement("Blood Castle starts soon");
        auto* document = fixture.RenderNotices();
        REQUIRE(document != nullptr);
        CHECK(document->IsVisible());
        CHECK(document->GetInnerRML().find("Blood Castle starts soon") != std::string::npos);

        SceneFlag = LOG_IN_SCENE;
        g_MsgWin.PopUp(RECEIVE_LOG_IN_FAIL_PASSWORD);
        REQUIRE(g_MsgWin.IsVisible());
        REQUIRE(g_MsgWin.PendingMessageCode() == RECEIVE_LOG_IN_FAIL_PASSWORD);
        UI::Notices::Move();
        fixture.RenderNotices();
        CHECK_FALSE(document->IsVisible());
        CHECK(g_MsgWin.IsVisible());

        SceneFlag = MAIN_SCENE;
        document = fixture.RenderNotices();
        REQUIRE(document != nullptr);
        CHECK(document->GetInnerRML().find("Blood Castle starts soon") == std::string::npos);
        UI::Notices::Clear();
    }
}

TEST_CASE("A fresh gameplay session discards repeated slide announcements [ui][notices]")
{
    NoticeFixture fixture;
    UI::HUD::SlideLane lane;
    lane.Init();
    lane.AddSlide(3, 30, L"Old Golden invasion", -1, 2.5f, 0xFFFFFFFF);
    REQUIRE(lane.m_SlideQueue.size() == 3);
    lane.Init();
    CHECK(lane.m_SlideQueue.empty());
}

namespace
{
struct PresentationLane
{
    const wchar_t* text = L"";
    int alpha = 0;
    float x = 640.f;
    BOOL HaveText() const { return text[0] == L'\0'; }
    int GetAlphaRate() const { return alpha; }
    void Advance(BOOL forceFade = FALSE)
    {
        alpha = forceFade || HaveText() ? std::max(0, alpha - 30) : std::min(205, alpha + 30);
        if (alpha > 0)
            x -= 2.5f;
    }
    UI::HUD::SlideDisplay Display(BOOL = FALSE) const
    {
        UI::HUD::SlideDisplay display;
        display.text = text;
        display.x = x;
        display.alpha = static_cast<unsigned char>(alpha);
        display.shown = alpha > 0;
        return display;
    }
};
} // namespace

TEST_CASE("An active slide notice advances and becomes visible after the help lane fades [ui][notices]")
{
    PresentationLane help{L"Local help", 60};
    PresentationLane notice{L"EVENTS-SLIDE-REGRESSION", 0};
    auto display = UI::HUD::Detail::AdvanceSlideDisplay(help, notice);
    CHECK(display.text == help.text);
    CHECK(help.alpha == 30);
    CHECK(notice.alpha == 0);

    for (int frame = 0; frame < 20; ++frame)
        display = UI::HUD::Detail::AdvanceSlideDisplay(help, notice);
    REQUIRE(display.shown);
    CHECK(display.text == notice.text);
    CHECK(display.alpha > 0);
    const float previousX = display.x;
    display = UI::HUD::Detail::AdvanceSlideDisplay(help, notice);
    CHECK(display.x < previousX);
}

TEST_CASE("Local slide help continues when no server notice is pending [ui][notices]")
{
    PresentationLane help{L"Local help", 0};
    PresentationLane notice;
    const auto display = UI::HUD::Detail::AdvanceSlideDisplay(help, notice);
    CHECK(display.shown);
    CHECK(display.text == help.text);
    CHECK(help.alpha > 0);
}

TEST_CASE("Local slide help resumes after the server notice fades away [ui][notices]")
{
    PresentationLane help{L"Local help", 0};
    PresentationLane notice{L"", 60};
    auto display = UI::HUD::Detail::AdvanceSlideDisplay(help, notice);
    CHECK(notice.alpha == 30);
    CHECK(help.alpha == 0);
    for (int frame = 0; frame < 3; ++frame)
        display = UI::HUD::Detail::AdvanceSlideDisplay(help, notice);
    CHECK(display.shown);
    CHECK(display.text == help.text);
    CHECK(help.alpha > 0);
}

