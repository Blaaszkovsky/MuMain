#include "stdafx.h"

#include "doctest.h"
#include "ItemStatProfilesScope.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/DataHandler/ItemData/ItemJsonStorage.h"
#include "Data/GameData/ItemData/ItemAttributeConversion.h"
#include "Data/GameData/ItemData/ItemDatabase.h"
#include "Data/GameData/ItemData/ItemType.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "GameLogic/Items/EquipmentRestrictions.h"
#include "GameLogic/Items/ItemCategories.h"
#include "GameLogic/Items/ShopRestrictions.h"
#include "GameLogic/Items/ItemStats.h"
#include "GameLogic/Items/TradeRestrictions.h"
#include "World/MapInfra/MapManager.h"

#include <filesystem>
#include <vector>
#include <utility>

extern CHARACTER* Hero;

using namespace Data::Items;
using namespace GameLogic::Items;

// The item rules and categories with the item data shipped in src/bin/Data.
// They catch a changed flag or tag in the data as well as a changed check in
// the rule code.
namespace
{
constexpr int KrisType = MakeItemType(0, 0);

struct ItemState
{
    int level = 0;
    bool rented = false;
    bool expired = false;
    int durability = 0;
};

ITEM MakeItem(int itemType, const ItemState& state = {})
{
    ITEM item{};
    item.Type = static_cast<short>(itemType);
    item.Level = state.level;
    item.bPeriodItem = state.rented;
    item.bExpiredPeriod = state.expired;
    item.Durability = static_cast<BYTE>(state.durability);
    return item;
}

// The item at 0 durability.
ITEM Broken(ITEM item)
{
    item.Durability = 0;
    return item;
}

// Loads the shipped items and gives the rules a Hero (for the GM Gift).
class ShippedItemsScope
{
public:
    ShippedItemsScope()
        : m_previousHero(Hero), m_attributes(MAX_ITEM), m_previousAttributes(ItemAttribute),
          m_previousCharacterAttributes(CharacterAttribute)
    {
        mu::log::Init();
        const ItemDataLoadResult result =
            LoadItemDataDirectory(std::filesystem::path(MU_TEST_DATA_DIR) / "Items");
        REQUIRE_FALSE(HasFatal(result.issues));
        const ItemStatsLoadResult stats =
            LoadItemStatsFile(std::filesystem::path(MU_TEST_DATA_DIR).parent_path() / GetItemStatsFile());
        REQUIRE_FALSE(HasFatal(stats.issues));
        g_ItemDatabase.SetStatProfiles(ItemStatProfiles(stats.file));
        g_ItemDatabase.Build(result.items);
        for (const ItemDefinition& definition : result.items)
        {
            ToItemAttribute(definition, m_attributes[MakeItemType(definition.group, definition.number)]);
        }
        ItemAttribute = m_attributes.data();
        Hero = &m_character;
        CharacterAttribute = &m_characterAttributes;
    }

    ~ShippedItemsScope()
    {
        Hero = m_previousHero;
        ItemAttribute = m_previousAttributes;
        CharacterAttribute = m_previousCharacterAttributes;
    }

    void SetGameMaster(bool isGameMaster)
    {
        Hero->CtlCode = isGameMaster ? CTLCODE_20OPERATOR : 0;
    }

    void SetCharacter(CLASS_TYPE characterClass, WORD level)
    {
        Hero->Class = characterClass;
        CharacterAttribute->Level = level;
    }

private:
    ItemStatProfilesScope m_restoreProfiles;
    CHARACTER m_character{};
    CHARACTER* m_previousHero;
    std::vector<ITEM_ATTRIBUTE> m_attributes;
    ITEM_ATTRIBUTE* m_previousAttributes;
    CHARACTER_ATTRIBUTE m_characterAttributes{};
    CHARACTER_ATTRIBUTE* m_previousCharacterAttributes;
};

bool TradeBan(int itemType, const ItemState& state = {})
{
    const ITEM item = MakeItem(itemType, state);
    return IsTradeBan(&item);
}

bool DropBan(int itemType, const ItemState& state = {})
{
    const ITEM item = MakeItem(itemType, state);
    return IsDropBan(&item);
}

bool StoreBan(int itemType, const ItemState& state = {})
{
    const ITEM item = MakeItem(itemType, state);
    return IsStoreBan(&item);
}

bool PersonalShopBan(int itemType, const ItemState& state = {})
{
    const ITEM item = MakeItem(itemType, state);
    return IsPersonalShopBan(&item);
}

bool SellingBan(int itemType, const ItemState& state = {})
{
    const ITEM item = MakeItem(itemType, state);
    return IsSellingBan(&item);
}

bool RepairBan(int itemType, const ItemState& state = {})
{
    const ITEM item = MakeItem(itemType, state);
    return IsRepairBan(&item);
}

bool Valuable(int itemType, const ItemState& state = {})
{
    const ITEM item = MakeItem(itemType, state);
    return IsHighValueItem(&item);
}
} // namespace

