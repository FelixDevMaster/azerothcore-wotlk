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
#include "CommandScript.h"
#include "Creature.h"
#include "GameObject.h"
#include "Group.h"
#include "GroupScript.h"
#include "Item.h"
#include "Optional.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "ServerScript.h"
#include "StringFormat.h"
#include "WorldPacket.h"
#include "WorldSession.h"

using namespace Acore::ChatCommands;

enum MythicGossipAction : uint32
{
    GOSSIP_MYTHIC_HELLO     = 0,
    GOSSIP_MYTHIC_START     = 1,
    GOSSIP_MYTHIC_TELEPORT  = 2,
    GOSSIP_MYTHIC_CLAIM_KEY = 3,
    GOSSIP_MYTHIC_AFFIXES   = 4,
    GOSSIP_MYTHIC_SCORE     = 5,
    GOSSIP_MYTHIC_BOARD     = 6,
    GOSSIP_MYTHIC_VAULT1    = 11,
    GOSSIP_MYTHIC_VAULT2    = 12,
    GOSSIP_MYTHIC_VAULT3    = 13
};

namespace
{
void BuildBrokerGossip(Player* player)
{
    ClearGossipMenuFor(player);
    bool const es = MythicPlusMgr::IsSpanish(player);
    MythicProfile profile = sMythicPlus->GetProfile(player->GetGUID());
    MythicAffixSet weekly = sMythicPlus->GetWeeklyAffixes();

    AddGossipItemFor(player, GOSSIP_ICON_CHAT,
        Acore::StringFormat(es ? "Temporada {}  semana {}" : "Season {}  week {}",
            sMythicPlus->GetSeasonId(), sMythicPlus->GetWeekIndex() + 1),
        GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_HELLO);

    if (profile.Key.Level)
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
            Acore::StringFormat(es ? "Piedra: +{} {}{}" : "Keystone: +{} {}{}",
                profile.Key.Level, MythicPlusMgr::DungeonName(profile.Key.DungeonId, es),
                profile.Key.Depleted ? (es ? " (agotada)" : " (depleted)") : ""),
            GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_HELLO);
    else
        AddGossipItemFor(player, GOSSIP_ICON_TABARD,
            es ? "Reclamar piedra +2" : "Claim a +2 keystone",
            GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_CLAIM_KEY);

    AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
        es ? "Insertar piedra (comenzar)" : "Insert keystone (start)",
        GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_START);
    AddGossipItemFor(player, GOSSIP_ICON_TAXI,
        es ? "Teleport a la mazmorra de la piedra" : "Teleport to your keystone dungeon",
        GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_TELEPORT);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT,
        Acore::StringFormat(es ? "Affijos: {} / {} / {} / {}"
                               : "Affixes: {} / {} / {} / {}",
            MythicPlusMgr::AffixName(weekly.FortTyr, es),
            MythicPlusMgr::AffixName(weekly.Plus4, es),
            MythicPlusMgr::AffixName(weekly.Plus7, es),
            MythicPlusMgr::AffixName(weekly.Seasonal, es)),
        GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_AFFIXES);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT,
        Acore::StringFormat(es ? "Puntuacion: {:.1f}  mejor semana +{}"
                               : "Score: {:.1f}  week best +{}",
            profile.OverallScore, profile.WeekBestLevel),
        GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_SCORE);
    AddGossipItemFor(player, GOSSIP_ICON_TABARD,
        es ? "Ranking" : "Leaderboard",
        GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_BOARD);
    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG,
        es ? "Cofre semanal ranura 1" : "Weekly vault slot 1",
        GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_VAULT1);
    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG,
        es ? "Cofre semanal ranura 2 (4 runs)" : "Weekly vault slot 2 (4 runs)",
        GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_VAULT2);
    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG,
        es ? "Cofre semanal ranura 3 (8 runs)" : "Weekly vault slot 3 (8 runs)",
        GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_VAULT3);
}

