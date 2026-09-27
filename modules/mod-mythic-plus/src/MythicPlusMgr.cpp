/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MythicPlusMgr.h"
#include "Chat.h"
#include "Common.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "Item.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include "LFGMgr.h"
#include <algorithm>
#include <cmath>

MythicPlusMgr* MythicPlusMgr::instance()
{
    static MythicPlusMgr instance;
    return &instance;
}

bool MythicPlusMgr::IsSpanish(Player const* player)
{
    if (!player || !player->GetSession())
        return false;

    LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
    if (locale == LOCALE_esES || locale == LOCALE_esMX)
        return true;
    locale = player->GetSession()->GetSessionDbLocaleIndex();
    return locale == LOCALE_esES || locale == LOCALE_esMX;
}

char const* MythicPlusMgr::Text(Player const* player, char const* en, char const* es)
{
    return IsSpanish(player) ? es : en;
}

char const* MythicPlusMgr::DungeonName(uint8 dungeonId, bool spanish)
{
    MythicDungeonDef const* def = FindMythicDungeon(dungeonId);
    if (!def)
        return spanish ? "Desconocida" : "Unknown";
    return spanish ? def->NameEs : def->NameEn;
}

void MythicPlusMgr::LoadConfig(bool /*reload*/)
{
    _enabled = sConfigMgr->GetOption<bool>("MythicPlus.Enable", true);
    _announce = sConfigMgr->GetOption<bool>("MythicPlus.Announce", true);
    _allowTeleport = sConfigMgr->GetOption<bool>("MythicPlus.AllowTeleport", true);
    _requireRoles = sConfigMgr->GetOption<bool>("MythicPlus.RequireRoles", true);
    _minLevel = static_cast<uint8>(sConfigMgr->GetOption<uint32>("MythicPlus.MinLevel", 80));
    _minPlayers = static_cast<uint8>(sConfigMgr->GetOption<uint32>("MythicPlus.MinPlayers", 5));
    _maxPlayers = static_cast<uint8>(sConfigMgr->GetOption<uint32>("MythicPlus.MaxPlayers", 5));
    _roleCheckMs = sConfigMgr->GetOption<uint32>("MythicPlus.RoleCheckMs", 45000);
    _maxKeyLevel = static_cast<uint8>(sConfigMgr->GetOption<uint32>("MythicPlus.MaxKeyLevel", 25));
    _deathPenaltyMs = sConfigMgr->GetOption<uint32>("MythicPlus.DeathPenaltyMs", 5000);
    _scalePerLevel = sConfigMgr->GetOption<float>("MythicPlus.ScalePerLevel", 0.08f);
    _forcesPercent = static_cast<uint8>(sConfigMgr->GetOption<uint32>("MythicPlus.ForcesPercent", 70));
    _seasonWeeks = sConfigMgr->GetOption<uint32>("MythicPlus.SeasonWeeks", 12);
    _npcEntry = sConfigMgr->GetOption<uint32>("MythicPlus.NPCEntry", NPC_MYTHIC_BROKER);
    _keystoneItem = sConfigMgr->GetOption<uint32>("MythicPlus.KeystoneItem", ITEM_MYTHIC_KEYSTONE);
    _fontEntry = sConfigMgr->GetOption<uint32>("MythicPlus.FontEntry", GO_MYTHIC_FONT);
    _residuumItem = sConfigMgr->GetOption<uint32>("MythicPlus.ResiduumItem", ITEM_MYTHIC_RESIDUUM);
    _abandonMs = sConfigMgr->GetOption<uint32>("MythicPlus.AbandonMs", 60000);

    uint32 forcedSeason = sConfigMgr->GetOption<uint32>("MythicPlus.SeasonId", 0);
    if (forcedSeason)
        _seasonId = forcedSeason;

    LOG_INFO("module", "Mythic+ BFA: {} (season {}, week {}, cap +{})",
        _enabled ? "enabled" : "disabled", _seasonId, _weekIndex + 1, _maxKeyLevel);
}

