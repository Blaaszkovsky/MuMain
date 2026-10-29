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

#ifdef MU_TEST_NPC_AOT_TRANSPORT
#include <winsock2.h>
#include "Dotnet/Connection.h"
#include "Dotnet/PacketFunctions_ClientToServer.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Sprites/GpuUploadSkip.h"
#include "Render/Textures/TextureFailure.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Events/CursedTempleEnter.h"

#include <chrono>
#include <memory>
#include <vector>

extern bool g_bWndActive;
#endif

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

#ifdef MU_TEST_NPC_AOT_TRANSPORT
namespace
{
constexpr BYTE TalkCode = 0x30;
constexpr BYTE CloseCode = 0x31;
constexpr std::uint16_t MirageId = 0x123;
constexpr std::uint16_t NextNpcId = 0x456;

// Same real, unencrypted AOT transport as test_control_attack; never connects to a game host.
class NpcPacketPeer
{
public:
    NpcPacketPeer()
    {
        WSADATA data{};
        REQUIRE(WSAStartup(MAKEWORD(2, 2), &data) == 0);
        m_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        REQUIRE(m_listener != INVALID_SOCKET);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE(bind(m_listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        REQUIRE(listen(m_listener, 1) == 0);
        int size = sizeof(address);
        REQUIRE(getsockname(m_listener, reinterpret_cast<sockaddr*>(&address), &size) == 0);
        port = ntohs(address.sin_port);
    }

    ~NpcPacketPeer()
    {
        if (m_peer != INVALID_SOCKET) closesocket(m_peer);
        if (m_listener != INVALID_SOCKET) closesocket(m_listener);
        WSACleanup();
    }

    void Accept()
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_listener, &readable);
        timeval wait{2, 0};
        REQUIRE(select(0, &readable, nullptr, nullptr, &wait) == 1);
        m_peer = accept(m_listener, nullptr, nullptr);
        REQUIRE(m_peer != INVALID_SOCKET);
    }

    std::vector<BYTE> Collect()
    {
        std::vector<BYTE> bytes;
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
        while (std::chrono::steady_clock::now() < until)
        {
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(m_peer, &readable);
            timeval wait{0, 10000};
            const int ready = select(0, &readable, nullptr, nullptr, &wait);
            REQUIRE(ready != SOCKET_ERROR);
            if (ready == 0) continue;
            char buffer[256];
            const int count = recv(m_peer, buffer, sizeof(buffer), 0);
            REQUIRE(count > 0);
            bytes.insert(bytes.end(), buffer, buffer + count);
        }
        std::vector<BYTE> codes;
        for (std::size_t offset = 0; offset < bytes.size();)
        {
            REQUIRE(bytes.size() - offset >= 3);
            REQUIRE((bytes[offset] == 0xC1 || bytes[offset] == 0xC3));
            const std::size_t size = bytes[offset + 1];
            REQUIRE(size >= 3);
            REQUIRE(size <= bytes.size() - offset);
            codes.push_back(bytes[offset + 2]);
            if (codes.back() == CloseCode)
            {
                CHECK(bytes[offset] == 0xC1);
                CHECK(size == 3);
            }
            if (codes.back() == TalkCode)
            {
                CHECK(bytes[offset] == 0xC3);
                CHECK(size == 5);
            }
            offset += size;
        }
        return codes;
    }

    int port = 0;

private:
    SOCKET m_listener = INVALID_SOCKET;
    SOCKET m_peer = INVALID_SOCKET;
};

class TempleEntryFixture : public EventEntryFixture
{
public:
    explicit TempleEntryFixture(const char* theme)
        : EventEntryFixture(theme), m_previousSocket(SocketClient), m_previousHero(Hero),
          m_previousAttribute(CharacterAttribute), m_previousActive(g_bWndActive),
          m_previousLanguage(g_strSelectedML),
          m_previousTextureCollector(Render::Textures::GetTextureErrorCollector())
    {
        Hero = &m_hero;
        CharacterAttribute = &m_machine.Character;
        CharacterAttribute->Level = 400;
        g_bWndActive = true;
        g_strSelectedML = L"Eng";
        Render::Sprites::SetGpuUploadsSkipped(true);
        Render::Textures::SetTextureErrorCollector([](const std::string&) {});
        REQUIRE(g_pNewUISystem->Create());
        window = std::make_unique<mu::ui::window::CCursedTempleEnter>();
        REQUIRE(window->Create(g_pNewUIMng));
        window->Show(true);
        window->Update();
        Refresh();
        m_connection = std::make_unique<Connection>(u"127.0.0.1", peer.port, false, nullptr);
        REQUIRE(m_connection->IsConnected());
        peer.Accept();
        SocketClient = m_connection.get();
    }

