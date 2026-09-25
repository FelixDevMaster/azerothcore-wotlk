/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ArenaDampening.h"
#include "AllBattlegroundScript.h"
#include "ArenaScript.h"
#include "Battleground.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "UnitScript.h"
#include "WorldScript.h"

using namespace Acore::ChatCommands;

class ArenaDampeningWorldScript : public WorldScript
{
public:
    ArenaDampeningWorldScript() : WorldScript("ArenaDampeningWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        sArenaDampening->LoadConfig(reload);
    }
};

class ArenaDampeningArenaScript : public ArenaScript
{
public:
    ArenaDampeningArenaScript() : ArenaScript("ArenaDampeningArenaScript", {
        ARENAHOOK_ON_ARENA_START
    }) { }

    void OnArenaStart(Battleground* bg) override
    {
        sArenaDampening->HandleArenaStart(bg);
    }
};

class ArenaDampeningBattlegroundScript : public AllBattlegroundScript
{
public:
    ArenaDampeningBattlegroundScript() : AllBattlegroundScript("ArenaDampeningBattlegroundScript", {
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_UPDATE,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_ADD_PLAYER,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_END,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY
    }) { }

    void OnBattlegroundUpdate(Battleground* bg, uint32 diff) override
    {
        sArenaDampening->HandleBattlegroundUpdate(bg, diff);
    }

    void OnBattlegroundAddPlayer(Battleground* bg, Player* player) override
    {
        sArenaDampening->HandleAddPlayer(bg, player);
    }

    void OnBattlegroundEnd(Battleground* bg, TeamId /*winnerTeam*/) override
    {
        sArenaDampening->HandleBattlegroundEnd(bg);
    }

    void OnBattlegroundDestroy(Battleground* bg) override
    {
        sArenaDampening->HandleBattlegroundDestroy(bg);
    }
};

class ArenaDampeningUnitScript : public UnitScript
{
public:
    ArenaDampeningUnitScript() : UnitScript("ArenaDampeningUnitScript", true, {
        UNITHOOK_ON_AURA_APPLY
    }) { }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        sArenaDampening->HandleAbsorbApply(unit, aura);
    }
};

class arena_dampening_commandscript : public CommandScript
{
public:
    arena_dampening_commandscript() : CommandScript("arena_dampening_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable dampeningTable =
        {
            { "status", HandleStatus, SEC_PLAYER, Console::No },
            { "", HandleStatus, SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "dampening", dampeningTable }
        };

        return commandTable;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        bool const spanish = sArenaDampening->IsSpanish(player);
        if (!sArenaDampening->IsModuleEnabled())
        {
            handler->PSendSysMessage(spanish ? "Dampening esta desactivado." : "Dampening is disabled.");
            return true;
        }

        Battleground* bg = player->GetBattleground();
        if (!bg || !bg->isArena())
        {
            handler->PSendSysMessage(spanish
                ? "No estas en una arena."
                : "You are not in an arena.");
            return true;
        }

        uint32 const percent = sArenaDampening->GetPercent(bg);
        uint32 const elapsed = sArenaDampening->GetElapsedMs(bg) / IN_MILLISECONDS;
        uint32 const startAt = sArenaDampening->GetStartDelayMs(bg->GetArenaType()) / IN_MILLISECONDS;

        if (!percent)
        {
            uint32 const remaining = elapsed >= startAt ? 0 : startAt - elapsed;
            handler->PSendSysMessage(spanish
                ? "Dampening aun no ha comenzado. Empieza en {}s (transcurridos {}s)."
                : "Dampening has not started yet. Begins in {}s (elapsed {}s).",
                remaining, elapsed);
            return true;
        }

        handler->PSendSysMessage(spanish
            ? "Dampening activo: {}%. Tiempo de combate: {}s."
            : "Dampening active: {}%. Match time: {}s.",
            percent, elapsed);
        return true;
    }
};

void AddSC_arena_dampening()
{
    new ArenaDampeningWorldScript();
    new ArenaDampeningArenaScript();
    new ArenaDampeningBattlegroundScript();
    new ArenaDampeningUnitScript();
    new arena_dampening_commandscript();
}
