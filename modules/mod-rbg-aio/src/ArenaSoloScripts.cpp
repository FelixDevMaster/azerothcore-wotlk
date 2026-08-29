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

#include "ArenaSoloMgr.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Creature.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "StringFormat.h"

using namespace Acore::ChatCommands;

enum ArenaSoloGossipAction
{
    GOSSIP_ACTION_ARENA_SOLO_MAIN        = 0,
    GOSSIP_ACTION_ARENA_SOLO_QUEUE_1V1   = 1,
    GOSSIP_ACTION_ARENA_SOLO_QUEUE_3V3   = 2,
    GOSSIP_ACTION_ARENA_SOLO_LEAVE       = 3,
    GOSSIP_ACTION_ARENA_SOLO_BOARD_1V1   = 4,
    GOSSIP_ACTION_ARENA_SOLO_BOARD_3V3   = 5,
    GOSSIP_ACTION_ARENA_SOLO_BACK        = 6
};

namespace
{
void SendBracketStats(ChatHandler* handler, Player* player, uint8 bracket)
{
    ArenaSoloStats stats = sArenaSoloMgr->GetStats(player->GetGUID(), bracket);
    ArenaSoloBracketConfig const& config = sArenaSoloMgr->GetBracketConfig(bracket);
    uint32 losses = stats.Games > stats.Wins ? stats.Games - stats.Wins : 0;
    uint32 weekLosses = stats.WeekGames > stats.WeekWins ? stats.WeekGames - stats.WeekWins : 0;

    handler->PSendSysMessage("{} — {} rating, {} MMR, {}-{} (week {}-{}, points {}/{}).",
        ArenaSoloMgr::GetBracketName(bracket), stats.Rating, stats.MMR, stats.Wins, losses,
        stats.WeekWins, weekLosses, stats.WeekPoints, config.WeeklyCap);
}

bool HandleQueueCommand(ChatHandler* handler, uint8 bracket)
{
    Player* player = handler->GetPlayer();
    if (!player)
        return false;

    std::string error;
    if (!sArenaSoloMgr->Queue(player, bracket, error))
    {
        handler->SendSysMessage(error);
        handler->SetSentErrorMessage(true);
        return false;
    }

    return true;
}

void SendLeaderboard(ChatHandler* handler, uint8 bracket)
{
    handler->PSendSysMessage("{} leaderboard:", ArenaSoloMgr::GetBracketName(bracket));

    uint32 rank = 1;
    for (PvPLeaderboardRow const& row : sArenaSoloMgr->GetLeaderboard(bracket, 15))
    {
        handler->PSendSysMessage("{}. {} — {} ({}-{})", rank, row.Name, row.Rating, row.Wins, row.Losses);
        ++rank;
    }

    if (rank == 1)
        handler->SendSysMessage("No games have been played yet.");
}
}

class ArenaSoloWorldScript : public WorldScript
{
public:
    ArenaSoloWorldScript() : WorldScript("ArenaSoloWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE,
        WORLDHOOK_ON_UPDATE
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        sArenaSoloMgr->LoadConfig(reload);
    }

    void OnLoadCustomDatabaseTable() override
    {
        sArenaSoloMgr->EnsureDatabase();
    }

    void OnUpdate(uint32 diff) override
    {
        sArenaSoloMgr->Update(diff);
    }
};

class ArenaSoloBattlegroundScript : public AllBattlegroundScript
{
public:
    ArenaSoloBattlegroundScript() : AllBattlegroundScript("ArenaSoloBattlegroundScript", {
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_END,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY
    }) { }

    void OnBattlegroundEnd(Battleground* bg, TeamId winner) override
    {
        sArenaSoloMgr->HandleBattlegroundEnd(bg, winner);
    }

    void OnBattlegroundDestroy(Battleground* bg) override
    {
        sArenaSoloMgr->HandleBattlegroundDestroy(bg);
    }
};

