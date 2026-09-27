/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MODULE_MYTHIC_PLUS_H
#define MODULE_MYTHIC_PLUS_H

#include "ObjectGuid.h"
#include "Position.h"
#include "SharedDefines.h"
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Creature;
class Map;
class Player;
class Unit;

enum MythicAffix : uint8
{
    AFFIX_NONE        = 0,
    AFFIX_FORTIFIED   = 1,
    AFFIX_TYRANNICAL  = 2,
    AFFIX_BOLSTERING  = 3,
    AFFIX_BURSTING    = 4,
    AFFIX_RAGING      = 5,
    AFFIX_SANGUINE    = 6,
    AFFIX_TEEMING     = 7,
    AFFIX_NECROTIC    = 8,
    AFFIX_SKITTISH    = 9,
    AFFIX_VOLCANIC    = 10,
    AFFIX_EXPLOSIVE   = 11,
    AFFIX_QUAKING     = 12,
    AFFIX_GRIEVOUS    = 13,
    AFFIX_INSPIRING   = 14,
    AFFIX_SPITEFUL    = 15,
    AFFIX_STORMING    = 16,
    AFFIX_INFESTED    = 17,
    AFFIX_REAPING     = 18,
    AFFIX_BEGUILING   = 19,
    AFFIX_AWAKENED    = 20,
    AFFIX_MAX
};

enum MythicSeasonal : uint8
{
    SEASONAL_INFESTED  = 0,
    SEASONAL_REAPING   = 1,
    SEASONAL_BEGUILING = 2,
    SEASONAL_AWAKENED  = 3,
    SEASONAL_COUNT     = 4
};

enum MythicNpcConst : uint32
{
    NPC_MYTHIC_BROKER          = 190020,
    NPC_MYTHIC_EXPLOSIVE       = 190023,
    NPC_MYTHIC_SANGUINE        = 190024,
    NPC_MYTHIC_SPITEFUL        = 190025,
    NPC_MYTHIC_STORMING        = 190026,
    NPC_MYTHIC_INFESTED        = 190027,
    NPC_MYTHIC_REAPING         = 190028,
    NPC_MYTHIC_BEGUILING_VOID  = 190029,
    NPC_MYTHIC_BEGUILING_ENCH  = 190030,
    NPC_MYTHIC_BEGUILING_EMIS  = 190031,
    NPC_MYTHIC_AWAKENED        = 190032,
    NPC_MYTHIC_VOLCANIC        = 190033,
    ITEM_MYTHIC_KEYSTONE       = 190022,
    ITEM_MYTHIC_RESIDUUM       = 190034,
    GO_MYTHIC_FONT             = 254620,
    NPC_TEXT_MYTHIC_BROKER     = 190020,
    GOSSIP_MENU_MYTHIC_BROKER  = 190020
};

enum MythicRequestAction : uint8
{
    MYTHIC_REQ_START     = 1,
    MYTHIC_REQ_VAULT     = 2,
    MYTHIC_REQ_TELEPORT  = 3,
    MYTHIC_REQ_CLAIM_KEY = 4
};

struct MythicDungeonDef
{
    uint8 Id;
    uint32 MapId;
    uint32 TimerMs;
    char const* NameEn;
    char const* NameEs;
    char const* Short;
    float X;
    float Y;
    float Z;
    float O;
};

struct MythicKeystone
{
    uint8 DungeonId = 0;
    uint8 Level = 0;
    bool Depleted = false;
};

struct MythicAffixSet
{
    uint8 FortTyr = AFFIX_NONE;
    uint8 Plus4 = AFFIX_NONE;
    uint8 Plus7 = AFFIX_NONE;
    uint8 Seasonal = AFFIX_NONE;
};

struct MythicSanguinePool
{
    Position Pos;
    uint32 ExpireMs = 0;
};

struct MythicBurstingState
{
    uint8 Stacks = 0;
    uint32 ExpireMs = 0;
};

struct MythicNecroticState
{
    uint8 Stacks = 0;
    uint32 LastHitMs = 0;
};

struct MythicTimedGuid
{
    ObjectGuid Guid;
    uint32 ExpireMs = 0;
};

struct MythicRun
{
    uint32 InstanceId = 0;
    uint32 MapId = 0;
    uint8 DungeonId = 0;
    uint8 Level = 0;
    MythicAffixSet Affixes;
    ObjectGuid LeaderGuid;
    std::vector<ObjectGuid> Members;

    uint32 TimeLimitMs = 0;
    uint32 ElapsedMs = 0;
    uint32 Deaths = 0;
    uint32 Forces = 0;
    uint32 ForcesRequired = 0;
    uint32 BossesKilled = 0;
    uint32 BossesRequired = 0;

    bool Active = false;
    bool Completed = false;
    bool Failed = false;
    bool Timed = false;
    uint8 Upgrade = 0;
    uint32 EmptyMs = 0;
    uint32 AnnounceMs = 0;
    uint32 LiveWriteMs = 0;

