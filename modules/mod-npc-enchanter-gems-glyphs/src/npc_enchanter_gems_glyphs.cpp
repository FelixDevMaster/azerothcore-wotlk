/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "npc_enchanter_gems_glyphs.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_set>
#include <vector>

namespace
{
constexpr uint8 ENCHANT_PAGE_SIZE = 24;
constexpr uint32 GOSSIP_ENCHANT_HELLO      = 0;
constexpr uint32 GOSSIP_ENCHANT_SLOT_BASE  = 10;
constexpr uint32 GOSSIP_ENCHANT_ITEM_BASE  = 50;
constexpr uint32 GOSSIP_ENCHANT_PAGE_NEXT  = 80;
constexpr uint32 GOSSIP_ENCHANT_PAGE_PREV  = 81;
constexpr uint32 GOSSIP_ENCHANT_BACK       = 82;
constexpr uint32 GOSSIP_ENCHANT_APPLY_BASE = 1000;

uint32 PackEnchantSender(uint8 category, uint8 equipSlot, uint8 page)
{
    return (uint32(category) << 16) | (uint32(equipSlot) << 8) | uint32(page);
}

uint8 UnpackCategory(uint32 sender)
{
    return uint8(sender >> 16);
}

uint8 UnpackEquipSlot(uint32 sender)
{
    return uint8((sender >> 8) & 0xFF);
}

uint8 UnpackPage(uint32 sender)
{
    return uint8(sender & 0xFF);
}

char const* LocalizedName(std::array<char const*, 16> const& names, LocaleConstant locale)
{
    if (locale < TOTAL_LOCALES && names[locale] && names[locale][0])
        return names[locale];
    if (names[DEFAULT_LOCALE] && names[DEFAULT_LOCALE][0])
        return names[DEFAULT_LOCALE];
    for (uint8 i = 0; i < TOTAL_LOCALES; ++i)
        if (names[i] && names[i][0])
            return names[i];
    return "";
}

bool EnchantFitsItem(Item* item, SpellInfo const* spellInfo)
{
    if (!item || !spellInfo || !item->GetTemplate())
        return false;
    if (item->IsFitToSpellRequirements(spellInfo))
        return true;

    // Formula weapon enchants (Berserking, Black Magic, ...) often omit INVTYPE_2HWEAPON
    // in EquippedItemInventoryTypeMask even though they apply to two-handers.
    if (spellInfo->EquippedItemClass != -1 && spellInfo->EquippedItemClass != ITEM_CLASS_WEAPON)
        return false;
    if (item->GetTemplate()->Class != ITEM_CLASS_WEAPON)
        return false;
    int32 subMask = spellInfo->EquippedItemSubClassMask;
    return !subMask || (subMask & (1 << item->GetTemplate()->SubClass));
}

std::vector<EnchantOption> FilterEnchantsForItem(Item* item, EnchantSlotCategory category)
{
    std::vector<EnchantOption> filtered;
    for (EnchantOption const& option : sGearShopMgr->GetEnchants(category))
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(option.SpellId);
        if (EnchantFitsItem(item, spellInfo))
            filtered.push_back(option);
    }
    return filtered;
}

std::vector<Item*> EquippedItemsInCategory(Player* player, EnchantSlotCategory category)
{
    std::vector<Item*> items;
    for (uint8 slot : GearShopMgr::GetEquipSlots(category))
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && GearShopMgr::ItemMatchesCategory(item, category))
            items.push_back(item);
    }
    return items;
}

bool ApplyEnchantToItem(Player* player, Item* item, EnchantOption const& option)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(option.SpellId);
    SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(option.EnchantId);
    if (!spellInfo || !enchant || !EnchantFitsItem(item, spellInfo))
        return false;

    if (enchant->requiredLevel && player->GetLevel() < enchant->requiredLevel)
    {
        ChatHandler(player->GetSession()).PSendSysMessage(
            GearShopMgr::IsSpanish(player)
                ? "Necesitas nivel {} para ese encantamiento."
                : "You must be level {} for that enchantment.",
            enchant->requiredLevel);
        return false;
    }

    uint32 cost = sGearShopMgr->GetEnchantCost();
    if (cost && !player->HasEnoughMoney(cost))
    {
        player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
        return false;
    }

    player->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, false);
    item->SetEnchantment(PERM_ENCHANTMENT_SLOT, option.EnchantId, 0, 0, player->GetGUID());
    player->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, true);

    if (cost)
        player->ModifyMoney(-int32(cost));

    LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
    ChatHandler(player->GetSession()).PSendSysMessage(
        GearShopMgr::IsSpanish(player) ? "Has encantado {} con {}." : "Enchanted {} with {}.",
        GearShopMgr::GetItemName(item, player),
        GearShopMgr::GetEnchantLabel(option, locale));
    return true;
}
}

GearShopMgr* GearShopMgr::instance()
{
    static GearShopMgr instance;
    return &instance;
}

bool GearShopMgr::IsSpanish(Player const* player)
{
    if (!player || !player->GetSession())
        return false;

    LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
    return locale == LOCALE_esES || locale == LOCALE_esMX;
}

