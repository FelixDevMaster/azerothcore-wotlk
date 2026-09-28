/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MythicPlusMgr.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include <unordered_map>

namespace
{
bool HasItemStat(ItemTemplate const* proto, uint32 stat)
{
    if (!proto)
        return false;
    for (uint32 i = 0; i < proto->StatsCount; ++i)
        if (proto->ItemStat[i].ItemStatType == stat)
            return true;
    return false;
}

void ItemLevelBand(uint8 keyLevel, uint32& minIlvl, uint32& maxIlvl)
{
    if (keyLevel >= 14)
    {
        minIlvl = 264;
        maxIlvl = 284;
    }
    else if (keyLevel >= 11)
    {
        minIlvl = 251;
        maxIlvl = 277;
    }
    else if (keyLevel >= 8)
    {
        minIlvl = 232;
        maxIlvl = 258;
    }
    else if (keyLevel >= 5)
    {
        minIlvl = 219;
        maxIlvl = 245;
    }
    else
    {
        minIlvl = 187;
        maxIlvl = 226;
    }
}

uint8 ArmorSubclassForClass(uint8 playerClass)
{
    switch (playerClass)
    {
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
        case CLASS_ROGUE:
        case CLASS_DRUID:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return ITEM_SUBCLASS_ARMOR_MAIL;
        default:
            return ITEM_SUBCLASS_ARMOR_PLATE;
    }
}

int32 ScoreItemForRole(ItemTemplate const* proto, MythicLootRole role)
{
    bool const hasStr = HasItemStat(proto, ITEM_MOD_STRENGTH);
    bool const hasAgi = HasItemStat(proto, ITEM_MOD_AGILITY);
    bool const hasInt = HasItemStat(proto, ITEM_MOD_INTELLECT);
    bool const hasSpi = HasItemStat(proto, ITEM_MOD_SPIRIT);
    bool const hasDef = HasItemStat(proto, ITEM_MOD_DEFENSE_SKILL_RATING)
        || HasItemStat(proto, ITEM_MOD_DODGE_RATING)
        || HasItemStat(proto, ITEM_MOD_PARRY_RATING)
        || HasItemStat(proto, ITEM_MOD_BLOCK_RATING)
        || HasItemStat(proto, ITEM_MOD_BLOCK_VALUE);
    bool const hasSp = HasItemStat(proto, ITEM_MOD_SPELL_POWER)
        || HasItemStat(proto, ITEM_MOD_SPELL_HEALING_DONE)
        || HasItemStat(proto, ITEM_MOD_MANA_REGENERATION);
    bool const hasAp = HasItemStat(proto, ITEM_MOD_ATTACK_POWER)
        || HasItemStat(proto, ITEM_MOD_RANGED_ATTACK_POWER)
        || HasItemStat(proto, ITEM_MOD_EXPERTISE_RATING)
        || HasItemStat(proto, ITEM_MOD_ARMOR_PENETRATION_RATING);
    bool const hasResi = HasItemStat(proto, ITEM_MOD_RESILIENCE_RATING);

    if (hasResi && !hasStr && !hasAgi && !hasInt && !hasSp && !hasAp && !hasDef)
        return -8;

    int32 score = 0;
    switch (role)
    {
        case MYTHIC_LOOT_TANK:
            if (hasStr || hasAgi)
                score += 3;
            if (hasDef)
                score += 4;
            if (hasInt && !hasDef && !hasStr)
                score -= 4;
            if (hasSp && !hasDef)
                score -= 3;
            break;
        case MYTHIC_LOOT_HEAL:
            if (hasInt || hasSpi || hasSp)
                score += 4;
            if (hasStr || hasAgi || hasAp || hasDef)
                score -= 5;
            break;
        case MYTHIC_LOOT_CASTER:
            if (hasInt || hasSp)
                score += 4;
            if (hasStr || hasAgi || hasAp || hasDef)
                score -= 5;
            break;
        case MYTHIC_LOOT_RANGED:
            if (hasAgi || hasAp)
                score += 4;
            if (hasStr || hasInt || hasSp || hasSpi || hasDef)
                score -= 4;
            break;
        case MYTHIC_LOOT_MELEE:
        default:
            if (hasStr || hasAgi || hasAp)
                score += 4;
            if (hasInt || hasSp || hasSpi)
                score -= 5;
            if (hasDef)
                score -= 2;
            break;
    }
    return score;
}

bool IsJewelryOrCloak(uint32 invType)
{
    return invType == INVTYPE_NECK || invType == INVTYPE_FINGER || invType == INVTYPE_TRINKET
        || invType == INVTYPE_CLOAK;
}
}

