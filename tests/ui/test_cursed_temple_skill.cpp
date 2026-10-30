#include "stdafx.h"
#include <winsock2.h>
#include <doctest.h>

#include "RmlLayoutFixture.h"
#include "Dotnet/Connection.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Events/w_CursedTemple.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Sprites/GpuUploadSkip.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Render/Textures/TextureFailure.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/CustomMessageBox.h"
#include "UI/Events/CursedTempleSystem.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/RmlThemedView.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <vector>

extern int TargetX;
extern int TargetY;
extern bool g_bWndActive;

namespace
{
constexpr short HeroId = 512;
constexpr short TeamId = 513;
constexpr short EnemyId = 514;
constexpr int TeamSlot = 1;
constexpr int EnemySlot = MAX_CHARACTERS_CLIENT - 1;
constexpr BYTE SkillRange = 5;
constexpr BYTE SkillCost = 5;
constexpr BYTE SkillBudget = 20;
constexpr DWORD MissingSelection = static_cast<DWORD>(-1);
constexpr WORD StatueKey = 0x123;
constexpr DWORD CaptureMilliseconds = 10000;

// Exercise real progress callbacks and the manager's event queue without waiting ten seconds or
// constructing the unrelated main-frame hotkey UI called by the progress box's per-frame tick.
class QueuedTempleProgress : public mu::ui::window::CCursedTempleProgressMsgBox
{
public:
    ~QueuedTempleProgress() override { if (destroyed) *destroyed = true; }
    bool Update() override
    {
        if (GetCallbackFunc(mu::ui::window::MSGBOX_EVENT_USER_CUSTOM_PROGRESS_COMPLETEPROCESS) == nullptr)
            return CCursedTempleProgressMsgBox::Update();
        return true;
    }
    bool* destroyed = nullptr;
};

class QueuedOrdinaryProgress : public mu::ui::window::CProgressMsgBox
{
public:
    bool Update() override { ++updates; return true; }
    int updates = 0;
};

// End the real character table at an inaccessible page. Index400 must not silently read padding.
class GuardedCharacters
{
public:
    GuardedCharacters()
    {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const std::size_t bytes = sizeof(CHARACTER) * MAX_CHARACTERS_CLIENT;
        const std::size_t committed = (bytes + info.dwPageSize - 1) / info.dwPageSize * info.dwPageSize;
        m_storage = VirtualAlloc(nullptr, committed + info.dwPageSize, MEM_RESERVE, PAGE_NOACCESS);
        REQUIRE(m_storage != nullptr);
        REQUIRE(VirtualAlloc(m_storage, committed, MEM_COMMIT, PAGE_READWRITE) == m_storage);
        characters = reinterpret_cast<CHARACTER*>(static_cast<BYTE*>(m_storage) + committed - bytes);
        std::uninitialized_value_construct_n(characters, MAX_CHARACTERS_CLIENT);
    }

    ~GuardedCharacters()
    {
        std::destroy_n(characters, MAX_CHARACTERS_CLIENT);
        VirtualFree(m_storage, 0, MEM_RELEASE);
    }

    CHARACTER* characters = nullptr;

private:
    void* m_storage = nullptr;
};

// Same real asynchronous NativeAOT transport as the NPC/attack fixtures; no game host is contacted.
class SkillPacketPeer
{
public:
    SkillPacketPeer()
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

    ~SkillPacketPeer()
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
        return bytes;
    }

    int port = 0;

private:
    SOCKET m_listener = INVALID_SOCKET;
    SOCKET m_peer = INVALID_SOCKET;
};