void HandleBrokerSelect(Player* player, uint32 action)
{
    std::string error;
    bool const es = MythicPlusMgr::IsSpanish(player);
    ChatHandler handler(player->GetSession());

    switch (action)
    {
        case GOSSIP_MYTHIC_START:
            if (!sMythicPlus->StartRun(player, error))
                handler.SendSysMessage(error);
            break;
        case GOSSIP_MYTHIC_TELEPORT:
            if (!sMythicPlus->TeleportToKey(player, error))
                handler.SendSysMessage(error);
            break;
        case GOSSIP_MYTHIC_CLAIM_KEY:
            if (!sMythicPlus->ClaimStarterKey(player, error))
                handler.SendSysMessage(error);
            break;
        case GOSSIP_MYTHIC_AFFIXES:
        {
            MythicAffixSet weekly = sMythicPlus->GetWeeklyAffixes();
            uint8 affixes[4] = { weekly.FortTyr, weekly.Plus4, weekly.Plus7, weekly.Seasonal };
            char const* gates[4] = { "+2", "+4", "+7", "+10" };
            for (uint8 i = 0; i < 4; ++i)
            {
                handler.PSendSysMessage("|cff00ccff{}|r {} — {}", gates[i],
                    MythicPlusMgr::AffixName(affixes[i], es),
                    MythicPlusMgr::AffixDesc(affixes[i], es));
            }
            break;
        }
        case GOSSIP_MYTHIC_SCORE:
            sMythicPlus->SendStatus(&handler, player);
            break;
        case GOSSIP_MYTHIC_BOARD:
        {
            uint32 rank = 1;
            handler.SendSysMessage(es ? "Ranking de Miticas:" : "Mythic+ leaderboard:");
            for (MythicLeaderboardRow const& row : sMythicPlus->GetLeaderboard(10))
            {
                handler.PSendSysMessage(es ? "{}. {} — {:.1f}  (semana +{}, {} runs)"
                                           : "{}. {} — {:.1f}  (week +{}, {} runs)",
                    rank, row.Name, row.Score, row.WeekBest, row.Runs);
                ++rank;
            }
            if (rank == 1)
                handler.SendSysMessage(es ? "Todavia no hay puntuaciones." : "No scores yet.");
            break;
        }
        case GOSSIP_MYTHIC_VAULT1:
        case GOSSIP_MYTHIC_VAULT2:
        case GOSSIP_MYTHIC_VAULT3:
            if (!sMythicPlus->ClaimVaultSlot(player, uint8(action - 10), error))
                handler.SendSysMessage(error);
            break;
        default:
            break;
    }
}
}

class MythicPlusWorldScript : public WorldScript
{
public:
    MythicPlusWorldScript() : WorldScript("MythicPlusWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE,
        WORLDHOOK_ON_UPDATE
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        sMythicPlus->LoadConfig(reload);
    }

    void OnLoadCustomDatabaseTable() override
    {
        sMythicPlus->EnsureDatabase();
    }

    void OnUpdate(uint32 diff) override
    {
        sMythicPlus->Update(diff);
    }
};

class MythicPlusMapScript : public AllMapScript
{
public:
    MythicPlusMapScript() : AllMapScript("MythicPlusMapScript", {
        ALLMAPHOOK_ON_PLAYER_ENTER_ALL,
        ALLMAPHOOK_ON_PLAYER_LEAVE_ALL,
        ALLMAPHOOK_ON_MAP_UPDATE,
        ALLMAPHOOK_ON_DESTROY_MAP
    }) { }

    void OnPlayerEnterAll(Map* map, Player* player) override
    {
        sMythicPlus->HandlePlayerEnter(map, player);
    }

    void OnPlayerLeaveAll(Map* map, Player* player) override
    {
        sMythicPlus->HandlePlayerLeave(map, player);
    }

    void OnMapUpdate(Map* map, uint32 diff) override
    {
        sMythicPlus->UpdateRun(map, diff);
    }

    void OnDestroyMap(Map* map) override
    {
        sMythicPlus->DestroyMap(map);
    }
};

class MythicPlusServerScript : public ServerScript
{
public:
    MythicPlusServerScript() : ServerScript("MythicPlusServerScript", {
        SERVERHOOK_CAN_PACKET_RECEIVE
    }) { }

    bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
    {
        if (sMythicPlus->TryConsumeLfgPacket(session, packet))
            return false;
        if (sMythicPlus->TryConsumeItemQuery(session, packet))
            return false;
        return true;
    }
};

class MythicPlusGroupScript : public GroupScript
{
public:
    MythicPlusGroupScript() : GroupScript("MythicPlusGroupScript", {
        GROUPHOOK_ON_REMOVE_MEMBER,
        GROUPHOOK_ON_DISBAND
    }) { }

    void OnRemoveMember(Group* group, ObjectGuid /*guid*/, RemoveMethod /*method*/,
        ObjectGuid /*kicker*/, char const* /*reason*/) override
    {
        if (group)
            sMythicPlus->HandleGroupMemberRemoved(group->GetGUID());
    }

