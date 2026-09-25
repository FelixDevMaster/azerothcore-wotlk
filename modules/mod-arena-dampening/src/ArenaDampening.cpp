/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "ArenaDampening.h"
#include "Battleground.h"
#include "Chat.h"
#include "Config.h"
#include "Log.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "StringFormat.h"
#include "Util.h"
#include "WorldSession.h"
#include <algorithm>
#include <string>

ArenaDampening* ArenaDampening::instance()
{
    static ArenaDampening instance;
    return &instance;
}

void ArenaDampening::LoadConfig(bool /*reload*/)
{
    _enabled = sConfigMgr->GetOption<bool>("ArenaDampening.Enable", true);
    _ratedOnly = sConfigMgr->GetOption<bool>("ArenaDampening.RatedOnly", false);
    _applyToPets = sConfigMgr->GetOption<bool>("ArenaDampening.ApplyToPets", true);
    _overrideWotlkAura = sConfigMgr->GetOption<bool>("ArenaDampening.OverrideWotLKAura", true);
    _announce = sConfigMgr->GetOption<bool>("ArenaDampening.Announce", true);
    _announceEveryPercent = sConfigMgr->GetOption<uint32>("ArenaDampening.AnnounceEveryPercent", 10);
    _startPercent = sConfigMgr->GetOption<uint32>("ArenaDampening.StartPercent", 1);
    _incrementPercent = sConfigMgr->GetOption<uint32>("ArenaDampening.IncrementPercent", 1);
    _maxPercent = sConfigMgr->GetOption<uint32>("ArenaDampening.MaxPercent", 100);
    _tickMs = sConfigMgr->GetOption<uint32>("ArenaDampening.TickSeconds", 10) * IN_MILLISECONDS;
    _startDelay2v2Ms = sConfigMgr->GetOption<uint32>("ArenaDampening.StartDelay.2v2", 300) * IN_MILLISECONDS;
    _startDelay3v3Ms = sConfigMgr->GetOption<uint32>("ArenaDampening.StartDelay.3v3", 300) * IN_MILLISECONDS;
    _startDelay5v5Ms = sConfigMgr->GetOption<uint32>("ArenaDampening.StartDelay.5v5", 300) * IN_MILLISECONDS;

    if (!_startPercent)
        _startPercent = 1;
    if (!_incrementPercent)
        _incrementPercent = 1;
    if (!_maxPercent)
        _maxPercent = 100;
    if (_startPercent > _maxPercent)
        _startPercent = _maxPercent;
    if (!_tickMs)
        _tickMs = 10 * IN_MILLISECONDS;

    LOG_INFO("module", "Arena Dampening: {} (2v2 {}s, 3v3 {}s, 5v5 {}s, +{}% / {}s, cap {}%)",
        _enabled ? "enabled" : "disabled",
        _startDelay2v2Ms / IN_MILLISECONDS,
        _startDelay3v3Ms / IN_MILLISECONDS,
        _startDelay5v5Ms / IN_MILLISECONDS,
        _incrementPercent,
        _tickMs / IN_MILLISECONDS,
        _maxPercent);
}

bool ArenaDampening::IsMatchTracked(Battleground const* bg) const
{
    if (!_enabled || !bg || !bg->isArena())
        return false;

    if (_ratedOnly && !bg->isRated())
        return false;

    return true;
}

ArenaDampeningMatch* ArenaDampening::GetMatch(uint32 instanceId)
{
    auto itr = _matches.find(instanceId);
    return itr != _matches.end() ? &itr->second : nullptr;
}

ArenaDampeningMatch const* ArenaDampening::GetMatch(uint32 instanceId) const
{
    auto const itr = _matches.find(instanceId);
    return itr != _matches.end() ? &itr->second : nullptr;
}

