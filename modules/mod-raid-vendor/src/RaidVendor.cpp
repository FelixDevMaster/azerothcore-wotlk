/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RaidVendor.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DBCEnums.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "GameObjectData.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include <algorithm>

enum RaidVendorGossip
{
    GOSSIP_RAID_VENDOR_MAIN      = GOSSIP_ACTION_INFO_DEF,
    GOSSIP_RAID_VENDOR_RAID_BASE = GOSSIP_ACTION_INFO_DEF + 1,
    GOSSIP_RAID_VENDOR_PAGE_BASE = GOSSIP_ACTION_INFO_DEF + 100
};

uint32 constexpr RAID_VENDOR_ENTRY_BASE = 191000;
uint8 constexpr RAID_VENDOR_PAGE_SIZE = MAX_VENDOR_ITEMS;
uint8 constexpr RAID_VENDOR_PAGES_PER_RAID = 32;
uint32 constexpr RAID_VENDOR_TEXT = 190013;

namespace
{
struct RaidMapDef
{
    uint16 MapId;
    char const* ConfigKey;
    char const* NameEn;
    char const* NameEs;
    uint32 DefaultRating;
};

RaidMapDef const RaidDefs[RAID_VENDOR_COUNT] =
{
    { 533, "Naxxramas",            "Naxxramas",                  "Naxxramas",                       1000 },
    { 615, "ObsidianSanctum",      "The Obsidian Sanctum",       "El Sagrario Obsidiana",           1000 },
    { 616, "EyeOfEternity",        "The Eye of Eternity",        "El Ojo de la Eternidad",          1000 },
    { 624, "VaultOfArchavon",      "Vault of Archavon",          "La Camara de Archavon",           1200 },
    { 603, "Ulduar",               "Ulduar",                     "Ulduar",                          1400 },
    { 649, "TrialOfTheCrusader",   "Trial of the Crusader",      "Prueba del Cruzado",              1600 },
    { 631, "IcecrownCitadel",      "Icecrown Citadel",           "Ciudadela de la Corona de Hielo", 1800 },
    { 724, "RubySanctum",          "The Ruby Sanctum",           "El Sagrario Rubi",                2000 },
    { 249, "Onyxia",               "Onyxia's Lair",              "La Guarida de Onyxia",            1000 }
};
}

RaidVendorMgr* RaidVendorMgr::instance()
{
    static RaidVendorMgr instance;
    return &instance;
}

uint32 RaidVendorMgr::VendorEntry(uint8 raid, uint8 page)
{
    return RAID_VENDOR_ENTRY_BASE + uint32(raid) * RAID_VENDOR_PAGES_PER_RAID + page;
}

bool RaidVendorMgr::IsSpanish(Player const* player)
{
    if (!player || !player->GetSession())
        return false;

    LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
    return locale == LOCALE_esES || locale == LOCALE_esMX;
}

char const* RaidVendorMgr::Msg(Player const* player, char const* spanish, char const* english)
{
    return IsSpanish(player) ? spanish : english;
}

char const* RaidVendorMgr::RaidName(Player const* player, uint8 raid) const
{
    if (raid >= RAID_VENDOR_COUNT)
        return "";

    return IsSpanish(player) ? _raids[raid].NameEs : _raids[raid].NameEn;
}

void RaidVendorMgr::LoadConfig(bool /*reload*/)
{
    _enabled = sConfigMgr->GetOption<bool>("RaidVendor.Enable", true);
    _npcEntry = sConfigMgr->GetOption<uint32>("RaidVendor.NPCEntry", 190013);
    _minQuality = sConfigMgr->GetOption<uint32>("RaidVendor.MinQuality", ITEM_QUALITY_RARE);
    _skipWorldLoot = sConfigMgr->GetOption<bool>("RaidVendor.SkipWorldLoot", true);
    _worldLootMinRef = sConfigMgr->GetOption<uint32>("RaidVendor.SkipWorldLootMinRef", 1000000);
    _ratingSlot = sConfigMgr->GetOption<int32>("RaidVendor.RatingSlot", -1);

    if (_minQuality > ITEM_QUALITY_HEIRLOOM)
        _minQuality = ITEM_QUALITY_RARE;

    if (_ratingSlot > 2)
        _ratingSlot = -1;

    for (uint8 i = 0; i < RAID_VENDOR_COUNT; ++i)
    {
        _raids[i].MapId = RaidDefs[i].MapId;
        _raids[i].ConfigKey = RaidDefs[i].ConfigKey;
        _raids[i].NameEn = RaidDefs[i].NameEn;
        _raids[i].NameEs = RaidDefs[i].NameEs;
        std::string key = Acore::StringFormat("RaidVendor.{}.Rating", RaidDefs[i].ConfigKey);
        _raids[i].RequiredRating = sConfigMgr->GetOption<uint32>(key, RaidDefs[i].DefaultRating);
    }

    LOG_INFO("server.loading", ">> Raid Vendor: {} (npc {}, min quality {})",
        _enabled ? "enabled" : "disabled", _npcEntry, _minQuality);
}

