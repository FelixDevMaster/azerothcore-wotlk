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

#ifndef MODULE_RBG_MGR_H
#define MODULE_RBG_MGR_H

#include "ObjectGuid.h"
#include "SharedDefines.h"
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Battleground;
class Group;
class Player;

struct RBGPlayerStats
{
    uint32 Rating = 1500;
    uint32 MMR = 1500;
    uint32 Games = 0;
    uint32 Wins = 0;
    uint32 SeasonGames = 0;
    uint32 SeasonWins = 0;
    uint16 WeekGames = 0;
    uint16 WeekWins = 0;
    uint16 WeekConquest = 0;
    uint32 HighestRating = 1500;
};

struct RBGQueueEntry
{
    ObjectGuid LeaderGuid;
    std::vector<ObjectGuid> Members;
    TeamId Team = TEAM_ALLIANCE;
    uint32 Rating = 1500;
    uint32 MMR = 1500;
    uint32 JoinTime = 0;
};

struct RBGMatch
{
    uint32 InstanceId = 0;
    BattlegroundTypeId BgTypeId = BATTLEGROUND_WS;
    uint32 AllianceMMR = 1500;
    uint32 HordeMMR = 1500;
    std::vector<ObjectGuid> Alliance;
    std::vector<ObjectGuid> Horde;
};

struct RBGLeaderboardRow
{
    std::string Name;
    uint32 Rating = 0;
    uint32 Wins = 0;
    uint32 Losses = 0;
};

class RBGMgr
{
public:
    static RBGMgr* instance();

    void LoadConfig(bool reload);
    void EnsureDatabase();
    void Update(uint32 diff);

    bool Queue(Player* leader, std::string& error);
    bool Dequeue(Player* player, std::string& error);
    [[nodiscard]] bool IsQueued(ObjectGuid guid) const;
    void RemovePlayer(ObjectGuid guid);

    RBGPlayerStats GetStats(ObjectGuid guid);
    void HandleBattlegroundEnd(Battleground* bg, TeamId winner);
    void HandleBattlegroundDestroy(Battleground* bg);

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] uint32 GetTeamSize() const { return _teamSize; }
    [[nodiscard]] uint32 GetWeeklyCap() const { return _weeklyCap; }
    [[nodiscard]] uint32 GetNPCEntry() const { return _npcEntry; }
    [[nodiscard]] uint32 GetQueuedTeamCount() const { return static_cast<uint32>(_queue.size()); }

    std::vector<RBGLeaderboardRow> GetLeaderboard(uint32 limit = 20);

private:
    RBGMgr() = default;

    void ProcessLuaRequests();
    void PruneQueue();
    void TryMatch();
    bool StartMatch(RBGQueueEntry& teamA, RBGQueueEntry& teamB);
    bool ValidateGroup(Player* leader, Group* group, std::string& error) const;
    uint32 AverageStat(std::vector<ObjectGuid> const& members, bool mmr);
    RBGPlayerStats LoadStats(ObjectGuid guid);
    void SaveStats(ObjectGuid guid, RBGPlayerStats const& stats);
    void ApplyResult(ObjectGuid guid, uint32 opponentMMR, bool won);
    void CheckWeekReset();
    int32 GetRatingMod(uint32 ownRating, uint32 opponentRating, bool won) const;
    int32 GetMMRMod(uint32 ownMMR, uint32 opponentMMR, bool won) const;
    static float GetChanceAgainst(uint32 ownRating, uint32 opponentRating);
    BattlegroundTypeId PickMap() const;
    void NotifyGroup(Group* group, std::string const& message) const;
    void GrantTitleIfNeeded(Player* player, uint32 rating) const;

    bool _enabled = true;
    uint32 _minLevel = 80;
    uint32 _teamSize = 10;
    bool _allowSameFaction = false;
    uint32 _startRating = 1500;
    uint32 _startMMR = 1500;
    uint32 _maxRatingDiff = 150;
    uint32 _ratingDiscardTimer = 300000;
    uint32 _queueUpdateInterval = 1000;
    uint32 _honorWin = 378;
    uint32 _honorLoss = 189;
    float _arenaPointsWinBase = 180.f;
    float _arenaPointsWinScale = 1.3f;
    uint32 _weeklyCap = 1650;
    uint32 _npcEntry = 190010;
    bool _announceQueue = false;
    bool _enableTitles = false;
    float _winRatingModifier1 = 48.f;
    float _winRatingModifier2 = 24.f;
    float _loseRatingModifier = 24.f;
    float _mmrModifier = 24.f;

    uint32 _updateTimer = 0;
    uint32 _weekStart = 0;

    std::vector<BattlegroundTypeId> _maps;
    std::list<RBGQueueEntry> _queue;
    std::unordered_set<uint32> _queuedPlayers;
    std::unordered_map<uint32, RBGMatch> _matches;
    std::unordered_map<uint32, RBGPlayerStats> _statsCache;
};

#define sRBGMgr RBGMgr::instance()

#endif
