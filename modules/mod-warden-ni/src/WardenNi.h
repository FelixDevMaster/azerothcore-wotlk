/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MODULE_WARDEN_NI_H
#define MODULE_WARDEN_NI_H

#include "Common.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <unordered_set>

class Player;

class WardenNi
{
public:
    static WardenNi* instance();

    void LoadConfig();

    bool QueueWatcher(Player* player, bool forceChecks);
    bool HandleDetection(Player* player);
    void ClearPlayer(ObjectGuid guid);
    void AddRequeueDiff(Player* player, uint32 diff);

private:
    WardenNi() = default;

    bool _enabled = true;
    bool _announceWorld = true;
    bool _announceGMs = false;
    bool _notifyPlayer = true;
    uint32 _kickDelaySeconds = 0;
    uint32 _requeueMs = 60000;

    std::unordered_set<ObjectGuid> _pendingKicks;
    std::unordered_set<ObjectGuid> _injected;
    std::unordered_map<ObjectGuid, uint32> _queueTimers;
};

#define sWardenNi WardenNi::instance()

#endif
