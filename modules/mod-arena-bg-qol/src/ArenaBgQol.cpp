/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ArenaBgQol.h"
#include "Battleground.h"
#include "Chat.h"
#include "Config.h"
#include "GameObject.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"

ArenaBgQol* ArenaBgQol::instance()
{
    static ArenaBgQol instance;
    return &instance;
}

void ArenaBgQol::LoadConfig(bool /*reload*/)
{
    _enabled = sConfigMgr->GetOption<bool>("ArenaBgQol.Enable", true);
    _ritualsEnabled = sConfigMgr->GetOption<bool>("ArenaBgQol.Rituals.Enable", true);
    _ritualsArenas = sConfigMgr->GetOption<bool>("ArenaBgQol.Rituals.Arenas", true);
    _ritualsBattlegrounds = sConfigMgr->GetOption<bool>("ArenaBgQol.Rituals.Battlegrounds", true);
    _ritualDuration = sConfigMgr->GetOption<uint32>("ArenaBgQol.Rituals.Duration", 180);
    _readyCheckEnabled = sConfigMgr->GetOption<bool>("ArenaBgQol.ReadyCheck.Enable", true);

    uint32 triggerSeconds = sConfigMgr->GetOption<uint32>("ArenaBgQol.ReadyCheck.TriggerTime", 30);
    uint32 skipToSeconds = sConfigMgr->GetOption<uint32>("ArenaBgQol.ReadyCheck.SkipToTime", 15);
    uint32 timeoutSeconds = sConfigMgr->GetOption<uint32>("ArenaBgQol.ReadyCheck.Timeout", 0);

    if (skipToSeconds >= triggerSeconds)
        skipToSeconds = triggerSeconds > 0 ? triggerSeconds - 1 : 0;

    _readyCheckTriggerMs = int32(triggerSeconds * IN_MILLISECONDS);
    _readyCheckSkipToMs = int32(skipToSeconds * IN_MILLISECONDS);
    _readyCheckTimeoutMs = timeoutSeconds * IN_MILLISECONDS;

    LOG_INFO("server.loading", ">> Arena/BG QoL: rituals {}, ready-check at {}s -> {}s",
        _ritualsEnabled ? "on" : "off", triggerSeconds, skipToSeconds);
}

uint64 ArenaBgQol::MakeKey(Battleground const* bg)
{
    return (uint64(bg->GetMapId()) << 32) | bg->GetInstanceID();
}

bool ArenaBgQol::IsSpanish(Player const* player)
{
    if (!player || !player->GetSession())
        return false;

    LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
    return locale == LOCALE_esES || locale == LOCALE_esMX;
}

char const* ArenaBgQol::Msg(Player const* player, char const* spanish, char const* english)
{
    return IsSpanish(player) ? spanish : english;
}

ArenaBgQol::ReadyCheckState& ArenaBgQol::GetState(Battleground const* bg)
{
    return _states[MakeKey(bg)];
}

void ArenaBgQol::HandleAddPlayer(Battleground* bg, Player* player)
{
    if (!_enabled || !bg || !player)
        return;

    TrySpawnRitual(bg, player);

    if (!_readyCheckEnabled || !bg->isArena())
        return;

    ReadyCheckState& state = GetState(bg);
    if (state.Status != READY_CHECK_WAITING)
        return;

    state.Pending.insert(player->GetGUID());

    WorldPacket data(MSG_RAID_READY_CHECK, 8);
    data << player->GetGUID();
    player->SendDirectMessage(&data);

    if (WorldSession* session = player->GetSession())
        session->SendAreaTriggerMessage("{}",
            Msg(player, "Confirma si estas listo para comenzar.", "Confirm if you are ready to start."));
}

void ArenaBgQol::HandleRemovePlayer(Battleground* bg, Player* player)
{
    if (!bg || !player)
        return;

    auto itr = _states.find(MakeKey(bg));
    if (itr == _states.end())
        return;

    ReadyCheckState& state = itr->second;
    state.Pending.erase(player->GetGUID());
    state.PendingRituals.erase(player->GetGUID());
    state.SpawnedRituals.erase(player->GetGUID());

    if (state.Status == READY_CHECK_WAITING && state.Pending.empty() && !bg->GetPlayers().empty())
        FinishReadyCheck(bg, state, true);
}

void ArenaBgQol::HandleDestroy(Battleground* bg)
{
    if (!bg)
        return;

    _states.erase(MakeKey(bg));
}