TEST_CASE("A normal weapon allows every action [data][items][rules]")
{
    ShippedItemsScope items;

    CHECK_FALSE(TradeBan(KrisType));
    CHECK_FALSE(DropBan(KrisType));
    CHECK_FALSE(StoreBan(KrisType));
    CHECK_FALSE(PersonalShopBan(KrisType));
    CHECK_FALSE(SellingBan(KrisType));
    CHECK_FALSE(RepairBan(KrisType));
}

TEST_CASE("Items the client does not know allow no action [data][items][rules]")
{
    ShippedItemsScope items;
    const int emptyType = MakeItemType(0, 500);
    REQUIRE(g_ItemDatabase.Find(emptyType) == nullptr);

    CHECK(TradeBan(emptyType));
    CHECK(DropBan(emptyType));
    CHECK(StoreBan(emptyType));
    CHECK(PersonalShopBan(emptyType));
    CHECK(SellingBan(emptyType));
    CHECK(RepairBan(emptyType));
}

TEST_CASE("Item level variants have their own rules [data][items][rules]")
{
    ShippedItemsScope items;

    // Rena +3 is the Sign of Lord; Rena +1 (Stone) cannot be sold.
    CHECK(TradeBan(ITEM_RENA));
    CHECK_FALSE(TradeBan(ITEM_RENA, {3}));
    CHECK_FALSE(StoreBan(ITEM_RENA, {3}));
    CHECK_FALSE(PersonalShopBan(ITEM_RENA, {3}));
    CHECK(SellingBan(ITEM_RENA, {1}));
    CHECK_FALSE(SellingBan(ITEM_RENA));

    // Box of Luck +13 is the Heart of Dark Lord.
    CHECK_FALSE(TradeBan(ITEM_BOX_OF_LUCK));
    CHECK(TradeBan(ITEM_BOX_OF_LUCK, {13}));
    CHECK(StoreBan(ITEM_BOX_OF_LUCK, {13}));
    CHECK(PersonalShopBan(ITEM_BOX_OF_LUCK, {13}));
    CHECK(SellingBan(ITEM_BOX_OF_LUCK));

    // The Wizard's Ring is bound to the character on the server (OpenMU), so it
    // can't be traded, stored or sold at any level; it can still be dropped.
    for (int level : {0, 1, 2})
    {
        CHECK(TradeBan(ITEM_WIZARDS_RING, {level}));
        CHECK(StoreBan(ITEM_WIZARDS_RING, {level}));
        CHECK(PersonalShopBan(ITEM_WIZARDS_RING, {level}));
        CHECK(SellingBan(ITEM_WIZARDS_RING, {level}));
    }
    CHECK_FALSE(DropBan(ITEM_WIZARDS_RING));

    // Remedy of Love +1 to +5 cannot be sold.
    CHECK(SellingBan(ITEM_REMEDY_OF_LOVE, {3}));
    CHECK_FALSE(SellingBan(ITEM_REMEDY_OF_LOVE));
}

TEST_CASE("Rented items have their own rules [data][items][rules]")
{
    ShippedItemsScope items;

    CHECK(StoreBan(KrisType, {0, true}));

    CHECK_FALSE(DropBan(ITEM_CHAOS_CARD));
    CHECK(DropBan(ITEM_CHAOS_CARD, {0, true}));

    CHECK_FALSE(PersonalShopBan(ITEM_DEMON));
    CHECK(PersonalShopBan(ITEM_DEMON, {0, true}));
    CHECK_FALSE(PersonalShopBan(ITEM_SPIRIT_OF_GUARDIAN));
    CHECK(PersonalShopBan(ITEM_SPIRIT_OF_GUARDIAN, {0, true}));

    CHECK(SellingBan(ITEM_DEMON));
    CHECK(SellingBan(ITEM_DEMON, {0, true}));
    CHECK_FALSE(SellingBan(ITEM_DEMON, {0, true, true}));
}

TEST_CASE("Durability and the player change some rules [data][items][rules]")
{
    ShippedItemsScope items;

    CHECK(StoreBan(ITEM_TALISMAN_OF_MOBILITY, {0, false, false, 1}));
    CHECK_FALSE(StoreBan(ITEM_TALISMAN_OF_MOBILITY, {0, false, false, 2}));

    items.SetGameMaster(false);
    CHECK(TradeBan(ITEM_GM_GIFT));
    items.SetGameMaster(true);
    CHECK_FALSE(TradeBan(ITEM_GM_GIFT));
}

TEST_CASE("Potions cannot be repaired [data][items][rules]")
{
    ShippedItemsScope items;

    CHECK(RepairBan(ITEM_APPLE));
    CHECK(RepairBan(ITEM_JEWEL_OF_BLESS));
}

