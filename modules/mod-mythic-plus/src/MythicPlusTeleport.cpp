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
#include "DBCStores.h"
#include "Group.h"
#include "LFG.h"
#include "LFGMgr.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "StringFormat.h"
#include "WorldPacket.h"
#include "WorldSession.h"

bool MythicPlusMgr::CollectGroupMembers(Player* player, std::vector<Player*>& members, std::string& error) const
{
    members.clear();
    if (!player)
        return false;

    Group* group = player->GetGroup();
    if (!group)
    {
        if (_requireRoles || _minPlayers > 1)
        {
            error = IsSpanish(player)
                ? "Necesitas un grupo de mazmorra de 5 jugadores."
                : "You need a 5-player dungeon group.";
            return false;
        }
        members.push_back(player);
        return true;
    }
    if (group->isRaidGroup() || group->isLFGGroup() || group->isBGGroup())
    {
        error = IsSpanish(player)
            ? "Necesitas un grupo de mazmorra de 5 jugadores."
            : "You need a 5-player dungeon group.";
        return false;
    }

    group->DoForAllMembers([&members](Player* member)
    {
        members.push_back(member);
    });

    if (group->GetMembersCount() != members.size())
    {
        error = IsSpanish(player)
            ? "Todos los miembros del grupo deben estar conectados."
            : "Every group member must be online.";
        return false;
    }

    return true;
}

bool MythicPlusMgr::ValidatePartySize(Player* player, std::vector<Player*> const& members, std::string& error) const
{
    if (!_requireRoles)
    {
        if (members.size() < _minPlayers || members.size() > _maxPlayers)
        {
            error = Acore::StringFormat(
                IsSpanish(player) ? "El grupo debe tener entre {} y {} jugadores."
                                  : "The group must have between {} and {} players.",
                _minPlayers, _maxPlayers);
            return false;
        }
        return true;
    }

    if (members.size() != 5)
    {
        error = IsSpanish(player)
            ? "El grupo debe estar completo: 5 jugadores (1 tanque, 1 sanador, 3 DPS)."
            : "The group must be full: 5 players (1 tank, 1 healer, 3 DPS).";
        return false;
    }
    return true;
}

bool MythicPlusMgr::CanPlayerPerformRole(Player* player, uint8 role) const
{
    if (!player)
        return false;

    role &= ~lfg::PLAYER_ROLE_LEADER;
    if (role & lfg::PLAYER_ROLE_TANK)
    {
        if (player->HasTankSpec())
            return true;
        return player->GetSpec() == TALENT_TREE_DRUID_FERAL_COMBAT;
    }
    if (role & lfg::PLAYER_ROLE_HEALER)
        return player->HasHealSpec();
    if (role & lfg::PLAYER_ROLE_DAMAGE)
        return true;
    return false;
}

bool MythicPlusMgr::ValidatePartyComposition(Player* reporter, std::vector<Player*> const& members,
    std::string& error) const
{
    if (!_requireRoles)
        return true;
    if (members.size() != 5)
    {
        error = IsSpanish(reporter)
            ? "El grupo debe estar completo: 5 jugadores (1 tanque, 1 sanador, 3 DPS)."
            : "The group must be full: 5 players (1 tank, 1 healer, 3 DPS).";
        return false;
    }

    uint8 tanks = 0;
    uint8 heals = 0;
    uint8 flex = 0;
    uint8 dps = 0;
    for (Player* member : members)
    {
        if (!member)
            continue;
        if (member->HasHealSpec())
            ++heals;
        else if (member->HasTankSpec())
            ++tanks;
        else if (member->GetSpec() == TALENT_TREE_DRUID_FERAL_COMBAT)
            ++flex;
        else
            ++dps;
    }

    if (!tanks && flex)
    {
        ++tanks;
        --flex;
    }
    dps += flex;

    if (tanks != 1 || heals != 1 || dps != 3)
    {
        error = IsSpanish(reporter)
            ? "Composicion invalida. Se necesita 1 tanque, 1 sanador y 3 DPS."
            : "Invalid composition. Need 1 tank, 1 healer and 3 DPS.";
        return false;
    }
    return true;
}

