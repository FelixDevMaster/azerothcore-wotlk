/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ArenaBgQol.h"
#include "AllBattlegroundScript.h"
#include "ScriptMgr.h"
#include "ServerScript.h"
#include "WorldScript.h"

class ArenaBgQolWorldScript : public WorldScript
{
public:
    ArenaBgQolWorldScript() : WorldScript("ArenaBgQolWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        sArenaBgQol->LoadConfig(reload);
    }
};

class ArenaBgQolBattlegroundScript : public AllBattlegroundScript
{
public:
    ArenaBgQolBattlegroundScript() : AllBattlegroundScript("ArenaBgQolBattlegroundScript", {
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_ADD_PLAYER,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_REMOVE_PLAYER_AT_LEAVE,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_UPDATE,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY
    }) { }

    void OnBattlegroundAddPlayer(Battleground* bg, Player* player) override
    {
        sArenaBgQol->HandleAddPlayer(bg, player);
    }

    void OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player) override
    {
        sArenaBgQol->HandleRemovePlayer(bg, player);
    }

    void OnBattlegroundUpdate(Battleground* bg, uint32 diff) override
    {
        sArenaBgQol->HandleUpdate(bg, diff);
    }

    void OnBattlegroundDestroy(Battleground* bg) override
    {
        sArenaBgQol->HandleDestroy(bg);
    }
};

class ArenaBgQolServerScript : public ServerScript
{
public:
    ArenaBgQolServerScript() : ServerScript("ArenaBgQolServerScript", {
        SERVERHOOK_CAN_PACKET_RECEIVE
    }) { }

    bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
    {
        return sArenaBgQol->HandleReadyCheckPacket(session, packet);
    }
};

void AddSC_arena_bg_qol()
{
    new ArenaBgQolWorldScript();
    new ArenaBgQolBattlegroundScript();
    new ArenaBgQolServerScript();
}