char const* GearShopMgr::GetCategoryName(EnchantSlotCategory category, bool spanish)
{
    switch (category)
    {
        case ENCHANT_CAT_HEAD:      return spanish ? "Encantar cabeza" : "Enchant Head";
        case ENCHANT_CAT_SHOULDERS: return spanish ? "Encantar hombros" : "Enchant Shoulders";
        case ENCHANT_CAT_CHEST:     return spanish ? "Encantar pecho" : "Enchant Chest";
        case ENCHANT_CAT_CLOAK:     return spanish ? "Encantar capa" : "Enchant Cloak";
        case ENCHANT_CAT_BRACER:    return spanish ? "Encantar brazales" : "Enchant Bracers";
        case ENCHANT_CAT_GLOVES:    return spanish ? "Encantar guantes" : "Enchant Gloves";
        case ENCHANT_CAT_BELT:      return spanish ? "Encantar cinturon" : "Enchant Belt";
        case ENCHANT_CAT_LEGS:      return spanish ? "Encantar piernas" : "Enchant Legs";
        case ENCHANT_CAT_BOOTS:     return spanish ? "Encantar botas" : "Enchant Boots";
        case ENCHANT_CAT_RING:      return spanish ? "Encantar anillos" : "Enchant Rings";
        case ENCHANT_CAT_SHIELD:    return spanish ? "Encantar escudo" : "Enchant Shield";
        case ENCHANT_CAT_WEAPON:    return spanish ? "Encantar arma" : "Enchant Weapon";
        case ENCHANT_CAT_TWO_HAND:  return spanish ? "Encantar arma de dos manos" : "Enchant Two-Handed Weapon";
        case ENCHANT_CAT_OFFHAND:   return spanish ? "Encantar mano izquierda" : "Enchant Off-hand";
        case ENCHANT_CAT_RANGED:    return spanish ? "Encantar a distancia" : "Enchant Ranged";
        default:                    return spanish ? "Encantar" : "Enchant";
    }
}

std::vector<uint8> GearShopMgr::GetEquipSlots(EnchantSlotCategory category)
{
    switch (category)
    {
        case ENCHANT_CAT_HEAD:      return { EQUIPMENT_SLOT_HEAD };
        case ENCHANT_CAT_SHOULDERS: return { EQUIPMENT_SLOT_SHOULDERS };
        case ENCHANT_CAT_CHEST:     return { EQUIPMENT_SLOT_CHEST };
        case ENCHANT_CAT_CLOAK:     return { EQUIPMENT_SLOT_BACK };
        case ENCHANT_CAT_BRACER:    return { EQUIPMENT_SLOT_WRISTS };
        case ENCHANT_CAT_GLOVES:    return { EQUIPMENT_SLOT_HANDS };
        case ENCHANT_CAT_BELT:      return { EQUIPMENT_SLOT_WAIST };
        case ENCHANT_CAT_LEGS:      return { EQUIPMENT_SLOT_LEGS };
        case ENCHANT_CAT_BOOTS:     return { EQUIPMENT_SLOT_FEET };
        case ENCHANT_CAT_RING:      return { EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2 };
        case ENCHANT_CAT_SHIELD:    return { EQUIPMENT_SLOT_OFFHAND };
        case ENCHANT_CAT_WEAPON:    return { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND };
        case ENCHANT_CAT_TWO_HAND:  return { EQUIPMENT_SLOT_MAINHAND };
        case ENCHANT_CAT_OFFHAND:   return { EQUIPMENT_SLOT_OFFHAND };
        case ENCHANT_CAT_RANGED:    return { EQUIPMENT_SLOT_RANGED };
        default:                    return {};
    }
}

bool GearShopMgr::ItemMatchesCategory(Item const* item, EnchantSlotCategory category)
{
    if (!item || !item->GetTemplate())
        return false;

    uint32 inv = item->GetTemplate()->InventoryType;
    switch (category)
    {
        case ENCHANT_CAT_CHEST:
            return inv == INVTYPE_CHEST || inv == INVTYPE_ROBE;
        case ENCHANT_CAT_SHIELD:
            return inv == INVTYPE_SHIELD;
        case ENCHANT_CAT_WEAPON:
            return inv == INVTYPE_WEAPON || inv == INVTYPE_WEAPONMAINHAND || inv == INVTYPE_WEAPONOFFHAND;
        case ENCHANT_CAT_TWO_HAND:
            return inv == INVTYPE_2HWEAPON;
        case ENCHANT_CAT_OFFHAND:
            return inv == INVTYPE_HOLDABLE;
        case ENCHANT_CAT_RANGED:
            return inv == INVTYPE_RANGED || inv == INVTYPE_RANGEDRIGHT || inv == INVTYPE_THROWN;
        default:
            return true;
    }
}

std::string GearShopMgr::GetEnchantLabel(EnchantOption const& option, LocaleConstant locale)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(option.SpellId);
    SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(option.EnchantId);
    if (!spellInfo)
        return "Unknown";

    std::string name = LocalizedName(spellInfo->SpellName, locale);
    size_t pos = name.rfind(" - ");
    if (pos == std::string::npos)
        pos = name.rfind(": ");
    if (pos != std::string::npos)
        name = name.substr(name[pos] == ':' ? pos + 2 : pos + 3);

    if (!enchant)
        return name;

    char const* description = nullptr;
    if (locale < TOTAL_LOCALES && enchant->description[locale] && enchant->description[locale][0])
        description = enchant->description[locale];
    else if (enchant->description[DEFAULT_LOCALE] && enchant->description[DEFAULT_LOCALE][0])
        description = enchant->description[DEFAULT_LOCALE];

    if (description && description[0])
        return Acore::StringFormat("{} ({})", name, description);
    return name;
}

