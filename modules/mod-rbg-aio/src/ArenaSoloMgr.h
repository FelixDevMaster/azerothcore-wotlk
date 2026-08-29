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

#ifndef MODULE_ARENA_SOLO_MGR_H
#define MODULE_ARENA_SOLO_MGR_H

#include "ObjectGuid.h"
#include "Optional.h"
#include "PvPShared.h"
#include "SharedDefines.h"
#include <algorithm>
#include <array>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Battleground;
class Map;
class Player;

// Values are persisted in arena_solo_stats.bracket, so never renumber them.
enum ArenaSoloBracket : uint8
{
    ARENA_SOLO_BRACKET_1V1 = 0,
    ARENA_SOLO_BRACKET_3V3 = 1,
    ARENA_SOLO_BRACKET_2V2 = 2,
    ARENA_SOLO_BRACKET_MAX = 3
};

struct ArenaSoloStats
{
    uint32 Rating = 1500;
    uint32 MMR = 1500;
    uint32 Games = 0;
    uint32 Wins = 0;
    uint16 WeekGames = 0;
    uint16 WeekWins = 0;
    uint16 WeekPoints = 0;
    uint32 HighestRating = 1500;
    // Filled when the bracket uses a personal arena_team.
    uint32 TeamRating = 0;
    std::string TeamName;
};

// One queue entry is one entering unit: a single player in the solo brackets,
// or a party of two in 2v2 (each player keeps a personal arena team).
struct ArenaSoloQueueEntry
{
    ObjectGuid LeaderGuid;
    std::vector<ObjectGuid> Members;
    TeamId Team = TEAM_ALLIANCE;
    uint32 Rating = 1500;
    uint32 MMR = 1500;
    uint32 JoinTime = 0;
    uint32 Healers = 0;
    uint32 ArenaTeamId = 0;
    uint8 PlayerClass = 0;
    uint32 TalentTab = 0;

    [[nodiscard]] bool Contains(ObjectGuid guid) const
    {
        return std::find(Members.begin(), Members.end(), guid) != Members.end();
    }
};

struct ArenaSoloMatch
{
    uint32 InstanceId = 0;
    uint8 Bracket = ARENA_SOLO_BRACKET_1V1;
    uint32 AllianceMMR = 1500;
    uint32 HordeMMR = 1500;
    std::vector<ObjectGuid> Alliance;
    std::vector<ObjectGuid> Horde;
    std::string AllianceComp;
    std::string HordeComp;
};

struct ArenaSoloBracketConfig
{
    bool Enabled = true;
    uint32 TeamSize = 1;
    // Players that queue together: 1 for the solo brackets, TeamSize for premade
    // brackets like 2v2. TeamSize must stay a multiple of it.
    uint32 GroupSize = 1;
    uint8 ArenaType = 2;
    // When set, rating lives on arena_team / arena_team_member. Each player
    // owns a personal team of ArenaTeamType (captain = character GUID).
    bool UseCoreArenaTeam = false;
    uint8 ArenaTeamType = 0;
    uint8 ArenaTeamSlot = 0;
    bool PreferComps = false;
    uint32 MinLevel = 80;
    uint32 StartRating = 1500;
    uint32 StartMMR = 1500;
    uint32 MaxRatingDiff = 150;
    uint32 RatingDiscardTimer = 300000;
    uint32 HonorWin = 189;
    uint32 HonorLoss = 94;
    uint32 PointsWin = 30;
    uint32 WeeklyCap = 1350;
    bool RequireRoleBalance = false;
    PvPRating::EloConfig Elo;
};

class ArenaSoloMgr
{
public:
    static ArenaSoloMgr* instance();

    void LoadConfig(bool reload);
    void EnsureDatabase();
    void Update(uint32 diff);

    bool Queue(Player* player, uint8 bracket, std::string& error);
    bool Dequeue(Player* player, std::string& error);
    [[nodiscard]] bool IsQueued(ObjectGuid guid) const;
    [[nodiscard]] Optional<uint8> GetQueuedBracket(ObjectGuid guid) const;
    void RemovePlayer(ObjectGuid guid);