void MythicPlusMgr::EnsureDatabase()
{
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `character_mythic_profile` ("
        " `guid` INT UNSIGNED NOT NULL,"
        " `dungeon_id` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `key_level` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `depleted` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `overall_score` FLOAT NOT NULL DEFAULT 0,"
        " `week_best_level` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_best_dungeon` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_runs` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `vault_claimed` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_key1` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_key2` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_key3` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `season_id` INT UNSIGNED NOT NULL DEFAULT 1,"
        " PRIMARY KEY (`guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `character_mythic_best` ("
        " `guid` INT UNSIGNED NOT NULL,"
        " `dungeon_id` TINYINT UNSIGNED NOT NULL,"
        " `fort_level` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `fort_score` FLOAT NOT NULL DEFAULT 0,"
        " `tyr_level` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `tyr_score` FLOAT NOT NULL DEFAULT 0,"
        " PRIMARY KEY (`guid`, `dungeon_id`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `character_mythic_runs` ("
        " `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
        " `guid` INT UNSIGNED NOT NULL,"
        " `dungeon_id` TINYINT UNSIGNED NOT NULL,"
        " `level` TINYINT UNSIGNED NOT NULL,"
        " `timed` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `upgrade` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `score` FLOAT NOT NULL DEFAULT 0,"
        " `deaths` SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " `duration_ms` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `season_id` INT UNSIGNED NOT NULL DEFAULT 1,"
        " `completed_at` INT UNSIGNED NOT NULL DEFAULT 0,"
        " PRIMARY KEY (`id`),"
        " KEY `idx_guid` (`guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `mythic_state` ("
        " `id` TINYINT UNSIGNED NOT NULL,"
        " `week_start` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_index` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `season_id` INT UNSIGNED NOT NULL DEFAULT 1,"
        " PRIMARY KEY (`id`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `mythic_request` ("
        " `guid` INT UNSIGNED NOT NULL,"
        " `action` TINYINT UNSIGNED NOT NULL,"
        " `extra` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `created_at` INT UNSIGNED NOT NULL DEFAULT 0,"
        " PRIMARY KEY (`guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `mythic_run_live` ("
        " `instance_id` INT UNSIGNED NOT NULL,"
        " `map_id` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `dungeon_id` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `level` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `affix0` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `affix1` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `affix2` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `affix3` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `elapsed_ms` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `limit_ms` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `deaths` SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " `forces` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `forces_req` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `bosses` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `bosses_req` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `active` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " PRIMARY KEY (`instance_id`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `mythic_run_member` ("
        " `guid` INT UNSIGNED NOT NULL,"
        " `instance_id` INT UNSIGNED NOT NULL,"
        " PRIMARY KEY (`guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    LoadState();
}

void MythicPlusMgr::LoadState()
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT week_start, week_index, season_id FROM mythic_state WHERE id = 1");
    if (result)
    {
        Field* fields = result->Fetch();
        _weekStart = fields[0].Get<uint32>();
        _weekIndex = fields[1].Get<uint8>();
        if (!_seasonId)
            _seasonId = fields[2].Get<uint32>();
        if (!_seasonId)
            _seasonId = 1;
    }
    else
    {
        _weekStart = static_cast<uint32>(GameTime::GetGameTime().count());
        _weekIndex = 0;
        if (!_seasonId)
            _seasonId = 1;
        SaveState();
    }
}

void MythicPlusMgr::SaveState()
{
    CharacterDatabase.Execute(
        "REPLACE INTO mythic_state (id, week_start, week_index, season_id) VALUES (1, {}, {}, {})",
        _weekStart, _weekIndex, _seasonId);
}

void MythicPlusMgr::CheckWeekReset()
{
    uint32 const now = static_cast<uint32>(GameTime::GetGameTime().count());
    if (!_weekStart || now < _weekStart + 7 * DAY)
        return;

    uint32 weeksPassed = (now - _weekStart) / (7 * DAY);
    _weekStart += weeksPassed * 7 * DAY;
    uint32 next = uint32(_weekIndex) + weeksPassed;
    uint32 weeks = _seasonWeeks ? _seasonWeeks : 12;
    _seasonId += next / weeks;
    _weekIndex = static_cast<uint8>(next % weeks);

    CharacterDatabase.Execute(
        "UPDATE character_mythic_profile SET week_best_level = 0, week_best_dungeon = 0, "
        "week_runs = 0, vault_claimed = 0, week_key1 = 0, week_key2 = 0, week_key3 = 0");
    _profiles.clear();
    SaveState();
    LOG_INFO("module", "Mythic+: new week {} of season {}", _weekIndex + 1, _seasonId);
}

void MythicPlusMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    _updateTimer += diff;
    if (_updateTimer < 500)
        return;

    uint32 step = _updateTimer;
    _updateTimer = 0;
    CheckWeekReset();
    ProcessLuaRequests();
    TickTeleportChecks(step);
}