TEST_CASE("Valuable items ask for confirmation [data][items][rules]")
{
    ShippedItemsScope items;

    CHECK(Valuable(ITEM_JEWEL_OF_BLESS));
    CHECK(Valuable(ITEM_WING));
    CHECK_FALSE(Valuable(KrisType));
    CHECK(Valuable(KrisType, {7}));
    CHECK(Valuable(ITEM_WIZARDS_RING));
    CHECK_FALSE(Valuable(ITEM_WIZARDS_RING, {1}));

    // Rented items are not valuable while rented; rented pets only once
    // the rental time ran out.
    CHECK_FALSE(Valuable(ITEM_JEWEL_OF_BLESS, {0, true}));
    CHECK_FALSE(Valuable(ITEM_DEMON));
    CHECK(Valuable(ITEM_DEMON, {0, true, true}));
}

TEST_CASE("Item categories come from the item data [data][items][rules]")
{
    ShippedItemsScope items;
    const ITEM wingsOfElf = MakeItem(ITEM_WING);
    const ITEM wingsOfSpirits = MakeItem(ITEM_WINGS_OF_SPIRITS);
    const ITEM capeOfLord = MakeItem(ITEM_CAPE_OF_LORD);
    const ITEM uniria = MakeItem(ITEM_HORN_OF_UNIRIA);
    const ITEM fenrir = MakeItem(ITEM_HORN_OF_FENRIR);
    const ITEM durableFenrir = MakeItem(ITEM_HORN_OF_FENRIR, {0, false, false, 255});
    const ITEM kris = MakeItem(KrisType);

    CHECK(IsWingItem(&wingsOfElf));
    CHECK(IsWingItem(&capeOfLord));
    CHECK_FALSE(IsWingItem(&kris));
    CHECK(IsSecondTierWingExceptCape(&wingsOfSpirits));
    CHECK_FALSE(IsSecondTierWingExceptCape(&capeOfLord));
    const ITEM wingOfStorm = MakeItem(ITEM_WING_OF_STORM);
    const ITEM capeOfEmperor = MakeItem(ITEM_CAPE_OF_EMPEROR);
    const ITEM smallCapeOfLord = MakeItem(ITEM_SMALL_CAPE_OF_LORD);
    CHECK(IsSecondOrThirdTierWing(&wingsOfSpirits));
    CHECK(IsSecondOrThirdTierWing(&capeOfLord));
    CHECK(IsSecondOrThirdTierWing(&wingOfStorm));
    CHECK_FALSE(IsSecondOrThirdTierWing(&wingsOfElf));
    CHECK_FALSE(IsSecondOrThirdTierWing(&smallCapeOfLord));
    CHECK(IsSecondTierWing(&wingsOfSpirits));
    CHECK(IsSecondTierWing(&capeOfLord));
    CHECK_FALSE(IsSecondTierWing(&wingOfStorm));
    CHECK_FALSE(IsSecondTierWing(&wingsOfElf));
    CHECK_FALSE(IsSecondTierWing(&smallCapeOfLord));
    CHECK(IsSecondTierCape(&capeOfLord));
    CHECK_FALSE(IsSecondTierCape(&wingsOfSpirits));
    CHECK_FALSE(IsSecondTierCape(&capeOfEmperor));
    CHECK_FALSE(IsSecondTierCape(&smallCapeOfLord));

    CHECK(IsPetSlotModel(MODEL_ITEM + ITEM_HELPER));
    CHECK(IsPetSlotModel(MODEL_ITEM + ITEM_HORN_OF_FENRIR));
    CHECK_FALSE(IsPetSlotModel(MODEL_ITEM + KrisType));
    CHECK(IsRideableMount(&uniria));
    CHECK(IsHornMountModel(MODEL_ITEM + ITEM_HORN_OF_UNIRIA));
    CHECK_FALSE(IsHornMountModel(MODEL_ITEM + ITEM_DARK_HORSE_ITEM));
    CHECK_FALSE(IsFlyingMount(&uniria));
    CHECK(IsFlyingMount(&fenrir));
    CHECK(HasFlightEquipment(&durableFenrir, &kris));

    CHECK(IsSocketSeedOrSphereType(ITEM_SEED_FIRE));
    CHECK_FALSE(IsSocketSeedOrSphereType(KrisType));

    // Model ids outside the item range are no items.
    CHECK_FALSE(IsRideableMountModel(-1));
    CHECK_FALSE(IsRideableMountModel(MODEL_ITEM + MAX_ITEM));
    CHECK_FALSE(IsPetSlotModel(-1));
    CHECK_FALSE(IsPetSlotModel(MODEL_ITEM + MAX_ITEM));
}

