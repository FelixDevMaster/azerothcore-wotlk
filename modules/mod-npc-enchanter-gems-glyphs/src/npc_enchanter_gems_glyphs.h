/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MODULE_NPC_ENCHANTER_GEMS_GLYPHS_H
#define MODULE_NPC_ENCHANTER_GEMS_GLYPHS_H

#include "Common.h"
#include "SharedDefines.h"
#include <array>
#include <string>
#include <vector>

class Player;
class Item;
class SpellInfo;

enum EnchantSlotCategory : uint8
{
    ENCHANT_CAT_HEAD = 0,
    ENCHANT_CAT_SHOULDERS,
    ENCHANT_CAT_CHEST,
    ENCHANT_CAT_CLOAK,
    ENCHANT_CAT_BRACER,
    ENCHANT_CAT_GLOVES,
    ENCHANT_CAT_BELT,
    ENCHANT_CAT_LEGS,
    ENCHANT_CAT_BOOTS,
    ENCHANT_CAT_RING,
    ENCHANT_CAT_SHIELD,
    ENCHANT_CAT_WEAPON,
    ENCHANT_CAT_TWO_HAND,
    ENCHANT_CAT_OFFHAND,
    ENCHANT_CAT_RANGED,
    ENCHANT_CAT_MAX
};

enum GearShopNpcConst : uint32
{
    NPC_ENCHANTER              = 190034,
    NPC_GEM_VENDOR             = 190035,
    NPC_GLYPH_VENDOR           = 190036,
    NPC_TEXT_ENCHANTER         = 190034,
    NPC_TEXT_GEM_VENDOR        = 190035,
    NPC_TEXT_GLYPH_VENDOR      = 190036,
    GEM_VENDOR_LIST_BASE       = 1901400,
    GLYPH_VENDOR_LIST_BASE     = 1901500
};

struct EnchantOption
{
    uint32 SpellId = 0;
    uint32 EnchantId = 0;
};

class GearShopMgr
{
public:
    static GearShopMgr* instance();

    void LoadConfig(bool reload);
    void LoadEnchants();
    void EnsureEnchantsLoaded();

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] uint32 GetEnchantCost() const { return _enchantCost; }
    [[nodiscard]] uint32 GetEnchanterEntry() const { return _enchanterEntry; }
    [[nodiscard]] uint32 GetGemVendorEntry() const { return _gemVendorEntry; }
    [[nodiscard]] uint32 GetGlyphVendorEntry() const { return _glyphVendorEntry; }

    [[nodiscard]] std::vector<EnchantOption> const& GetEnchants(EnchantSlotCategory category) const;
    [[nodiscard]] bool HasEnchants(EnchantSlotCategory category) const;

    static bool IsSpanish(Player const* player);
    static char const* GetCategoryName(EnchantSlotCategory category, bool spanish);
    static std::vector<uint8> GetEquipSlots(EnchantSlotCategory category);
    static bool ItemMatchesCategory(Item const* item, EnchantSlotCategory category);
    static std::string GetEnchantLabel(EnchantOption const& option, LocaleConstant locale);
    static std::string GetItemName(Item const* item, Player const* player);

private:
    GearShopMgr() = default;

    static void CollectCategories(SpellInfo const* spellInfo, std::vector<EnchantSlotCategory>& cats);
    static void CollectCategoriesFromName(std::string name, std::vector<EnchantSlotCategory>& cats);
    static uint32 GetEnchantingSkillRank(uint32 spellId);
    bool IsEligibleEnchant(SpellInfo const* spellInfo, uint32 requiredLevel, uint32 skillRank) const;

    bool _enabled = true;
    uint32 _enchantMinLevel = 78;
    uint32 _enchantMinSkill = 350;
    uint32 _enchantCost = 0;
    uint32 _enchanterEntry = NPC_ENCHANTER;
    uint32 _gemVendorEntry = NPC_GEM_VENDOR;
    uint32 _glyphVendorEntry = NPC_GLYPH_VENDOR;

    std::array<std::vector<EnchantOption>, ENCHANT_CAT_MAX> _enchants;
};

#define sGearShopMgr GearShopMgr::instance()

#endif