void ArenaBgQol::TrySpawnRitual(Battleground* bg, Player* player)
{
    if (!_ritualsEnabled || !bg || !player)
        return;

    if (bg->isArena() && !_ritualsArenas)
        return;

    if (bg->isBattleground() && !_ritualsBattlegrounds)
        return;

    uint8 playerClass = player->getClass();
    if (playerClass != CLASS_MAGE && playerClass != CLASS_WARLOCK)
        return;

    ReadyCheckState& state = GetState(bg);
    if (state.SpawnedRituals.count(player->GetGUID()))
        return;

    if (!player->IsInWorld())
    {
        state.PendingRituals.insert(player->GetGUID());
        return;
    }

    state.PendingRituals.erase(player->GetGUID());
    state.SpawnedRituals.insert(player->GetGUID());
    SpawnRitual(player);
}

void ArenaBgQol::SpawnPendingRituals(Battleground* bg, ReadyCheckState& state)
{
    if (!_ritualsEnabled || state.PendingRituals.empty())
        return;

    for (auto itr = state.PendingRituals.begin(); itr != state.PendingRituals.end();)
    {
        Player* player = ObjectAccessor::FindPlayer(*itr);
        if (!player || player->GetBattleground() != bg)
        {
            itr = state.PendingRituals.erase(itr);
            continue;
        }

        if (!player->IsInWorld())
        {
            ++itr;
            continue;
        }

        ObjectGuid guid = *itr;
        itr = state.PendingRituals.erase(itr);
        if (state.SpawnedRituals.insert(guid).second)
            SpawnRitual(player);
    }
}

void ArenaBgQol::SpawnRitual(Player* player)
{
    uint32 goEntry = 0;
    char const* spanish = nullptr;
    char const* english = nullptr;

    switch (player->getClass())
    {
        case CLASS_MAGE:
            goEntry = player->HasSpell(SPELL_RITUAL_OF_REFRESHMENT_R2)
                ? GO_REFRESHMENT_TABLE_R2 : GO_REFRESHMENT_TABLE_R1;
            spanish = "Ritual de Refrigerio colocado en tu posicion.";
            english = "Ritual of Refreshment placed at your position.";
            break;
        case CLASS_WARLOCK:
            goEntry = player->HasSpell(SPELL_RITUAL_OF_SOULS_R2) ? GO_SOULWELL_R2 : GO_SOULWELL_R1;
            spanish = "Ritual de Almas colocado en tu posicion.";
            english = "Ritual of Souls placed at your position.";
            break;
        default:
            return;
    }

    GameObject* go = player->SummonGameObject(goEntry, player->GetPositionX(), player->GetPositionY(),
        player->GetPositionZ(), player->GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, _ritualDuration);
    if (!go)
    {
        LOG_ERROR("module.arena_bg_qol", "Failed to summon ritual GO {} for player {}", goEntry, player->GetName());
        return;
    }

    go->SetSpawnedByDefault(false);

    if (WorldSession* session = player->GetSession())
    {
        ChatHandler(session).SendSysMessage(Msg(player, spanish, english));
        session->SendAreaTriggerMessage("{}", Msg(player, spanish, english));
    }
}

void ArenaBgQol::HandleUpdate(Battleground* bg, uint32 diff)
{
    if (!_enabled || !bg)
        return;

    auto itr = _states.find(MakeKey(bg));
    if (itr != _states.end())
        SpawnPendingRituals(bg, itr->second);

    if (!bg->isArena() || !_readyCheckEnabled || bg->GetStatus() != STATUS_WAIT_JOIN)
        return;

    ReadyCheckState& state = itr != _states.end() ? itr->second : GetState(bg);
    int32 remaining = bg->GetStartDelayTime();

    if (state.Status == READY_CHECK_NONE && remaining <= _readyCheckTriggerMs && remaining > _readyCheckSkipToMs)
        SendReadyCheck(bg);

    if (state.Status != READY_CHECK_WAITING)
        return;

    state.ElapsedMs += diff;

    bool timedOut = _readyCheckTimeoutMs && state.ElapsedMs >= _readyCheckTimeoutMs;
    if (timedOut || remaining <= _readyCheckSkipToMs)
        FinishReadyCheck(bg, state, false);
}