TEST_CASE("Weapon kinds come from the item data [data][items][rules]")
{
    ShippedItemsScope items;
    // Numbers without item data in the bow and shield groups.
    const int noBow = ITEM_BOW + 27;
    const int noShield = ITEM_SHIELD + 27;
    const ITEM smallShield = MakeItem(ITEM_SHIELD);
    const ITEM shortBow = MakeItem(ITEM_BOW);
    const ITEM crossbow = MakeItem(ITEM_CROSSBOW);
    const ITEM arrows = MakeItem(ITEM_ARROWS);
    const ITEM bolt = MakeItem(ITEM_BOLT);
    const ITEM kris = MakeItem(KrisType);

    // Bows have the slot offHand, crossbows mainHand; arrows and bolts are neither.
    CHECK(IsBowType(ITEM_BOW));
    CHECK(IsBowType(ITEM_CELESTIAL_BOW));
    CHECK_FALSE(IsBowType(noBow));
    CHECK_FALSE(IsBowType(ITEM_CROSSBOW));
    CHECK_FALSE(IsBowType(ITEM_BOLT));
    CHECK(IsCrossbowType(ITEM_CROSSBOW));
    CHECK(IsCrossbowType(ITEM_DIVINE_CB_OF_ARCHANGEL));
    CHECK_FALSE(IsCrossbowType(ITEM_BOW));
    CHECK_FALSE(IsCrossbowType(ITEM_ARROWS));
    CHECK(IsBowModel(MODEL_ITEM + ITEM_BOW));
    CHECK(IsCrossbowModel(MODEL_ITEM + ITEM_CROSSBOW));

    CHECK(IsSpearType(ITEM_SPEAR));
    CHECK(IsSpearModel(MODEL_ITEM + ITEM_SPEAR));
    CHECK_FALSE(IsSpearType(KrisType));

    CHECK(IsShieldType(ITEM_SHIELD));
    CHECK_FALSE(IsShieldType(noShield));
    CHECK_FALSE(IsShieldType(KrisType));

    CHECK(IsBowCrossbowOrAmmunition(&shortBow));
    CHECK(IsBowCrossbowOrAmmunition(&crossbow));
    CHECK(IsBowCrossbowOrAmmunition(&arrows));
    CHECK(IsBowCrossbowOrAmmunition(&bolt));
    CHECK_FALSE(IsBowCrossbowOrAmmunition(&kris));
    CHECK_FALSE(IsBowCrossbowOrAmmunition(&smallShield));

    // Model ids outside the item range are no weapons.
    CHECK_FALSE(IsSpearModel(-1));
    CHECK_FALSE(IsBowModel(MODEL_ITEM + MAX_ITEM));
}

TEST_CASE("Swords, gloves, staffs, books, sceptres and magic swords come from the item data [data][items][rules]")
{
    ShippedItemsScope items;
    // Numbers without item data in the sword, staff and mace groups.
    const int noGlove = MakeItemType(0, 40);
    const int noBook = MakeItemType(5, 40);
    const int noSceptre = MakeItemType(2, 21);
    const ITEM kris = MakeItem(KrisType);
    const ITEM bolt = MakeItem(ITEM_BOLT);
    const ITEM bookOfSahamutt = MakeItem(ITEM_BOOK_OF_SAHAMUTT);
    const ITEM smallShield = MakeItem(ITEM_SHIELD);
    const ITEM apple = MakeItem(ITEM_APPLE);

    CHECK(IsSwordOrGloveModel(MODEL_ITEM + KrisType));
    CHECK(IsSwordOrGloveModel(MODEL_ITEM + ITEM_SACRED_GLOVE));
    CHECK_FALSE(IsSwordOrGloveModel(MODEL_ITEM + ITEM_AXE));

    CHECK(IsSwordAxeMaceOrGloveType(KrisType));
    CHECK(IsSwordAxeMaceOrGloveType(ITEM_AXE));
    CHECK(IsSwordAxeMaceOrGloveType(ITEM_FROST_MACE));
    CHECK_FALSE(IsSwordAxeMaceOrGloveType(noGlove));
    CHECK_FALSE(IsSwordAxeMaceOrGloveType(ITEM_SPEAR));
    CHECK_FALSE(IsSwordAxeMaceOrGloveType(ITEM_STAFF));

    CHECK(IsStaffOrBookType(ITEM_STAFF));
    CHECK(IsStaffOrBookType(ITEM_BOOK_OF_SAHAMUTT));
    CHECK_FALSE(IsStaffOrBookType(noBook));
    CHECK_FALSE(IsStaffOrBookType(KrisType));
    CHECK(IsStaffOrBookModel(MODEL_ITEM + ITEM_STAFF));

    CHECK(IsWeaponOrAmmunition(&kris));
    CHECK(IsWeaponOrAmmunition(&bolt));
    CHECK(IsWeaponOrAmmunition(&bookOfSahamutt));
    CHECK_FALSE(IsWeaponOrAmmunition(&smallShield));
    CHECK_FALSE(IsWeaponOrAmmunition(&apple));

    CHECK(IsSceptreType(ITEM_BATTLE_SCEPTER));
    CHECK_FALSE(IsSceptreType(noSceptre));
    CHECK_FALSE(IsSceptreType(ITEM_FROST_MACE));

    CHECK(IsMagicSwordType(ITEM_DARK_REIGN_BLADE));
    CHECK_FALSE(IsMagicSwordType(ITEM_BONE_BLADE));
}