uint32 RaidVendorMgr::GetPlayerRating(Player const* player) const
{
    if (!player)
        return 0;

    if (_ratingSlot >= 0)
        return player->GetArenaPersonalRating(uint8(_ratingSlot));

    uint32 best = 0;
    for (uint8 slot = 0; slot < 3; ++slot)
    {
        uint32 rating = player->GetArenaPersonalRating(slot);
        if (rating > best)
            best = rating;
    }

    return best;
}

bool RaidVendorMgr::CanOpenRaid(Player const* player, uint8 raid) const
{
    if (raid >= RAID_VENDOR_COUNT)
        return false;

    return GetPlayerRating(player) >= _raids[raid].RequiredRating;
}

bool RaidVendorMgr::IsRaidItem(ItemTemplate const* proto) const
{
    if (!proto)
        return false;

    if (proto->Quality < _minQuality)
        return false;

    if (proto->Class == ITEM_CLASS_QUEST || proto->Class == ITEM_CLASS_MONEY)
        return false;

    if (proto->Bonding == BIND_QUEST_ITEM || proto->Bonding == BIND_QUEST_ITEM1)
        return false;

    if (proto->HasFlag(ITEM_FLAG_CONJURED) || proto->HasFlag(ITEM_FLAG_DEPRECATED) ||
        proto->HasFlag(ITEM_FLAG_NO_PICKUP))
        return false;

    return true;
}

void RaidVendorMgr::CollectLoot(uint32 lootId, LootTableType type, std::unordered_set<uint32>& items,
    std::unordered_set<uint64>& visited) const
{
    if (!lootId)
        return;

    uint64 key = (uint64(type) << 32) | lootId;
    if (!visited.insert(key).second)
        return;

    char const* table = "creature_loot_template";
    if (type == LOOT_TABLE_GAMEOBJECT)
        table = "gameobject_loot_template";
    else if (type == LOOT_TABLE_REFERENCE)
        table = "reference_loot_template";

    QueryResult result = WorldDatabase.Query(
        "SELECT Item, Reference, QuestRequired FROM {} WHERE Entry = {}", table, lootId);
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint32 itemId = fields[0].Get<uint32>();
        int32 reference = fields[1].Get<int32>();
        int8 questRequired = fields[2].Get<int8>();

        if (questRequired)
            continue;

        if (reference > 0)
        {
            if (_skipWorldLoot && uint32(reference) >= _worldLootMinRef)
                continue;

            CollectLoot(uint32(reference), LOOT_TABLE_REFERENCE, items, visited);
            continue;
        }

        if (!itemId)
            continue;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (IsRaidItem(proto))
            items.insert(itemId);
    } while (result->NextRow());
}

void RaidVendorMgr::AddLootFromCreature(uint32 creatureEntry, std::unordered_set<uint32>& items,
    std::unordered_set<uint64>& visited) const
{
    CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(creatureEntry);
    if (!info)
        return;

    CollectLoot(info->lootid, LOOT_TABLE_CREATURE, items, visited);

    for (uint32 difficultyEntry : info->DifficultyEntry)
    {
        CreatureTemplate const* difficultyInfo = sObjectMgr->GetCreatureTemplate(difficultyEntry);
        if (difficultyInfo)
            CollectLoot(difficultyInfo->lootid, LOOT_TABLE_CREATURE, items, visited);
    }
}

void RaidVendorMgr::AddEncounterLoot(uint8 raid, std::unordered_set<uint32>& items,
    std::unordered_set<uint64>& visited) const
{
    for (uint8 diff = 0; diff < MAX_DIFFICULTY; ++diff)
    {
        DungeonEncounterList const* encounters =
            sObjectMgr->GetDungeonEncounterList(_raids[raid].MapId, Difficulty(diff));
        if (!encounters)
            continue;

        for (DungeonEncounter const* encounter : *encounters)
        {
            if (!encounter || encounter->creditType != ENCOUNTER_CREDIT_KILL_CREATURE)
                continue;

            AddLootFromCreature(encounter->creditEntry, items, visited);
        }
    }
}