void MythicPlusMgr::ProcessLuaRequests()
{
    QueryResult result = CharacterDatabase.Query("SELECT guid, action, extra FROM mythic_request");
    if (!result)
        return;

    struct Request { uint32 GuidLow; uint8 Action; uint8 Extra; };
    std::vector<Request> requests;
    do
    {
        Field* fields = result->Fetch();
        requests.push_back({ fields[0].Get<uint32>(), fields[1].Get<uint8>(), fields[2].Get<uint8>() });
    } while (result->NextRow());

    CharacterDatabase.Execute("DELETE FROM mythic_request");

    for (Request const& request : requests)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(request.GuidLow));
        if (!player)
            continue;

        std::string error;
        bool ok = false;
        switch (request.Action)
        {
            case MYTHIC_REQ_START:
                ok = StartRun(player, error);
                break;
            case MYTHIC_REQ_VAULT:
                ok = ClaimVaultSlot(player, request.Extra, error);
                break;
            case MYTHIC_REQ_TELEPORT:
                ok = TeleportToKey(player, error);
                break;
            case MYTHIC_REQ_CLAIM_KEY:
                ok = ClaimStarterKey(player, error);
                break;
            default:
                break;
        }

        if (!ok && !error.empty())
            ChatHandler(player->GetSession()).SendSysMessage(error);
    }
}

MythicAffixSet MythicPlusMgr::GetWeeklyAffixes() const
{
    static MythicAffixSet const rotation[12] =
    {
        { AFFIX_FORTIFIED,  AFFIX_BOLSTERING, AFFIX_BURSTING },
        { AFFIX_TYRANNICAL, AFFIX_RAGING,     AFFIX_VOLCANIC },
        { AFFIX_FORTIFIED,  AFFIX_SANGUINE,   AFFIX_STORMING },
        { AFFIX_TYRANNICAL, AFFIX_BURSTING,   AFFIX_EXPLOSIVE },
        { AFFIX_FORTIFIED,  AFFIX_SPITEFUL,   AFFIX_QUAKING },
        { AFFIX_TYRANNICAL, AFFIX_INSPIRING,  AFFIX_NECROTIC },
        { AFFIX_FORTIFIED,  AFFIX_SANGUINE,   AFFIX_GRIEVOUS },
        { AFFIX_TYRANNICAL, AFFIX_BOLSTERING, AFFIX_EXPLOSIVE },
        { AFFIX_FORTIFIED,  AFFIX_BURSTING,   AFFIX_VOLCANIC },
        { AFFIX_TYRANNICAL, AFFIX_RAGING,     AFFIX_NECROTIC },
        { AFFIX_FORTIFIED,  AFFIX_INSPIRING,  AFFIX_STORMING },
        { AFFIX_TYRANNICAL, AFFIX_SPITEFUL,   AFFIX_GRIEVOUS }
    };

    MythicAffixSet set = rotation[_weekIndex % 12];
    set.Seasonal = SeasonalAffixForSeason(_seasonId);
    return set;
}

uint8 MythicPlusMgr::SeasonalAffixForSeason(uint32 seasonId) const
{
    switch ((seasonId - 1) % SEASONAL_COUNT)
    {
        case SEASONAL_INFESTED:  return AFFIX_INFESTED;
        case SEASONAL_REAPING:   return AFFIX_REAPING;
        case SEASONAL_BEGUILING: return AFFIX_BEGUILING;
        default:                 return AFFIX_AWAKENED;
    }
}

MythicAffixSet MythicPlusMgr::GetAffixesForLevel(uint8 keyLevel) const
{
    MythicAffixSet weekly = GetWeeklyAffixes();
    MythicAffixSet applied;
    if (keyLevel >= 2)
        applied.FortTyr = weekly.FortTyr;
    if (keyLevel >= 4)
        applied.Plus4 = weekly.Plus4;
    if (keyLevel >= 7)
        applied.Plus7 = weekly.Plus7;
    if (keyLevel >= 10)
        applied.Seasonal = weekly.Seasonal;
    return applied;
}