TEST_CASE("Lucky items, shield potions and transformation rings come from the item data [data][items][rules]")
{
    ShippedItemsScope items;
    const int scaleHelmLucky = MakeItemType(7, 62);
    const int stormJahadBootsLucky = MakeItemType(11, 72);
    const int phoenixSoulHelm = MakeItemType(7, 73);

    CHECK(IsLuckyItemType(scaleHelmLucky));
    CHECK(IsLuckyItemType(stormJahadBootsLucky));
    CHECK_FALSE(IsLuckyItemType(phoenixSoulHelm));
    CHECK_FALSE(IsLuckyItemType(ITEM_HELM));

    CHECK(IsShieldPotionType(ITEM_SMALL_SHIELD_POTION));
    CHECK(IsShieldPotionType(ITEM_LARGE_SHIELD_POTION));
    CHECK_FALSE(IsShieldPotionType(ITEM_ELITE_SD_POTION));
    CHECK_FALSE(IsShieldPotionType(ITEM_SMALL_COMPLEX_POTION));

    CHECK(IsTransformationRingType(ITEM_TRANSFORMATION_RING));
    CHECK(IsTransformationRingType(ITEM_SNOWMAN_TRANSFORMATION_RING));
    CHECK(IsTransformationRingType(ITEM_PANDA_TRANSFORMATION_RING));
    CHECK_FALSE(IsTransformationRingType(ITEM_RING_OF_ICE));
    CHECK(IsPandaOrSkeletonType(ITEM_PANDA_TRANSFORMATION_RING));
    CHECK_FALSE(IsPandaOrSkeletonType(ITEM_TRANSFORMATION_RING));
}

TEST_CASE("The shipped potions are taken largest first [data][items][rules]")
{
    ShippedItemsScope items;
    const auto inUseOrder = [](ItemTag tag)
    {
        const auto itemTypes = g_ItemDatabase.GetItemTypesWithTagInUseOrder(tag);
        return std::vector<int>(itemTypes.begin(), itemTypes.end());
    };

    CHECK(inUseOrder(ItemTag::HealingPotion) == std::vector<int>{ITEM_LARGE_HEALING_POTION, ITEM_MEDIUM_HEALING_POTION,
                                                                 ITEM_SMALL_HEALING_POTION, ITEM_APPLE});
    CHECK(inUseOrder(ItemTag::ManaPotion) ==
          std::vector<int>{ITEM_LARGE_MANA_POTION, ITEM_MEDIUM_MANA_POTION, ITEM_SMALL_MANA_POTION});
    CHECK(inUseOrder(ItemTag::ShieldPotion) ==
          std::vector<int>{ITEM_LARGE_SHIELD_POTION, ITEM_MEDIUM_SHIELD_POTION, ITEM_SMALL_SHIELD_POTION});
    CHECK(inUseOrder(ItemTag::ComplexPotion) ==
          std::vector<int>{ITEM_LARGE_COMPLEX_POTION, ITEM_MEDIUM_COMPLEX_POTION, ITEM_SMALL_COMPLEX_POTION});
}

TEST_CASE("The item types with a tag come lowest first [data][items][rules]")
{
    ShippedItemsScope items;

    const auto manaPotions = g_ItemDatabase.GetItemTypesWithTag(ItemTag::ManaPotion);
    CHECK(std::vector<int>(manaPotions.begin(), manaPotions.end()) ==
          std::vector<int>{ITEM_SMALL_MANA_POTION, ITEM_MEDIUM_MANA_POTION, ITEM_LARGE_MANA_POTION});
    CHECK(g_ItemDatabase.GetItemTypesWithTag(ItemTag::Ammunition).size() == 2);
}

TEST_CASE("Wings of Unity keep level 400 through every upgrade [data][items][rules]")
{
    ShippedItemsScope items;
    constexpr int UnityType = MakeItemType(12, 245);
    constexpr int MaximumUpgradeLevel = 15;
    constexpr int RequiredLevel = 400;
    const ItemDefinition* definition = g_ItemDatabase.Find(UnityType);
    REQUIRE(definition != nullptr);
    CHECK(definition->names.GetNeutral() == "Wings of Unity");
    CHECK(definition->slot == ItemSlot::Wings);
    CHECK(definition->wingTier == WingTier::Third);
    CHECK(definition->width == 4);
    CHECK(definition->height == 3);
    CHECK(definition->defense == 60);
    CHECK(definition->durability == 220);
    for (const BYTE requirement : definition->classRequirements)
    {
        CHECK(requirement == 1);
    }

    for (int level = 0; level <= MaximumUpgradeLevel; ++level)
    {
        INFO("upgrade level " << level);
        ITEM wings = MakeItem(UnityType, {level, false, false, definition->durability});
        SetItemAttributes(&wings);
        CHECK(wings.RequireLevel == RequiredLevel);
        CHECK(IsWingItem(&wings));
        CHECK(IsThirdTierWing(&wings));
        const ITEM noHelper = MakeItem(-1);
        CHECK(HasFlightEquipment(&noHelper, &wings));
    }
}