void RaidVendorMgr::BuildRaidVendors(uint8 raid, std::unordered_set<uint32> const& items)
{
    RaidInfo& info = _raids[raid];
    info.VendorEntries.clear();
    info.Items.assign(items.begin(), items.end());

    std::sort(info.Items.begin(), info.Items.end(), [](uint32 a, uint32 b)
    {
        ItemTemplate const* ia = sObjectMgr->GetItemTemplate(a);
        ItemTemplate const* ib = sObjectMgr->GetItemTemplate(b);
        if (!ia || !ib)
            return a < b;
        if (ia->Quality != ib->Quality)
            return ia->Quality > ib->Quality;
        if (ia->ItemLevel != ib->ItemLevel)
            return ia->ItemLevel > ib->ItemLevel;
        return ia->Name1 < ib->Name1;
    });

    uint32 const pageCount = (info.Items.size() + RAID_VENDOR_PAGE_SIZE - 1) / RAID_VENDOR_PAGE_SIZE;
    uint32 const cappedPages = std::min<uint32>(pageCount, RAID_VENDOR_PAGES_PER_RAID);
    if (cappedPages && info.Items.size() > cappedPages * RAID_VENDOR_PAGE_SIZE)
        info.Items.resize(cappedPages * RAID_VENDOR_PAGE_SIZE);

    for (uint32 page = 0; page < cappedPages; ++page)
    {
        info.VendorEntries.push_back(VendorEntry(raid, uint8(page)));
        FillVendorPage(raid, uint8(page));
    }
}

void RaidVendorMgr::FillVendorPage(uint8 raid, uint8 page) const
{
    if (raid >= RAID_VENDOR_COUNT)
        return;

    RaidInfo const& info = _raids[raid];
    if (page >= info.VendorEntries.size())
        return;

    uint32 vendorEntry = info.VendorEntries[page];
    VendorItemData const* data = sObjectMgr->GetNpcVendorItemList(vendorEntry);
    if (data && !data->Empty())
        return;

    uint32 start = uint32(page) * RAID_VENDOR_PAGE_SIZE;
    if (start >= info.Items.size())
        return;

    uint32 end = std::min<uint32>(uint32(info.Items.size()), start + RAID_VENDOR_PAGE_SIZE);
    for (uint32 i = start; i < end; ++i)
        sObjectMgr->AddVendorItem(vendorEntry, info.Items[i], 0, 0, 0, false);
}

void RaidVendorMgr::LoadVendors()
{
    if (!_enabled || _loaded)
        return;

    std::array<std::unordered_set<uint32>, RAID_VENDOR_COUNT> raidItems;
    std::array<std::unordered_set<uint64>, RAID_VENDOR_COUNT> visited;

    auto RaidForMap = [](uint16 mapId) -> int8
    {
        for (uint8 i = 0; i < RAID_VENDOR_COUNT; ++i)
            if (RaidDefs[i].MapId == mapId)
                return int8(i);
        return -1;
    };

    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        int8 raid = RaidForMap(data.mapid);
        if (raid < 0)
            continue;

        CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(data.id);
        if (!info)
            continue;

        if (info->expansion < 2 && info->minlevel < 80)
            continue;

        AddLootFromCreature(data.id, raidItems[uint8(raid)], visited[uint8(raid)]);
        if (data.id2)
            AddLootFromCreature(data.id2, raidItems[uint8(raid)], visited[uint8(raid)]);
        if (data.id3)
            AddLootFromCreature(data.id3, raidItems[uint8(raid)], visited[uint8(raid)]);
    }

    for (auto const& [spawnId, data] : sObjectMgr->GetAllGOData())
    {
        int8 raid = RaidForMap(data.mapid);
        if (raid < 0)
            continue;

        GameObjectTemplate const* info = sObjectMgr->GetGameObjectTemplate(data.id);
        if (!info)
            continue;

        uint32 lootId = info->GetLootId();
        if (!lootId)
            continue;

        CollectLoot(lootId, LOOT_TABLE_GAMEOBJECT, raidItems[uint8(raid)], visited[uint8(raid)]);
    }

    uint32 total = 0;
    for (uint8 raid = 0; raid < RAID_VENDOR_COUNT; ++raid)
    {
        AddEncounterLoot(raid, raidItems[raid], visited[raid]);
        BuildRaidVendors(raid, raidItems[raid]);
        total += uint32(_raids[raid].Items.size());
        LOG_INFO("server.loading", ">> Raid Vendor: {} - {} items in {} page(s)",
            _raids[raid].NameEn, uint32(_raids[raid].Items.size()), uint32(_raids[raid].VendorEntries.size()));
    }

    _loaded = true;
    LOG_INFO("server.loading", ">> Raid Vendor: loaded {} unique raid items", total);
}

void RaidVendorMgr::SendDenied(Player* player, uint8 raid) const
{
    uint32 required = _raids[raid].RequiredRating;
    uint32 have = GetPlayerRating(player);
    ChatHandler(player->GetSession()).PSendSysMessage(Msg(player,
        "Necesitas {} de rating de arena para abrir {}. Tienes {}.",
        "You need {} arena rating to browse {}. You have {}."),
        required, RaidName(player, raid), have);

    if (WorldSession* session = player->GetSession())
        session->SendAreaTriggerMessage("{}", Msg(player,
            "Rating de arena insuficiente.",
            "Not enough arena rating."));
}

