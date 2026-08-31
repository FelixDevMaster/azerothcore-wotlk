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
#include "WorldSessionMgr.h"

namespace
{
// Warden string length is a uint8 — payloads longer than 255 bytes are truncated and never run.
constexpr uint16 NI_PAYLOAD_HOOK_ID = 9000;
constexpr uint16 NI_PAYLOAD_SCAN_ID = 9001;
constexpr uint16 NI_PAYLOAD_NI_ID = 9002;
constexpr uint32 NI_RETRY_MS = 3000;
constexpr char const NI_DETECT_PREFIX[] = "_NI\t";

// ni-v3 never puts `ni` on _G. Profile toggle prints "Primary started"; the rotation
// folder/name is "che paladin". Warden payload length is a uint8 (max 255 bytes).
constexpr char NI_HOOK_LUA[] =
    "local f=ChatFrame1 if f and not f.__n then f.__n=1 local o=f.AddMessage "
    "f.AddMessage=function(s,m,...) if type(m)==\"string\" and "
    "(m:find(\"Primary started\") or m:find(\"che paladin\")) then "
    "SendAddonMessage(\"_NI\",\"1\",\"GUILD\") end return o(s,m,...) end end";

constexpr char NI_SCAN_LUA[] =
    "local f=ChatFrame1 if f then for i=1,f:GetNumMessages() do local t=f:GetMessageInfo(i) "
    "if t and (t:find(\"Primary started\") or t:find(\"che paladin\")) then "
    "SendAddonMessage(\"_NI\",\"1\",\"GUILD\") break end end end";

constexpr char NI_GLOBAL_LUA[] =
    "local n=_G[string.char(110,105)] if type(n)==\"table\" then SendAddonMessage(\"_NI\",\"1\",\"GUILD\") end";

static_assert(sizeof(NI_HOOK_LUA) - 1 <= 255, "Warden payload length is a uint8");
static_assert(sizeof(NI_SCAN_LUA) - 1 <= 255, "Warden payload length is a uint8");
static_assert(sizeof(NI_GLOBAL_LUA) - 1 <= 255, "Warden payload length is a uint8");

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
    _kickDelaySeconds = sConfigMgr->GetOption<uint32>("NiWarden.KickDelaySeconds", 0);
    _announceWorld = sConfigMgr->GetOption<bool>("NiWarden.AnnounceWorld", true);
    _announceGMs = sConfigMgr->GetOption<bool>("NiWarden.AnnounceGMs", false);
    _notifyPlayer = sConfigMgr->GetOption<bool>("NiWarden.NotifyPlayer", true);

    uint32 const requeueSeconds = sConfigMgr->GetOption<uint32>("NiWarden.RequeueSeconds", 60);
    _requeueMs = requeueSeconds * IN_MILLISECONDS;

    LOG_INFO("module.wardenni", "Ni Warden: {} (world announce {}, kick delay {}s)",
        _enabled ? "enabled" : "disabled", _announceWorld ? "yes" : "no", _kickDelaySeconds);
}

bool WardenNi::QueueWatcher(Player* player, bool forceChecks)
{
    if (!_enabled || !player)
        return false;

    WorldSession* session = player->GetSession();
    if (!session)
        return false;

    Warden* warden = session->GetWarden();
    if (!warden || !warden->IsInitialized())
        return false;

    WardenPayloadMgr* payloadMgr = warden->GetPayloadMgr();
    if (!payloadMgr)
        return false;

    payloadMgr->RegisterPayload(NI_HOOK_LUA, NI_PAYLOAD_HOOK_ID, true);
    payloadMgr->RegisterPayload(NI_SCAN_LUA, NI_PAYLOAD_SCAN_ID, true);
    payloadMgr->RegisterPayload(NI_GLOBAL_LUA, NI_PAYLOAD_NI_ID, true);
    payloadMgr->QueuePayload(NI_PAYLOAD_HOOK_ID, true);
    payloadMgr->QueuePayload(NI_PAYLOAD_SCAN_ID, true);
    payloadMgr->QueuePayload(NI_PAYLOAD_NI_ID, true);

    if (forceChecks)
        warden->ForceChecks();

    _injected.insert(player->GetGUID());

    if (forceChecks)
    {
        LOG_INFO("module.wardenni", "Queued ni watcher for {} (account {})",
            player->GetName(), session->GetAccountId());
    }

    return true;
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
        "Player {} ({}, account {}) activated che paladin / ni loader. Kick in {} seconds (no ban).",
        playerName, playerGuid, accountId, delay);

    if (_announceWorld)
    {
        std::string const worldMsg = delay
            ? Acore::StringFormat(
                "|cffff0000[Warden]|r {} ha activado el script che paladin (ni loader). "
                "Expulsado en {} segundos.",
                playerName, delay)
            : Acore::StringFormat(
                "|cffff0000[Warden]|r {} ha activado el script che paladin (ni loader). "
                "Ha sido expulsado.",
                playerName);

        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, worldMsg);
        ChatHandler(nullptr).SendWorldText("{}", worldMsg);
    }

    if (_announceGMs)
    {
        ChatHandler(nullptr).SendGMText(
            "Warden: player {} ({}, account {}) activated che paladin / ni loader. "
            "Kick in {} seconds (no ban).",
            playerName, playerGuid, accountId, delay);
    }

    if (_notifyPlayer && delay)
    {
        bool const spanish = IsSpanish(player);
        std::string const notice = spanish
            ? Acore::StringFormat(
                "Se detecto el script che paladin (ni loader). Desconexion en {} segundos.", delay)
            : Acore::StringFormat(
                "che paladin (ni loader) detected. You will be disconnected in {} seconds.", delay);

        ChatHandler(session).SendSysMessage(notice);
        ChatHandler(session).SendNotification(notice);
    }

    if (!delay)
    {
        session->KickPlayer("Warden: che paladin / ni loader detected");
        return true;
    }

    player->m_Events.AddEventAtOffset([guid]()
    {
        Player* target = ObjectAccessor::FindConnectedPlayer(guid);
        if (!target || !target->GetSession())
            return;

        target->GetSession()->KickPlayer("Warden: che paladin / ni loader detected");
    }, Seconds(delay));

    return true;
}

void WardenNi::ClearPlayer(ObjectGuid guid)
{
    _pendingKicks.erase(guid);
    _injected.erase(guid);
    _queueTimers.erase(guid);
}

void WardenNi::AddRequeueDiff(Player* player, uint32 diff)
{
    if (!_enabled || !player)
        return;

    ObjectGuid const guid = player->GetGUID();
    bool const injected = _injected.find(guid) != _injected.end();
    uint32 const interval = injected ? _requeueMs : NI_RETRY_MS;
    if (!interval)
        return;

    uint32& timer = _queueTimers[guid];
    timer += diff;
    if (timer < interval)
        return;

    timer = 0;
    if (QueueWatcher(player, !injected))
        _injected.insert(guid);
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
        PLAYERHOOK_CAN_PLAYER_USE_GUILD_CHAT,
        PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player)
            return;

        ObjectGuid const guid = player->GetGUID();
        player->m_Events.AddEventAtOffset([guid]()
        {
            Player* target = ObjectAccessor::FindConnectedPlayer(guid);
            if (target)
                sWardenNi->QueueWatcher(target, true);
        }, 2s);
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

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& lang, std::string& msg) override
    {
        if (lang != LANG_ADDON || !IsNiDetectionMessage(msg))
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

    bool OnPlayerCanUseChat(Player* /*player*/, uint32 /*type*/, uint32 language, std::string& msg,
        Player* /*receiver*/) override
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