TEST_CASE("Wings of Unity reject level 399 and allow every class at 400 [data][items][rules]")
{
    ShippedItemsScope items;
    constexpr int UnityType = MakeItemType(12, 245);
    constexpr int MaximumUpgradeLevel = 15;
    const CLASS_TYPE classes[] = {CLASS_WIZARD, CLASS_KNIGHT, CLASS_ELF, CLASS_DARK,
                                  CLASS_DARK_LORD, CLASS_SUMMONER, CLASS_RAGEFIGHTER};
    for (const CLASS_TYPE characterClass : classes)
    {
        for (const int upgrade : {0, MaximumUpgradeLevel})
        {
            INFO("class " << int(characterClass) << " upgrade " << upgrade);
            ITEM wings = MakeItem(UnityType, {upgrade});
            // No harmony service is initialized in these item-rule tests.
            // Keep that unrelated branch inactive at every tested upgrade.
            wings.Jewel_Of_Harmony_OptionLevel = MaximumUpgradeLevel + 1;
            SetItemAttributes(&wings);
            items.SetCharacter(characterClass, 399);
            CHECK_FALSE(IsRequireEquipItem(&wings));
            items.SetCharacter(characterClass, 400);
            CHECK(IsRequireEquipItem(&wings));
        }
    }

    ITEM storm = MakeItem(ITEM_WING_OF_STORM);
    storm.Jewel_Of_Harmony_OptionLevel = MaximumUpgradeLevel + 1;
    SetItemAttributes(&storm);
    items.SetCharacter(CLASS_WIZARD, 400);
    CHECK_FALSE(IsRequireEquipItem(&storm));
    items.SetCharacter(CLASS_KNIGHT, 400);
    CHECK_FALSE(IsRequireEquipItem(&storm));
    items.SetCharacter(CLASS_BLADEMASTER, 400);
    CHECK(IsRequireEquipItem(&storm));
}

TEST_CASE("Original wings keep their upgrade level requirements [data][items][rules]")
{
    ShippedItemsScope items;
    struct RequirementCase
    {
        int itemType;
        int level;
        int requiredLevel;
    };
    const RequirementCase cases[] = {
        {ITEM_WING, 0, 180},
        {ITEM_WING, 15, 240},
        {ITEM_WINGS_OF_SPIRITS, 0, 215},
        {ITEM_WINGS_OF_SPIRITS, 15, 290},
        {ITEM_WING_OF_STORM, 0, 400},
        {ITEM_WING_OF_STORM, 15, 400},
        {ITEM_CAPE_OF_OVERRULE, 0, 400},
        {ITEM_CAPE_OF_OVERRULE, 15, 400},
        {KrisType, 15, 0},
    };
    for (const RequirementCase& entry : cases)
    {
        INFO("type " << entry.itemType << " upgrade " << entry.level);
        ITEM item = MakeItem(entry.itemType, {entry.level});
        SetItemAttributes(&item);
        CHECK(item.RequireLevel == entry.requiredLevel);
    }
}

TEST_CASE("Wings of Unity use third tier luck and excellent options [data][items][rules]")
{
    ShippedItemsScope items;
    constexpr int UnityType = MakeItemType(12, 245);
    constexpr BYTE AllThirdTierExcellentOptions = 0x0F;
    ITEM wings = MakeItem(UnityType);
    wings.HasLuck = true;
    wings.ExcellentFlags = AllThirdTierExcellentOptions;
    SetItemAttributes(&wings);

    REQUIRE(wings.SpecialNum == 5);
    CHECK(wings.Special[0] == AT_ONE_PERCENT_DAMAGE);
    CHECK(wings.Special[1] == AT_DAMAGE_REFLECTION);
    CHECK(wings.Special[2] == AT_RECOVER_FULL_LIFE);
    CHECK(wings.Special[3] == AT_RECOVER_FULL_MANA);
    for (int option = 0; option < 4; ++option)
    {
        CHECK(wings.SpecialValue[option] == 5);
    }
    CHECK(wings.Special[4] == AT_LUCK);
    CHECK(wings.RequireLevel == 400);

    wings.OptionType = 3;
    wings.OptionLevel = 7;
    SetItemAttributes(&wings);
    REQUIRE(wings.SpecialNum == 6);
    CHECK(wings.Special[4] == AT_LUCK);
    CHECK(wings.Special[5] == AT_IMPROVE_DAMAGE);
    CHECK(wings.SpecialValue[5] == 28);
}