class TempleSkillFixture : public UI::Tests::RmlLayoutFixture
{
public:
    explicit TempleSkillFixture(const char* theme = "legacy")
        : m_previousHero(Hero), m_previousCharacters(CharactersClient), m_previousModels(Models),
          m_previousMachine(CharacterMachine), m_previousAttribute(CharacterAttribute), m_previousSkills(SkillAttribute),
          m_previousSocket(SocketClient), m_previousScene(SceneFlag), m_previousWorld(gMapManager.WorldActive),
          m_previousSelection(SelectedCharacter), m_previousTargetX(TargetX), m_previousTargetY(TargetY),
          m_previousMousePush(MouseRButtonPush),
          m_previousActive(g_bWndActive), m_previousLanguage(g_strSelectedML),
          m_previousTheme(UI::RmlBridge::GetActiveThemeName()), m_previousGauge(g_CursedTemple->IsGaugebarEnabled()),
          m_previousDirectory(std::filesystem::current_path()),
          m_previousTextureCollector(Render::Textures::GetTextureErrorCollector()),
          m_models(std::make_unique<BMD[]>(MODEL_PLAYER + 1)), m_skills(std::make_unique<SKILL_ATTRIBUTE[]>(MAX_SKILLS))
    {
        std::filesystem::current_path(MU_BIN_DIR);
        UI::RmlBridge::SetContextResolver([this](std::string_view) { return context; });
        UI::RmlBridge::SetActiveThemeName(theme);
        Render::Sprites::SetGpuUploadsSkipped(true);
        Render::Textures::SetTextureErrorCollector([](const std::string&) {});
        CharactersClient = m_table.characters;
        Hero = &CharactersClient[0];
        CharacterMachine = &m_machine;
        CharacterAttribute = &m_machine.Character;
        CharacterAttribute->Level = 400;
        SkillAttribute = m_skills.get();
        g_bWndActive = true;
        g_strSelectedML = L"Eng";
        Models = m_models.get();
        Models[MODEL_PLAYER].NumActions = MAX_PLAYER_ACTION;
        Models[MODEL_PLAYER].Actions = new Action_t[MAX_PLAYER_ACTION]{};
        SceneFlag = MAIN_SCENE;
        gMapManager.WorldActive = WD_45CURSEDTEMPLE_LV1;
        Place(*Hero, HeroId);
        Place(CharactersClient[TeamSlot], TeamId);
        Place(CharactersClient[EnemySlot], EnemyId);
        TargetX = TargetY = 40;
        SelectedCharacter = -1;

        for (int i = 0; i < 4; ++i)
        {
            const int skill = AT_SKILL_CURSED_TEMPLE_PRODECTION + static_cast<int>(i);
            SkillAttribute[skill].Distance = SkillRange;
            SkillAttribute[skill].KillCount = SkillCost;
        }

        REQUIRE(g_pNewUISystem->Create());
        m_window = std::make_unique<mu::ui::window::CCursedTempleSystem>();
        REQUIRE(m_window->Create(g_pNewUIMng));
        const std::array<UI::CursedTemple::PartyPosition, 2> team{
            UI::CursedTemple::PartyPosition{HeroId, WD_45CURSEDTEMPLE_LV1, 40, 40},
            UI::CursedTemple::PartyPosition{TeamId, WD_45CURSEDTEMPLE_LV1, 40, 40}};
        UI::CursedTemple::MatchStatus status{};
        status.relicHolderIndex = EnemyId;
        status.localTeam = SEASON3A::eTeam_Allied;
        status.party = team;
        m_window->SetMatchStatus(status);
        m_window->SetSkillPoints(SkillBudget);
        m_connection = std::make_unique<Connection>(u"127.0.0.1", peer.port, false, nullptr);
        REQUIRE(m_connection->IsConnected());
        peer.Accept();
        SocketClient = m_connection.get();
    }

    ~TempleSkillFixture()
    {
        for (const int slot : {0, TeamSlot, EnemySlot})
        {
            for (const int effect : std::array<int, 4>{MODEL_SKILL_INFERNO, MODEL_SHIELD_CRASH, BITMAP_SHOCK_WAVE, MODEL_FENRIR_THUNDER})
                DeleteEffect(effect, &CharactersClient[slot].Object);
        }
        m_window.reset();
        g_pNewUISystem->Release();
        Bitmaps.UnloadAllImages();
        SocketClient = m_previousSocket;
        m_connection.reset();
        UI::RmlBridge::SetContextResolver({});
        Render::Textures::SetTextureErrorCollector(m_previousTextureCollector);
        Render::Sprites::SetGpuUploadsSkipped(false);
        SkillAttribute = m_previousSkills;
        Hero = m_previousHero;
        CharactersClient = m_previousCharacters;
        Models = m_previousModels;
        CharacterMachine = m_previousMachine;
        CharacterAttribute = m_previousAttribute;
        SceneFlag = m_previousScene;
        gMapManager.WorldActive = m_previousWorld;
        SelectedCharacter = m_previousSelection;
        TargetX = m_previousTargetX;
        TargetY = m_previousTargetY;
        MouseRButtonPush = m_previousMousePush;
        g_bWndActive = m_previousActive;
        g_strSelectedML = m_previousLanguage;
        UI::RmlBridge::SetActiveThemeName(m_previousTheme);
        g_CursedTemple->SetGaugebarEnabled(m_previousGauge);
        std::filesystem::current_path(m_previousDirectory);
    }