void MythicPlusMgr::LoadProfile(ObjectGuid guid)
{
    if (_profiles.find(guid) != _profiles.end())
        return;

    MythicProfile profile;
    QueryResult result = CharacterDatabase.Query(
        "SELECT dungeon_id, key_level, depleted, overall_score, week_best_level, week_best_dungeon, "
        "week_runs, vault_claimed, week_key1, week_key2, week_key3, season_id "
        "FROM character_mythic_profile WHERE guid = {}", guid.GetCounter());
    if (result)
    {
        Field* fields = result->Fetch();
        profile.Key.DungeonId = fields[0].Get<uint8>();
        profile.Key.Level = fields[1].Get<uint8>();
        profile.Key.Depleted = fields[2].Get<uint8>() != 0;
        profile.OverallScore = fields[3].Get<float>();
        profile.WeekBestLevel = fields[4].Get<uint8>();
        profile.WeekBestDungeon = fields[5].Get<uint8>();
        profile.WeekRuns = fields[6].Get<uint8>();
        profile.VaultClaimed = fields[7].Get<uint8>();
        profile.WeekKeys[0] = fields[8].Get<uint8>();
        profile.WeekKeys[1] = fields[9].Get<uint8>();
        profile.WeekKeys[2] = fields[10].Get<uint8>();
        profile.SeasonId = fields[11].Get<uint32>();
    }

    _profiles[guid] = profile;
}

void MythicPlusMgr::SaveProfile(ObjectGuid guid)
{
    auto it = _profiles.find(guid);
    if (it == _profiles.end())
        return;

    MythicProfile const& p = it->second;
    CharacterDatabase.Execute(
        "REPLACE INTO character_mythic_profile (guid, dungeon_id, key_level, depleted, overall_score, "
        "week_best_level, week_best_dungeon, week_runs, vault_claimed, week_key1, week_key2, week_key3, "
        "season_id) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
        guid.GetCounter(), p.Key.DungeonId, p.Key.Level, p.Key.Depleted ? 1 : 0, p.OverallScore,
        p.WeekBestLevel, p.WeekBestDungeon, p.WeekRuns, p.VaultClaimed,
        p.WeekKeys[0], p.WeekKeys[1], p.WeekKeys[2], p.SeasonId);
}

MythicProfile MythicPlusMgr::GetProfile(ObjectGuid guid)
{
    LoadProfile(guid);
    return _profiles[guid];
}

MythicRun* MythicPlusMgr::GetRun(uint32 instanceId)
{
    auto it = _runs.find(instanceId);
    return it == _runs.end() ? nullptr : &it->second;
}

MythicRun const* MythicPlusMgr::GetRun(uint32 instanceId) const
{
    auto it = _runs.find(instanceId);
    return it == _runs.end() ? nullptr : &it->second;
}

MythicRun* MythicPlusMgr::GetRunForPlayer(Player* player)
{
    if (!player)
        return nullptr;
    auto it = _playerRun.find(player->GetGUID());
    if (it == _playerRun.end())
        return nullptr;
    return GetRun(it->second);
}

MythicRun const* MythicPlusMgr::GetRunForPlayer(Player const* player) const
{
    if (!player)
        return nullptr;
    auto it = _playerRun.find(player->GetGUID());
    if (it == _playerRun.end())
        return nullptr;
    return GetRun(it->second);
}

void MythicPlusMgr::HandleLogin(Player* player)
{
    if (!player)
        return;

    LoadProfile(player->GetGUID());
    MythicProfile const& profile = _profiles[player->GetGUID()];
    if (profile.Key.Level)
        EnsureKeyItem(player);
    if (_residuumItem && player->HasItemCount(_residuumItem, 1, true))
        SendCustomItemQuery(player, _residuumItem);
}

void MythicPlusMgr::HandleLogout(ObjectGuid guid)
{
    auto it = _playerTeleportCheck.find(guid);
    if (it != _playerTeleportCheck.end())
        FinishTeleportCheck(it->second, lfg::LFG_ROLECHECK_ABORTED);

    SaveProfile(guid);
    _profiles.erase(guid);
}

void MythicPlusMgr::EnsureKeyItem(Player* player)
{
    if (!player || !_keystoneItem)
        return;
    if (!player->HasItemCount(_keystoneItem, 1, true))
        GiveItem(player, _keystoneItem, 1);
    SendCustomItemQuery(player, _keystoneItem);
}

void MythicPlusMgr::RemoveKeyItem(Player* player)
{
    if (!player || !_keystoneItem)
        return;
    player->DestroyItemCount(_keystoneItem, 1, true, false);
}

void MythicPlusMgr::GiveItem(Player* player, uint32 itemId, uint32 count)
{
    if (!player || !itemId || !count)
        return;
    if (!sObjectMgr->GetItemTemplate(itemId))
        return;

    if (!player->StoreNewItemInBestSlots(itemId, count))
        player->SendItemRetrievalMail(itemId, count);
    if (itemId == _keystoneItem || itemId == _residuumItem)
        SendCustomItemQuery(player, itemId);
}

