/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "RBGMgr.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Creature.h"
#include "Group.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "StringFormat.h"

using namespace Acore::ChatCommands;

namespace
{
void SendStatsTo(ChatHandler* handler, Player* player)
{
    RBGPlayerStats stats = sRBGMgr->GetStats(player->GetGUID());
    uint32 losses = stats.Games > stats.Wins ? stats.Games - stats.Wins : 0;
    handler->PSendSysMessage("Rated BG — {}: {} rating, {} MMR, {}-{} (week {}-{}, conquest {}/{}).",
        player->GetName(), stats.Rating, stats.MMR, stats.Wins, losses,
        stats.WeekWins, stats.WeekGames > stats.WeekWins ? stats.WeekGames - stats.WeekWins : 0,
        stats.WeekConquest, sRBGMgr->GetWeeklyCap());
}
}

class RBGWorldScript : public WorldScript
{
public:
    RBGWorldScript() : WorldScript("RBGWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE,
        WORLDHOOK_ON_UPDATE
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        sRBGMgr->LoadConfig(reload);
    }

    void OnLoadCustomDatabaseTable() override
    {
        sRBGMgr->EnsureDatabase();
    }

    void OnUpdate(uint32 diff) override
    {
        sRBGMgr->Update(diff);
    }
};

class RBGBattlegroundScript : public AllBattlegroundScript
{
public:
    RBGBattlegroundScript() : AllBattlegroundScript("RBGBattlegroundScript", {
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_END,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY
    }) { }

    void OnBattlegroundEnd(Battleground* bg, TeamId winner) override
    {
        sRBGMgr->HandleBattlegroundEnd(bg, winner);
    }

    void OnBattlegroundDestroy(Battleground* bg) override
    {
        sRBGMgr->HandleBattlegroundDestroy(bg);
    }
};

class RBGPlayerScript : public PlayerScript
{
public:
    RBGPlayerScript() : PlayerScript("RBGPlayerScript", {
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_CAN_JOIN_IN_BATTLEGROUND_QUEUE,
        PLAYERHOOK_CAN_JOIN_IN_ARENA_QUEUE
    }) { }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
            sRBGMgr->RemovePlayer(player->GetGUID());
    }

    bool OnPlayerCanJoinInBattlegroundQueue(Player* player, ObjectGuid /*battlemaster*/,
        BattlegroundTypeId /*bgTypeId*/, uint8 /*joinAsGroup*/, GroupJoinBattlegroundResult& err) override
    {
        if (player && sRBGMgr->IsQueued(player->GetGUID()))
        {
            err = ERR_BATTLEGROUND_QUEUED_FOR_RATED;
            return false;
        }

        return true;
    }

    bool OnPlayerCanJoinInArenaQueue(Player* player, ObjectGuid /*battlemaster*/, uint8 /*slot*/,
        BattlegroundTypeId /*bgTypeId*/, uint8 /*joinAsGroup*/, uint8 /*isRated*/,
        GroupJoinBattlegroundResult& err) override
    {
        if (player && sRBGMgr->IsQueued(player->GetGUID()))
        {
            err = ERR_BATTLEGROUND_QUEUED_FOR_RATED;
            return false;
        }

        return true;
    }
};

class RBGGroupScript : public GroupScript
{
public:
    RBGGroupScript() : GroupScript("RBGGroupScript", {
        GROUPHOOK_ON_REMOVE_MEMBER,
        GROUPHOOK_ON_CHANGE_LEADER,
        GROUPHOOK_ON_DISBAND
    }) { }

    void OnRemoveMember(Group* /*group*/, ObjectGuid guid, RemoveMethod /*method*/,
        ObjectGuid /*kicker*/, char const* /*reason*/) override
    {
        sRBGMgr->RemovePlayer(guid);
    }

    void OnChangeLeader(Group* /*group*/, ObjectGuid /*newLeader*/, ObjectGuid oldLeader) override
    {
        sRBGMgr->RemovePlayer(oldLeader);
    }

    void OnDisband(Group* group) override
    {
        if (!group)
            return;

        group->DoForAllMembers([](Player* member)
        {
            sRBGMgr->RemovePlayer(member->GetGUID());
        });
    }
};