    QueuedTempleProgress* StartProgress(bool& destroyed)
    {
        const mu::ui::window::CMessageBoxFactory::TContainer<QueuedTempleProgress> container;
        auto* progress = g_MessageBox->NewMessageBox(container);
        REQUIRE(progress != nullptr);
        progress->destroyed = &destroyed;
        REQUIRE(progress->Create(CaptureMilliseconds));
        progress->SetNpcIndex(StatueKey);
        g_CursedTemple->SetGaugebarEnabled(true);
        ExpectTalk();
        return progress;
    }

    void Resolve(bool succeeded, short target = HeroId, int skill = AT_SKILL_CURSED_TEMPLE_SUBLIMATION)
    {
        const std::array<int, 3> slots{0, TeamSlot, EnemySlot};
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            if (!m_bones[i]) m_bones[i] = std::make_unique<vec34_t[]>(MAX_BONES);
            CharactersClient[slots[i]].Object.BoneTransform = m_bones[i].get();
        }
        m_window->ResolveSkill({static_cast<WORD>(skill), EnemyId, static_cast<WORD>(target), succeeded});
    }

    void ExpectTalk()
    {
        // C3:30, NPC key big-endian, sent by the actual NativeAOT packet writer.
        const std::vector<BYTE> expected{0xC3, 5, 0x30, static_cast<BYTE>(StatueKey >> 8), static_cast<BYTE>(StatueKey)};
        CHECK(peer.Collect() == expected);
    }

    void Cast(int skill, DWORD selection)
    {
        Hero->m_CursedTempleCurSkill = skill;
        Hero->m_CursedTempleCurSkillPacket = false;
        MouseRButtonPush = true;
        m_window->SetCursedTempleSkill(Hero, &Hero->Object, selection);
    }

    void ExpectRejected(int skill, DWORD selection)
    {
        Cast(skill, selection);
        CHECK_FALSE(Hero->m_CursedTempleCurSkillPacket);
        CHECK_FALSE(MouseRButtonPush);
        CHECK(peer.Collect().empty());
    }

    void ExpectRequest(int skill, DWORD selection, short target)
    {
        Cast(skill, selection);
        CHECK(Hero->m_CursedTempleCurSkillPacket);
        CHECK_FALSE(MouseRButtonPush);
        // C1:BF:02, skill big-endian, target little-endian, range: canonical packet XML.
        const std::vector<BYTE> expected{0xC1, 9, 0xBF, 2, static_cast<BYTE>(skill >> 8),
            static_cast<BYTE>(skill), static_cast<BYTE>(target), static_cast<BYTE>(target >> 8), SkillRange};
        CHECK(peer.Collect() == expected);
    }

    SkillPacketPeer peer;

private:
    static void Place(CHARACTER& character, short key)
    {
        character.Key = key;
        character.Helper.Type = -1;
        character.Object.Live = true;
        character.Object.Type = MODEL_PLAYER;
        character.Object.Kind = KIND_PLAYER;
        character.Object.Position[0] = character.Object.Position[1] = (40.f + 0.5f) * TERRAIN_SCALE;
    }

    CHARACTER* m_previousHero;
    CHARACTER* m_previousCharacters;
    BMD* m_previousModels;
    CHARACTER_MACHINE* m_previousMachine;
    decltype(CharacterAttribute) m_previousAttribute;
    SKILL_ATTRIBUTE* m_previousSkills;
    Connection* m_previousSocket;
    EGameScene m_previousScene;
    int m_previousWorld;
    int m_previousSelection;
    int m_previousTargetX;
    int m_previousTargetY;
    bool m_previousMousePush;
    bool m_previousActive;
    std::wstring m_previousLanguage;
    std::string m_previousTheme;
    bool m_previousGauge;
    std::filesystem::path m_previousDirectory;
    Render::Textures::TextureErrorCollector m_previousTextureCollector;
    GuardedCharacters m_table;
    CHARACTER_MACHINE m_machine{};
    std::unique_ptr<BMD[]> m_models;
    std::unique_ptr<SKILL_ATTRIBUTE[]> m_skills;
    std::array<std::unique_ptr<vec34_t[]>, 3> m_bones;
    std::unique_ptr<Connection> m_connection;
    std::unique_ptr<mu::ui::window::CCursedTempleSystem> m_window;
};
}

