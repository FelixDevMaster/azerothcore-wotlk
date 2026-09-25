/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MODULE_ARENA_DAMPENING_H
#define MODULE_ARENA_DAMPENING_H

#include "Common.h"
#include <unordered_map>

class Aura;
class Battleground;
class Player;
class SpellInfo;
class Unit;

enum ArenaDampeningSpell : uint32
{
    SPELL_ARENA_DAMPENING = 74410
};

enum ArenaDampeningAbsorbSpell : uint32
{
    SPELL_SACRED_SHIELD_ABSORB = 58597
};

struct ArenaDampeningMatch
{
    uint32 ElapsedMs = 0;
    uint32 Percent = 0;
    uint32 TickTimerMs = 0;
    bool Started = false;
};

class ArenaDampening
{
public:
    static ArenaDampening* instance();

    void LoadConfig(bool reload);

    void HandleArenaStart(Battleground* bg);
    void HandleBattlegroundUpdate(Battleground* bg, uint32 diff);
    void HandleAddPlayer(Battleground* bg, Player* player);
    void HandleBattlegroundEnd(Battleground* bg);
    void HandleBattlegroundDestroy(Battleground* bg);
    void HandleAbsorbApply(Unit* unit, Aura* aura);

    [[nodiscard]] bool IsModuleEnabled() const { return _enabled; }
    [[nodiscard]] uint32 GetPercent(Battleground const* bg) const;
    [[nodiscard]] uint32 GetPercent(Unit const* unit) const;
    [[nodiscard]] uint32 GetElapsedMs(Battleground const* bg) const;
    [[nodiscard]] uint32 GetStartDelayMs(uint8 arenaType) const;
    [[nodiscard]] bool IsSpanish(Player const* player) const;

private:
    ArenaDampening() = default;

    [[nodiscard]] bool IsMatchTracked(Battleground const* bg) const;
    ArenaDampeningMatch* GetMatch(uint32 instanceId);
    ArenaDampeningMatch const* GetMatch(uint32 instanceId) const;
    void StartDampening(Battleground* bg, ArenaDampeningMatch& match);
    void IncrementDampening(Battleground* bg, ArenaDampeningMatch& match);
    void ApplyToBattleground(Battleground* bg, uint32 percent);
    void ApplyToPlayer(Player* player, uint32 percent);
    void ApplyToUnit(Unit* unit, uint32 percent);
    void Announce(Battleground* bg, uint32 percent, bool started) const;
    [[nodiscard]] static bool IsCoreDampenedAbsorb(SpellInfo const* info);

    bool _enabled = true;
    bool _ratedOnly = false;
    bool _applyToPets = true;
    bool _overrideWotlkAura = true;
    bool _announce = true;
    uint32 _announceEveryPercent = 10;
    uint32 _startPercent = 1;
    uint32 _incrementPercent = 1;
    uint32 _maxPercent = 100;
    uint32 _tickMs = 10 * IN_MILLISECONDS;
    uint32 _startDelay2v2Ms = 5 * MINUTE * IN_MILLISECONDS;
    uint32 _startDelay3v3Ms = 5 * MINUTE * IN_MILLISECONDS;
    uint32 _startDelay5v5Ms = 5 * MINUTE * IN_MILLISECONDS;

    std::unordered_map<uint32, ArenaDampeningMatch> _matches;
};

#define sArenaDampening ArenaDampening::instance()

#endif