class ArenaSoloPlayerScript : public PlayerScript
{
public:
    ArenaSoloPlayerScript() : PlayerScript("ArenaSoloPlayerScript", {
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_CAN_JOIN_IN_BATTLEGROUND_QUEUE,
        PLAYERHOOK_CAN_JOIN_IN_ARENA_QUEUE
    }) { }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
            sArenaSoloMgr->RemovePlayer(player->GetGUID());
    }

    bool OnPlayerCanJoinInBattlegroundQueue(Player* player, ObjectGuid /*battlemaster*/,
        BattlegroundTypeId /*bgTypeId*/, uint8 /*joinAsGroup*/, GroupJoinBattlegroundResult& err) override
    {
        if (player && sArenaSoloMgr->IsQueued(player->GetGUID()))
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
        if (player && sArenaSoloMgr->IsQueued(player->GetGUID()))
        {
            err = ERR_BATTLEGROUND_QUEUED_FOR_RATED;
            return false;
        }

        return true;
    }
};

class ArenaSoloGroupScript : public GroupScript
{
public:
    ArenaSoloGroupScript() : GroupScript("ArenaSoloGroupScript", {
        GROUPHOOK_ON_ADD_MEMBER
    }) { }

    // Solo queue means solo: joining a group drops you from the queue.
    void OnAddMember(Group* /*group*/, ObjectGuid guid) override
    {
        sArenaSoloMgr->RemovePlayer(guid);
    }
};

class npc_arena_solo_master : public CreatureScript
{
public:
    npc_arena_solo_master() : CreatureScript("npc_arena_solo_master") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        for (uint8 bracket = 0; bracket < ARENA_SOLO_BRACKET_MAX; ++bracket)
        {
            if (!sArenaSoloMgr->IsBracketEnabled(bracket))
                continue;

            ArenaSoloStats stats = sArenaSoloMgr->GetStats(player->GetGUID(), bracket);
            uint32 losses = stats.Games > stats.Wins ? stats.Games - stats.Wins : 0;
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                Acore::StringFormat("{}: {} rating, {}-{}",
                    ArenaSoloMgr::GetBracketName(bracket), stats.Rating, stats.Wins, losses),
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_MAIN);
        }

        if (sArenaSoloMgr->IsQueued(player->GetGUID()))
        {
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Leave the solo queue",
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_LEAVE);
        }
        else
        {
            if (sArenaSoloMgr->IsBracketEnabled(ARENA_SOLO_BRACKET_1V1))
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
                    Acore::StringFormat("Queue 1v1 ({} waiting)",
                        sArenaSoloMgr->GetQueuedCount(ARENA_SOLO_BRACKET_1V1)),
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_QUEUE_1V1);

            if (sArenaSoloMgr->IsBracketEnabled(ARENA_SOLO_BRACKET_3V3))
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
                    Acore::StringFormat("Queue 3v3 SoloQ ({} waiting)",
                        sArenaSoloMgr->GetQueuedCount(ARENA_SOLO_BRACKET_3V3)),
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_QUEUE_3V3);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TABARD, "1v1 leaderboard",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_BOARD_1V1);
        AddGossipItemFor(player, GOSSIP_ICON_TABARD, "3v3 SoloQ leaderboard",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_BOARD_3V3);

        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);
        std::string error;

        switch (action)
        {
            case GOSSIP_ACTION_ARENA_SOLO_QUEUE_1V1:
                if (!sArenaSoloMgr->Queue(player, ARENA_SOLO_BRACKET_1V1, error))
                    ChatHandler(player->GetSession()).SendSysMessage(error);
                CloseGossipMenuFor(player);
                return true;
            case GOSSIP_ACTION_ARENA_SOLO_QUEUE_3V3:
                if (!sArenaSoloMgr->Queue(player, ARENA_SOLO_BRACKET_3V3, error))
                    ChatHandler(player->GetSession()).SendSysMessage(error);
                CloseGossipMenuFor(player);
                return true;
            case GOSSIP_ACTION_ARENA_SOLO_LEAVE:
                if (!sArenaSoloMgr->Dequeue(player, error))
                    ChatHandler(player->GetSession()).SendSysMessage(error);
                CloseGossipMenuFor(player);
                return true;
            case GOSSIP_ACTION_ARENA_SOLO_BOARD_1V1:
            case GOSSIP_ACTION_ARENA_SOLO_BOARD_3V3:
            {
                uint8 bracket = action == GOSSIP_ACTION_ARENA_SOLO_BOARD_1V1
                    ? ARENA_SOLO_BRACKET_1V1 : ARENA_SOLO_BRACKET_3V3;

                AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                    Acore::StringFormat("=== {} ===", ArenaSoloMgr::GetBracketName(bracket)),
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_MAIN);

                uint32 rank = 1;
                for (PvPLeaderboardRow const& row : sArenaSoloMgr->GetLeaderboard(bracket, 10))
                {
                    AddGossipItemFor(player, GOSSIP_ICON_TABARD,
                        Acore::StringFormat("{}. {} — {} ({}-{})",
                            rank, row.Name, row.Rating, row.Wins, row.Losses),
                        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_MAIN);
                    ++rank;
                }

                if (rank == 1)
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "No games played yet.",
                        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_MAIN);

                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back",
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_BACK);
                SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
                return true;
            }
            default:
                break;
        }

        return OnGossipHello(player, creature);
    }
};

