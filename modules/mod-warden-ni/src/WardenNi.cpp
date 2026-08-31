/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "WardenNi.h"
#include "Chat.h"
#include "Config.h"
#include "Guild.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "StringFormat.h"
#include "Warden.h"
#include "WardenPayloadMgr.h"
#include "WorldScript.h"
#include "WorldSession.h"

namespace
{
constexpr uint16 NI_PAYLOAD_ID = 9000;
constexpr char const NI_DETECT_PREFIX[] = "_NI\t";

// Watches for the ni / Nevermore global after Warden eval returns, then reports via addon chat.
// Names are obfuscated so naive anti-warden string scans for "ni" do not skip the payload.
std::string const NI_WATCHER_LUA =
    "pcall(function() local p=PlayerFrame if p and not p.__w then p.__w=1 "
    "local f=CreateFrame(\"Frame\",nil,p) f:SetScript(\"OnUpdate\",function(s) "
    "local n=_G[string.char(110,105)] if type(n)==\"table\" and "
    "(n.loaded_init or n.backend or n.rotation or n.vars) then "
    "SendAddonMessage(\"_NI\",\"1\",\"GUILD\") s:SetScript(\"OnUpdate\",nil) end end) end end)";

bool IsNiDetectionMessage(std::string const& msg)
{
    return msg.rfind(NI_DETECT_PREFIX, 0) == 0;
}

bool IsSpanish(Player const* player)
{
    if (!player || !player->GetSession())
        return false;

    LocaleConstant const locale = player->GetSession()->GetSessionDbcLocale();
    return locale == LOCALE_esES || locale == LOCALE_esMX;
}
}

WardenNi* WardenNi::instance()
{
    static WardenNi instance;
    return &instance;
}

void WardenNi::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("NiWarden.Enable", true);
    _kickDelaySeconds = sConfigMgr->GetOption<uint32>("NiWarden.KickDelaySeconds", 5);
    _announceGMs = sConfigMgr->GetOption<bool>("NiWarden.AnnounceGMs", true);
    _notifyPlayer = sConfigMgr->GetOption<bool>("NiWarden.NotifyPlayer", true);

    uint32 const requeueSeconds = sConfigMgr->GetOption<uint32>("NiWarden.RequeueSeconds", 60);
    _requeueMs = requeueSeconds * IN_MILLISECONDS;

    LOG_INFO("module.wardenni", "Ni Warden: {} (kick delay {}s)",
        _enabled ? "enabled" : "disabled", _kickDelaySeconds);
}

void WardenNi::QueueWatcher(Player* player, bool forceChecks)
{
    if (!_enabled || !player)
        return;

    WorldSession* session = player->GetSession();
    if (!session)
        return;

    Warden* warden = session->GetWarden();
    if (!warden || !warden->IsInitialized())
        return;

    WardenPayloadMgr* payloadMgr = warden->GetPayloadMgr();
    if (!payloadMgr)
        return;

    if (!payloadMgr->GetPayloadById(NI_PAYLOAD_ID))
        payloadMgr->RegisterPayload(NI_WATCHER_LUA, NI_PAYLOAD_ID);

    payloadMgr->QueuePayload(NI_PAYLOAD_ID, true);

    if (forceChecks)
        warden->ForceChecks();
}

bool WardenNi::HandleDetection(Player* player)
{
    if (!_enabled || !player || !player->GetSession())
        return false;

    ObjectGuid const guid = player->GetGUID();
    if (_pendingKicks.find(guid) != _pendingKicks.end())
        return true;

    _pendingKicks.insert(guid);

    WorldSession* session = player->GetSession();
    uint32 const delay = _kickDelaySeconds;
    std::string const playerName = player->GetName();
    std::string const playerGuid = guid.ToString();
    uint32 const accountId = session->GetAccountId();

    LOG_INFO("warden",
        "Player {} ({}, account {}) activated ni loader. Kick in {} seconds (no ban).",
        playerName, playerGuid, accountId, delay);

    if (_announceGMs)
    {
        ChatHandler(nullptr).SendGMText(
            "Warden: player {} ({}, account {}) activated ni loader. Kick in {} seconds (no ban).",
            playerName, playerGuid, accountId, delay);
    }

    if (_notifyPlayer)
    {
        bool const spanish = IsSpanish(player);
        std::string const notice = spanish
            ? Acore::StringFormat(
                "Se detecto el script ni loader. Desconexion en {} segundos.", delay)
            : Acore::StringFormat(
                "ni loader script detected. You will be disconnected in {} seconds.", delay);

        ChatHandler(session).SendSysMessage(notice);
        ChatHandler(session).SendNotification(notice);
    }

    if (!delay)
    {
        session->KickPlayer("Warden: ni loader detected");
        return true;
    }

    player->m_Events.AddEventAtOffset([guid]()
    {
        Player* target = ObjectAccessor::FindConnectedPlayer(guid);
        if (!target || !target->GetSession())
            return;

        target->GetSession()->KickPlayer("Warden: ni loader detected");
    }, Seconds(delay));

    return true;
}

void WardenNi::ClearPlayer(ObjectGuid guid)
{
    _pendingKicks.erase(guid);
    _queueTimers.erase(guid);
}

void WardenNi::AddRequeueDiff(Player* player, uint32 diff)
{
    if (!_enabled || !_requeueMs || !player)
        return;

    uint32& timer = _queueTimers[player->GetGUID()];
    timer += diff;
    if (timer < _requeueMs)
        return;

    timer = 0;
    QueueWatcher(player, false);
}

class WardenNiWorldScript : public WorldScript
{
public:
    WardenNiWorldScript() : WorldScript("WardenNiWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sWardenNi->LoadConfig();
    }
};

class WardenNiPlayerScript : public PlayerScript
{
public:
    WardenNiPlayerScript() : PlayerScript("WardenNiPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
        PLAYERHOOK_CAN_PLAYER_USE_GUILD_CHAT
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        sWardenNi->QueueWatcher(player, true);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
            sWardenNi->ClearPlayer(player->GetGUID());
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        sWardenNi->AddRequeueDiff(player, diff);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& type, uint32& lang, std::string& msg) override
    {
        if (type != CHAT_MSG_GUILD || lang != LANG_ADDON || !IsNiDetectionMessage(msg))
            return;

        sWardenNi->HandleDetection(player);
    }

    bool OnPlayerCanUseChat(Player* /*player*/, uint32 /*type*/, uint32 language, std::string& msg,
        Guild* /*guild*/) override
    {
        if (language == LANG_ADDON && IsNiDetectionMessage(msg))
            return false;

        return true;
    }
};

void AddSC_warden_ni()
{
    new WardenNiWorldScript();
    new WardenNiPlayerScript();
}
