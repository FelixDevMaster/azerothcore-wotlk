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
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include <algorithm>
#include <functional>

void MythicPlusMgr::PrepareInstance(Map* map, MythicRun& run)
{
    uint32 forcesPossible = 0;
    run.BossesRequired = 0;

    for (auto const& pair : map->GetCreatureBySpawnIdStore())
    {
        Creature* creature = pair.second;
        if (!creature || !creature->IsAlive())
            continue;

        ScaleCreature(creature, run);

        if (IsBoss(creature))
            ++run.BossesRequired;
        else if (IsEnemyForcesCreature(creature))
            forcesPossible += ForceValue(creature);

        if (run.HasAffix(AFFIX_TEEMING) && IsEnemyForcesCreature(creature) && roll_chance_i(20))
        {
            if (Player* source = FirstOnlineMember(run))
            {
                if (TempSummon* clone = source->SummonCreature(creature->GetEntry(), *creature,
                        TEMPSUMMON_CORPSE_TIMED_DESPAWN, 15000))
                {
                    clone->SetFaction(creature->GetFaction());
                    ScaleCreature(clone, run);
                }
            }
        }
    }

    if (!run.BossesRequired)
        run.BossesRequired = 1;
    run.ForcesRequired = std::max<uint32>(1, forcesPossible * _forcesPercent / 100);

    if (Player* source = FirstOnlineMember(run))
    {
        MythicDungeonDef const* def = FindMythicDungeon(run.DungeonId);
        if (def)
            source->SummonGameObject(_fontEntry, def->X, def->Y, def->Z, def->O, 0.f, 0.f, 0.f, 0.f, 7200);
        SpawnSeasonal(map, run, source);
    }
}

void MythicPlusMgr::SpawnSeasonal(Map* map, MythicRun& run, Player* source)
{
    if (!source)
        return;

    std::vector<Creature*> trash;
    for (auto const& pair : map->GetCreatureBySpawnIdStore())
    {
        Creature* creature = pair.second;
        if (creature && IsEnemyForcesCreature(creature))
            trash.push_back(creature);
    }
    if (trash.empty())
        return;

    auto spawnAt = [&](uint32 entry, Creature* nearCreature)
    {
        if (!nearCreature)
            return;
        if (TempSummon* summoned = source->SummonCreature(entry, *nearCreature,
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30000))
            ScaleCreature(summoned, run);
    };

    if (run.HasAffix(AFFIX_BEGUILING))
    {
        spawnAt(NPC_MYTHIC_BEGUILING_VOID, trash[urand(0, trash.size() - 1)]);
        spawnAt(NPC_MYTHIC_BEGUILING_ENCH, trash[urand(0, trash.size() - 1)]);
        spawnAt(NPC_MYTHIC_BEGUILING_EMIS, trash[urand(0, trash.size() - 1)]);
    }

    if (run.HasAffix(AFFIX_AWAKENED))
    {
        uint32 count = std::min<uint32>(4, trash.size());
        for (uint32 i = 0; i < count; ++i)
            spawnAt(NPC_MYTHIC_AWAKENED, trash[urand(0, trash.size() - 1)]);
    }
}