std::string GearShopMgr::GetItemName(Item const* item, Player const* player)
{
    if (!item || !item->GetTemplate())
        return "";

    ItemTemplate const* proto = item->GetTemplate();
    std::string name = proto->Name1;
    if (player && player->GetSession())
        if (ItemLocale const* il = sObjectMgr->GetItemLocale(proto->ItemId))
            ObjectMgr::GetLocaleString(il->Name, player->GetSession()->GetSessionDbLocaleIndex(), name);
    return name;
}

void GearShopMgr::CollectCategories(SpellInfo const* spellInfo, std::vector<EnchantSlotCategory>& cats)
{
    if (!spellInfo)
        return;

    std::array<bool, ENCHANT_CAT_MAX> used{};
    auto add = [&](EnchantSlotCategory category)
    {
        if (used[category])
            return;
        used[category] = true;
        cats.push_back(category);
    };

    int32 inventoryMask = spellInfo->EquippedItemInventoryTypeMask;
    auto addIf = [&](int32 invType, EnchantSlotCategory category)
    {
        if (inventoryMask & (1 << invType))
            add(category);
    };

    addIf(INVTYPE_HEAD, ENCHANT_CAT_HEAD);
    addIf(INVTYPE_SHOULDERS, ENCHANT_CAT_SHOULDERS);
    addIf(INVTYPE_CHEST, ENCHANT_CAT_CHEST);
    addIf(INVTYPE_ROBE, ENCHANT_CAT_CHEST);
    addIf(INVTYPE_CLOAK, ENCHANT_CAT_CLOAK);
    addIf(INVTYPE_WRISTS, ENCHANT_CAT_BRACER);
    addIf(INVTYPE_HANDS, ENCHANT_CAT_GLOVES);
    addIf(INVTYPE_WAIST, ENCHANT_CAT_BELT);
    addIf(INVTYPE_LEGS, ENCHANT_CAT_LEGS);
    addIf(INVTYPE_FEET, ENCHANT_CAT_BOOTS);
    addIf(INVTYPE_FINGER, ENCHANT_CAT_RING);
    addIf(INVTYPE_SHIELD, ENCHANT_CAT_SHIELD);
    addIf(INVTYPE_HOLDABLE, ENCHANT_CAT_OFFHAND);
    addIf(INVTYPE_RANGED, ENCHANT_CAT_RANGED);
    addIf(INVTYPE_RANGEDRIGHT, ENCHANT_CAT_RANGED);
    addIf(INVTYPE_THROWN, ENCHANT_CAT_RANGED);

    bool const has1H = inventoryMask & ((1 << INVTYPE_WEAPON) | (1 << INVTYPE_WEAPONMAINHAND) |
        (1 << INVTYPE_WEAPONOFFHAND));
    bool const has2H = inventoryMask & (1 << INVTYPE_2HWEAPON);
    if (has1H)
        add(ENCHANT_CAT_WEAPON);
    if (has2H)
        add(ENCHANT_CAT_TWO_HAND);
    // Enchant Weapon - Berserking / Black Magic apply to two-handers too.
    if (has1H && !has2H)
        add(ENCHANT_CAT_TWO_HAND);

    if (!cats.empty())
        return;

    if (spellInfo->SpellName[DEFAULT_LOCALE] && spellInfo->SpellName[DEFAULT_LOCALE][0])
        CollectCategoriesFromName(spellInfo->SpellName[DEFAULT_LOCALE], cats);

    if (!cats.empty())
        return;

    if (spellInfo->EquippedItemClass == ITEM_CLASS_WEAPON)
    {
        add(ENCHANT_CAT_WEAPON);
        add(ENCHANT_CAT_TWO_HAND);
    }
}

void GearShopMgr::CollectCategoriesFromName(std::string name, std::vector<EnchantSlotCategory>& cats)
{
    std::array<bool, ENCHANT_CAT_MAX> used{};
    for (EnchantSlotCategory category : cats)
        used[category] = true;

    auto add = [&](EnchantSlotCategory category)
    {
        if (used[category])
            return;
        used[category] = true;
        cats.push_back(category);
    };

    for (char& ch : name)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));

    auto has = [&](char const* token) { return name.find(token) != std::string::npos; };
    if (has("glove") || has("guante"))
        add(ENCHANT_CAT_GLOVES);
    if (has("chest") || has("pecho") || has("pechera"))
        add(ENCHANT_CAT_CHEST);
    if (has("cloak") || has("capa"))
        add(ENCHANT_CAT_CLOAK);
    if (has("bracer") || has("brazal") || has("wrist") || has("muneca") || has("muñeca"))
        add(ENCHANT_CAT_BRACER);
    if (has("boot") || has("bota") || has("boots") || has("feet"))
        add(ENCHANT_CAT_BOOTS);
    if (has("shield") || has("escudo"))
        add(ENCHANT_CAT_SHIELD);
    if (has("ring") || has("anillo"))
        add(ENCHANT_CAT_RING);
    if (has("off-hand") || has("offhand") || has("mano izquierda"))
        add(ENCHANT_CAT_OFFHAND);

    bool const isTwoHand = has("2h") || has("two-hand") || has("two hand") || has("dos manos")
        || has("staff") || has("baston");
    if (isTwoHand)
        add(ENCHANT_CAT_TWO_HAND);
    else if (has("weapon") || has("arma"))
    {
        add(ENCHANT_CAT_WEAPON);
        add(ENCHANT_CAT_TWO_HAND);
    }
}