uint8 MythicPlusMgr::PickRandomDungeon(uint8 except) const
{
    uint8 pick = static_cast<uint8>(urand(1, MYTHIC_DUNGEON_COUNT));
    if (pick == except && MYTHIC_DUNGEON_COUNT > 1)
        pick = pick == MYTHIC_DUNGEON_COUNT ? 1 : pick + 1;
    return pick;
}

bool MythicPlusMgr::SetKey(Player* player, uint8 dungeonId, uint8 level, std::string& error)
{
    if (!player)
        return false;
    if (!FindMythicDungeon(dungeonId))
    {
        error = Text(player, "Unknown dungeon id (1-16).", "Id de mazmorra desconocido (1-16).");
        return false;
    }

    level = std::max<uint8>(2, std::min(level, _maxKeyLevel));
    LoadProfile(player->GetGUID());
    MythicProfile& profile = _profiles[player->GetGUID()];
    profile.Key.DungeonId = dungeonId;
    profile.Key.Level = level;
    profile.Key.Depleted = false;
    SaveProfile(player->GetGUID());
    RefreshKeyItem(player);
    error.clear();
    return true;
}

bool MythicPlusMgr::ClaimStarterKey(Player* player, std::string& error)
{
    if (!player)
        return false;
    if (player->GetLevel() < _minLevel)
    {
        error = Text(player, "You must be level 80.", "Necesitas ser nivel 80.");
        return false;
    }

    LoadProfile(player->GetGUID());
    MythicProfile& profile = _profiles[player->GetGUID()];
    if (profile.Key.Level)
    {
        error = Text(player, "You already have a keystone.", "Ya tienes una piedra angular.");
        return false;
    }

    profile.Key.DungeonId = PickRandomDungeon();
    profile.Key.Level = 2;
    profile.Key.Depleted = false;
    SaveProfile(player->GetGUID());
    RefreshKeyItem(player);

    bool const es = IsSpanish(player);
    ChatHandler(player->GetSession()).PSendSysMessage(
        es ? "Recibes una Piedra angular mitica +2: {}." : "You receive a Mythic Keystone +2: {}.",
        DungeonName(profile.Key.DungeonId, es));
    return true;
}

void MythicPlusMgr::HandlePlayerEnter(Map* map, Player* player)
{
    if (!map || !player || !map->IsDungeon() || map->IsRaid())
        return;

    uint32 instanceId = map->GetInstanceId();
    if (MythicRun* run = GetRun(instanceId))
    {
        if (run->Active)
        {
            _playerRun[player->GetGUID()] = instanceId;
            CharacterDatabase.Execute(
                "REPLACE INTO mythic_run_member (guid, instance_id) VALUES ({}, {})",
                player->GetGUID().GetCounter(), instanceId);
            SendRunObjective(player, *run);
        }
        return;
    }

    MythicDungeonDef const* def = FindMythicDungeonByMap(map->GetId());
    if (!def)
        return;

    LoadProfile(player->GetGUID());
    MythicKeystone const& key = _profiles[player->GetGUID()].Key;
    if (key.DungeonId == def->Id && key.Level && map->IsHeroic())
    {
        bool const es = IsSpanish(player);
        ChatHandler(player->GetSession()).PSendSysMessage(
            es ? "Piedra +{} — {}. Usa la Fuente de Poder para comenzar."
               : "Keystone +{} — {}. Use the Font of Power to start.",
            key.Level, DungeonName(key.DungeonId, es));
    }
}

void MythicPlusMgr::HandlePlayerLeave(Map* map, Player* /*player*/)
{
    if (!map)
        return;
    MythicRun* run = GetRun(map->GetInstanceId());
    if (!run || !run->Active)
        return;

    bool anyoneLeft = false;
    map->DoForAllPlayers([&anyoneLeft](Player* /*member*/)
    {
        anyoneLeft = true;
    });
    if (!anyoneLeft)
        run->EmptyMs = 1;
}

void MythicPlusMgr::DestroyMap(Map* map)
{
    if (!map)
        return;
    uint32 instanceId = map->GetInstanceId();
    if (MythicRun* run = GetRun(instanceId))
    {
        if (run->Active && !run->Completed)
            FailRun(*run, true);
        ClearLiveState(instanceId);
        for (ObjectGuid const& guid : run->Members)
            _playerRun.erase(guid);
        _runs.erase(instanceId);
    }
}

float MythicPlusMgr::KeyMultiplier(uint8 level) const
{
    return std::pow(1.f + _scalePerLevel, float(level));
}

bool MythicPlusMgr::IsBoss(Creature const* creature)
{
    if (!creature)
        return false;
    CreatureTemplate const* info = creature->GetCreatureTemplate();
    return creature->IsDungeonBoss() || creature->isWorldBoss()
        || (info && info->rank == CREATURE_ELITE_WORLDBOSS);
}