MythicLootRole MythicPlusMgr::LootRoleFor(Player const* player)
{
    if (!player)
        return MYTHIC_LOOT_MELEE;
    if (player->HasHealSpec())
        return MYTHIC_LOOT_HEAL;
    if (player->HasTankSpec())
        return MYTHIC_LOOT_TANK;

    switch (player->getClass())
    {
        case CLASS_PRIEST:
            return player->GetSpec() == TALENT_TREE_PRIEST_SHADOW ? MYTHIC_LOOT_CASTER : MYTHIC_LOOT_HEAL;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            return MYTHIC_LOOT_CASTER;
        case CLASS_HUNTER:
            return MYTHIC_LOOT_RANGED;
        case CLASS_DRUID:
            if (player->GetSpec() == TALENT_TREE_DRUID_BALANCE)
                return MYTHIC_LOOT_CASTER;
            return MYTHIC_LOOT_MELEE;
        case CLASS_SHAMAN:
            if (player->GetSpec() == TALENT_TREE_SHAMAN_ELEMENTAL)
                return MYTHIC_LOOT_CASTER;
            return MYTHIC_LOOT_MELEE;
        case CLASS_PALADIN:
            if (player->GetSpec() == TALENT_TREE_PALADIN_HOLY)
                return MYTHIC_LOOT_HEAL;
            return MYTHIC_LOOT_MELEE;
        default:
            return MYTHIC_LOOT_MELEE;
    }
}

bool MythicPlusMgr::ItemFitsSpec(ItemTemplate const* proto, Player const* player, uint8 keyLevel, bool relaxStats) const
{
    if (!proto || !player)
        return false;
    if (proto->HasFlag(ITEM_FLAG_DEPRECATED))
        return false;
    if (proto->Quality < ITEM_QUALITY_RARE || proto->Quality > ITEM_QUALITY_EPIC)
        return false;
    if (proto->RequiredLevel > player->GetLevel())
        return false;
    if (proto->Bonding == BIND_QUEST_ITEM || proto->StartQuest || proto->Duration || proto->RequiredSkill)
        return false;
    if (proto->HasFlag2(ITEM_FLAG2_FACTION_HORDE) && player->GetTeamId() != TEAM_HORDE)
        return false;
    if (proto->HasFlag2(ITEM_FLAG2_FACTION_ALLIANCE) && player->GetTeamId() != TEAM_ALLIANCE)
        return false;

    uint32 const classMask = 1u << (player->getClass() - 1);
    if (proto->AllowableClass != uint32(-1) && !(proto->AllowableClass & classMask))
        return false;
    uint32 const raceMask = 1u << (player->getRace() - 1);
    if (proto->AllowableRace != uint32(-1) && !(proto->AllowableRace & raceMask))
        return false;

    uint32 minIlvl = 0;
    uint32 maxIlvl = 0;
    ItemLevelBand(keyLevel, minIlvl, maxIlvl);
    if (relaxStats)
    {
        if (minIlvl > 20)
            minIlvl -= 20;
        maxIlvl += 15;
    }
    if (proto->ItemLevel < minIlvl || proto->ItemLevel > maxIlvl)
        return false;

    uint32 const inv = proto->InventoryType;
    if (inv == INVTYPE_NON_EQUIP || inv == INVTYPE_BODY || inv == INVTYPE_BAG || inv == INVTYPE_TABARD
        || inv == INVTYPE_AMMO || inv == INVTYPE_QUIVER)
        return false;

    uint8 const playerClass = player->getClass();
    MythicLootRole const role = LootRoleFor(player);

    if (proto->Class == ITEM_CLASS_ARMOR)
    {
        if (proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD || proto->SubClass == ITEM_SUBCLASS_ARMOR_BUCKLER)
        {
            if (role != MYTHIC_LOOT_TANK && !(role == MYTHIC_LOOT_HEAL
                && (playerClass == CLASS_PALADIN || playerClass == CLASS_SHAMAN)))
                return false;
        }
        else if (proto->SubClass == ITEM_SUBCLASS_ARMOR_LIBRAM)
        {
            if (playerClass != CLASS_PALADIN)
                return false;
        }
        else if (proto->SubClass == ITEM_SUBCLASS_ARMOR_IDOL)
        {
            if (playerClass != CLASS_DRUID)
                return false;
        }
        else if (proto->SubClass == ITEM_SUBCLASS_ARMOR_TOTEM)
        {
            if (playerClass != CLASS_SHAMAN)
                return false;
        }
        else if (proto->SubClass == ITEM_SUBCLASS_ARMOR_SIGIL)
        {
            if (playerClass != CLASS_DEATH_KNIGHT)
                return false;
        }
        else if (!IsJewelryOrCloak(inv) && proto->SubClass != ITEM_SUBCLASS_ARMOR_MISC)
        {
            if (proto->SubClass != ArmorSubclassForClass(playerClass))
                return false;
        }
    }
    else if (proto->Class == ITEM_CLASS_WEAPON)
    {
        if (inv == INVTYPE_HOLDABLE && role != MYTHIC_LOOT_HEAL && role != MYTHIC_LOOT_CASTER)
            return false;
        if (inv == INVTYPE_RANGED || inv == INVTYPE_RANGEDRIGHT || inv == INVTYPE_THROWN)
        {
            if (role != MYTHIC_LOOT_RANGED && playerClass != CLASS_ROGUE && playerClass != CLASS_WARRIOR)
                if (proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND
                    || (role != MYTHIC_LOOT_CASTER && role != MYTHIC_LOOT_HEAL))
                    return false;
        }
    }
    else
        return false;

    if (relaxStats)
        return true;
    if (!proto->StatsCount)
        return proto->Class == ITEM_CLASS_WEAPON;
    return ScoreItemForRole(proto, role) >= 0;
}