    std::unordered_set<ObjectGuid> ScaledCreatures;
    std::unordered_set<ObjectGuid> CountedCreatures;
    std::unordered_set<ObjectGuid> DeadBosses;
    std::unordered_set<ObjectGuid> Inspiring;
    std::unordered_set<ObjectGuid> Infested;
    std::unordered_set<ObjectGuid> Raging;
    std::unordered_map<ObjectGuid, uint8> BolsterStacks;
    std::vector<MythicSanguinePool> Pools;
    std::unordered_map<ObjectGuid, MythicBurstingState> Bursting;
    std::unordered_map<ObjectGuid, MythicNecroticState> Necrotic;
    std::unordered_map<ObjectGuid, uint8> Grievous;
    std::vector<MythicTimedGuid> Explosives;
    std::vector<MythicTimedGuid> Storms;
    std::vector<MythicTimedGuid> Volcanoes;

    uint32 QuakingMs = 20000;
    uint32 VolcanicMs = 10000;
    uint32 ExplosiveMs = 8000;
    uint32 StormingMs = 15000;
    uint32 AffixDotMs = 0;
    uint32 ReapingKills = 0;

    [[nodiscard]] bool HasAffix(uint8 affix) const
    {
        return Affixes.FortTyr == affix || Affixes.Plus4 == affix
            || Affixes.Plus7 == affix || Affixes.Seasonal == affix;
    }
};

struct MythicProfile
{
    MythicKeystone Key;
    float OverallScore = 0.f;
    uint8 WeekBestLevel = 0;
    uint8 WeekBestDungeon = 0;
    uint8 WeekRuns = 0;
    uint8 VaultClaimed = 0;
    std::array<uint8, 3> WeekKeys{};
    uint32 SeasonId = 1;
};

struct MythicDungeonBest
{
    uint8 FortLevel = 0;
    float FortScore = 0.f;
    uint8 TyrLevel = 0;
    float TyrScore = 0.f;
};

struct MythicLeaderboardRow
{
    std::string Name;
    float Score = 0.f;
    uint8 WeekBest = 0;
    uint32 Runs = 0;
};

uint32 const MYTHIC_DUNGEON_COUNT = 16;

MythicDungeonDef const MythicDungeons[MYTHIC_DUNGEON_COUNT] =
{
    { 1, 574, 1980000, "Utgarde Keep", "Fortaleza de Utgarde", "uk",
      153.789f, -86.548f, 12.551f, 0.304f },
    { 2, 576, 1980000, "The Nexus", "El Nexo", "nx",
      145.870f, -10.554f, -16.636f, 1.528f },
    { 3, 601, 1620000, "Azjol-Nerub", "Azjol-Nerub", "an",
      413.314f, 795.968f, 831.351f, 5.500f },
    { 4, 619, 2100000, "Ahn'kahet", "Ahn'kahet", "ak",
      333.351f, -1109.940f, 69.772f, 0.553f },
    { 5, 600, 1980000, "Drak'Tharon Keep", "Fortaleza de Drak'Tharon", "dtk",
      -517.343f, -487.976f, 11.010f, 4.831f },
    { 6, 604, 2100000, "Gundrak", "Gundrak", "gd",
      1891.840f, 832.169f, 176.669f, 2.109f },
    { 7, 602, 2100000, "Halls of Lightning", "Camaras de Relampagos", "hol",
      1331.470f, 259.619f, 53.398f, 4.772f },
    { 8, 575, 1980000, "Utgarde Pinnacle", "Pinaculo de Utgarde", "up",
      584.117f, -327.974f, 110.138f, 3.122f },
    { 9, 608, 1800000, "The Violet Hold", "El Bastion Violeta", "vh",
      1808.820f, 803.930f, 44.364f, 6.282f },
    { 10, 599, 2100000, "Halls of Stone", "Camaras de Piedra", "hos",
      1153.240f, 806.164f, 195.937f, 4.715f },
    { 11, 578, 1980000, "The Oculus", "El Oculus", "occ",
      1055.930f, 986.850f, 361.070f, 5.745f },
    { 12, 595, 2280000, "Culling of Stratholme", "La Matanza de Stratholme", "cos",
      1431.100f, 556.920f, 36.690f, 5.160f },
    { 13, 650, 1500000, "Trial of the Champion", "Prueba del Campeon", "toc",
      805.227f, 618.038f, 412.393f, 3.146f },
    { 14, 632, 1620000, "The Forge of Souls", "La Forja de Almas", "fos",
      4922.860f, 2175.630f, 638.734f, 2.004f },
    { 15, 658, 1980000, "Pit of Saron", "Foso de Saron", "pos",
      435.743f, 212.413f, 528.709f, 6.256f },
    { 16, 668, 1620000, "Halls of Reflection", "Camaras de Reflexion", "hor",
      5239.010f, 1932.640f, 707.695f, 0.801f }
};

[[nodiscard]] inline MythicDungeonDef const* FindMythicDungeon(uint8 id)
{
    if (!id || id > MYTHIC_DUNGEON_COUNT)
        return nullptr;
    return &MythicDungeons[id - 1];
}

[[nodiscard]] inline MythicDungeonDef const* FindMythicDungeonByMap(uint32 mapId)
{
    for (uint32 i = 0; i < MYTHIC_DUNGEON_COUNT; ++i)
        if (MythicDungeons[i].MapId == mapId)
            return &MythicDungeons[i];
    return nullptr;
}

[[nodiscard]] inline bool IsMythicAffixNpc(uint32 entry)
{
    return entry >= NPC_MYTHIC_EXPLOSIVE && entry <= NPC_MYTHIC_VOLCANIC;
}

#endif