uint32 GearShopMgr::GetEnchantingSkillRank(uint32 spellId)
{
    uint32 rank = 0;
    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
    for (SkillLineAbilityMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
        if (itr->second->SkillLine == SKILL_ENCHANTING)
            rank = std::max(rank, itr->second->MinSkillLineRank);
    return rank;
}

bool GearShopMgr::IsEligibleEnchant(SpellInfo const* spellInfo, uint32 requiredLevel, uint32 skillRank) const
{
    if (requiredLevel >= _enchantMinLevel)
        return true;
    if (spellInfo->SpellLevel >= _enchantMinLevel)
        return true;
    if (spellInfo->BaseLevel >= _enchantMinLevel)
        return true;
    return skillRank >= _enchantMinSkill;
}

void GearShopMgr::LoadConfig(bool /*reload*/)
{
    _enabled = sConfigMgr->GetOption<bool>("EnchanterGemsGlyphs.Enable", true);
    _enchantMinLevel = sConfigMgr->GetOption<uint32>("EnchanterGemsGlyphs.EnchantMinLevel", 78);
    _enchantMinSkill = sConfigMgr->GetOption<uint32>("EnchanterGemsGlyphs.EnchantMinSkill", 350);
    _enchantCost = sConfigMgr->GetOption<uint32>("EnchanterGemsGlyphs.EnchantCost", 0);
    _enchanterEntry = sConfigMgr->GetOption<uint32>("EnchanterGemsGlyphs.EnchanterEntry", NPC_ENCHANTER);
    _gemVendorEntry = sConfigMgr->GetOption<uint32>("EnchanterGemsGlyphs.GemVendorEntry", NPC_GEM_VENDOR);
    _glyphVendorEntry = sConfigMgr->GetOption<uint32>("EnchanterGemsGlyphs.GlyphVendorEntry", NPC_GLYPH_VENDOR);
}

std::vector<EnchantOption> const& GearShopMgr::GetEnchants(EnchantSlotCategory category) const
{
    return _enchants[category];
}

bool GearShopMgr::HasEnchants(EnchantSlotCategory category) const
{
    return !_enchants[category].empty();
}

void GearShopMgr::EnsureEnchantsLoaded()
{
    for (uint8 i = 0; i < ENCHANT_CAT_MAX; ++i)
        if (!_enchants[i].empty())
            return;
    LoadEnchants();
}

void GearShopMgr::LoadEnchants()
{
    for (std::vector<EnchantOption>& list : _enchants)
        list.clear();

    std::array<std::unordered_set<uint32>, ENCHANT_CAT_MAX> seen;

    auto addOption = [&](uint32 spellId, EnchantSlotCategory category, uint32 enchantId)
    {
        if (!seen[category].insert(enchantId).second)
            return;
        EnchantOption option;
        option.SpellId = spellId;
        option.EnchantId = enchantId;
        _enchants[category].push_back(option);
    };

    auto addSpell = [&](uint32 spellId, EnchantSlotCategory category)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return;
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (!effect.IsEffect(SPELL_EFFECT_ENCHANT_ITEM) && !effect.IsEffect(SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC))
                continue;
            uint32 enchantId = uint32(effect.MiscValue);
            if (!enchantId || !sSpellItemEnchantmentStore.LookupEntry(enchantId))
                continue;
            addOption(spellId, category, enchantId);
            return;
        }
    };

    // Known WotLK Grand Master / end-game enchant spells. Category is explicit so
    // a missing EquippedItemInventoryTypeMask cannot hide the gossip menus.
    static uint32 const chestSpells[] = { 60692, 47900, 47766, 44588, 44509, 44623, 44492, 27957, 46594 };
    static uint32 const cloakSpells[] = { 47898, 47672, 44591, 60663, 44500, 44582, 47899, 44631, 60609 };
    static uint32 const bracerSpells[] = { 62256, 60767, 44575, 44598, 44593, 44616, 44555, 60616, 44635 };
    static uint32 const gloveSpells[] = { 60668, 44513, 44529, 44488, 44484, 44592, 44625, 44506, 71692 };
    static uint32 const bootSpells[] = { 60763, 47901, 44589, 44528, 44584, 44508, 60623 };
    static uint32 const weaponSpells[] =
    {
        59619, 59621, 59625, 60707, 60714, 44633, 44510, 44629, 44576, 44524,
        44621, 60621, 42974, 46578, 64441, 64568, 27984, 28004, 28003, 27982, 27981
    };
    static uint32 const twoHandSpells[] = { 60691, 44630, 44595, 62948, 62959 };
    static uint32 const shieldSpells[] = { 44489, 60653, 44383, 27945 };
    static uint32 const ringSpells[] = { 44645, 44636, 59636 };

    auto addList = [&](uint32 const* spells, uint32 count, EnchantSlotCategory category)
    {
        for (uint32 i = 0; i < count; ++i)
            addSpell(spells[i], category);
    };
    addList(chestSpells, uint32(sizeof(chestSpells) / sizeof(uint32)), ENCHANT_CAT_CHEST);
    addList(cloakSpells, uint32(sizeof(cloakSpells) / sizeof(uint32)), ENCHANT_CAT_CLOAK);
    addList(bracerSpells, uint32(sizeof(bracerSpells) / sizeof(uint32)), ENCHANT_CAT_BRACER);
    addList(gloveSpells, uint32(sizeof(gloveSpells) / sizeof(uint32)), ENCHANT_CAT_GLOVES);
    addList(bootSpells, uint32(sizeof(bootSpells) / sizeof(uint32)), ENCHANT_CAT_BOOTS);
    addList(weaponSpells, uint32(sizeof(weaponSpells) / sizeof(uint32)), ENCHANT_CAT_WEAPON);
    addList(weaponSpells, uint32(sizeof(weaponSpells) / sizeof(uint32)), ENCHANT_CAT_TWO_HAND);
    addList(twoHandSpells, uint32(sizeof(twoHandSpells) / sizeof(uint32)), ENCHANT_CAT_TWO_HAND);
    addList(shieldSpells, uint32(sizeof(shieldSpells) / sizeof(uint32)), ENCHANT_CAT_SHIELD);
    addList(ringSpells, uint32(sizeof(ringSpells) / sizeof(uint32)), ENCHANT_CAT_RING);

    uint32 storeSize = sSpellMgr->GetSpellInfoStoreSize();
    for (uint32 spellId = 1; spellId < storeSize; ++spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || !spellInfo->IsAbilityOfSkillType(SKILL_ENCHANTING))
            continue;
        if (!spellInfo->HasEffect(SPELL_EFFECT_ENCHANT_ITEM))
            continue;
        if (!spellInfo->SpellName[DEFAULT_LOCALE] || !spellInfo->SpellName[DEFAULT_LOCALE][0])
            continue;

        uint32 skillRank = GetEnchantingSkillRank(spellId);

        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
        {
            if (!effect.IsEffect(SPELL_EFFECT_ENCHANT_ITEM))
                continue;

            uint32 enchantId = uint32(effect.MiscValue);
            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (!enchant || !enchantId)
                continue;
            if (!IsEligibleEnchant(spellInfo, enchant->requiredLevel, skillRank))
                continue;

            std::vector<EnchantSlotCategory> cats;
            CollectCategories(spellInfo, cats);
            for (EnchantSlotCategory category : cats)
                addOption(spellId, category, enchantId);
        }
    }

    // Every Enchanting formula item (Berserking, Black Magic, Crusher, ...).
    if (ItemTemplateContainer const* items = sObjectMgr->GetItemTemplateStore())
    {
        for (auto const& pair : *items)
        {
            ItemTemplate const& proto = pair.second;
            if (proto.Class != ITEM_CLASS_RECIPE || proto.SubClass != ITEM_SUBCLASS_ENCHANTING_FORMULA)
                continue;

            uint32 taughtSpell = 0;
            for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            {
                if (proto.Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_LEARN_SPELL_ID && proto.Spells[i].SpellId > 0)
                {
                    taughtSpell = uint32(proto.Spells[i].SpellId);
                    break;
                }
            }
            if (!taughtSpell)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(taughtSpell);
            if (!spellInfo || !spellInfo->HasEffect(SPELL_EFFECT_ENCHANT_ITEM))
                continue;

            uint32 skillRank = proto.RequiredSkill == SKILL_ENCHANTING
                ? proto.RequiredSkillRank
                : GetEnchantingSkillRank(taughtSpell);

            for (SpellEffectInfo const& effect : spellInfo->GetEffects())
            {
                if (!effect.IsEffect(SPELL_EFFECT_ENCHANT_ITEM))
                    continue;

                uint32 enchantId = uint32(effect.MiscValue);
                SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
                if (!enchant || !enchantId)
                    continue;
                if (!IsEligibleEnchant(spellInfo, enchant->requiredLevel, skillRank))
                    continue;

                std::vector<EnchantSlotCategory> cats;
                CollectCategories(spellInfo, cats);
                CollectCategoriesFromName(proto.Name1, cats);
                for (EnchantSlotCategory category : cats)
                    addOption(taughtSpell, category, enchantId);
            }
        }
    }

    uint32 total = 0;
    for (uint8 i = 0; i < ENCHANT_CAT_MAX; ++i)
    {
        std::sort(_enchants[i].begin(), _enchants[i].end(),
            [](EnchantOption const& left, EnchantOption const& right)
            {
                return GetEnchantLabel(left, LOCALE_enUS) < GetEnchantLabel(right, LOCALE_enUS);
            });
        total += uint32(_enchants[i].size());
    }

    LOG_INFO("server.loading", ">> Loaded {} enchants (level {}+ / skill {}+) for the enchanter NPC.",
        total, _enchantMinLevel, _enchantMinSkill);
}