    ~TempleEntryFixture()
    {
        window.reset();
        g_pNewUISystem->Release();
        SocketClient = m_previousSocket;
        m_connection.reset();
        g_pNewKeyInput->SetKeyState(VK_ESCAPE, mu::ui::window::CNewKeyInput::KEY_NONE);
        Render::Textures::SetTextureErrorCollector(m_previousTextureCollector);
        Render::Sprites::SetGpuUploadsSkipped(false);
        Hero = m_previousHero;
        CharacterAttribute = m_previousAttribute;
        g_bWndActive = m_previousActive;
        g_strSelectedML = m_previousLanguage;
    }

    void Click(const char* id)
    {
        auto* document = window->GetFillDocument();
        REQUIRE(document != nullptr);
        auto* button = document->GetElementById(id);
        REQUIRE(button != nullptr);
        button->Click();
    }

    void Talk(std::uint16_t id) { SocketClient->ToGameServer()->SendTalkToNpcRequest(id); }

    NpcPacketPeer peer;
    std::unique_ptr<mu::ui::window::CCursedTempleEnter> window;

private:
    Connection* m_previousSocket;
    CHARACTER* m_previousHero;
    decltype(CharacterAttribute) m_previousAttribute;
    bool m_previousActive;
    std::wstring m_previousLanguage;
    Render::Textures::TextureErrorCollector m_previousTextureCollector;
    CHARACTER m_hero{};
    CHARACTER_MACHINE m_machine{};
    std::unique_ptr<Connection> m_connection;
};
} // namespace

TEST_CASE("Mirage manual Close and Escape release the NPC before the next talk [ui][event-entry][mirage-close]")
{
    for (const char* theme : {"legacy", "modern"})
    for (const bool escape : {false, true})
    {
        CAPTURE(std::string(theme));
        CAPTURE(escape);
        TempleEntryFixture fixture(theme);
        fixture.Talk(MirageId);
        if (escape)
        {
            g_pNewKeyInput->SetKeyState(VK_ESCAPE, mu::ui::window::CNewKeyInput::KEY_PRESS);
            CHECK_FALSE(fixture.window->UpdateKeyEvent());
        }
        else
        {
            fixture.Click("btn_close");
            fixture.window->Update();
        }
        CHECK_FALSE(fixture.window->IsVisible());
        fixture.window->Update(); // A queued click must not close a later dialog twice.
        fixture.Talk(NextNpcId);
        CHECK(fixture.peer.Collect() == std::vector<BYTE>{TalkCode, CloseCode, TalkCode});
    }
}

TEST_CASE("Mirage successful entry and programmatic hides never close a later NPC [ui][event-entry][mirage-close]")
{
    for (const char* theme : {"legacy", "modern"})
    {
        CAPTURE(std::string(theme));
        TempleEntryFixture fixture(theme);
        fixture.Click("btn_enter");
        fixture.window->Update();
        g_pNewUISystem->HideAll(); // The actual successful entry/warp visibility path.
        fixture.Talk(NextNpcId);
        fixture.window->Update();
        CHECK(fixture.peer.Collect() == std::vector<BYTE>{0xBF, TalkCode});

        fixture.window->Show(true);
        fixture.Click("btn_close"); // Reported by RmlUi before a warp has hidden the window.
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_CURSEDTEMPLE_NPC);
        fixture.Talk(NextNpcId);
        fixture.window->Update();
        CHECK(fixture.peer.Collect() == std::vector<BYTE>{TalkCode});
    }
}
#endif