TEST_CASE("Custom third tier wing additional options match the server profile [data][items][rules]")
{
    ShippedItemsScope items;
    constexpr int UnityType = MakeItemType(12, 245);
    constexpr BYTE MaximumOptionLevel = 7;
    for (const int itemType : {UnityType, MakeItemType(12, 248), MakeItemType(12, 249)})
    {
        const WORD damageOption = itemType == MakeItemType(12, 249) ? AT_IMPROVE_MAGIC : AT_IMPROVE_DAMAGE;
        const WORD expectedOptions[] = {AT_LIFE_REGENERATION, 0, AT_IMPROVE_DEFENSE, damageOption};
        for (BYTE type = 0; type < std::size(expectedOptions); ++type)
        {
            for (BYTE level = 1; level <= MaximumOptionLevel; ++level)
            {
                INFO("type " << int(type) << " level " << int(level));
                ITEM wings = MakeItem(itemType);
                wings.OptionType = type;
                wings.OptionLevel = level;
                SetItemAttributes(&wings);
                if (type == 1)
                {
                    CHECK(wings.SpecialNum == 0);
                    continue;
                }
                REQUIRE(wings.SpecialNum == 1);
                CHECK(wings.Special[0] == expectedOptions[type]);
                CHECK(wings.SpecialValue[0] == (type == 0 ? level : level * 4));
            }
        }
    }

    // Original third tier wings retain their differing physical/magic profile.
    for (const int itemType : {ITEM_WING_OF_STORM, ITEM_WING_OF_ETERNAL})
    {
        ITEM wings = MakeItem(itemType);
        wings.OptionType = 3;
        wings.OptionLevel = MaximumOptionLevel;
        SetItemAttributes(&wings);
        REQUIRE(wings.SpecialNum == 1);
        CHECK(wings.Special[0] == (itemType == ITEM_WING_OF_STORM ? AT_IMPROVE_DAMAGE : AT_IMPROVE_MAGIC));
        CHECK(wings.SpecialValue[0] == MaximumOptionLevel * 4);
    }
}

TEST_CASE("Wings, the Dinorant, the Dark Horse and the Fenrir are flight equipment [data][items][rules]")
{
    ShippedItemsScope items;
    const ItemState durable{0, false, false, 255};
    const ITEM none = MakeItem(-1);
    const ITEM wings = MakeItem(ITEM_WING, durable);
    const ITEM capeOfLord = MakeItem(ITEM_CAPE_OF_LORD, durable);
    const ITEM dinorant = MakeItem(ITEM_HORN_OF_DINORANT, durable);
    const ITEM fenrir = MakeItem(ITEM_HORN_OF_FENRIR, durable);
    const ITEM darkHorse = MakeItem(ITEM_DARK_HORSE_ITEM, durable);
    const ITEM uniria = MakeItem(ITEM_HORN_OF_UNIRIA, durable);
    const ITEM brokenWings = Broken(wings);
    const ITEM brokenFenrir = Broken(fenrir);

    CHECK(HasFlightEquipment(&none, &wings));
    CHECK(HasFlightEquipment(&none, &capeOfLord));
    CHECK(HasFlightEquipment(&dinorant, &none));
    CHECK(HasFlightEquipment(&fenrir, &none));
    CHECK(HasFlightEquipment(&darkHorse, &none));
    CHECK_FALSE(HasFlightEquipment(&uniria, &none));
    // A broken item (0 durability) gives nothing, so it does not fly.
    CHECK_FALSE(HasFlightEquipment(&none, &brokenWings));
    CHECK_FALSE(HasFlightEquipment(&brokenFenrir, &none));
}

TEST_CASE("In Icarus the last flight equipment cannot be taken off [data][items][rules]")
{
    ShippedItemsScope items;
    const ItemState durable{0, false, false, 255};
    const ITEM none = MakeItem(-1);
    const ITEM wings = MakeItem(ITEM_WING, durable);
    const ITEM fenrir = MakeItem(ITEM_HORN_OF_FENRIR, durable);
    const ITEM darkHorse = MakeItem(ITEM_DARK_HORSE_ITEM, durable);
    const ITEM brokenWings = Broken(wings);
    const ITEM brokenFenrir = Broken(fenrir);

    // Wings and a flying mount: either one can go, the other still flies.
    CHECK(CanTakeOff(EQUIPMENT_WING, WD_10HEAVEN, &fenrir, &wings));
    CHECK(CanTakeOff(EQUIPMENT_HELPER, WD_10HEAVEN, &fenrir, &wings));

    // Only one of them: it stays on.
    CHECK_FALSE(CanTakeOff(EQUIPMENT_WING, WD_10HEAVEN, &none, &wings));
    CHECK_FALSE(CanTakeOff(EQUIPMENT_HELPER, WD_10HEAVEN, &fenrir, &none));

    // The Dark Horse flies: with wings either one can go, alone it stays on.
    CHECK(CanTakeOff(EQUIPMENT_WING, WD_10HEAVEN, &darkHorse, &wings));
    CHECK(CanTakeOff(EQUIPMENT_HELPER, WD_10HEAVEN, &darkHorse, &wings));
    CHECK_FALSE(CanTakeOff(EQUIPMENT_HELPER, WD_10HEAVEN, &darkHorse, &none));

    // Broken Fenrir: the wings are the last flight equipment.
    CHECK_FALSE(CanTakeOff(EQUIPMENT_WING, WD_10HEAVEN, &brokenFenrir, &wings));
    // Nothing flies anyway: taking off broken wings changes nothing.
    CHECK(CanTakeOff(EQUIPMENT_WING, WD_10HEAVEN, &none, &brokenWings));

    // Other slots and other maps are not restricted; only Icarus requires flight.
    CHECK(CanTakeOff(EQUIPMENT_WEAPON_RIGHT, WD_10HEAVEN, &none, &wings));
    CHECK(CanTakeOff(EQUIPMENT_WING, WD_0LORENCIA, &none, &wings));
    CHECK(CanTakeOff(EQUIPMENT_WING, WD_39KANTURU_3RD, &none, &wings));
    CHECK(RequiresFlight(WD_10HEAVEN));
    CHECK_FALSE(RequiresFlight(WD_39KANTURU_3RD));
}

