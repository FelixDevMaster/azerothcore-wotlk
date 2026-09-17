/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MODULE_ARENA_BG_QOL_H
#define MODULE_ARENA_BG_QOL_H

#include "Common.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <unordered_set>

class Battleground;
class Player;
class WorldPacket;
class WorldSession;

enum RitualSpellIds : uint32
{
    SPELL_RITUAL_OF_REFRESHMENT_R1 = 43987,
    SPELL_RITUAL_OF_REFRESHMENT_R2 = 58659,
    SPELL_RITUAL_OF_SOULS_R1       = 29893,
    SPELL_RITUAL_OF_SOULS_R2       = 58887
};

enum RitualGameObjectIds : uint32
{
    GO_REFRESHMENT_TABLE_R1 = 186812,
    GO_REFRESHMENT_TABLE_R2 = 193061,
    GO_SOULWELL_R1          = 181621,
    GO_SOULWELL_R2          = 193169
};

class ArenaBgQol
{
public:
    static ArenaBgQol* instance();

    void LoadConfig(bool reload);

    [[nodiscard]] bool IsEnabled() const { return _enabled; }

    void HandleAddPlayer(Battleground* bg, Player* player);
    void HandleRemovePlayer(Battleground* bg, Player* player);
    void HandleUpdate(Battleground* bg, uint32 diff);
    void HandleDestroy(Battleground* bg);
    bool HandleReadyCheckPacket(WorldSession* session, WorldPacket const& packet);

private:
    enum ReadyCheckStatus : uint8
    {
        READY_CHECK_NONE      = 0,
        READY_CHECK_WAITING   = 1,
        READY_CHECK_FINISHED  = 2
    };

    struct ReadyCheckState
    {
        ReadyCheckStatus Status = READY_CHECK_NONE;
        uint32 ElapsedMs = 0;
        std::unordered_set<ObjectGuid> Pending;
        std::unordered_set<ObjectGuid> PendingRituals;
        std::unordered_set<ObjectGuid> SpawnedRituals;
    };

    [[nodiscard]] static uint64 MakeKey(Battleground const* bg);
    [[nodiscard]] static bool IsSpanish(Player const* player);
    [[nodiscard]] static char const* Msg(Player const* player, char const* spanish, char const* english);

    ReadyCheckState& GetState(Battleground const* bg);
    void SpawnRitual(Player* player);
    void TrySpawnRitual(Battleground* bg, Player* player);
    void SpawnPendingRituals(Battleground* bg, ReadyCheckState& state);
    void SendReadyCheck(Battleground* bg);
    void FinishReadyCheck(Battleground* bg, ReadyCheckState& state, bool skipWait);
    void HandleReadyAnswer(Battleground* bg, Player* player, bool ready);
    void Announce(Battleground* bg, char const* spanish, char const* english);
    void SendReadyCheckFinished(Battleground* bg);

    bool _enabled = true;
    bool _ritualsEnabled = true;
    bool _ritualsArenas = true;
    bool _ritualsBattlegrounds = true;
    uint32 _ritualDuration = 180;
    bool _readyCheckEnabled = true;
    int32 _readyCheckTriggerMs = 30 * IN_MILLISECONDS;
    int32 _readyCheckSkipToMs = 15 * IN_MILLISECONDS;
    uint32 _readyCheckTimeoutMs = 0;

    std::unordered_map<uint64, ReadyCheckState> _states;
};

#define sArenaBgQol ArenaBgQol::instance()

#endif
