/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MODULE_RAID_VENDOR_H
#define MODULE_RAID_VENDOR_H

#include "Common.h"
#include <array>
#include <string>
#include <unordered_set>
#include <vector>

class Creature;
class Player;
struct ItemTemplate;

enum RaidVendorRaid : uint8
{
    RAID_VENDOR_NAXX    = 0,
    RAID_VENDOR_OS      = 1,
    RAID_VENDOR_EOE     = 2,
    RAID_VENDOR_VOA     = 3,
    RAID_VENDOR_ULDUAR  = 4,
    RAID_VENDOR_TOC     = 5,
    RAID_VENDOR_ICC     = 6,
    RAID_VENDOR_RS      = 7,
    RAID_VENDOR_ONYXIA  = 8,
    RAID_VENDOR_COUNT   = 9
};

class RaidVendorMgr
{
public:
    static RaidVendorMgr* instance();

    void LoadConfig(bool reload);
    void LoadVendors();

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] uint32 GetNpcEntry() const { return _npcEntry; }

    void BuildMainMenu(Player* player, Creature* creature) const;
    void HandleGossipSelect(Player* player, Creature* creature, uint32 action) const;

private:
    enum LootTableType : uint8
    {
        LOOT_TABLE_CREATURE   = 0,
        LOOT_TABLE_GAMEOBJECT = 1,
        LOOT_TABLE_REFERENCE  = 2
    };

    struct RaidInfo
    {
        uint16 MapId = 0;
        char const* ConfigKey = "";
        char const* NameEn = "";
        char const* NameEs = "";
        uint32 RequiredRating = 0;
        std::vector<uint32> Items;
        std::vector<uint32> VendorEntries;
    };

    [[nodiscard]] static bool IsSpanish(Player const* player);
    [[nodiscard]] static char const* Msg(Player const* player, char const* spanish, char const* english);
    [[nodiscard]] char const* RaidName(Player const* player, uint8 raid) const;
    [[nodiscard]] uint32 GetPlayerRating(Player const* player) const;
    [[nodiscard]] bool CanOpenRaid(Player const* player, uint8 raid) const;
    [[nodiscard]] bool IsRaidItem(ItemTemplate const* proto) const;
    [[nodiscard]] static uint32 VendorEntry(uint8 raid, uint8 page);

    void CollectLoot(uint32 lootId, LootTableType type, std::unordered_set<uint32>& items,
        std::unordered_set<uint64>& visited) const;
    void AddLootFromCreature(uint32 creatureEntry, std::unordered_set<uint32>& items,
        std::unordered_set<uint64>& visited) const;
    void AddEncounterLoot(uint8 raid, std::unordered_set<uint32>& items, std::unordered_set<uint64>& visited) const;
    void BuildRaidVendors(uint8 raid, std::unordered_set<uint32> const& items);
    void FillVendorPage(uint8 raid, uint8 page) const;
    void SendDenied(Player* player, uint8 raid) const;
    void OpenRaid(Player* player, Creature* creature, uint8 raid) const;
    void OpenPage(Player* player, Creature* creature, uint8 raid, uint8 page) const;
    void BuildPageMenu(Player* player, Creature* creature, uint8 raid) const;

    bool _enabled = true;
    bool _loaded = false;
    bool _skipWorldLoot = true;
    uint32 _npcEntry = 190013;
    uint32 _minQuality = 3;
    uint32 _worldLootMinRef = 1000000;
    int32 _ratingSlot = -1;
    std::array<RaidInfo, RAID_VENDOR_COUNT> _raids;
};

#define sRaidVendorMgr RaidVendorMgr::instance()

#endif