class GearShopWorldScript : public WorldScript
{
public:
    GearShopWorldScript() : WorldScript("GearShopWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        sGearShopMgr->LoadConfig(reload);
        if (reload)
            sGearShopMgr->LoadEnchants();
    }

    void OnBeforeWorldInitialized() override
    {
        sGearShopMgr->LoadEnchants();
    }

    void OnStartup() override
    {
        sGearShopMgr->EnsureEnchantsLoaded();
    }
};

class npc_gear_enchanter : public CreatureScript
{
public:
    npc_gear_enchanter() : CreatureScript("npc_gear_enchanter") { }

    static uint32 GossipTextId(Player* player, Creature* creature)
    {
        uint32 textId = player->GetGossipTextId(creature);
        if (textId && textId != DEFAULT_GOSSIP_MESSAGE)
            return textId;
        return DEFAULT_GOSSIP_MESSAGE;
    }

    static void SendHello(Player* player, Creature* creature)
    {
        sGearShopMgr->EnsureEnchantsLoaded();
        ClearGossipMenuFor(player);
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(creature->GetGossipMenuId());
        bool spanish = GearShopMgr::IsSpanish(player);

        // Always list the Enchanting slots. Hiding empty categories used to send a
        // gossip packet with 0 options, which the 3.3.5 client treats as no menu.
        static EnchantSlotCategory const helloSlots[] =
        {
            ENCHANT_CAT_CHEST, ENCHANT_CAT_CLOAK, ENCHANT_CAT_BRACER, ENCHANT_CAT_GLOVES,
            ENCHANT_CAT_BOOTS, ENCHANT_CAT_WEAPON, ENCHANT_CAT_TWO_HAND, ENCHANT_CAT_SHIELD,
            ENCHANT_CAT_RING
        };
        std::array<bool, ENCHANT_CAT_MAX> listed{};
        for (EnchantSlotCategory category : helloSlots)
        {
            listed[category] = true;
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, GearShopMgr::GetCategoryName(category, spanish),
                GOSSIP_SENDER_MAIN, GOSSIP_ENCHANT_SLOT_BASE + uint32(category));
        }
        for (uint8 i = 0; i < ENCHANT_CAT_MAX; ++i)
        {
            if (listed[i] || !sGearShopMgr->HasEnchants(EnchantSlotCategory(i)))
                continue;
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, GearShopMgr::GetCategoryName(EnchantSlotCategory(i), spanish),
                GOSSIP_SENDER_MAIN, GOSSIP_ENCHANT_SLOT_BASE + i);
        }

        SendGossipMenuFor(player, GossipTextId(player, creature), creature);
    }

    static void SendItemPicker(Player* player, Creature* creature, EnchantSlotCategory category)
    {
        std::vector<Item*> items = EquippedItemsInCategory(player, category);
        bool spanish = GearShopMgr::IsSpanish(player);
        ClearGossipMenuFor(player);
        for (Item* item : items)
        {
            uint8 slot = item->GetSlot();
            std::string label = GearShopMgr::GetItemName(item, player);
            if (category == ENCHANT_CAT_RING)
                label = Acore::StringFormat("{} - {}",
                    slot == EQUIPMENT_SLOT_FINGER1 ? (spanish ? "Anillo 1" : "Ring 1")
                                                   : (spanish ? "Anillo 2" : "Ring 2"),
                    label);
            else if (category == ENCHANT_CAT_WEAPON)
                label = Acore::StringFormat("{} - {}",
                    slot == EQUIPMENT_SLOT_MAINHAND ? (spanish ? "Mano derecha" : "Main hand")
                                                    : (spanish ? "Mano izquierda" : "Off hand"),
                    label);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, label,
                uint32(category), GOSSIP_ENCHANT_ITEM_BASE + slot);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, spanish ? "Atras" : "Back",
            GOSSIP_SENDER_MAIN, GOSSIP_ENCHANT_BACK);
        SendGossipMenuFor(player, GossipTextId(player, creature), creature);
    }

    static void SendEnchantList(Player* player, Creature* creature, EnchantSlotCategory category,
        uint8 equipSlot, uint8 page)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot);
        bool spanish = GearShopMgr::IsSpanish(player);
        if (!item || !GearShopMgr::ItemMatchesCategory(item, category))
        {
            ChatHandler(player->GetSession()).SendSysMessage(spanish
                ? "Equipa un objeto en esa ranura primero."
                : "Equip an item in that slot first.");
            SendHello(player, creature);
            return;
        }

        std::vector<EnchantOption> options = FilterEnchantsForItem(item, category);
        ClearGossipMenuFor(player);

        if (options.empty())
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                spanish ? "No hay encantamientos para este objeto." : "No enchants available for this item.",
                GOSSIP_SENDER_MAIN, GOSSIP_ENCHANT_BACK);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, spanish ? "Atras" : "Back",
                GOSSIP_SENDER_MAIN, GOSSIP_ENCHANT_BACK);
            SendGossipMenuFor(player, GossipTextId(player, creature), creature);
            return;
        }

        uint32 sender = PackEnchantSender(uint8(category), equipSlot, page);
        uint32 start = uint32(page) * ENCHANT_PAGE_SIZE;
        uint32 end = std::min<uint32>(start + ENCHANT_PAGE_SIZE, uint32(options.size()));
        LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
        std::string confirm = spanish ? "¿Encantar el objeto equipado?" : "Enchant the equipped item?";
        uint32 cost = sGearShopMgr->GetEnchantCost();

        for (uint32 i = start; i < end; ++i)
        {
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, GearShopMgr::GetEnchantLabel(options[i], locale),
                sender, GOSSIP_ENCHANT_APPLY_BASE + (i - start), confirm, cost, false);
        }

        if (page > 0)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, spanish ? "Pagina anterior" : "Previous page",
                sender, GOSSIP_ENCHANT_PAGE_PREV);
        if (end < options.size())
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, spanish ? "Pagina siguiente" : "Next page",
                sender, GOSSIP_ENCHANT_PAGE_NEXT);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, spanish ? "Atras" : "Back",
            GOSSIP_SENDER_MAIN, GOSSIP_ENCHANT_BACK);
        SendGossipMenuFor(player, GossipTextId(player, creature), creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!sGearShopMgr->IsEnabled())
        {
            ClearGossipMenuFor(player);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                GearShopMgr::IsSpanish(player) ? "El encantador esta desactivado." : "The enchanter is disabled.",
                GOSSIP_SENDER_MAIN, GOSSIP_ENCHANT_HELLO);
            SendGossipMenuFor(player, GossipTextId(player, creature), creature);
            return true;
        }

        SendHello(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (!sGearShopMgr->IsEnabled())
            return false;

        if (action == GOSSIP_ENCHANT_BACK || action == GOSSIP_ENCHANT_HELLO)
        {
            SendHello(player, creature);
            return true;
        }

        if (action >= GOSSIP_ENCHANT_SLOT_BASE && action < GOSSIP_ENCHANT_SLOT_BASE + uint32(ENCHANT_CAT_MAX))
        {
            EnchantSlotCategory category = EnchantSlotCategory(action - GOSSIP_ENCHANT_SLOT_BASE);
            std::vector<Item*> items = EquippedItemsInCategory(player, category);
            if (items.empty())
            {
                ChatHandler(player->GetSession()).SendSysMessage(GearShopMgr::IsSpanish(player)
                    ? "Equipa un objeto en esa ranura primero."
                    : "Equip an item in that slot first.");
                SendHello(player, creature);
                return true;
            }
            if (items.size() == 1)
            {
                SendEnchantList(player, creature, category, items.front()->GetSlot(), 0);
                return true;
            }
            SendItemPicker(player, creature, category);
            return true;
        }

        if (action >= GOSSIP_ENCHANT_ITEM_BASE && action < GOSSIP_ENCHANT_ITEM_BASE + uint32(EQUIPMENT_SLOT_END))
        {
            SendEnchantList(player, creature, EnchantSlotCategory(sender),
                uint8(action - GOSSIP_ENCHANT_ITEM_BASE), 0);
            return true;
        }

        if (action == GOSSIP_ENCHANT_PAGE_NEXT || action == GOSSIP_ENCHANT_PAGE_PREV)
        {
            uint8 page = UnpackPage(sender);
            if (action == GOSSIP_ENCHANT_PAGE_NEXT)
                ++page;
            else if (page > 0)
                --page;
            SendEnchantList(player, creature, EnchantSlotCategory(UnpackCategory(sender)),
                UnpackEquipSlot(sender), page);
            return true;
        }

        if (action >= GOSSIP_ENCHANT_APPLY_BASE)
        {
            EnchantSlotCategory category = EnchantSlotCategory(UnpackCategory(sender));
            uint8 equipSlot = UnpackEquipSlot(sender);
            uint8 page = UnpackPage(sender);
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot);
            if (!item)
            {
                SendHello(player, creature);
                return true;
            }

            std::vector<EnchantOption> options = FilterEnchantsForItem(item, category);
            uint32 index = uint32(page) * ENCHANT_PAGE_SIZE + (action - GOSSIP_ENCHANT_APPLY_BASE);
            if (index >= options.size())
            {
                SendEnchantList(player, creature, category, equipSlot, page);
                return true;
            }

            ApplyEnchantToItem(player, item, options[index]);
            CloseGossipMenuFor(player);
            return true;
        }

        SendHello(player, creature);
        return true;
    }
};