bool MythicPlusMgr::StartRun(Player* player, std::string& error)
{
    if (!player)
        return false;
    if (!_enabled)
    {
        error = Text(player, "Mythic+ is disabled.", "Las miticas estan desactivadas.");
        return false;
    }
    if (player->GetLevel() < _minLevel)
    {
        error = Text(player, "You must be level 80.", "Necesitas ser nivel 80.");
        return false;
    }

    Map* map = player->GetMap();
    if (!map || !map->IsDungeon() || map->IsRaid() || !map->IsHeroic())
    {
        error = IsSpanish(player)
            ? "Debes estar en la mazmorra heroica de tu piedra."
            : "You must be inside your keystone heroic dungeon.";
        return false;
    }

    if (GetRun(map->GetInstanceId()))
    {
        error = Text(player, "A mythic run is already active.", "Ya hay una mitica en curso.");
        return false;
    }

    LoadProfile(player->GetGUID());
    MythicKeystone const& key = _profiles[player->GetGUID()].Key;
    MythicDungeonDef const* def = FindMythicDungeon(key.DungeonId);
    if (!def || !key.Level)
    {
        error = Text(player, "You have no keystone.", "No tienes piedra angular.");
        return false;
    }
    if (def->MapId != map->GetId())
    {
        error = IsSpanish(player)
            ? Acore::StringFormat("Esta piedra es de {}.", DungeonName(key.DungeonId, true))
            : Acore::StringFormat("This keystone is for {}.", DungeonName(key.DungeonId, false));
        return false;
    }

    std::vector<Player*> members;
    if (!CollectGroupMembers(player, members, error) || !ValidatePartySize(player, members, error))
        return false;
    if (!ValidatePartyComposition(player, members, error))
        return false;

    for (Player* member : members)
    {
        if (member->GetLevel() < _minLevel)
        {
            error = Acore::StringFormat(
                IsSpanish(player) ? "{} no es nivel {}." : "{} is not level {}.",
                member->GetName(), _minLevel);
            return false;
        }
        if (!member->GetMap() || member->GetMap()->GetInstanceId() != map->GetInstanceId())
        {
            error = IsSpanish(player)
                ? "Todo el grupo debe estar dentro de la instancia."
                : "The whole group must be inside the instance.";
            return false;
        }
    }

    MythicRun run;
    run.InstanceId = map->GetInstanceId();
    run.MapId = map->GetId();
    run.DungeonId = key.DungeonId;
    run.Level = key.Level;
    run.Affixes = GetAffixesForLevel(key.Level);
    run.LeaderGuid = player->GetGUID();
    run.TimeLimitMs = def->TimerMs;
    run.Active = true;
    for (Player* member : members)
        run.Members.push_back(member->GetGUID());

    _runs[run.InstanceId] = run;
    MythicRun& stored = _runs[run.InstanceId];
    for (ObjectGuid const& guid : stored.Members)
        _playerRun[guid] = stored.InstanceId;

    PrepareInstance(map, stored);
    WriteLiveState(stored);
    for (ObjectGuid const& guid : stored.Members)
        CharacterDatabase.Execute(
            "REPLACE INTO mythic_run_member (guid, instance_id) VALUES ({}, {})",
            guid.GetCounter(), stored.InstanceId);

    SendRunObjective(map, stored);

    if (_announce)
        LOG_INFO("module", "Mythic+ start: +{} {} instance {} by {}",
            stored.Level, def->NameEn, stored.InstanceId, player->GetName());
    return true;
}

void MythicPlusMgr::HandlePlayerDeath(Player* player)
{
    if (!player)
        return;
    MythicRun* run = GetRunForPlayer(player);
    if (!run || !run->Active || run->Completed)
        return;

    ++run->Deaths;
    run->ElapsedMs += _deathPenaltyMs;
    WriteLiveState(*run);
    Announce(player->GetMap(),
        Acore::StringFormat("|cffff0000+{}s|r ({}) — {} deaths",
            _deathPenaltyMs / 1000, player->GetName(), run->Deaths),
        Acore::StringFormat("|cffff0000+{}s|r ({}) — {} muertes",
            _deathPenaltyMs / 1000, player->GetName(), run->Deaths));
}

void MythicPlusMgr::HandleUnitDeath(Unit* unit, Unit* /*killer*/)
{
    if (!unit || !unit->IsCreature())
        return;

    Creature* creature = unit->ToCreature();
    Map* map = creature->GetMap();
    if (!map || !map->IsDungeon())
        return;

    MythicRun* run = GetRun(map->GetInstanceId());
    if (!run || !run->Active || run->Completed)
    {
        if (IsBoss(creature) && map->IsHeroic())
        {
            bool allDead = true;
            for (auto const& pair : map->GetCreatureBySpawnIdStore())
            {
                Creature* other = pair.second;
                if (other && other->IsAlive() && IsBoss(other) && other != creature)
                {
                    allDead = false;
                    break;
                }
            }
            if (allDead)
            {
                uint32 mapId = map->GetId();
                map->DoForAllPlayers([this, mapId](Player* member)
                {
                    GrantM0Key(member, mapId);
                });
            }
        }
        return;
    }

    if (IsMythicAffixNpc(creature->GetEntry()))
        return;

    if (IsBoss(creature) && !run->DeadBosses.count(creature->GetGUID()))
    {
        run->DeadBosses.insert(creature->GetGUID());
        ++run->BossesKilled;
        Announce(map,
            Acore::StringFormat("|cffffd100Boss {} / {}|r", run->BossesKilled, run->BossesRequired),
            Acore::StringFormat("|cffffd100Jefe {} / {}|r", run->BossesKilled, run->BossesRequired));
        WriteLiveState(*run);
    }
    else if (IsEnemyForcesCreature(creature) && !run->CountedCreatures.count(creature->GetGUID()))
    {
        run->CountedCreatures.insert(creature->GetGUID());
        run->Forces += ForceValue(creature);
        if (run->HasAffix(AFFIX_REAPING))
            ++run->ReapingKills;
        ApplyAffixDeath(map, *run, creature);
        WriteLiveState(*run);
    }

    if (run->BossesKilled >= run->BossesRequired && run->Forces >= run->ForcesRequired)
        CompleteRun(map, *run, run->ElapsedMs <= run->TimeLimitMs);
}

