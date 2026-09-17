/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "RaidVendor.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldScript.h"

class RaidVendorWorldScript : public WorldScript
{
public:
    RaidVendorWorldScript() : WorldScript("RaidVendorWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        sRaidVendorMgr->LoadConfig(reload);
        if (reload)
            sRaidVendorMgr->LoadVendors();
    }

    void OnStartup() override
    {
        sRaidVendorMgr->LoadVendors();
    }
};

class npc_raid_vendor : public CreatureScript
{
public:
    npc_raid_vendor() : CreatureScript("npc_raid_vendor") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!sRaidVendorMgr->IsEnabled() || creature->GetEntry() != sRaidVendorMgr->GetNpcEntry())
            return false;

        sRaidVendorMgr->BuildMainMenu(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!sRaidVendorMgr->IsEnabled() || creature->GetEntry() != sRaidVendorMgr->GetNpcEntry())
            return false;

        sRaidVendorMgr->HandleGossipSelect(player, creature, action);
        return true;
    }
};

void AddSC_raid_vendor()
{
    new RaidVendorWorldScript();
    new npc_raid_vendor();
}