void ArenaBgQol::SendReadyCheck(Battleground* bg)
{
    ReadyCheckState& state = GetState(bg);
    state.Status = READY_CHECK_WAITING;
    state.ElapsedMs = 0;
    state.Pending.clear();

    uint32 skipSeconds = uint32(_readyCheckSkipToMs / IN_MILLISECONDS);

    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (!player || !player->IsInWorld() || !player->GetSession())
            continue;

        state.Pending.insert(guid);

        WorldPacket data(MSG_RAID_READY_CHECK, 8);
        data << player->GetGUID();
        player->SendDirectMessage(&data);

        player->GetSession()->SendAreaTriggerMessage("{}", Msg(player,
            "Estas listo? Si todos aceptan, la arena comenzara en breve.",
            "Are you ready? If everyone accepts, the arena will start soon."));
        ChatHandler(player->GetSession()).PSendSysMessage(Msg(player,
            "Ready check: si todos aceptan, la espera pasa a {} segundos.",
            "Ready check: if everyone accepts, the wait drops to {} seconds."), skipSeconds);
    }

    if (state.Pending.empty())
        state.Status = READY_CHECK_FINISHED;
}

void ArenaBgQol::FinishReadyCheck(Battleground* bg, ReadyCheckState& state, bool skipWait)
{
    if (state.Status != READY_CHECK_WAITING)
        return;

    state.Status = READY_CHECK_FINISHED;
    state.Pending.clear();
    SendReadyCheckFinished(bg);

    if (!skipWait)
    {
        if (bg->GetStartDelayTime() > _readyCheckSkipToMs)
            Announce(bg,
                "No todos estan listos. Se mantiene el tiempo de espera.",
                "Not everyone is ready. The remaining wait continues.");
        return;
    }

    int32 remaining = bg->GetStartDelayTime();
    if (remaining > _readyCheckSkipToMs)
        bg->SetStartDelayTime(_readyCheckSkipToMs);

    uint32 skipSeconds = uint32(_readyCheckSkipToMs / IN_MILLISECONDS);
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (!player || !player->GetSession())
            continue;

        ChatHandler(player->GetSession()).PSendSysMessage(Msg(player,
            "Todos listos. La arena comenzara en {} segundos.",
            "Everyone is ready. The arena starts in {} seconds."), skipSeconds);
        player->GetSession()->SendAreaTriggerMessage("{}", Msg(player,
            "Todos listos. La arena comienza en breve.",
            "Everyone is ready. The arena starts soon."));
    }
}

void ArenaBgQol::HandleReadyAnswer(Battleground* bg, Player* player, bool ready)
{
    if (!bg || !player)
        return;

    auto itr = _states.find(MakeKey(bg));
    if (itr == _states.end() || itr->second.Status != READY_CHECK_WAITING)
        return;

    ReadyCheckState& state = itr->second;
    if (!state.Pending.erase(player->GetGUID()))
        return;

    if (!ready)
    {
        FinishReadyCheck(bg, state, false);
        return;
    }

    if (state.Pending.empty())
        FinishReadyCheck(bg, state, true);
}

void ArenaBgQol::Announce(Battleground* bg, char const* spanish, char const* english)
{
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (!player || !player->GetSession())
            continue;

        ChatHandler(player->GetSession()).SendSysMessage(Msg(player, spanish, english));
    }
}

void ArenaBgQol::SendReadyCheckFinished(Battleground* bg)
{
    WorldPacket data(MSG_RAID_READY_CHECK_FINISHED);
    for (auto const& [guid, player] : bg->GetPlayers())
        if (player)
            player->SendDirectMessage(&data);
}

bool ArenaBgQol::HandleReadyCheckPacket(WorldSession* session, WorldPacket const& packet)
{
    if (!_enabled || !_readyCheckEnabled || !session)
        return true;

    if (packet.GetOpcode() != MSG_RAID_READY_CHECK || packet.size() < sizeof(uint8))
        return true;

    Player* player = session->GetPlayer();
    if (!player || !player->IsInWorld())
        return true;

    Battleground* bg = player->GetBattleground();
    if (!bg || !bg->isArena() || bg->GetStatus() != STATUS_WAIT_JOIN)
        return true;

    auto itr = _states.find(MakeKey(bg));
    if (itr == _states.end() || itr->second.Status != READY_CHECK_WAITING)
        return true;

    WorldPacket data(packet);
    data.rpos(0);
    uint8 state = 0;
    data >> state;

    HandleReadyAnswer(bg, player, state != 0);
    return true;
}