class npc_gear_gem_vendor : public CreatureScript
{
public:
    npc_gear_gem_vendor() : CreatureScript("npc_gear_gem_vendor") { }

    static char const* GemColorName(uint32 subclass, bool spanish)
    {
        switch (subclass)
        {
            case ITEM_SUBCLASS_GEM_RED:       return spanish ? "Gemas rojas" : "Red gems";
            case ITEM_SUBCLASS_GEM_BLUE:      return spanish ? "Gemas azules" : "Blue gems";
            case ITEM_SUBCLASS_GEM_YELLOW:    return spanish ? "Gemas amarillas" : "Yellow gems";
            case ITEM_SUBCLASS_GEM_PURPLE:    return spanish ? "Gemas moradas" : "Purple gems";
            case ITEM_SUBCLASS_GEM_GREEN:     return spanish ? "Gemas verdes" : "Green gems";
            case ITEM_SUBCLASS_GEM_ORANGE:    return spanish ? "Gemas naranjas" : "Orange gems";
            case ITEM_SUBCLASS_GEM_META:      return spanish ? "Gemas meta" : "Meta gems";
            case ITEM_SUBCLASS_GEM_PRISMATIC: return spanish ? "Gemas prismaticas" : "Prismatic gems";
            default:                          return spanish ? "Gemas" : "Gems";
        }
    }

