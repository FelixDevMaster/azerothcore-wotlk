/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MODULE_MYTHIC_PLUS_MGR_H
#define MODULE_MYTHIC_PLUS_MGR_H

#include "MythicPlus.h"
#include <string>
#include <unordered_set>
#include <vector>

class ChatHandler;
class ItemTemplate;
class WorldPacket;
class WorldSession;

class MythicPlusMgr
{
public:
    static MythicPlusMgr* instance();

    void LoadConfig(bool reload);
    void EnsureDatabase();
    void Update(uint32 diff);

    bool StartRun(Player* player, std::string& error);
    bool TeleportToKey(Player* player, std::string& error);
    bool TryConsumeLfgPacket(WorldSession* session, WorldPacket const& packet);
    bool TryConsumeItemQuery(WorldSession* session, WorldPacket const& packet);
    void RefreshKeyItem(Player* player);
    void AbortTeleportCheck(ObjectGuid groupGuid, uint8 state);
    bool ClaimStarterKey(Player* player, std::string& error);
    bool ClaimVaultSlot(Player* player, uint8 slot, std::string& error);
    void EnsureVaultChoices(Player* player);
    void SendVaultPreview(Player* player);
    [[nodiscard]] uint32 GetNextResetTime() const;
    std::string FormatNextReset(Player const* player) const;
    static std::string LocalizedItemName(Player const* player, uint32 itemId);
    static std::string ItemChatLink(Player const* player, uint32 itemId);
    bool SetKey(Player* player, uint8 dungeonId, uint8 level, std::string& error);
    bool ForceComplete(Player* player, std::string& error);

    void HandleLogin(Player* player);
    void HandleLogout(ObjectGuid guid);
    void HandleGroupMemberRemoved(ObjectGuid groupGuid);
    void HandlePlayerDeath(Player* player);
    void HandlePlayerEnter(Map* map, Player* player);
    void HandlePlayerLeave(Map* map, Player* player);
    void HandleCreatureAdd(Creature* creature);
    void HandleUnitDeath(Unit* unit, Unit* killer);
    void UpdateRun(Map* map, uint32 diff);
    void DestroyMap(Map* map);

    void ModifyCreatureDamage(Unit* attacker, Unit* victim, uint32& damage);
    void ModifyHealReceived(Unit* target, uint32& heal);
    void OnCreatureMeleeHit(Unit* attacker, Unit* victim);

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] uint32 GetNpcEntry() const { return _npcEntry; }
    [[nodiscard]] uint32 GetKeystoneItem() const { return _keystoneItem; }
    [[nodiscard]] uint32 GetFontEntry() const { return _fontEntry; }
    [[nodiscard]] uint32 GetSeasonId() const { return _seasonId; }
    [[nodiscard]] uint8 GetWeekIndex() const { return _weekIndex; }
    [[nodiscard]] uint8 GetMinLevel() const { return _minLevel; }
    [[nodiscard]] MythicAffixSet GetWeeklyAffixes() const;
    [[nodiscard]] MythicAffixSet GetAffixesForLevel(uint8 keyLevel) const;

    MythicProfile GetProfile(ObjectGuid guid);
    MythicRun const* GetRun(uint32 instanceId) const;
    MythicRun* GetRun(uint32 instanceId);
    MythicRun const* GetRunForPlayer(Player const* player) const;
    MythicRun* GetRunForPlayer(Player* player);

    void SendStatus(ChatHandler* handler, Player* player) const;
    std::vector<MythicLeaderboardRow> GetLeaderboard(uint32 limit = 15);
    static bool IsSpanish(Player const* player);
    static char const* Text(Player const* player, char const* en, char const* es);
    static char const* AffixName(uint8 affix, bool spanish);
    static char const* AffixDesc(uint8 affix, bool spanish);
    static char const* DungeonName(uint8 dungeonId, bool spanish);
    std::string KeystoneItemName(Player const* player);
    std::string KeystoneItemDescription(Player const* player);

    static bool IsBoss(Creature const* creature);
    static bool IsEnemyForcesCreature(Creature const* creature);
    float KeyMultiplier(uint8 level) const;
    float DamageMultiplier(MythicRun const& run, Creature const* creature) const;