uint32 ArenaDampening::GetStartDelayMs(uint8 arenaType) const
{
    switch (arenaType)
    {
        case ARENA_TYPE_2v2:
            return _startDelay2v2Ms;
        case ARENA_TYPE_5v5:
            return _startDelay5v5Ms;
        case ARENA_TYPE_3v3:
        default:
            return _startDelay3v3Ms;
    }
}

uint32 ArenaDampening::GetPercent(Battleground const* bg) const
{
    if (!bg)
        return 0;

    if (ArenaDampeningMatch const* match = GetMatch(bg->GetInstanceID()))
        return match->Percent;

    return 0;
}

uint32 ArenaDampening::GetPercent(Unit const* unit) const
{
    if (!unit)
        return 0;

    Player const* player = unit->ToPlayer();
    if (!player)
        player = unit->GetCharmerOrOwnerPlayerOrPlayerItself();

    if (!player)
        return 0;

    return GetPercent(player->GetBattleground());
}

uint32 ArenaDampening::GetElapsedMs(Battleground const* bg) const
{
    if (!bg)
        return 0;

    if (ArenaDampeningMatch const* match = GetMatch(bg->GetInstanceID()))
        return match->ElapsedMs;

    return 0;
}

bool ArenaDampening::IsSpanish(Player const* player) const
{
    if (!player || !player->GetSession())
        return false;

    LocaleConstant const locale = player->GetSession()->GetSessionDbcLocale();
    return locale == LOCALE_esES || locale == LOCALE_esMX;
}

void ArenaDampening::HandleArenaStart(Battleground* bg)
{
    if (!IsMatchTracked(bg))
        return;

    ArenaDampeningMatch& match = _matches[bg->GetInstanceID()];
    match = {};

    if (_overrideWotlkAura)
        ApplyToBattleground(bg, 0);
}

void ArenaDampening::HandleBattlegroundUpdate(Battleground* bg, uint32 diff)
{
    if (!IsMatchTracked(bg) || bg->GetStatus() != STATUS_IN_PROGRESS)
        return;

    ArenaDampeningMatch& match = _matches[bg->GetInstanceID()];
    match.ElapsedMs += diff;

    if (!match.Started)
    {
        if (match.ElapsedMs < GetStartDelayMs(bg->GetArenaType()))
            return;

        StartDampening(bg, match);
        return;
    }

    if (match.Percent >= _maxPercent)
        return;

    match.TickTimerMs += diff;
    while (match.TickTimerMs >= _tickMs && match.Percent < _maxPercent)
    {
        match.TickTimerMs -= _tickMs;
        IncrementDampening(bg, match);
    }
}

void ArenaDampening::HandleAddPlayer(Battleground* bg, Player* player)
{
    if (!IsMatchTracked(bg) || !player)
        return;

    ArenaDampeningMatch* match = GetMatch(bg->GetInstanceID());
    uint32 const percent = match ? match->Percent : 0;
    if (percent || _overrideWotlkAura)
        ApplyToPlayer(player, percent);
}

void ArenaDampening::HandleBattlegroundEnd(Battleground* bg)
{
    if (!bg)
        return;

    _matches.erase(bg->GetInstanceID());
}

void ArenaDampening::HandleBattlegroundDestroy(Battleground* bg)
{
    if (!bg)
        return;

    _matches.erase(bg->GetInstanceID());
}

void ArenaDampening::StartDampening(Battleground* bg, ArenaDampeningMatch& match)
{
    match.Started = true;
    match.Percent = std::min(_startPercent, _maxPercent);
    match.TickTimerMs = 0;
    ApplyToBattleground(bg, match.Percent);
    Announce(bg, match.Percent, true);

    LOG_DEBUG("module", "Arena Dampening: match {} started at {}%", bg->GetInstanceID(), match.Percent);
}