void MythicPlusMgr::UpdateRun(Map* map, uint32 diff)
{
    if (!map)
        return;
    MythicRun* run = GetRun(map->GetInstanceId());
    if (!run || !run->Active || run->Completed)
        return;

    run->ElapsedMs += diff;
    if (run->EmptyMs)
        run->EmptyMs += diff;

    bool anyone = false;
    map->DoForAllPlayers([&anyone](Player* /*player*/) { anyone = true; });
    if (anyone)
        run->EmptyMs = 0;
    else if (run->EmptyMs >= _abandonMs)
    {
        FailRun(*run, true);
        return;
    }

    TickAffixes(map, *run, diff);

    run->AnnounceMs += diff;
    if (run->AnnounceMs >= 60000)
    {
        run->AnnounceMs = 0;
        uint32 remain = run->TimeLimitMs > run->ElapsedMs ? run->TimeLimitMs - run->ElapsedMs : 0;
        Announce(map,
            Acore::StringFormat("|cffffcc00{:02}:{:02}|r  forces {}/{}  bosses {}/{}  deaths {}",
                remain / 60000, (remain / 1000) % 60, run->Forces, run->ForcesRequired,
                run->BossesKilled, run->BossesRequired, run->Deaths),
            Acore::StringFormat("|cffffcc00{:02}:{:02}|r  fuerzas {}/{}  jefes {}/{}  muertes {}",
                remain / 60000, (remain / 1000) % 60, run->Forces, run->ForcesRequired,
                run->BossesKilled, run->BossesRequired, run->Deaths));
    }

    run->LiveWriteMs += diff;
    if (run->LiveWriteMs >= 1000)
    {
        run->LiveWriteMs = 0;
        WriteLiveState(*run);
    }
}

void MythicPlusMgr::CompleteRun(Map* map, MythicRun& run, bool timed)
{
    if (run.Completed)
        return;

    run.Completed = true;
    run.Active = false;
    run.Timed = timed;
    uint32 remaining = timed && run.TimeLimitMs > run.ElapsedMs ? run.TimeLimitMs - run.ElapsedMs : 0;
    run.Upgrade = timed ? UpgradeForTime(remaining, run.TimeLimitMs) : 0;
    float overtime = timed ? 0.f
        : (run.TimeLimitMs ? float(run.ElapsedMs - run.TimeLimitMs) / float(run.TimeLimitMs) : 1.f);
    float score = ScoreForRun(run.Level, run.Upgrade, timed, overtime);
    bool const tyrannical = run.HasAffix(AFFIX_TYRANNICAL);

    for (ObjectGuid const& guid : run.Members)
    {
        LoadProfile(guid);
        MythicProfile& profile = _profiles[guid];
        ++profile.WeekRuns;
        if (timed && run.Level > profile.WeekBestLevel)
        {
            profile.WeekBestLevel = run.Level;
            profile.WeekBestDungeon = run.DungeonId;
        }

        std::array<uint8, 4> keys = {
            profile.WeekKeys[0], profile.WeekKeys[1], profile.WeekKeys[2], run.Level
        };
        std::sort(keys.begin(), keys.end(), std::greater<uint8>());
        profile.WeekKeys[0] = keys[0];
        profile.WeekKeys[1] = keys[1];
        profile.WeekKeys[2] = keys[2];

        if (guid == run.LeaderGuid)
        {
            uint8 newLevel = timed
                ? std::min<uint8>(_maxKeyLevel, uint8(run.Level + run.Upgrade))
                : run.Level;
            profile.Key.DungeonId = PickRandomDungeon(run.DungeonId);
            profile.Key.Level = std::max<uint8>(2, newLevel);
            profile.Key.Depleted = !timed;
        }

        SaveBest(guid, run.DungeonId, tyrannical, run.Level, score);
        RecalcOverall(guid);
        CharacterDatabase.Execute(
            "INSERT INTO character_mythic_runs (guid, dungeon_id, level, timed, upgrade, score, deaths, "
            "duration_ms, season_id, completed_at) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
            guid.GetCounter(), run.DungeonId, run.Level, timed ? 1 : 0, run.Upgrade, score,
            run.Deaths, run.ElapsedMs, _seasonId, uint32(GameTime::GetGameTime().count()));

        if (Player* member = ObjectAccessor::FindConnectedPlayer(guid))
            RewardRun(member, run, true);
    }

    ClearLiveState(run.InstanceId);
    Announce(map,
        Acore::StringFormat(
            timed ? "|cff00ff00Mythic +{} complete|r — key +{}  score {:.1f}"
                  : "|cffffff00Mythic +{} overtime|r — key unchanged  score {:.1f}",
            run.Level, timed ? run.Upgrade : 0, score),
        Acore::StringFormat(
            timed ? "|cff00ff00Mitica +{} completada|r — piedra +{}  puntuacion {:.1f}"
                  : "|cffffff00Mitica +{} fuera de tiempo|r — piedra sin cambios  puntuacion {:.1f}",
            run.Level, timed ? run.Upgrade : 0, score));
}