    void OnDisband(Group* group) override
    {
        if (group)
            sMythicPlus->HandleGroupMemberRemoved(group->GetGUID());
    }
};

class MythicPlusPlayerScript : public PlayerScript
{
public:
    MythicPlusPlayerScript() : PlayerScript("MythicPlusPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_PLAYER_JUST_DIED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        sMythicPlus->HandleLogin(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
            sMythicPlus->HandleLogout(player->GetGUID());
    }

    void OnPlayerJustDied(Player* player) override
    {
        sMythicPlus->HandlePlayerDeath(player);
    }
};

class MythicPlusCreatureScript : public AllCreatureScript
{
public:
    MythicPlusCreatureScript() : AllCreatureScript("MythicPlusCreatureScript") { }

    void OnCreatureAddWorld(Creature* creature) override
    {
        sMythicPlus->HandleCreatureAdd(creature);
    }
};

class MythicPlusUnitScript : public UnitScript
{
public:
    MythicPlusUnitScript() : UnitScript("MythicPlusUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_HEAL_RECEIVED,
        UNITHOOK_ON_UNIT_DEATH
    }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& /*damage*/) override
    {
        sMythicPlus->OnCreatureMeleeHit(attacker, victim);
    }

    void ModifyMeleeDamage(Unit* /*target*/, Unit* attacker, uint32& damage) override
    {
        sMythicPlus->ModifyCreatureDamage(attacker, nullptr, damage);
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage,
        SpellInfo const* /*spellInfo*/) override
    {
        if (damage <= 0)
            return;
        uint32 value = uint32(damage);
        sMythicPlus->ModifyCreatureDamage(attacker, nullptr, value);
        damage = int32(value);
    }

    void ModifyHealReceived(Unit* target, Unit* /*healer*/, uint32& heal, SpellInfo const* /*spellInfo*/) override
    {
        sMythicPlus->ModifyHealReceived(target, heal);
    }

    void OnUnitDeath(Unit* unit, Unit* killer) override
    {
        sMythicPlus->HandleUnitDeath(unit, killer);
    }
};

class npc_mythic_broker : public CreatureScript
{
public:
    npc_mythic_broker() : CreatureScript("npc_mythic_broker") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        BuildBrokerGossip(player);
        SendGossipMenuFor(player, NPC_TEXT_MYTHIC_BROKER, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);
        if (action != GOSSIP_MYTHIC_HELLO)
            HandleBrokerSelect(player, action);
        if (action == GOSSIP_MYTHIC_TELEPORT || action == GOSSIP_MYTHIC_START)
        {
            CloseGossipMenuFor(player);
            return true;
        }
        BuildBrokerGossip(player);
        SendGossipMenuFor(player, NPC_TEXT_MYTHIC_BROKER, creature);
        return true;
    }
};

class gobject_mythic_font : public GameObjectScript
{
public:
    gobject_mythic_font() : GameObjectScript("gobject_mythic_font") { }

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        ClearGossipMenuFor(player);
        bool const es = MythicPlusMgr::IsSpanish(player);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
            es ? "Insertar la Piedra angular mitica" : "Insert Mythic Keystone",
            GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_START);
        SendGossipMenuFor(player, player->GetGossipTextId(go), go->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, GameObject* /*go*/, uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);
        if (action == GOSSIP_MYTHIC_START)
        {
            std::string error;
            if (!sMythicPlus->StartRun(player, error))
                ChatHandler(player->GetSession()).SendSysMessage(error);
        }
        return true;
    }
};

class item_mythic_keystone : public ItemScript
{
public:
    item_mythic_keystone() : ItemScript("item_mythic_keystone") { }

    bool OnUse(Player* player, Item* /*item*/, SpellCastTargets const& /*targets*/) override
    {
        if (!player)
            return true;
        std::string error;
        if (!sMythicPlus->StartRun(player, error))
            ChatHandler(player->GetSession()).SendSysMessage(error);
        return true;
    }
};

