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
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "StringFormat.h"

using namespace Acore::ChatCommands;

enum ArenaSoloGossipAction
{
    GOSSIP_ACTION_ARENA_SOLO_MAIN        = 0,
    GOSSIP_ACTION_ARENA_SOLO_LEAVE       = 1,
    GOSSIP_ACTION_ARENA_SOLO_BACK        = 2,
    // The bracket id is added to these bases.
    GOSSIP_ACTION_ARENA_SOLO_QUEUE_BASE  = 10,
    GOSSIP_ACTION_ARENA_SOLO_BOARD_BASE  = 20
};

namespace
{
void SendBracketStats(ChatHandler* handler, Player* player, uint8 bracket)
{
    ArenaSoloStats stats = sArenaSoloMgr->GetStats(player->GetGUID(), bracket);
    ArenaSoloBracketConfig const& config = sArenaSoloMgr->GetBracketConfig(bracket);
    uint32 losses = stats.Games > stats.Wins ? stats.Games - stats.Wins : 0;
    uint32 weekLosses = stats.WeekGames > stats.WeekWins ? stats.WeekGames - stats.WeekWins : 0;

    if (sArenaSoloMgr->UsesCoreArenaTeam(bracket))
    {
        if (stats.TeamName.empty())
            handler->PSendSysMessage("{} — personal 2v2 team not ready yet. Queue once to create it.",
                ArenaSoloMgr::GetBracketName(bracket));
        else
            handler->PSendSysMessage("{} — {} personal / {} team rating ({}), {}-{} (week {}-{}).",
                ArenaSoloMgr::GetBracketName(bracket), stats.Rating, stats.TeamRating, stats.TeamName,
                stats.Wins, losses, stats.WeekWins, weekLosses);
        return;
    }

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
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_CAN_JOIN_IN_BATTLEGROUND_QUEUE,
        PLAYERHOOK_CAN_JOIN_IN_ARENA_QUEUE
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !sArenaSoloMgr->IsEnabled() || !sArenaSoloMgr->IsBracketEnabled(ARENA_SOLO_BRACKET_2V2)
            || !sArenaSoloMgr->UsesCoreArenaTeam(ARENA_SOLO_BRACKET_2V2))
            return;

        std::string error;
        if (!sArenaSoloMgr->EnsurePersonalArenaTeam(player, error))
            LOG_DEBUG("module.arenasolo", "Could not ensure personal 2v2 team for {}: {}",
                player->GetName(), error);
    }

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
        GROUPHOOK_ON_ADD_MEMBER,
        GROUPHOOK_ON_REMOVE_MEMBER,
        GROUPHOOK_ON_DISBAND
    }) { }

    // Changing party composition invalidates the queued entry, whether it was a
    // solo player picking up a group or a queued 2v2 duo gaining a third member.
    void OnAddMember(Group* /*group*/, ObjectGuid guid) override
    {
        sArenaSoloMgr->RemovePlayer(guid);
    }

    void OnRemoveMember(Group* /*group*/, ObjectGuid guid, RemoveMethod /*method*/,
        ObjectGuid /*kicker*/, char const* /*reason*/) override
    {
        sArenaSoloMgr->RemovePlayer(guid);
    }

    void OnDisband(Group* group) override
    {
        if (!group)
            return;

        group->DoForAllMembers([](Player* member)
        {
            sArenaSoloMgr->RemovePlayer(member->GetGUID());
        });
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
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Leave the arena queue",
                GOSSIP_SENDER_MAIN, GOSSIP_ACTION_ARENA_SOLO_LEAVE);
        }
        else
        {
            for (uint8 bracket = 0; bracket < ARENA_SOLO_BRACKET_MAX; ++bracket)
            {
                if (!sArenaSoloMgr->IsBracketEnabled(bracket))
                    continue;

                ArenaSoloBracketConfig const& config = sArenaSoloMgr->GetBracketConfig(bracket);
                std::string queueKind = "solo";
                if (config.UseCoreArenaTeam)
                    queueKind = "party of 2, personal teams";
                else if (config.GroupSize > 1)
                    queueKind = Acore::StringFormat("party of {}", config.GroupSize);

                AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
                    Acore::StringFormat("Queue {} — {} ({} waiting)",
                        ArenaSoloMgr::GetBracketName(bracket), queueKind,
                        sArenaSoloMgr->GetQueuedCount(bracket)),
                    GOSSIP_SENDER_MAIN, uint32(GOSSIP_ACTION_ARENA_SOLO_QUEUE_BASE) + bracket);
            }
        }

        for (uint8 bracket = 0; bracket < ARENA_SOLO_BRACKET_MAX; ++bracket)
            if (sArenaSoloMgr->IsBracketEnabled(bracket))
                AddGossipItemFor(player, GOSSIP_ICON_TABARD,
                    Acore::StringFormat("{} leaderboard", ArenaSoloMgr::GetBracketName(bracket)),
                    GOSSIP_SENDER_MAIN, uint32(GOSSIP_ACTION_ARENA_SOLO_BOARD_BASE) + bracket);

        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);
        std::string error;

        if (action == GOSSIP_ACTION_ARENA_SOLO_LEAVE)
        {
            if (!sArenaSoloMgr->Dequeue(player, error))
                ChatHandler(player->GetSession()).SendSysMessage(error);

            CloseGossipMenuFor(player);
            return true;
        }

        uint32 const queueBase = GOSSIP_ACTION_ARENA_SOLO_QUEUE_BASE;
        uint32 const boardBase = GOSSIP_ACTION_ARENA_SOLO_BOARD_BASE;
        uint32 const bracketCount = ARENA_SOLO_BRACKET_MAX;

        if (action >= queueBase && action < queueBase + bracketCount)
        {
            uint8 bracket = static_cast<uint8>(action - queueBase);
            if (!sArenaSoloMgr->Queue(player, bracket, error))
                ChatHandler(player->GetSession()).SendSysMessage(error);

            CloseGossipMenuFor(player);
            return true;
        }

        if (action >= boardBase && action < boardBase + bracketCount)
        {
            uint8 bracket = static_cast<uint8>(action - boardBase);

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
            { "2v2",    HandleQueue2v2,   SEC_PLAYER, Console::No },
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

    static bool HandleQueue2v2(ChatHandler* handler)
    {
        return HandleQueueCommand(handler, ARENA_SOLO_BRACKET_2V2);
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
            handler->PSendSysMessage("You are queued for {} ({} entr{} waiting).",
                ArenaSoloMgr::GetBracketName(*queued), sArenaSoloMgr->GetQueuedCount(*queued),
                sArenaSoloMgr->GetQueuedCount(*queued) == 1 ? "y" : "ies");
        else
            handler->SendSysMessage(
                "Not queued. Use .solo 1v1 or .solo 3v3 while ungrouped, "
                "or .solo 2v2 in a party of 2 (each player gets a personal 2v2 team).");

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
                handler->SendSysMessage("Usage: .solo top [1v1|2v2|3v3]");
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