void RaidVendorMgr::BuildMainMenu(Player* player, Creature* creature) const
{
    ClearGossipMenuFor(player);

    uint32 rating = GetPlayerRating(player);
    for (uint8 raid = 0; raid < RAID_VENDOR_COUNT; ++raid)
    {
        RaidInfo const& info = _raids[raid];
        if (info.Items.empty())
            continue;

        bool locked = rating < info.RequiredRating;
        std::string label = Acore::StringFormat("{} [{}] ({})",
            RaidName(player, raid),
            info.RequiredRating,
            uint32(info.Items.size()));

        if (locked)
            label = Acore::StringFormat("|cff777777{}|r", label);

        AddGossipItemFor(player, locked ? GOSSIP_ICON_CHAT : GOSSIP_ICON_VENDOR,
            label, GOSSIP_SENDER_MAIN, GOSSIP_RAID_VENDOR_RAID_BASE + raid);
    }

    SendGossipMenuFor(player, RAID_VENDOR_TEXT, creature->GetGUID());
}

void RaidVendorMgr::BuildPageMenu(Player* player, Creature* creature, uint8 raid) const
{
    ClearGossipMenuFor(player);

    RaidInfo const& info = _raids[raid];
    uint32 pages = uint32(info.VendorEntries.size());
    for (uint32 page = 0; page < pages; ++page)
    {
        uint32 start = page * RAID_VENDOR_PAGE_SIZE + 1;
        uint32 end = std::min<uint32>(uint32(info.Items.size()), (page + 1) * RAID_VENDOR_PAGE_SIZE);
        std::string label = Acore::StringFormat("{} ({}/{})  {}-{}",
            RaidName(player, raid), page + 1, pages, start, end);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label, GOSSIP_SENDER_MAIN,
            GOSSIP_RAID_VENDOR_PAGE_BASE + raid * RAID_VENDOR_PAGES_PER_RAID + page);
    }

    AddGossipItemFor(player, GOSSIP_ICON_CHAT,
        Msg(player, "Volver", "Back"), GOSSIP_SENDER_MAIN, GOSSIP_RAID_VENDOR_MAIN);

    SendGossipMenuFor(player, RAID_VENDOR_TEXT, creature->GetGUID());
}

void RaidVendorMgr::OpenPage(Player* player, Creature* creature, uint8 raid, uint8 page) const
{
    if (raid >= RAID_VENDOR_COUNT || page >= _raids[raid].VendorEntries.size())
        return;

    FillVendorPage(raid, page);
    CloseGossipMenuFor(player);
    player->GetSession()->SendListInventory(creature->GetGUID(), _raids[raid].VendorEntries[page]);
}

void RaidVendorMgr::OpenRaid(Player* player, Creature* creature, uint8 raid) const
{
    if (raid >= RAID_VENDOR_COUNT)
        return;

    if (!CanOpenRaid(player, raid))
    {
        SendDenied(player, raid);
        BuildMainMenu(player, creature);
        return;
    }

    if (_raids[raid].VendorEntries.empty())
    {
        ChatHandler(player->GetSession()).SendSysMessage(Msg(player,
            "Esta raid no tiene items cargados.",
            "This raid has no items loaded."));
        BuildMainMenu(player, creature);
        return;
    }

    if (_raids[raid].VendorEntries.size() == 1)
    {
        OpenPage(player, creature, raid, 0);
        return;
    }

    BuildPageMenu(player, creature, raid);
}

void RaidVendorMgr::HandleGossipSelect(Player* player, Creature* creature, uint32 action) const
{
    if (action == GOSSIP_RAID_VENDOR_MAIN)
    {
        BuildMainMenu(player, creature);
        return;
    }

    if (action >= GOSSIP_RAID_VENDOR_PAGE_BASE)
    {
        uint32 encoded = action - GOSSIP_RAID_VENDOR_PAGE_BASE;
        uint8 raid = uint8(encoded / RAID_VENDOR_PAGES_PER_RAID);
        uint8 page = uint8(encoded % RAID_VENDOR_PAGES_PER_RAID);
        if (raid >= RAID_VENDOR_COUNT)
            return;

        if (!CanOpenRaid(player, raid))
        {
            SendDenied(player, raid);
            BuildMainMenu(player, creature);
            return;
        }

        OpenPage(player, creature, raid, page);
        return;
    }

    if (action >= GOSSIP_RAID_VENDOR_RAID_BASE)
    {
        uint8 raid = uint8(action - GOSSIP_RAID_VENDOR_RAID_BASE);
        if (raid < RAID_VENDOR_COUNT)
            OpenRaid(player, creature, raid);
    }
}