class arena_solo_commandscript : public CommandScript
{
public:
    arena_solo_commandscript() : CommandScript("arena_solo_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable soloTable =
        {
            { "1v1",    HandleQueue1v1,   SEC_PLAYER, Console::No },
            { "3v3",    HandleQueue3v3,   SEC_PLAYER, Console::No },
            { "leave",  HandleLeave,      SEC_PLAYER, Console::No },
            { "status", HandleStatus,     SEC_PLAYER, Console::No },
            { "top",    HandleTop,        SEC_PLAYER, Console::No },
            { "",       HandleStatus,     SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "solo", soloTable }
        };

        return commandTable;
    }

    static bool HandleQueue1v1(ChatHandler* handler)
    {
        return HandleQueueCommand(handler, ARENA_SOLO_BRACKET_1V1);
    }

    static bool HandleQueue3v3(ChatHandler* handler)
    {
        return HandleQueueCommand(handler, ARENA_SOLO_BRACKET_3V3);
    }

    static bool HandleLeave(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        std::string error;
        if (!sArenaSoloMgr->Dequeue(player, error))
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

        for (uint8 bracket = 0; bracket < ARENA_SOLO_BRACKET_MAX; ++bracket)
            if (sArenaSoloMgr->IsBracketEnabled(bracket))
                SendBracketStats(handler, player, bracket);

        if (Optional<uint8> queued = sArenaSoloMgr->GetQueuedBracket(player->GetGUID()))
            handler->PSendSysMessage("You are queued for {} ({} player(s) waiting).",
                ArenaSoloMgr::GetBracketName(*queued), sArenaSoloMgr->GetQueuedCount(*queued));
        else
            handler->SendSysMessage("Not queued. Use .solo 1v1 or .solo 3v3 (you must be ungrouped).");

        return true;
    }

    static bool HandleTop(ChatHandler* handler, Optional<std::string> bracketArg)
    {
        uint8 bracket = ARENA_SOLO_BRACKET_1V1;
        if (bracketArg)
        {
            Optional<uint8> parsed = ArenaSoloMgr::ParseBracket(*bracketArg);
            if (!parsed)
            {
                handler->SendSysMessage("Usage: .solo top [1v1|3v3]");
                handler->SetSentErrorMessage(true);
                return false;
            }

            bracket = *parsed;
        }

        SendLeaderboard(handler, bracket);
        return true;
    }
};

void AddSC_arena_solo_scripts()
{
    new ArenaSoloWorldScript();
    new ArenaSoloBattlegroundScript();
    new ArenaSoloPlayerScript();
    new ArenaSoloGroupScript();
    new npc_arena_solo_master();
    new arena_solo_commandscript();
}