bool MythicPlusMgr::PlayerReadyForTeleport(Player* player, std::string& error) const
{
    if (!player)
        return false;

    if (player->GetLevel() < _minLevel)
    {
        error = Acore::StringFormat(
            IsSpanish(player) ? "{} no es nivel {}." : "{} is not level {}.",
            player->GetName(), _minLevel);
        return false;
    }
    if (player->IsInCombat())
    {
        error = Acore::StringFormat(
            IsSpanish(player) ? "{} esta en combate." : "{} is in combat.",
            player->GetName());
        return false;
    }
    if (player->IsInFlight() || player->GetVehicle())
    {
        error = Acore::StringFormat(
            IsSpanish(player) ? "{} esta en movimiento (vuelo/vehiculo)."
                              : "{} is in flight or in a vehicle.",
            player->GetName());
        return false;
    }
    if (player->InBattleground() || player->InArena())
    {
        error = Acore::StringFormat(
            IsSpanish(player) ? "{} esta en un campo de batalla." : "{} is in a battleground.",
            player->GetName());
        return false;
    }
    if (player->IsBeingTeleported())
    {
        error = Acore::StringFormat(
            IsSpanish(player) ? "{} se esta teletransportando." : "{} is already teleporting.",
            player->GetName());
        return false;
    }
    if (sLFGMgr->GetState(player->GetGUID()) != lfg::LFG_STATE_NONE)
    {
        error = IsSpanish(player)
            ? "El grupo ya esta usando el Buscador de Mazmorras."
            : "The group is already using the Dungeon Finder.";
        return false;
    }
    if (MythicRun const* run = GetRunForPlayer(player))
    {
        if (run->Active)
        {
            error = IsSpanish(player)
                ? "Ya hay una mitica en curso."
                : "A mythic run is already in progress.";
            return false;
        }
    }
    return true;
}

uint32 MythicPlusMgr::FindHeroicLfgDungeon(uint32 mapId) const
{
    uint32 fallback = 0;
    for (uint32 i = 0; i < sLFGDungeonStore.GetNumRows(); ++i)
    {
        LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(i);
        if (!dungeon)
            continue;

        bool const heroicDungeon = dungeon->Difficulty == DUNGEON_DIFFICULTY_HEROIC
            && (dungeon->TypeID == lfg::LFG_TYPE_DUNGEON || dungeon->TypeID == lfg::LFG_TYPE_HEROIC);
        if (!heroicDungeon)
            continue;
        if (dungeon->MapID == mapId)
            return dungeon->ID;
        if (!fallback)
            fallback = dungeon->ID;
    }
    return fallback;
}

bool MythicPlusMgr::IsInTeleportCheck(ObjectGuid guid) const
{
    return _playerTeleportCheck.find(guid) != _playerTeleportCheck.end();
}

bool MythicPlusMgr::TeleportToKey(Player* player, std::string& error)
{
    if (!player)
        return false;
    if (!_allowTeleport)
    {
        error = IsSpanish(player) ? "El teletransporte esta desactivado." : "Teleport is disabled.";
        return false;
    }

    LoadProfile(player->GetGUID());
    MythicKeystone const& key = _profiles[player->GetGUID()].Key;
    MythicDungeonDef const* def = FindMythicDungeon(key.DungeonId);
    if (!def || !key.Level)
    {
        error = IsSpanish(player) ? "No tienes piedra angular." : "You have no keystone.";
        return false;
    }

    std::vector<Player*> members;
    if (!CollectGroupMembers(player, members, error) || !ValidatePartySize(player, members, error))
        return false;

    for (Player* member : members)
    {
        if (!PlayerReadyForTeleport(member, error))
            return false;
    }
    if (!ValidatePartyComposition(player, members, error))
        return false;

    Group* group = player->GetGroup();
    if (group && _teleportChecks.find(group->GetGUID()) != _teleportChecks.end())
    {
        error = IsSpanish(player)
            ? "Ya hay una comprobacion de funciones en curso."
            : "A role check is already in progress.";
        return false;
    }

    MythicTeleportCheck check;
    check.RequesterGuid = player->GetGUID();
    check.GroupGuid = group ? group->GetGUID() : ObjectGuid::Empty;
    check.DungeonId = key.DungeonId;
    check.KeyLevel = key.Level;

    if (!_requireRoles)
        return TeleportGroup(check, error);

    if (!group)
    {
        error = IsSpanish(player)
            ? "Necesitas un grupo de mazmorra de 5 jugadores."
            : "You need a 5-player dungeon group.";
        return false;
    }

    check.LfgDungeonId = FindHeroicLfgDungeon(def->MapId);
    check.ExpireMs = _roleCheckMs;
    for (Player* member : members)
        check.Roles[member->GetGUID()] = lfg::PLAYER_ROLE_NONE;

    _teleportChecks[check.GroupGuid] = check;
    for (Player* member : members)
        _playerTeleportCheck[member->GetGUID()] = check.GroupGuid;

    BroadcastRoleCheck(_teleportChecks[check.GroupGuid], lfg::LFG_ROLECHECK_INITIALITING, true);

    bool const es = IsSpanish(player);
    ChatHandler(player->GetSession()).PSendSysMessage(
        es ? "Comprobacion de funciones iniciada para +{} {}. Elige tu rol."
           : "Role check started for +{} {}. Select your role.",
        key.Level, DungeonName(key.DungeonId, es));
    return true;
}