bool MythicPlusMgr::IsEnemyForcesCreature(Creature const* creature)
{
    if (!creature)
        return false;
    // OnUnitDeath runs after the NPC is already dead; do not require IsAlive().
    if (creature->IsTrigger() || creature->IsCivilian() || creature->IsCritter())
        return false;
    if (creature->IsPet() || creature->IsSummon())
        return false;
    if (IsMythicAffixNpc(creature->GetEntry()))
        return false;
    if (IsBoss(creature))
        return false;
    uint32 type = creature->GetCreatureType();
    if (type == CREATURE_TYPE_CRITTER || type == CREATURE_TYPE_TOTEM || type == CREATURE_TYPE_NON_COMBAT_PET)
        return false;
    return creature->IsHostileToPlayers();
}

uint32 MythicPlusMgr::ForceValue(Creature const* creature)
{
    if (!creature || !creature->GetCreatureTemplate())
        return 0;
    switch (creature->GetCreatureTemplate()->rank)
    {
        case CREATURE_ELITE_RARE:
        case CREATURE_ELITE_RAREELITE:
            return 12;
        case CREATURE_ELITE_ELITE:
            return 8;
        default:
            return 4;
    }
}

float MythicPlusMgr::DamageMultiplier(MythicRun const& run, Creature const* creature) const
{
    float mult = KeyMultiplier(run.Level);
    bool const boss = IsBoss(creature);
    if (boss && run.HasAffix(AFFIX_TYRANNICAL))
        mult *= 1.15f;
    else if (!boss && run.HasAffix(AFFIX_FORTIFIED))
        mult *= 1.30f;
    if (run.HasAffix(AFFIX_RAGING) && run.Raging.count(creature->GetGUID()))
        mult *= 1.75f;
    return mult;
}

void MythicPlusMgr::ScaleCreature(Creature* creature, MythicRun& run)
{
    if (!creature || !creature->IsAlive())
        return;
    if (run.ScaledCreatures.count(creature->GetGUID()))
        return;
    if (creature->IsTrigger() || creature->IsCritter() || creature->IsCivilian())
        return;

    run.ScaledCreatures.insert(creature->GetGUID());

    float hp = KeyMultiplier(run.Level);
    bool const boss = IsBoss(creature);
    if (boss && run.HasAffix(AFFIX_TYRANNICAL))
        hp *= 1.40f;
    else if (!boss && run.HasAffix(AFFIX_FORTIFIED))
        hp *= 1.20f;

    uint32 newMax = std::max<uint32>(1, uint32(float(creature->GetMaxHealth()) * hp));
    creature->SetCreateHealth(newMax);
    creature->SetMaxHealth(newMax);
    creature->SetHealth(newMax);

    if (!boss && run.HasAffix(AFFIX_INSPIRING) && roll_chance_i(15))
        run.Inspiring.insert(creature->GetGUID());
    if (!boss && run.HasAffix(AFFIX_INFESTED) && roll_chance_i(20))
        run.Infested.insert(creature->GetGUID());
}

void MythicPlusMgr::HandleCreatureAdd(Creature* creature)
{
    if (!creature)
        return;
    Map* map = creature->GetMap();
    if (!map || !map->IsDungeon())
        return;
    MythicRun* run = GetRun(map->GetInstanceId());
    if (!run || !run->Active)
        return;
    ScaleCreature(creature, *run);
}

void MythicPlusMgr::GrantM0Key(Player* player, uint32 mapId)
{
    if (!player)
        return;
    LoadProfile(player->GetGUID());
    MythicProfile& profile = _profiles[player->GetGUID()];
    if (profile.Key.Level)
        return;

    MythicDungeonDef const* def = FindMythicDungeonByMap(mapId);
    if (!def)
        return;

    profile.Key.DungeonId = PickRandomDungeon();
    profile.Key.Level = 2;
    profile.Key.Depleted = false;
    SaveProfile(player->GetGUID());
    RefreshKeyItem(player);

    bool const es = IsSpanish(player);
    ChatHandler(player->GetSession()).PSendSysMessage(
        es ? "Has desbloqueado las Miticas. Piedra +2: {}."
           : "You unlocked Mythic Keystones. Keystone +2: {}.",
        DungeonName(profile.Key.DungeonId, es));
}

Player* MythicPlusMgr::FirstOnlineMember(MythicRun const& run) const
{
    for (ObjectGuid const& guid : run.Members)
        if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
            return player;
    return nullptr;
}