private:
    MythicPlusMgr() = default;

    void ProcessLuaRequests();
    void CheckWeekReset();
    [[nodiscard]] uint32 LastWeeklyReset(uint32 now) const;
    [[nodiscard]] bool IsWeeklyResetAligned(uint32 timestamp) const;
    void LoadState();
    void SaveState();
    void LoadProfile(ObjectGuid guid);
    void SaveProfile(ObjectGuid guid);
    void SaveBest(ObjectGuid guid, uint8 dungeonId, bool tyrannical, uint8 level, float score);
    void RecalcOverall(ObjectGuid guid);
    void EnsureKeyItem(Player* player);
    void RemoveKeyItem(Player* player);
    void GiveItem(Player* player, uint32 itemId, uint32 count);
    void SendCustomItemQuery(Player* player, uint32 itemId);
    void BuildItemQueryPacket(WorldPacket& data, ItemTemplate const* proto, std::string const& name,
        std::string const& description) const;
    void Announce(Map* map, std::string const& message) const;
    void Announce(Map* map, std::string const& en, std::string const& es) const;
    void SendRunObjective(Player* player, MythicRun const& run) const;
    void SendRunObjective(Map* map, MythicRun const& run) const;
    void WriteLiveState(MythicRun const& run);
    void ClearLiveState(uint32 instanceId);
    void ScaleCreature(Creature* creature, MythicRun& run);
    void PrepareInstance(Map* map, MythicRun& run);
    void SpawnSeasonal(Map* map, MythicRun& run, Player* source);
    void CompleteRun(Map* map, MythicRun& run, bool timed);
    void FailRun(MythicRun& run, bool abandon);
    void RewardRun(Player* player, MythicRun const& run, bool endChest);
    void RewardGear(Player* player, uint8 keyLevel, bool vault);
    void GiveEmblems(Player* player, uint8 keyLevel);
    static MythicLootRole LootRoleFor(Player const* player);
    bool ItemFitsSpec(ItemTemplate const* proto, Player const* player, uint8 keyLevel, bool relaxStats) const;
    void BuildSpecLootPool(Player const* player, uint8 keyLevel, bool relaxStats, std::vector<uint32>& out) const;
    uint32 PickSpecItem(Player* player, uint8 keyLevel, std::unordered_set<uint32> const& exclude);
    uint32 ResiduumForLevel(uint8 level) const;
    uint8 UpgradeForTime(uint32 remainingMs, uint32 limitMs) const;
    float ScoreForRun(uint8 level, uint8 upgrade, bool timed, float overtimeRatio) const;
    uint8 PickRandomDungeon(uint8 except = 0) const;
    uint8 SeasonalAffixForSeason(uint32 seasonId) const;
    void ApplyAffixDeath(Map* map, MythicRun& run, Creature* creature);
    void TickAffixes(Map* map, MythicRun& run, uint32 diff);
    void GrantM0Key(Player* player, uint32 mapId);
    Player* FirstOnlineMember(MythicRun const& run) const;
    static uint32 ForceValue(Creature const* creature);
    static uint64 CrowdControlMask();

    bool CollectGroupMembers(Player* player, std::vector<Player*>& members, std::string& error) const;
    bool ValidatePartySize(Player* player, std::vector<Player*> const& members, std::string& error) const;
    bool ValidatePartyComposition(Player* reporter, std::vector<Player*> const& members,
        std::string& error) const;
    bool PlayerReadyForTeleport(Player* player, std::string& error) const;
    bool CanPlayerPerformRole(Player* player, uint8 role) const;
    bool IsInTeleportCheck(ObjectGuid guid) const;
    uint32 FindHeroicLfgDungeon(uint32 mapId) const;
    void TickTeleportChecks(uint32 diff);
    void HandleTeleportSetRoles(Player* player, uint8 roles);
    void HandleTeleportLeave(Player* player);
    void BroadcastRoleCheck(MythicTeleportCheck const& check, uint8 state, bool sendPartyUpdate,
        ObjectGuid chosenGuid = ObjectGuid::Empty, uint8 chosenRoles = 0) const;
    void FinishTeleportCheck(ObjectGuid groupGuid, uint8 state);
    bool TeleportGroup(MythicTeleportCheck const& check, std::string& error);

    bool _enabled = true;
    bool _announce = true;
    bool _allowTeleport = true;
    bool _requireRoles = true;
    uint8 _minLevel = 80;
    uint8 _minPlayers = 5;
    uint8 _maxPlayers = 5;
    uint32 _roleCheckMs = 45000;
    uint8 _maxKeyLevel = 25;
    uint32 _deathPenaltyMs = 5000;
    float _scalePerLevel = 0.08f;
    uint8 _forcesPercent = 80;
    uint32 _seasonWeeks = 12;
    uint32 _npcEntry = NPC_MYTHIC_BROKER;
    uint32 _keystoneItem = ITEM_MYTHIC_KEYSTONE;
    uint32 _fontEntry = GO_MYTHIC_FONT;
    uint32 _residuumItem = ITEM_MYTHIC_RESIDUUM;
    uint32 _abandonMs = 60000;
    uint8 _resetWday = 5;   // Friday (tm_wday)
    uint8 _resetHour = 20;  // 20:00 server time

    uint32 _weekStart = 0;
    uint32 _seasonId = 1;
    uint8 _weekIndex = 0;
    uint32 _updateTimer = 0;

    std::unordered_map<uint32, MythicRun> _runs;
    std::unordered_map<ObjectGuid, uint32> _playerRun;
    std::unordered_map<ObjectGuid, MythicProfile> _profiles;
    std::unordered_map<ObjectGuid, MythicTeleportCheck> _teleportChecks;
    std::unordered_map<ObjectGuid, ObjectGuid> _playerTeleportCheck;
};

#define sMythicPlus MythicPlusMgr::instance()

#endif