    static void SendHello(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        bool spanish = GearShopMgr::IsSpanish(player);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, spanish ? "Todas las gemas" : "All gems",
            GOSSIP_SENDER_MAIN, 20);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, GemColorName(ITEM_SUBCLASS_GEM_RED, spanish),
            GOSSIP_SENDER_MAIN, ITEM_SUBCLASS_GEM_RED);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, GemColorName(ITEM_SUBCLASS_GEM_YELLOW, spanish),
            GOSSIP_SENDER_MAIN, ITEM_SUBCLASS_GEM_YELLOW);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, GemColorName(ITEM_SUBCLASS_GEM_BLUE, spanish),
            GOSSIP_SENDER_MAIN, ITEM_SUBCLASS_GEM_BLUE);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, GemColorName(ITEM_SUBCLASS_GEM_ORANGE, spanish),
            GOSSIP_SENDER_MAIN, ITEM_SUBCLASS_GEM_ORANGE);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, GemColorName(ITEM_SUBCLASS_GEM_PURPLE, spanish),
            GOSSIP_SENDER_MAIN, ITEM_SUBCLASS_GEM_PURPLE);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, GemColorName(ITEM_SUBCLASS_GEM_GREEN, spanish),
            GOSSIP_SENDER_MAIN, ITEM_SUBCLASS_GEM_GREEN);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, GemColorName(ITEM_SUBCLASS_GEM_META, spanish),
            GOSSIP_SENDER_MAIN, ITEM_SUBCLASS_GEM_META);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, GemColorName(ITEM_SUBCLASS_GEM_PRISMATIC, spanish),
            GOSSIP_SENDER_MAIN, ITEM_SUBCLASS_GEM_PRISMATIC);
        SendGossipMenuFor(player, NPC_TEXT_GEM_VENDOR, creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!sGearShopMgr->IsEnabled())
            return false;

        SendHello(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!sGearShopMgr->IsEnabled())
            return false;

        uint32 vendorEntry = 0;
        if (action == 20)
            vendorEntry = sGearShopMgr->GetGemVendorEntry();
        else if (action <= ITEM_SUBCLASS_GEM_PRISMATIC)
            vendorEntry = GEM_VENDOR_LIST_BASE + action;

        if (!vendorEntry)
        {
            SendHello(player, creature);
            return true;
        }

        player->GetSession()->SendListInventory(creature->GetGUID(), vendorEntry);
        return true;
    }
};