TEST_CASE("Custom class wings retain their stock third tier rules [data][items][rules]")
{
    ShippedItemsScope items;
    const std::pair<int, int> wings[] = {{MakeItemType(12, 248), ITEM_WING_OF_STORM},
                                         {MakeItemType(12, 249), ITEM_WING_OF_ETERNAL}};
    for (const auto& [customType, originalType] : wings)
    {
        const auto* custom = g_ItemDatabase.Find(customType);
        const auto* original = g_ItemDatabase.Find(originalType);
        REQUIRE(custom != nullptr);
        REQUIRE(original != nullptr);
        CHECK(custom->classRequirements == original->classRequirements);
        for (int level = 0; level <= 15; ++level)
        {
            CAPTURE(customType);
            CAPTURE(level);
            const auto customStats = ComputeItemStats(ItemAttribute[customType], {customType, level});
            const auto originalStats = ComputeItemStats(ItemAttribute[originalType], {originalType, level});
            CHECK(customStats.defense == originalStats.defense);
            CHECK(customStats.requireLevel == 400);
            ITEM customWing = MakeItem(customType, {level});
            customWing.HasLuck = true;
            customWing.ExcellentFlags = 0x0F;
            ITEM originalWing = customWing;
            originalWing.Type = static_cast<short>(originalType);
            SetItemAttributes(&customWing);
            SetItemAttributes(&originalWing);
            CHECK(IsThirdTierWing(&customWing));
            REQUIRE(customWing.SpecialNum == originalWing.SpecialNum);
            for (int option = 0; option < customWing.SpecialNum; ++option)
            {
                CHECK(customWing.Special[option] == originalWing.Special[option]);
                CHECK(customWing.SpecialValue[option] == originalWing.SpecialValue[option]);
            }
        }
    }
}

TEST_CASE("All five sphere grades and their crafting results have usable inventory data [data][items][rules]")
{
    ShippedItemsScope items;
    for (int itemType = ITEM_SPHERE_MONO; itemType <= ITEM_SPHERE_5; ++itemType)
    {
        CAPTURE(itemType);
        const ItemDefinition* definition = g_ItemDatabase.Find(itemType);
        REQUIRE(definition != nullptr);
        CHECK(definition->width == 1);
        CHECK(definition->height == 1);
        CHECK(definition->slot == ItemSlot::None);
        const ITEM sphere = MakeItem(itemType);
        CHECK(IsSocketSphere(&sphere));
        CHECK(IsSocketSphereModel(MODEL_ITEM + itemType));
        CHECK(IsSocketSeedOrSphereType(itemType));
        if (itemType >= ITEM_SPHERE_4)
        {
            CHECK(definition->level == 0);
            CHECK(definition->durability == 1);
            CHECK(definition->buyPrice == 0);
            CHECK_FALSE(TradeBan(itemType));
            CHECK_FALSE(StoreBan(itemType));
        }
        constexpr int SeedTypes = 6;
        const int grade = itemType - ITEM_SPHERE_MONO;
        for (int element = 0; element < SeedTypes; ++element)
        {
            const int resultType = ITEM_SEED_SPHERE_FIRE_1 + grade * SeedTypes + element;
            CAPTURE(resultType);
            const ItemDefinition* result = g_ItemDatabase.Find(resultType);
            REQUIRE(result != nullptr);
            CHECK(result->width == 1);
            CHECK(result->height == 1);
            CHECK(result->slot == ItemSlot::None);
            const ITEM seedSphere = MakeItem(resultType);
            CHECK(IsSocketSeedSphere(&seedSphere));
            if (grade >= 3)
                CHECK(result->durability == 1);
        }
    }
}