// Each dangerous case is a separate discovered test/process so the old AV cannot mask other cases.
TEST_CASE("Restraint rejects missing character selection [ui][temple-skill][restraint-missing]")
{
    TempleSkillFixture fixture;
    fixture.ExpectRejected(AT_SKILL_CURSED_TEMPLE_RESTRAINT, MissingSelection);
}

TEST_CASE("Sublimation rejects missing character selection [ui][temple-skill][sublimation-missing]")
{
    TempleSkillFixture fixture;
    fixture.ExpectRejected(AT_SKILL_CURSED_TEMPLE_SUBLIMATION, MissingSelection);
}

TEST_CASE("Restraint rejects index at character table boundary [ui][temple-skill][restraint-boundary]")
{
    TempleSkillFixture fixture;
    fixture.ExpectRejected(AT_SKILL_CURSED_TEMPLE_RESTRAINT, MAX_CHARACTERS_CLIENT);
}

TEST_CASE("Sublimation rejects index at character table boundary [ui][temple-skill][sublimation-boundary]")
{
    TempleSkillFixture fixture;
    fixture.ExpectRejected(AT_SKILL_CURSED_TEMPLE_SUBLIMATION, MAX_CHARACTERS_CLIENT);
}

TEST_CASE("Temple hostile skills reject absent target keys and teammates [ui][temple-skill]")
{
    TempleSkillFixture fixture;
    for (const int skill : {AT_SKILL_CURSED_TEMPLE_RESTRAINT, AT_SKILL_CURSED_TEMPLE_SUBLIMATION})
    {
        CAPTURE(skill);
        fixture.ExpectRejected(skill, TeamSlot);
        CharactersClient[EnemySlot].Key = -1;
        fixture.ExpectRejected(skill, EnemySlot);
    }
}

TEST_CASE("Temple hostile skills send the last valid enemy slot [ui][temple-skill]")
{
    TempleSkillFixture fixture;
    for (const int skill : {AT_SKILL_CURSED_TEMPLE_RESTRAINT, AT_SKILL_CURSED_TEMPLE_SUBLIMATION})
    {
        CAPTURE(skill);
        fixture.ExpectRequest(skill, EnemySlot, EnemyId);
    }
}

TEST_CASE("Temple protection and tracking self-target without a selected character [ui][temple-skill]")
{
    TempleSkillFixture fixture;
    for (const int skill : {AT_SKILL_CURSED_TEMPLE_PRODECTION, AT_SKILL_CURSED_TEMPLE_TELEPORT})
    {
        CAPTURE(skill);
        fixture.ExpectRequest(skill, MissingSelection, HeroId);
    }
}

TEST_CASE("Shield burn interrupts local temple progress before queued completion can retry talk [ui][temple-skill][shield-burn-progress]")
{
    for (const char* theme : {"legacy", "modern"})
    for (const bool completionFirst : {false, true})
    {
        CAPTURE(std::string(theme));
        CAPTURE(completionFirst);
        bool destroyed = false;
        TempleSkillFixture fixture(theme);
        auto* progress = fixture.StartProgress(destroyed);
        if (completionFirst)
            g_MessageBox->SendEvent(progress, mu::ui::window::MSGBOX_EVENT_USER_CUSTOM_PROGRESS_COMPLETEPROCESS);

        fixture.Resolve(true);
        CHECK(Hero->Object.CurrentAction == PLAYER_SHOCK);
        if (!completionFirst)
            g_MessageBox->SendEvent(progress, mu::ui::window::MSGBOX_EVENT_USER_CUSTOM_PROGRESS_COMPLETEPROCESS);

        g_MessageBox->Update();
        CHECK(destroyed);
        CHECK(g_MessageBox->IsEmpty());
        CHECK_FALSE(g_CursedTemple->IsGaugebarEnabled());
        CHECK(fixture.peer.Collect().empty());
        g_MessageBox->Update();
        CHECK(fixture.peer.Collect().empty());
    }
}