void MythicPlusMgr::BuildSpecLootPool(Player const* player, uint8 keyLevel, bool relaxStats,
    std::vector<uint32>& out) const
{
    out.clear();
    if (!player)
        return;

    ItemTemplateContainer const* store = sObjectMgr->GetItemTemplateStore();
    if (!store)
        return;

    out.reserve(128);
    for (auto const& pair : *store)
    {
        ItemTemplate const* proto = &pair.second;
        if (ItemFitsSpec(proto, player, keyLevel, relaxStats))
            out.push_back(proto->ItemId);
    }
}

uint32 MythicPlusMgr::PickSpecItem(Player* player, uint8 keyLevel, std::unordered_set<uint32> const& exclude)
{
    if (!player)
        return 0;

    std::vector<uint32> pool;
    BuildSpecLootPool(player, keyLevel, false, pool);
    if (pool.size() < 8)
        BuildSpecLootPool(player, keyLevel, true, pool);

    std::vector<uint32> filtered;
    filtered.reserve(pool.size());
    for (uint32 id : pool)
        if (!exclude.count(id))
            filtered.push_back(id);
    if (filtered.empty())
        return 0;
    return filtered[urand(0, filtered.size() - 1)];
}

std::string MythicPlusMgr::LocalizedItemName(Player const* player, uint32 itemId)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return Acore::StringFormat("#{}", itemId);

    std::string name = proto->Name1;
    if (player && player->GetSession())
    {
        int loc = player->GetSession()->GetSessionDbLocaleIndex();
        if (loc >= 0)
            if (ItemLocale const* il = sObjectMgr->GetItemLocale(itemId))
                ObjectMgr::GetLocaleString(il->Name, loc, name);
    }
    return name;
}

void MythicPlusMgr::EnsureVaultChoices(Player* player)
{
    if (!player)
        return;

    LoadProfile(player->GetGUID());
    MythicProfile& profile = _profiles[player->GetGUID()];
    if (profile.VaultClaimed || !profile.WeekRuns)
        return;
    if (profile.VaultItems[0] && profile.VaultItems[1] && profile.VaultItems[2])
        return;

    uint8 keyLevel = std::max(profile.WeekBestLevel, profile.WeekKeys[0]);
    if (!keyLevel)
        return;

    std::vector<uint32> pool;
    BuildSpecLootPool(player, keyLevel, false, pool);
    if (pool.size() < 12)
        BuildSpecLootPool(player, keyLevel, true, pool);

    std::unordered_map<uint32, std::vector<uint32>> byType;
    for (uint32 id : pool)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(id);
        if (!proto)
            continue;
        byType[proto->InventoryType].push_back(id);
    }

    std::vector<uint32> types;
    types.reserve(byType.size());
    for (auto const& pair : byType)
        types.push_back(pair.first);

    std::array<uint32, 3> picked{};
    uint8 count = 0;
    while (count < 3 && !types.empty())
    {
        uint32 typeIdx = urand(0, types.size() - 1);
        uint32 invType = types[typeIdx];
        types[typeIdx] = types.back();
        types.pop_back();

        std::vector<uint32> const& items = byType[invType];
        if (items.empty())
            continue;
        picked[count++] = items[urand(0, items.size() - 1)];
    }

    if (count < 3 && !pool.empty())
    {
        std::unordered_set<uint32> used(picked.begin(), picked.begin() + count);
        for (uint32 n = 0; n < pool.size() && count < 3; ++n)
        {
            uint32 id = pool[urand(0, pool.size() - 1)];
            if (used.insert(id).second)
                picked[count++] = id;
        }
    }

    profile.VaultItems = picked;
    SaveProfile(player->GetGUID());
}

void MythicPlusMgr::GiveEmblems(Player* player, uint8 keyLevel)
{
    if (!player)
        return;

    uint32 emblems = 47241;
    uint32 emblemCount = 2;
    if (keyLevel >= 14)
    {
        emblems = 49426;
        emblemCount = 5;
    }
    else if (keyLevel >= 10)
    {
        emblems = 49426;
        emblemCount = 3;
    }
    else if (keyLevel >= 7)
    {
        emblems = 47241;
        emblemCount = 4;
    }
    GiveItem(player, emblems, emblemCount);
}