uint8 MythicPlusMgr::UpgradeForTime(uint32 remainingMs, uint32 limitMs) const
{
    if (!limitMs || remainingMs == 0)
        return 1;
    float ratio = float(remainingMs) / float(limitMs);
    if (ratio >= 0.40f)
        return 3;
    if (ratio >= 0.20f)
        return 2;
    return 1;
}

float MythicPlusMgr::ScoreForRun(uint8 level, uint8 upgrade, bool timed, float overtimeRatio) const
{
    float base = 25.f + float(level) * 5.f;
    if (!timed)
        return base * std::max(0.40f, 0.85f - overtimeRatio);
    if (upgrade >= 3)
        return base * 1.15f;
    if (upgrade >= 2)
        return base * 1.10f;
    return base * 1.05f;
}

uint32 MythicPlusMgr::ResiduumForLevel(uint8 level) const
{
    return uint32(15 * level * level);
}

void MythicPlusMgr::SaveBest(ObjectGuid guid, uint8 dungeonId, bool tyrannical, uint8 level, float score)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT fort_level, fort_score, tyr_level, tyr_score FROM character_mythic_best "
        "WHERE guid = {} AND dungeon_id = {}", guid.GetCounter(), dungeonId);

    uint8 fortLevel = 0;
    float fortScore = 0.f;
    uint8 tyrLevel = 0;
    float tyrScore = 0.f;
    if (result)
    {
        Field* fields = result->Fetch();
        fortLevel = fields[0].Get<uint8>();
        fortScore = fields[1].Get<float>();
        tyrLevel = fields[2].Get<uint8>();
        tyrScore = fields[3].Get<float>();
    }

    if (tyrannical)
    {
        if (score > tyrScore)
        {
            tyrScore = score;
            tyrLevel = level;
        }
    }
    else if (score > fortScore)
    {
        fortScore = score;
        fortLevel = level;
    }

    CharacterDatabase.Execute(
        "REPLACE INTO character_mythic_best (guid, dungeon_id, fort_level, fort_score, tyr_level, tyr_score) "
        "VALUES ({}, {}, {}, {}, {}, {})",
        guid.GetCounter(), dungeonId, fortLevel, fortScore, tyrLevel, tyrScore);
}

void MythicPlusMgr::RecalcOverall(ObjectGuid guid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT fort_score, tyr_score FROM character_mythic_best WHERE guid = {}", guid.GetCounter());
    float overall = 0.f;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            float a = fields[0].Get<float>();
            float b = fields[1].Get<float>();
            float high = std::max(a, b);
            float low = std::min(a, b);
            overall += high * 1.5f + low * 0.5f;
        } while (result->NextRow());
    }

    LoadProfile(guid);
    _profiles[guid].OverallScore = overall;
    SaveProfile(guid);
}

std::vector<MythicLeaderboardRow> MythicPlusMgr::GetLeaderboard(uint32 limit)
{
    std::vector<MythicLeaderboardRow> rows;
    QueryResult result = CharacterDatabase.Query(
        "SELECT c.name, p.overall_score, p.week_best_level, p.week_runs "
        "FROM character_mythic_profile p INNER JOIN characters c ON c.guid = p.guid "
        "WHERE p.overall_score > 0 ORDER BY p.overall_score DESC LIMIT {}", limit);
    if (!result)
        return rows;

    do
    {
        Field* fields = result->Fetch();
        MythicLeaderboardRow row;
        row.Name = fields[0].Get<std::string>();
        row.Score = fields[1].Get<float>();
        row.WeekBest = fields[2].Get<uint8>();
        row.Runs = fields[3].Get<uint32>();
        rows.push_back(row);
    } while (result->NextRow());
    return rows;
}

void MythicPlusMgr::Announce(Map* map, std::string const& message) const
{
    Announce(map, message, message);
}

void MythicPlusMgr::Announce(Map* map, std::string const& en, std::string const& es) const
{
    if (!map)
        return;
    map->DoForAllPlayers([&en, &es](Player* player)
    {
        ChatHandler(player->GetSession()).SendSysMessage(IsSpanish(player) ? es : en);
    });
}