void MythicPlusMgr::FailRun(MythicRun& run, bool abandon)
{
    if (run.Completed || run.Failed)
        return;

    run.Failed = true;
    run.Active = false;
    LoadProfile(run.LeaderGuid);
    MythicProfile& profile = _profiles[run.LeaderGuid];
    if (profile.Key.Level)
    {
        profile.Key.Level = std::max<uint8>(2, uint8(profile.Key.Level - 1));
        profile.Key.DungeonId = PickRandomDungeon(run.DungeonId);
        profile.Key.Depleted = true;
        SaveProfile(run.LeaderGuid);
    }

    if (Player* leader = ObjectAccessor::FindConnectedPlayer(run.LeaderGuid))
        RefreshKeyItem(leader);

    ClearLiveState(run.InstanceId);
    if (abandon)
        LOG_INFO("module", "Mythic+ abandoned: +{} instance {}", run.Level, run.InstanceId);
}

bool MythicPlusMgr::ForceComplete(Player* player, std::string& error)
{
    if (!player)
        return false;
    MythicRun* run = GetRunForPlayer(player);
    if (!run || !run->Active)
    {
        error = Text(player, "No active mythic run.", "No hay una mitica activa.");
        return false;
    }
    run->BossesKilled = run->BossesRequired;
    run->Forces = run->ForcesRequired;
    CompleteRun(player->GetMap(), *run, true);
    return true;
}

void MythicPlusMgr::RewardRun(Player* player, MythicRun const& run, bool endChest)
{
    if (!player)
        return;

    bool const es = IsSpanish(player);
    RewardGear(player, run.Level, false);
    GiveItem(player, _residuumItem, ResiduumForLevel(run.Level));
    if (endChest)
        ChatHandler(player->GetSession()).PSendSysMessage(
            es ? "Recompensa de +{} entregada." : "+{} end-of-run reward granted.", run.Level);
    RefreshKeyItem(player);
    EnsureVaultChoices(player);
}

void MythicPlusMgr::RewardGear(Player* player, uint8 keyLevel, bool /*vault*/)
{
    if (!player)
        return;

    uint32 itemId = PickSpecItem(player, keyLevel, {});
    if (itemId)
        GiveItem(player, itemId, 1);
    GiveEmblems(player, keyLevel);
}

bool MythicPlusMgr::ClaimVaultSlot(Player* player, uint8 slot, std::string& error)
{
    if (!player)
        return false;
    if (slot < 1 || slot > 3)
    {
        error = Text(player, "Choose vault option 1, 2 or 3.", "Elige la opcion 1, 2 o 3 del cofre.");
        return false;
    }

    LoadProfile(player->GetGUID());
    MythicProfile& profile = _profiles[player->GetGUID()];
    if (profile.VaultClaimed)
    {
        error = Text(player, "You already claimed this week's vault.",
            "Ya reclamaste el cofre de esta semana.");
        return false;
    }
    if (!profile.WeekRuns)
    {
        error = Text(player, "Complete a key this week to unlock the vault.",
            "Completa una mitica esta semana para abrir el cofre.");
        return false;
    }

    EnsureVaultChoices(player);
    uint32 itemId = profile.VaultItems[slot - 1];
    if (!itemId)
    {
        error = Text(player, "The vault has no item in that option.",
            "Esa opcion del cofre no tiene item.");
        return false;
    }

    uint8 keyLevel = std::max(profile.WeekBestLevel, profile.WeekKeys[0]);
    profile.VaultClaimed = 1;
    SaveProfile(player->GetGUID());
    GiveItem(player, itemId, 1);
    GiveEmblems(player, keyLevel);
    GiveItem(player, _residuumItem, ResiduumForLevel(keyLevel) * 2);

    bool const es = IsSpanish(player);
    ChatHandler(player->GetSession()).PSendSysMessage(
        es ? "Cofre semanal +{}: {}." : "Weekly vault +{}: {}.",
        keyLevel, LocalizedItemName(player, itemId));
    return true;
}