TEST_CASE("Rejected shield burn other targets and tracking preserve local temple completion [ui][temple-skill][shield-burn-progress]")
{
    for (const char* theme : {"legacy", "modern"})
    for (const int scenario : {0, 1, 2})
    {
        CAPTURE(std::string(theme));
        CAPTURE(scenario);
        bool destroyed = false;
        TempleSkillFixture fixture(theme);
        auto* progress = fixture.StartProgress(destroyed);
        const bool succeeded = scenario != 0;
        const short target = scenario == 1 ? TeamId : HeroId;
        const int skill = scenario == 2 ? AT_SKILL_CURSED_TEMPLE_TELEPORT : AT_SKILL_CURSED_TEMPLE_SUBLIMATION;
        fixture.Resolve(succeeded, target, skill);
        CHECK_FALSE(destroyed);
        CHECK_FALSE(g_MessageBox->IsEmpty());
        CHECK(g_CursedTemple->IsGaugebarEnabled());
        g_MessageBox->SendEvent(progress, mu::ui::window::MSGBOX_EVENT_USER_CUSTOM_PROGRESS_COMPLETEPROCESS);
        g_MessageBox->Update();
        CHECK(destroyed);
        fixture.ExpectTalk();
    }
}

TEST_CASE("Shield burn preserves an unrelated progress notice [ui][temple-skill][shield-burn-progress]")
{
    for (const char* theme : {"legacy", "modern"})
    {
        CAPTURE(std::string(theme));
        TempleSkillFixture fixture(theme);
        const mu::ui::window::CMessageBoxFactory::TContainer<QueuedOrdinaryProgress> container;
        auto* other = g_MessageBox->NewMessageBox(container);
        REQUIRE(other != nullptr);
        REQUIRE(other->Create(CaptureMilliseconds));
        const auto close = other->GetCallbackFunc(mu::ui::window::MSGBOX_EVENT_USER_CUSTOM_PROGRESS_CLOSINGPROCESS);
        REQUIRE(close != nullptr);
        g_CursedTemple->SetGaugebarEnabled(true);
        fixture.Resolve(true);
        g_MessageBox->Update();
        CHECK_FALSE(g_MessageBox->IsEmpty());
        CHECK(other->GetCallbackFunc(mu::ui::window::MSGBOX_EVENT_USER_CUSTOM_PROGRESS_CLOSINGPROCESS) == close);
        CHECK(g_CursedTemple->IsGaugebarEnabled());
        CHECK(Hero->Object.CurrentAction == PLAYER_SHOCK);
        CHECK(fixture.peer.Collect().empty());
    }
}

TEST_CASE("Interrupted temple progress closes after an unrelated foreground notice [ui][temple-skill][shield-burn-progress][shield-burn-background]")
{
    for (const char* theme : {"legacy", "modern"})
    {
        CAPTURE(std::string(theme));
        bool destroyed = false;
        TempleSkillFixture fixture(theme);
        const mu::ui::window::CMessageBoxFactory::TContainer<QueuedOrdinaryProgress> container;
        auto* other = g_MessageBox->NewMessageBox(container);
        REQUIRE(other != nullptr);
        REQUIRE(other->Create(CaptureMilliseconds));
        auto* progress = fixture.StartProgress(destroyed);
        fixture.Resolve(true);
        g_MessageBox->Update();
        REQUIRE(other->updates == 1); // The unrelated notice really owned this manager tick.
        CHECK_FALSE(destroyed);
        CHECK_FALSE(g_MessageBox->IsEmpty());
        CHECK(fixture.peer.Collect().empty());

        g_MessageBox->SendEvent(other, mu::ui::window::MSGBOX_EVENT_USER_CUSTOM_PROGRESS_CLOSINGPROCESS);
        g_MessageBox->Update();
        g_MessageBox->Update(); // Temple progress becomes foreground after the ordinary close.
        CHECK(destroyed);
        CHECK(g_MessageBox->IsEmpty());
        CHECK_FALSE(g_CursedTemple->IsGaugebarEnabled());
        CHECK(fixture.peer.Collect().empty());
    }
}