void MythicPlusMgr::SendRunObjective(Player* player, MythicRun const& run) const
{
    if (!player || !player->GetSession())
        return;

    bool const es = IsSpanish(player);
    std::string affixes;
    uint8 const ids[4] = {
        run.Affixes.FortTyr, run.Affixes.Plus4, run.Affixes.Plus7, run.Affixes.Seasonal
    };
    for (uint8 id : ids)
    {
        if (!id)
            continue;
        if (!affixes.empty())
            affixes += " / ";
        affixes += AffixName(id, es);
    }
    if (affixes.empty())
        affixes = es ? "Ninguno" : "None";

    std::string const line = Acore::StringFormat(
        es ? "+{} {} | {} | {:02}:00 | Fuerzas {} | Jefes {}"
           : "+{} {} | {} | {:02}:00 | Forces {} | Bosses {}",
        run.Level, DungeonName(run.DungeonId, es), affixes,
        run.TimeLimitMs / 60000, run.ForcesRequired, run.BossesRequired);

    ChatHandler handler(player->GetSession());
    handler.SendNotification("{}", line);
    player->GetSession()->SendAreaTriggerMessage("{}", line);
    handler.SendSysMessage(Acore::StringFormat("|cffff6600{}|r", line));
}

void MythicPlusMgr::SendRunObjective(Map* map, MythicRun const& run) const
{
    if (!map)
        return;
    map->DoForAllPlayers([this, &run](Player* player)
    {
        SendRunObjective(player, run);
    });
}

void MythicPlusMgr::WriteLiveState(MythicRun const& run)
{
    CharacterDatabase.Execute(
        "REPLACE INTO mythic_run_live (instance_id, map_id, dungeon_id, level, affix0, affix1, affix2, "
        "affix3, elapsed_ms, limit_ms, deaths, forces, forces_req, bosses, bosses_req, active) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
        run.InstanceId, run.MapId, run.DungeonId, run.Level,
        run.Affixes.FortTyr, run.Affixes.Plus4, run.Affixes.Plus7, run.Affixes.Seasonal,
        run.ElapsedMs, run.TimeLimitMs, run.Deaths, run.Forces, run.ForcesRequired,
        run.BossesKilled, run.BossesRequired, run.Active ? 1 : 0);
}

void MythicPlusMgr::ClearLiveState(uint32 instanceId)
{
    CharacterDatabase.Execute("DELETE FROM mythic_run_live WHERE instance_id = {}", instanceId);
    CharacterDatabase.Execute("DELETE FROM mythic_run_member WHERE instance_id = {}", instanceId);
}

void MythicPlusMgr::SendStatus(ChatHandler* handler, Player* player) const
{
    if (!handler || !player)
        return;

    bool const es = IsSpanish(player);
    MythicAffixSet weekly = GetWeeklyAffixes();
    handler->PSendSysMessage(es ? "Miticas BFA — Temporada {} semana {}."
                                : "Mythic+ BFA — Season {} week {}.",
        _seasonId, _weekIndex + 1);
    handler->PSendSysMessage("|cff00ccff+2|r {}  |cff00ccff+4|r {}  |cff00ccff+7|r {}  |cff00ccff+10|r {}",
        AffixName(weekly.FortTyr, es), AffixName(weekly.Plus4, es),
        AffixName(weekly.Plus7, es), AffixName(weekly.Seasonal, es));

    auto it = _profiles.find(player->GetGUID());
    if (it == _profiles.end() || !it->second.Key.Level)
    {
        handler->SendSysMessage(es ? "No tienes piedra angular." : "You have no keystone.");
        return;
    }

    MythicProfile const& p = it->second;
    handler->PSendSysMessage(es ? "Piedra: +{} {}{}" : "Keystone: +{} {}{}",
        p.Key.Level, DungeonName(p.Key.DungeonId, es),
        p.Key.Depleted ? (es ? " (agotada)" : " (depleted)") : "");
    handler->PSendSysMessage(es ? "Puntuacion: {:.1f}  Mejor semana: +{}  Runs: {}"
                                : "Score: {:.1f}  Week best: +{}  Runs: {}",
        p.OverallScore, p.WeekBestLevel, p.WeekRuns);

    if (MythicRun const* run = GetRunForPlayer(player))
    {
        uint32 remain = run->TimeLimitMs > run->ElapsedMs ? run->TimeLimitMs - run->ElapsedMs : 0;
        handler->PSendSysMessage(
            es ? "En curso: +{} {} — {:02}:{:02}  muertes {}  fuerzas {}/{}  jefes {}/{}"
               : "In progress: +{} {} — {:02}:{:02}  deaths {}  forces {}/{}  bosses {}/{}",
            run->Level, DungeonName(run->DungeonId, es),
            remain / 60000, (remain / 1000) % 60, run->Deaths,
            run->Forces, run->ForcesRequired, run->BossesKilled, run->BossesRequired);
    }
}