class npc_rbg_battlemaster : public CreatureScript
{
public:
    npc_rbg_battlemaster() : CreatureScript("npc_rbg_battlemaster") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        RBGPlayerStats stats = sRBGMgr->GetStats(player->GetGUID());
        uint32 losses = stats.Games > stats.Wins ? stats.Games - stats.Wins : 0;
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            Acore::StringFormat("Rating: {}  MMR: {}  Record: {}-{}",
                stats.Rating, stats.MMR, stats.Wins, losses),
            GOSSIP_SENDER_MAIN, 0);

        if (sRBGMgr->IsQueued(player->GetGUID()))
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Leave Rated BG queue", GOSSIP_SENDER_MAIN, 2);
        else
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Queue for Rated Battlegrounds", GOSSIP_SENDER_MAIN, 1);

        AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Show leaderboard", GOSSIP_SENDER_MAIN, 3);
        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);

        if (action == 1 || action == 2)
        {
            std::string error;
            bool ok = (action == 1) ? sRBGMgr->Queue(player, error) : sRBGMgr->Dequeue(player, error);
            if (!ok)
                ChatHandler(player->GetSession()).SendSysMessage(error);
            CloseGossipMenuFor(player);
            return true;
        }

        if (action == 3)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "=== Rated BG Leaderboard ===", GOSSIP_SENDER_MAIN, 0);
            uint32 rank = 1;
            for (RBGLeaderboardRow const& row : sRBGMgr->GetLeaderboard(10))
            {
                AddGossipItemFor(player, GOSSIP_ICON_TABARD,
                    Acore::StringFormat("{}. {} — {} ({}-{})", rank, row.Name, row.Rating, row.Wins, row.Losses),
                    GOSSIP_SENDER_MAIN, 0);
                ++rank;
            }
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back", GOSSIP_SENDER_MAIN, 4);
            SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
            return true;
        }

        return OnGossipHello(player, creature);
    }
};

class rbg_commandscript : public CommandScript
{
public:
    rbg_commandscript() : CommandScript("rbg_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable rbgTable =
        {
            { "queue", HandleQueue, SEC_PLAYER, Console::No },
            { "leave", HandleLeave, SEC_PLAYER, Console::No },
            { "status", HandleStatus, SEC_PLAYER, Console::No },
            { "top", HandleTop, SEC_PLAYER, Console::No },
            { "", HandleStatus, SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "rbg", rbgTable }
        };

        return commandTable;
    }

    static bool HandleQueue(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        std::string error;
        if (!sRBGMgr->Queue(player, error))
        {
            handler->SendSysMessage(error);
            handler->SetSentErrorMessage(true);
            return false;
        }

        return true;
    }

    static bool HandleLeave(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        std::string error;
        if (!sRBGMgr->Dequeue(player, error))
        {
            handler->SendSysMessage(error);
            handler->SetSentErrorMessage(true);
            return false;
        }

        return true;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        SendStatsTo(handler, player);
        if (sRBGMgr->IsQueued(player->GetGUID()))
            handler->PSendSysMessage("You are in the Rated BG queue ({} team(s) waiting).",
                sRBGMgr->GetQueuedTeamCount());
        else
            handler->SendSysMessage("You are not queued. Form a raid and use .rbg queue");

        return true;
    }

    static bool HandleTop(ChatHandler* handler)
    {
        handler->SendSysMessage("Rated Battleground leaderboard:");
        uint32 rank = 1;
        for (RBGLeaderboardRow const& row : sRBGMgr->GetLeaderboard(15))
        {
            handler->PSendSysMessage("{}. {} — {} ({}-{})", rank, row.Name, row.Rating, row.Wins, row.Losses);
            ++rank;
        }

        if (rank == 1)
            handler->SendSysMessage("No games have been played yet.");

        return true;
    }
};

void Addmod_rbg_aioScripts()
{
    new RBGWorldScript();
    new RBGBattlegroundScript();
    new RBGPlayerScript();
    new RBGGroupScript();
    new npc_rbg_battlemaster();
    new rbg_commandscript();
}
