#include "stdafx.h"

#include <doctest.h>

#include "RmlLayoutFixture.h"
#include "I18N/All.h"
#include "Network/Server/WSclient.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowManager.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <filesystem>
#include <string>

void ReceiveEventZoneOpenTime(const BYTE* buffer);

namespace
{
class EventEntryFixture : public UI::Tests::RmlLayoutFixture
{
public:
    explicit EventEntryFixture(const char* theme)
        : m_directory(std::filesystem::current_path()), m_scene(SceneFlag),
          m_theme(UI::RmlBridge::GetActiveThemeName()),
          m_previousDialog(mu::ui::window::g_pGenericConfirmDialog), m_locale(I18N::GetCurrentLocale())
    {
        std::filesystem::current_path(MU_BIN_DIR);
        UI::RmlBridge::SetContextResolver([this](std::string_view) { return context; });
        SceneFlag = MAIN_SCENE;
        UI::RmlBridge::SetActiveThemeName(theme);
        I18N::SetLocale("en");
        dialog.Create(&m_manager);
        mu::ui::window::g_pGenericConfirmDialog = &dialog;
    }

    ~EventEntryFixture()
    {
        mu::ui::window::g_pGenericConfirmDialog = m_previousDialog;
        dialog.Release();
        UI::RmlBridge::SetContextResolver({});
        UI::RmlBridge::SetActiveThemeName(m_theme);
        I18N::SetLocale(m_locale.c_str());
        SceneFlag = m_scene;
        std::filesystem::current_path(m_directory);
    }

    mu::ui::window::CGenericConfirmDialog dialog;

private:
    mu::ui::window::CManager m_manager;
    std::filesystem::path m_directory;
    EGameScene m_scene;
    std::string m_theme;
    mu::ui::window::CGenericConfirmDialog* m_previousDialog;
    std::string m_locale;
};
} // namespace

TEST_CASE("Chaos Castle opening shows the transmitted participant count without an invented capacity [ui][event-entry]")
{
    for (const char* theme : {"legacy", "modern"})
    {
        for (const BYTE count : {BYTE(0), BYTE(7)})
        {
            CAPTURE(std::string(theme));
            CAPTURE(count);
            EventEntryFixture fixture(theme);
            PHEADER_MATCH_OPEN_VALUE packet{};
            packet.Value = 4;
            packet.KeyM = count;
            ReceiveEventZoneOpenTime(reinterpret_cast<const BYTE*>(&packet));
            fixture.Refresh();
            REQUIRE(fixture.dialog.IsVisible());
            Rml::ElementDocument* document = nullptr;
            for (int index = 0; index < fixture.context->GetNumDocuments(); ++index)
            {
                auto* candidate = fixture.context->GetDocument(index);
                if (candidate->GetSourceURL().find("generic_confirm_dialog.rml") != std::string::npos)
                    document = candidate;
            }
            REQUIRE(document != nullptr);
            const auto markup = document->GetInnerRML();
            CHECK(markup.find("/100") == std::string::npos);
            CHECK(markup.find("Currently, " + std::to_string(count) + " people are registered to participate.") != std::string::npos);
        }
    }
}