void MythicPlusMgr::BroadcastRoleCheck(MythicTeleportCheck const& check, uint8 state, bool sendPartyUpdate,
    ObjectGuid chosenGuid, uint8 chosenRoles) const
{
    lfg::LfgRoleCheck roleCheck;
    roleCheck.state = lfg::LfgRoleCheckState(state);
    roleCheck.leader = check.RequesterGuid;
    roleCheck.rDungeonId = 0;
    roleCheck.cancelTime = 0;
    if (check.LfgDungeonId)
        roleCheck.dungeons.insert(check.LfgDungeonId);
    for (auto const& pair : check.Roles)
        roleCheck.roles[pair.first] = pair.second;

    lfg::LfgDungeonSet dungeons;
    if (check.LfgDungeonId)
        dungeons.insert(check.LfgDungeonId);

    for (auto const& pair : check.Roles)
    {
        Player* member = ObjectAccessor::FindConnectedPlayer(pair.first);
        if (!member || !member->GetSession())
            continue;

        if (chosenGuid)
            member->GetSession()->SendLfgRoleChosen(chosenGuid, chosenRoles);

        member->GetSession()->SendLfgRoleCheckUpdate(roleCheck);

        if (!sendPartyUpdate)
            continue;

        if (state == lfg::LFG_ROLECHECK_INITIALITING)
        {
            member->GetSession()->SendLfgUpdateParty(
                lfg::LfgUpdateData(lfg::LFG_UPDATETYPE_JOIN_QUEUE, dungeons, ""));
        }
        else
        {
            if (state != lfg::LFG_ROLECHECK_FINISHED && pair.first == check.RequesterGuid)
            {
                member->GetSession()->SendLfgJoinResult(
                    lfg::LfgJoinResultData(lfg::LFG_JOIN_FAILED, lfg::LfgRoleCheckState(state)));
            }
            lfg::LfgUpdateType updateType = state == lfg::LFG_ROLECHECK_FINISHED
                ? lfg::LFG_UPDATETYPE_REMOVED_FROM_QUEUE
                : lfg::LFG_UPDATETYPE_ROLECHECK_FAILED;
            member->GetSession()->SendLfgUpdateParty(lfg::LfgUpdateData(updateType));
        }
    }
}

void MythicPlusMgr::FinishTeleportCheck(ObjectGuid groupGuid, uint8 state)
{
    auto it = _teleportChecks.find(groupGuid);
    if (it == _teleportChecks.end())
        return;

    MythicTeleportCheck check = it->second;
    BroadcastRoleCheck(check, state, true);

    for (auto const& pair : check.Roles)
        _playerTeleportCheck.erase(pair.first);
    _teleportChecks.erase(it);

    if (state != lfg::LFG_ROLECHECK_FINISHED)
    {
        if (Player* requester = ObjectAccessor::FindConnectedPlayer(check.RequesterGuid))
        {
            bool const es = IsSpanish(requester);
            char const* reason = es ? "Comprobacion de funciones cancelada." : "Role check cancelled.";
            if (state == lfg::LFG_ROLECHECK_WRONG_ROLES)
                reason = es ? "Roles incompatibles: se necesita 1 tanque, 1 sanador y 3 DPS."
                            : "Incompatible roles: need 1 tank, 1 healer and 3 DPS.";
            else if (state == lfg::LFG_ROLECHECK_MISSING_ROLE)
                reason = es ? "Alguien no eligio rol a tiempo." : "Someone did not select a role in time.";
            else if (state == lfg::LFG_ROLECHECK_NO_ROLE)
                reason = es ? "Alguien no selecciono ningun rol." : "Someone selected no role.";
            ChatHandler(requester->GetSession()).SendSysMessage(reason);
        }
        return;
    }

    std::string error;
    if (!TeleportGroup(check, error))
    {
        if (Player* requester = ObjectAccessor::FindConnectedPlayer(check.RequesterGuid))
            if (!error.empty())
                ChatHandler(requester->GetSession()).SendSysMessage(error);
    }
}

void MythicPlusMgr::AbortTeleportCheck(ObjectGuid groupGuid, uint8 state)
{
    FinishTeleportCheck(groupGuid, state);
}

void MythicPlusMgr::HandleGroupMemberRemoved(ObjectGuid groupGuid)
{
    if (_teleportChecks.find(groupGuid) == _teleportChecks.end())
        return;
    FinishTeleportCheck(groupGuid, lfg::LFG_ROLECHECK_ABORTED);
}