void ArenaDampening::IncrementDampening(Battleground* bg, ArenaDampeningMatch& match)
{
    uint32 const next = std::min(match.Percent + _incrementPercent, _maxPercent);
    if (next == match.Percent)
        return;

    match.Percent = next;
    ApplyToBattleground(bg, match.Percent);

    bool const announceTick = _announceEveryPercent && (match.Percent % _announceEveryPercent == 0);
    if (announceTick)
        Announce(bg, match.Percent, false);

    LOG_DEBUG("module", "Arena Dampening: match {} now {}%", bg->GetInstanceID(), match.Percent);
}

void ArenaDampening::ApplyToBattleground(Battleground* bg, uint32 percent)
{
    for (auto const& itr : bg->GetPlayers())
        ApplyToPlayer(itr.second, percent);
}

void ArenaDampening::ApplyToPlayer(Player* player, uint32 percent)
{
    if (!player)
        return;

    ApplyToUnit(player, percent);

    if (!_applyToPets)
        return;

    for (Unit* controlled : player->m_Controlled)
        if (controlled && controlled != player)
            ApplyToUnit(controlled, percent);
}

void ArenaDampening::ApplyToUnit(Unit* unit, uint32 percent)
{
    if (!unit)
        return;

    int32 const amount = -static_cast<int32>(percent);
    if (AuraEffect* effect = unit->GetAuraEffect(SPELL_ARENA_DAMPENING, EFFECT_0))
    {
        if (effect->GetAmount() != amount)
        {
            effect->ChangeAmount(amount);
            if (Aura* aura = effect->GetBase())
                aura->SetNeedClientUpdateForTargets();
        }
        return;
    }

    unit->CastCustomSpell(unit, SPELL_ARENA_DAMPENING, &amount, nullptr, nullptr, true);
}

void ArenaDampening::Announce(Battleground* bg, uint32 percent, bool started) const
{
    if (!_announce || !bg)
        return;

    for (auto const& itr : bg->GetPlayers())
    {
        Player* player = itr.second;
        if (!player || !player->GetSession())
            continue;

        bool const spanish = IsSpanish(player);
        std::string message;
        if (started)
        {
            message = spanish
                ? Acore::StringFormat("Dampening ha comenzado. Sanacion y absorciones reducidas un {}%.", percent)
                : Acore::StringFormat("Dampening has begun. Healing and absorbs reduced by {}%.", percent);
        }
        else
        {
            message = spanish
                ? Acore::StringFormat("Dampening: {}%.", percent)
                : Acore::StringFormat("Dampening: {}%.", percent);
        }

        player->GetSession()->SendAreaTriggerMessage(message);
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff2020{}|r", message);
    }
}

bool ArenaDampening::IsCoreDampenedAbsorb(SpellInfo const* info)
{
    if (!info)
        return false;

    // spell_pri_power_word_shield already applies 74410 to PW:S absorbs
    if (info->SpellFamilyName == SPELLFAMILY_PRIEST && (info->SpellFamilyFlags[0] & 0x1))
        return true;

    // spell_pal_sacred_shield already applies 74410 to the Sacred Shield absorb proc
    if (info->Id == SPELL_SACRED_SHIELD_ABSORB)
        return true;

    return false;
}

void ArenaDampening::HandleAbsorbApply(Unit* unit, Aura* aura)
{
    if (!_enabled || !unit || !aura)
        return;

    SpellInfo const* info = aura->GetSpellInfo();
    if (!info || info->Id == SPELL_ARENA_DAMPENING || IsCoreDampenedAbsorb(info))
        return;

    uint32 const percent = GetPercent(unit);
    if (!percent)
        return;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        AuraEffect* effect = aura->GetEffect(i);
        if (!effect)
            continue;

        AuraType const type = effect->GetAuraType();
        if (type != SPELL_AURA_SCHOOL_ABSORB && type != SPELL_AURA_MANA_SHIELD)
            continue;

        int32 const amount = effect->GetAmount();
        if (amount <= 0)
            continue;

        effect->ChangeAmount(amount - CalculatePct(amount, percent));
    }
}