class mythic_commandscript : public CommandScript
{
public:
    mythic_commandscript() : CommandScript("mythic_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable mplusTable =
        {
            { "status", HandleStatus, SEC_PLAYER, Console::No },
            { "key", HandleKey, SEC_PLAYER, Console::No },
            { "week", HandleWeek, SEC_PLAYER, Console::No },
            { "start", HandleStart, SEC_PLAYER, Console::No },
            { "teleport", HandleTeleport, SEC_PLAYER, Console::No },
            { "vault", HandleVault, SEC_PLAYER, Console::No },
            { "top", HandleTop, SEC_PLAYER, Console::No },
            { "setkey", HandleSetKey, SEC_GAMEMASTER, Console::No },
            { "complete", HandleComplete, SEC_GAMEMASTER, Console::No },
            { "", HandleStatus, SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "mplus", mplusTable }
        };
        return commandTable;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        sMythicPlus->SendStatus(handler, player);
        return true;
    }

    static bool HandleKey(ChatHandler* handler)
    {
        return HandleStatus(handler);
    }

    static bool HandleWeek(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        bool const es = MythicPlusMgr::IsSpanish(player);
        MythicAffixSet weekly = sMythicPlus->GetWeeklyAffixes();
        handler->PSendSysMessage(es ? "Semana {} temporada {}" : "Week {} season {}",
            sMythicPlus->GetWeekIndex() + 1, sMythicPlus->GetSeasonId());
        uint8 affixes[4] = { weekly.FortTyr, weekly.Plus4, weekly.Plus7, weekly.Seasonal };
        char const* gates[4] = { "+2", "+4", "+7", "+10" };
        for (uint8 i = 0; i < 4; ++i)
            handler->PSendSysMessage("|cff00ccff{}|r {} — {}", gates[i],
                MythicPlusMgr::AffixName(affixes[i], es),
                MythicPlusMgr::AffixDesc(affixes[i], es));
        return true;
    }

    static bool HandleStart(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        std::string error;
        if (!sMythicPlus->StartRun(player, error))
        {
            handler->SendSysMessage(error);
            handler->SetSentErrorMessage(true);
            return false;
        }
        return true;
    }

    static bool HandleTeleport(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        std::string error;
        if (!sMythicPlus->TeleportToKey(player, error))
        {
            handler->SendSysMessage(error);
            handler->SetSentErrorMessage(true);
            return false;
        }
        return true;
    }

    static bool HandleVault(ChatHandler* handler, Optional<uint8> slot)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        std::string error;
        if (!sMythicPlus->ClaimVaultSlot(player, slot.value_or(1), error))
        {
            handler->SendSysMessage(error);
            handler->SetSentErrorMessage(true);
            return false;
        }
        return true;
    }

    static bool HandleTop(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        bool const es = player && MythicPlusMgr::IsSpanish(player);
        handler->SendSysMessage(es ? "Ranking de Miticas:" : "Mythic+ leaderboard:");
        uint32 rank = 1;
        for (MythicLeaderboardRow const& row : sMythicPlus->GetLeaderboard(15))
        {
            handler->PSendSysMessage(es ? "{}. {} — {:.1f}  (semana +{})"
                                       : "{}. {} — {:.1f}  (week +{})",
                rank, row.Name, row.Score, row.WeekBest);
            ++rank;
        }
        if (rank == 1)
            handler->SendSysMessage(es ? "Todavia no hay puntuaciones." : "No scores yet.");
        return true;
    }

    static bool HandleSetKey(ChatHandler* handler, uint8 dungeonId, Optional<uint8> level)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        std::string error;
        if (!sMythicPlus->SetKey(player, dungeonId, level.value_or(2), error))
        {
            handler->SendSysMessage(error);
            handler->SetSentErrorMessage(true);
            return false;
        }
        bool const es = MythicPlusMgr::IsSpanish(player);
        handler->PSendSysMessage(es ? "Piedra asignada: +{} {}" : "Keystone set: +{} {}",
            level.value_or(2), MythicPlusMgr::DungeonName(dungeonId, es));
        return true;
    }

    static bool HandleComplete(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        std::string error;
        if (!sMythicPlus->ForceComplete(player, error))
        {
            handler->SendSysMessage(error);
            handler->SetSentErrorMessage(true);
            return false;
        }
        return true;
    }
};

void AddSC_mythic_plus()
{
    new MythicPlusWorldScript();
    new MythicPlusMapScript();
    new MythicPlusPlayerScript();
    new MythicPlusServerScript();
    new MythicPlusGroupScript();
    new MythicPlusCreatureScript();
    new MythicPlusUnitScript();
    new npc_mythic_broker();
    new gobject_mythic_font();
    new item_mythic_keystone();
    new mythic_commandscript();
}