void MythicPlusMgr::HandleTeleportSetRoles(Player* player, uint8 roles)
{
    if (!player)
        return;

    auto mapIt = _playerTeleportCheck.find(player->GetGUID());
    if (mapIt == _playerTeleportCheck.end())
        return;

    auto checkIt = _teleportChecks.find(mapIt->second);
    if (checkIt == _teleportChecks.end())
        return;

    MythicTeleportCheck& check = checkIt->second;
    if (roles < lfg::PLAYER_ROLE_TANK)
    {
        FinishTeleportCheck(check.GroupGuid, lfg::LFG_ROLECHECK_NO_ROLE);
        return;
    }

    check.Roles[player->GetGUID()] = roles;
    BroadcastRoleCheck(check, lfg::LFG_ROLECHECK_INITIALITING, false, player->GetGUID(), roles);

    for (auto const& pair : check.Roles)
        if (pair.second == lfg::PLAYER_ROLE_NONE)
            return;

    lfg::LfgRolesMap assigned;
    for (auto const& pair : check.Roles)
        assigned[pair.first] = pair.second;

    if (!lfg::LFGMgr::CheckGroupRoles(assigned))
    {
        FinishTeleportCheck(check.GroupGuid, lfg::LFG_ROLECHECK_WRONG_ROLES);
        return;
    }

    for (auto const& pair : assigned)
    {
        Player* member = ObjectAccessor::FindConnectedPlayer(pair.first);
        if (!member || !CanPlayerPerformRole(member, pair.second))
        {
            FinishTeleportCheck(check.GroupGuid, lfg::LFG_ROLECHECK_WRONG_ROLES);
            return;
        }
    }

    check.Roles.clear();
    for (auto const& pair : assigned)
        check.Roles[pair.first] = pair.second;

    FinishTeleportCheck(check.GroupGuid, lfg::LFG_ROLECHECK_FINISHED);
}

void MythicPlusMgr::HandleTeleportLeave(Player* player)
{
    if (!player)
        return;
    auto it = _playerTeleportCheck.find(player->GetGUID());
    if (it == _playerTeleportCheck.end())
        return;
    FinishTeleportCheck(it->second, lfg::LFG_ROLECHECK_ABORTED);
}

bool MythicPlusMgr::TryConsumeLfgPacket(WorldSession* session, WorldPacket const& packet)
{
    if (!session)
        return false;
    Player* player = session->GetPlayer();
    if (!player || !IsInTeleportCheck(player->GetGUID()))
        return false;

    uint16 const opcode = packet.GetOpcode();
    if (opcode == CMSG_LFG_SET_ROLES)
    {
        WorldPacket data(packet);
        uint8 roles = 0;
        data >> roles;
        HandleTeleportSetRoles(player, roles);
        return true;
    }
    if (opcode == CMSG_LFG_LEAVE)
    {
        HandleTeleportLeave(player);
        return true;
    }
    return false;
}

void MythicPlusMgr::TickTeleportChecks(uint32 diff)
{
    std::vector<ObjectGuid> expired;
    for (auto& pair : _teleportChecks)
    {
        if (pair.second.ExpireMs <= diff)
            expired.push_back(pair.first);
        else
            pair.second.ExpireMs -= diff;
    }
    for (ObjectGuid const& groupGuid : expired)
        FinishTeleportCheck(groupGuid, lfg::LFG_ROLECHECK_MISSING_ROLE);
}

bool MythicPlusMgr::TeleportGroup(MythicTeleportCheck const& check, std::string& error)
{
    Player* requester = ObjectAccessor::FindConnectedPlayer(check.RequesterGuid);
    if (!requester)
    {
        error = "The key holder went offline.";
        return false;
    }

    MythicDungeonDef const* def = FindMythicDungeon(check.DungeonId);
    if (!def)
    {
        error = Text(requester, "Unknown dungeon.", "Mazmorra desconocida.");
        return false;
    }

    std::vector<Player*> members;
    if (!CollectGroupMembers(requester, members, error) || !ValidatePartySize(requester, members, error))
        return false;

    for (Player* member : members)
        if (!PlayerReadyForTeleport(member, error))
            return false;

    if (Group* group = requester->GetGroup())
        group->SetDungeonDifficulty(DUNGEON_DIFFICULTY_HEROIC);
    else
        requester->SetDungeonDifficulty(DUNGEON_DIFFICULTY_HEROIC);

    for (Player* member : members)
        member->SetDungeonDifficulty(DUNGEON_DIFFICULTY_HEROIC);

    requester->TeleportTo(def->MapId, def->X, def->Y, def->Z, def->O);
    for (Player* member : members)
    {
        if (member == requester)
            continue;
        member->TeleportTo(def->MapId, def->X, def->Y, def->Z, def->O);
    }

    bool const es = IsSpanish(requester);
    ChatHandler(requester->GetSession()).PSendSysMessage(
        es ? "Teletransporte a +{} {}." : "Teleporting to +{} {}.",
        check.KeyLevel, DungeonName(check.DungeonId, es));
    return true;
}