class npc_gear_glyph_vendor : public CreatureScript
{
public:
    npc_gear_glyph_vendor() : CreatureScript("npc_gear_glyph_vendor") { }

    static char const* ClassName(uint8 classId, bool spanish)
    {
        switch (classId)
        {
            case CLASS_WARRIOR:      return spanish ? "Guerrero" : "Warrior";
            case CLASS_PALADIN:      return spanish ? "Paladin" : "Paladin";
            case CLASS_HUNTER:       return spanish ? "Cazador" : "Hunter";
            case CLASS_ROGUE:        return spanish ? "Picaro" : "Rogue";
            case CLASS_PRIEST:       return spanish ? "Sacerdote" : "Priest";
            case CLASS_DEATH_KNIGHT: return spanish ? "Caballero de la Muerte" : "Death Knight";
            case CLASS_SHAMAN:       return spanish ? "Chaman" : "Shaman";
            case CLASS_MAGE:         return spanish ? "Mago" : "Mage";
            case CLASS_WARLOCK:      return spanish ? "Brujo" : "Warlock";
            case CLASS_DRUID:        return spanish ? "Druida" : "Druid";
            default:                 return spanish ? "Clase" : "Class";
        }
    }

    static void SendHello(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        bool spanish = GearShopMgr::IsSpanish(player);
        uint8 playerClass = player->getClass();
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR,
            Acore::StringFormat(spanish ? "Glifos de tu clase ({})" : "Glyphs for your class ({})",
                ClassName(playerClass, spanish)),
            GOSSIP_SENDER_MAIN, playerClass);

        static uint8 const classes[] =
        {
            CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE, CLASS_PRIEST,
            CLASS_DEATH_KNIGHT, CLASS_SHAMAN, CLASS_MAGE, CLASS_WARLOCK, CLASS_DRUID
        };
        for (uint8 classId : classes)
        {
            if (classId == playerClass)
                continue;
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR,
                Acore::StringFormat(spanish ? "Glifos de {}" : "{} glyphs", ClassName(classId, spanish)),
                GOSSIP_SENDER_MAIN, classId);
        }
        SendGossipMenuFor(player, NPC_TEXT_GLYPH_VENDOR, creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!sGearShopMgr->IsEnabled())
            return false;

        SendHello(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!sGearShopMgr->IsEnabled())
            return false;

        switch (action)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_HUNTER:
            case CLASS_ROGUE:
            case CLASS_PRIEST:
            case CLASS_DEATH_KNIGHT:
            case CLASS_SHAMAN:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
            case CLASS_DRUID:
                player->GetSession()->SendListInventory(creature->GetGUID(), GLYPH_VENDOR_LIST_BASE + action);
                return true;
            default:
                SendHello(player, creature);
                return true;
        }
    }
};

void AddSC_npc_enchanter_gems_glyphs()
{
    new GearShopWorldScript();
    new npc_gear_enchanter();
    new npc_gear_gem_vendor();
    new npc_gear_glyph_vendor();
}
