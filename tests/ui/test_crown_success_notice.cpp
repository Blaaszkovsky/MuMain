#include "stdafx.h"

#include <doctest.h>

#include "UI/Combat/CrownRegistrationMessageBox.h"

TEST_CASE("Crown access and crown-state success share one live popup [ui][siege][crown]")
{
    mu::ui::window::CMessageBoxFactory factory;
    auto* first = UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory);
    REQUIRE(first != nullptr);
    auto* repeated = UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory);
    CHECK(repeated == nullptr);
}

TEST_CASE("Crown success keeps unrelated popups and can reopen after targeted deletion [ui][siege][crown]")
{
    mu::ui::window::CMessageBoxFactory factory;
    const mu::ui::window::CMessageBoxFactory::TContainer<mu::ui::window::CProgressMsgBox> otherContainer;
    auto* other = factory.NewMessageBox(otherContainer);
    REQUIRE(other != nullptr);
    other->CMessageBoxBase::Create(123, 45);
    auto* success = UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory);
    REQUIRE(success != nullptr);
    CHECK(UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory) == nullptr);
    CHECK(other->GetSize().cx == 123);
    CHECK(other->GetSize().cy == 45);
    factory.DeleteMessageBox(success);
    CHECK(UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory) != nullptr);
    CHECK(other->GetSize().cx == 123);
}

TEST_CASE("Crown success resets with normal factory scene cleanup [ui][siege][crown]")
{
    mu::ui::window::CMessageBoxFactory factory;
    REQUIRE(UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory) != nullptr);
    factory.DeleteAllMessageBoxes();
    REQUIRE(UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory) != nullptr);
    CHECK(UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory) == nullptr);
}

TEST_CASE("Crown success replaces only its previous registration progress [ui][siege][crown]")
{
    mu::ui::window::CMessageBoxFactory factory;
    const mu::ui::window::CMessageBoxFactory::TContainer<mu::ui::window::CProgressMsgBox> otherContainer;
    auto* other = factory.NewMessageBox(otherContainer);
    other->CMessageBoxBase::Create(123, 45);
    REQUIRE(UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory, false) != nullptr);
    REQUIRE(UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory, true) != nullptr);
    CHECK(UI::Siege::CrownRegistrationMessageBox::CreateOnce(factory, true) == nullptr);
    CHECK(other->GetSize().cx == 123);
}