    ArenaSoloStats GetStats(ObjectGuid guid, uint8 bracket);
    void HandleBattlegroundEnd(Battleground* bg, TeamId winner);
    void HandleBattlegroundDestroy(Battleground* bg);
    void HandleDesertion(Player* player, uint8 desertionType);

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] bool IsBracketEnabled(uint8 bracket) const;
    [[nodiscard]] uint32 GetQueuedCount(uint8 bracket) const;
    [[nodiscard]] ArenaSoloBracketConfig const& GetBracketConfig(uint8 bracket) const;
    [[nodiscard]] uint32 GetNPCEntry() const { return _npcEntry; }
    [[nodiscard]] bool UsesCoreArenaTeam(uint8 bracket) const;
    bool EnsurePersonalArenaTeam(Player* player, uint8 bracket, std::string& error);

    std::vector<PvPLeaderboardRow> GetLeaderboard(uint8 bracket, uint32 limit = 15);

    static char const* GetBracketName(uint8 bracket);
    static Optional<uint8> ParseBracket(std::string_view token);

private:
    ArenaSoloMgr() = default;

    void ProcessLuaRequests();
    void PruneQueue(uint8 bracket);
    void TryMatch(uint8 bracket);
    bool StartMatch(uint8 bracket, std::vector<ArenaSoloQueueEntry> const& alliance,
        std::vector<ArenaSoloQueueEntry> const& horde, std::string const& allianceComp,
        std::string const& hordeComp);
    bool CanQueuePlayer(Player* player, uint8 bracket, std::string& error) const;
    bool CollectMembers(Player* leader, uint8 bracket, std::vector<Player*>& members, std::string& error) const;
    bool BindArenaTeam(std::vector<Player*> const& members, uint8 bracket, ArenaSoloQueueEntry& entry,
        std::string& error);
    bool BuildEntry(Player* leader, uint8 bracket, ArenaSoloQueueEntry& entry, std::string& error);
    ArenaSoloStats LoadArenaTeamMemberStats(ObjectGuid guid, uint8 bracket) const;
    bool RevalidateEntry(ArenaSoloQueueEntry const& entry, uint8 bracket, std::string& error);
    void ApplyArenaTeamSide(uint8 bracket, std::vector<ObjectGuid> const& side, uint32 opponentMMR, bool won,
        Map const* bgMap);
    bool BuildTeams(uint8 bracket, std::vector<ArenaSoloQueueEntry>& alliance,
        std::vector<ArenaSoloQueueEntry>& horde, std::string& allianceComp, std::string& hordeComp);
    bool BuildOfficialComps(std::vector<ArenaSoloQueueEntry> const& pool,
        std::vector<ArenaSoloQueueEntry>& alliance, std::vector<ArenaSoloQueueEntry>& horde,
        std::string& allianceComp, std::string& hordeComp) const;
    [[nodiscard]] bool IsInMatch(ObjectGuid guid) const;
    static uint32 TotalMMR(std::vector<ArenaSoloQueueEntry> const& side);
    static uint32 SidePlayerCount(std::vector<ArenaSoloQueueEntry> const& side);

    ArenaSoloStats LoadStats(ObjectGuid guid, uint8 bracket);
    void SaveStats(ObjectGuid guid, uint8 bracket, ArenaSoloStats const& stats);
    void ApplyResult(ObjectGuid guid, uint8 bracket, uint32 opponentMMR, bool won);
    static uint32 AverageMMR(std::vector<ArenaSoloQueueEntry> const& team);
    void CheckWeekReset();

    bool _enabled = true;
    bool _crossFaction = true;
    uint32 _queueUpdateInterval = 1000;
    uint32 _npcEntry = 190011;
    bool _announceQueue = false;

    uint32 _updateTimer = 0;
    uint32 _weekStart = 0;

    std::array<ArenaSoloBracketConfig, ARENA_SOLO_BRACKET_MAX> _brackets;
    std::array<std::list<ArenaSoloQueueEntry>, ARENA_SOLO_BRACKET_MAX> _queues;
    std::unordered_map<uint32, uint8> _queuedPlayers;
    std::unordered_map<uint32, ArenaSoloMatch> _matches;
    std::unordered_map<uint64, ArenaSoloStats> _statsCache;
};

#define sArenaSoloMgr ArenaSoloMgr::instance()

#endif
