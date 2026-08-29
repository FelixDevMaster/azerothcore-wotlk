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

#include "RBGMgr.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "Chat.h"
#include "Common.h"
#include "Config.h"
#include "Containers.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Optional.h"
#include "Player.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Tokenize.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include <algorithm>
#include <cmath>

namespace
{
uint32 constexpr SPELL_DESERTER = 26013;

// Alliance 1-14 / Horde 15-28 PvP rank titles (CharTitles.dbc).
uint32 AllianceTitleForRating(uint32 rating)
{
    if (rating >= 2400)
        return 14;
    if (rating >= 2300)
        return 13;
    if (rating >= 2200)
        return 12;
    if (rating >= 2100)
        return 11;
    if (rating >= 2000)
        return 10;
    if (rating >= 1900)
        return 9;
    if (rating >= 1800)
        return 8;
    if (rating >= 1700)
        return 7;
    if (rating >= 1600)
        return 6;
    if (rating >= 1500)
        return 5;
    if (rating >= 1400)
        return 4;
    if (rating >= 1300)
        return 3;
    if (rating >= 1200)
        return 2;
    if (rating >= 1100)
        return 1;
    return 0;
}

uint32 HordeTitleForRating(uint32 rating)
{
    uint32 allianceId = AllianceTitleForRating(rating);
    return allianceId ? allianceId + 14 : 0;
}
}

RBGMgr* RBGMgr::instance()
{
    static RBGMgr instance;
    return &instance;
}

void RBGMgr::LoadConfig(bool /*reload*/)
{
    _enabled = sConfigMgr->GetOption<bool>("RatedBG.Enable", true);
    _minLevel = sConfigMgr->GetOption<uint32>("RatedBG.MinLevel", 80);
    _teamSize = sConfigMgr->GetOption<uint32>("RatedBG.TeamSize", 10);
    _allowSameFaction = sConfigMgr->GetOption<bool>("RatedBG.AllowSameFaction", false);
    _startRating = sConfigMgr->GetOption<uint32>("RatedBG.StartRating", 1500);
    _startMMR = sConfigMgr->GetOption<uint32>("RatedBG.StartMMR", 1500);
    _maxRatingDiff = sConfigMgr->GetOption<uint32>("RatedBG.MaxRatingDiff", 150);
    _ratingDiscardTimer = sConfigMgr->GetOption<uint32>("RatedBG.RatingDiscardTimer", 300000);
    _queueUpdateInterval = sConfigMgr->GetOption<uint32>("RatedBG.QueueUpdateInterval", 1000);
    _honorWin = sConfigMgr->GetOption<uint32>("RatedBG.HonorWin", 378);
    _honorLoss = sConfigMgr->GetOption<uint32>("RatedBG.HonorLoss", 189);
    _arenaPointsWinBase = sConfigMgr->GetOption<float>("RatedBG.ArenaPointsWinBase", 180.f);
    _arenaPointsWinScale = sConfigMgr->GetOption<float>("RatedBG.ArenaPointsWinScale", 1.3f);
    _weeklyCap = sConfigMgr->GetOption<uint32>("RatedBG.WeeklyCap", 1650);
    _npcEntry = sConfigMgr->GetOption<uint32>("RatedBG.NPCEntry", 190010);
    _announceQueue = sConfigMgr->GetOption<bool>("RatedBG.AnnounceQueue", false);
    _enableTitles = sConfigMgr->GetOption<bool>("RatedBG.EnableTitles", false);
    _winRatingModifier1 = sConfigMgr->GetOption<float>("RatedBG.WinRatingModifier1", 48.f);
    _winRatingModifier2 = sConfigMgr->GetOption<float>("RatedBG.WinRatingModifier2", 24.f);
    _loseRatingModifier = sConfigMgr->GetOption<float>("RatedBG.LoseRatingModifier", 24.f);
    _mmrModifier = sConfigMgr->GetOption<float>("RatedBG.MatchmakerRatingModifier", 24.f);

    if (_teamSize < 1)
        _teamSize = 1;
    if (_queueUpdateInterval < 200)
        _queueUpdateInterval = 200;

    _maps.clear();
    std::string maps = sConfigMgr->GetOption<std::string>("RatedBG.Maps", "2,3,7,9");
    for (std::string_view token : Acore::Tokenize(maps, ',', false))
    {
        Optional<uint32> id = Acore::StringTo<uint32>(token);
        if (!id)
            continue;

        BattlegroundTypeId bgTypeId = static_cast<BattlegroundTypeId>(*id);
        if (sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId))
            _maps.push_back(bgTypeId);
    }

    if (_maps.empty())
        _maps.push_back(BATTLEGROUND_WS);

    LOG_INFO("module.rbg", "Rated Battlegrounds: {} (team size {}, {} maps)",
        _enabled ? "enabled" : "disabled", _teamSize, _maps.size());
}

void RBGMgr::EnsureDatabase()
{
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `rbg_stats` ("
        " `guid` INT UNSIGNED NOT NULL,"
        " `rating` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,"
        " `mmr` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,"
        " `games` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `wins` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `season_games` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `season_wins` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_games` SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_wins` SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_conquest` SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " `highest_rating` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,"
        " PRIMARY KEY (`guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `rbg_request` ("
        " `guid` INT UNSIGNED NOT NULL,"
        " `action` TINYINT UNSIGNED NOT NULL,"
        " `created_at` INT UNSIGNED NOT NULL DEFAULT 0,"
        " PRIMARY KEY (`guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `rbg_state` ("
        " `id` TINYINT UNSIGNED NOT NULL,"
        " `week_start` INT UNSIGNED NOT NULL DEFAULT 0,"
        " PRIMARY KEY (`id`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    QueryResult result = CharacterDatabase.Query("SELECT week_start FROM rbg_state WHERE id = 1");
    if (result)
        _weekStart = result->Fetch()[0].Get<uint32>();
    else
    {
        _weekStart = static_cast<uint32>(GameTime::GetGameTime().count());
        CharacterDatabase.Execute("INSERT INTO rbg_state (id, week_start) VALUES (1, {})", _weekStart);
    }
}

void RBGMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    _updateTimer += diff;
    if (_updateTimer < _queueUpdateInterval)
        return;

    _updateTimer = 0;
    CheckWeekReset();
    ProcessLuaRequests();
    TryMatch();
}

void RBGMgr::ProcessLuaRequests()
{
    QueryResult result = CharacterDatabase.Query("SELECT guid, action FROM rbg_request");
    if (!result)
        return;

    struct Request { uint32 GuidLow; uint8 Action; };
    std::vector<Request> requests;
    do
    {
        Field* fields = result->Fetch();
        requests.push_back({ fields[0].Get<uint32>(), fields[1].Get<uint8>() });
    } while (result->NextRow());

    CharacterDatabase.Execute("DELETE FROM rbg_request");

    for (Request const& request : requests)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(request.GuidLow));
        if (!player)
            continue;

        std::string error;
        bool ok = (request.Action == 1) ? Queue(player, error) : Dequeue(player, error);
        if (!ok)
            ChatHandler(player->GetSession()).SendSysMessage(error);
    }
}

bool RBGMgr::ValidateGroup(Player* leader, Group* group, std::string& error) const
{
    if (!group || group->GetLeaderGUID() != leader->GetGUID())
    {
        error = "Only the raid leader can queue for Rated Battlegrounds.";
        return false;
    }

    if (_teamSize > MAXGROUPSIZE && !group->isRaidGroup())
    {
        error = Acore::StringFormat("Rated BGs require a raid of {} players.", _teamSize);
        return false;
    }

    if (group->GetMembersCount() != _teamSize)
    {
        error = Acore::StringFormat("You need exactly {} raid members to queue (have {}).",
            _teamSize, group->GetMembersCount());
        return false;
    }

    bool valid = true;
    group->DoForAllMembers([&](Player* member)
    {
        if (!valid)
            return;

        if (member->GetLevel() < _minLevel)
        {
            error = Acore::StringFormat("{} is below the required level ({}).", member->GetName(), _minLevel);
            valid = false;
            return;
        }

        if (member->HasAura(SPELL_DESERTER) || member->IsDeserter())
        {
            error = Acore::StringFormat("{} has Deserter.", member->GetName());
            valid = false;
            return;
        }

        if (member->InBattleground() || member->InBattlegroundQueue())
        {
            error = Acore::StringFormat("{} is already in a battleground or queue.", member->GetName());
            valid = false;
            return;
        }

        if (IsQueued(member->GetGUID()))
        {
            error = Acore::StringFormat("{} is already in the Rated BG queue.", member->GetName());
            valid = false;
            return;
        }

        if (!member->HasFreeBattlegroundQueueId())
        {
            error = Acore::StringFormat("{} has no free battleground queue slot.", member->GetName());
            valid = false;
            return;
        }

        if (!_allowSameFaction && member->GetTeamId() != leader->GetTeamId())
        {
            error = "All raid members must be the same faction.";
            valid = false;
        }
    });

    return valid;
}

bool RBGMgr::Queue(Player* leader, std::string& error)
{
    if (!_enabled)
    {
        error = "Rated Battlegrounds are disabled.";
        return false;
    }

    if (!leader)
    {
        error = "Player not found.";
        return false;
    }

    Group* group = leader->GetGroup();
    if (!ValidateGroup(leader, group, error))
        return false;

    RBGQueueEntry entry;
    entry.LeaderGuid = leader->GetGUID();
    entry.Team = leader->GetTeamId();
    entry.JoinTime = GameTime::GetGameTimeMS().count();
    entry.Members.reserve(_teamSize);

    group->DoForAllMembers([&](Player* member)
    {
        entry.Members.push_back(member->GetGUID());
        _queuedPlayers.insert(member->GetGUID().GetCounter());
    });

    entry.Rating = AverageStat(entry.Members, false);
    entry.MMR = AverageStat(entry.Members, true);
    _queue.push_back(entry);

    NotifyGroup(group, Acore::StringFormat(
        "Your raid joined the Rated Battleground queue (MMR {}).", entry.MMR));

    if (_announceQueue)
    {
        std::string msg = Acore::StringFormat("|cff00ff00Rated BG:|r a {}-player team queued ({} in queue).",
            _teamSize, _queue.size());
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, msg);
    }

    LOG_INFO("module.rbg", "Queue join: {} ({} players, mmr {})",
        leader->GetName(), _teamSize, entry.MMR);
    return true;
}

bool RBGMgr::Dequeue(Player* player, std::string& error)
{
    if (!player)
    {
        error = "Player not found.";
        return false;
    }

    if (!IsQueued(player->GetGUID()))
    {
        error = "You are not in the Rated Battleground queue.";
        return false;
    }

    RemovePlayer(player->GetGUID());
    ChatHandler(player->GetSession()).SendSysMessage("You left the Rated Battleground queue.");
    return true;
}

bool RBGMgr::IsQueued(ObjectGuid guid) const
{
    return _queuedPlayers.find(guid.GetCounter()) != _queuedPlayers.end();
}

void RBGMgr::RemovePlayer(ObjectGuid guid)
{
    auto queued = _queuedPlayers.find(guid.GetCounter());
    if (queued == _queuedPlayers.end())
        return;

    for (auto itr = _queue.begin(); itr != _queue.end(); ++itr)
    {
        bool inEntry = itr->LeaderGuid == guid;
        if (!inEntry)
        {
            for (ObjectGuid const& member : itr->Members)
            {
                if (member == guid)
                {
                    inEntry = true;
                    break;
                }
            }
        }

        if (!inEntry)
            continue;

        for (ObjectGuid const& member : itr->Members)
        {
            _queuedPlayers.erase(member.GetCounter());
            if (Player* memberPlayer = ObjectAccessor::FindConnectedPlayer(member))
                ChatHandler(memberPlayer->GetSession()).SendSysMessage(
                    "Your raid left the Rated Battleground queue.");
        }

        _queue.erase(itr);
        return;
    }
}

RBGPlayerStats RBGMgr::GetStats(ObjectGuid guid)
{
    uint32 guidLow = guid.GetCounter();
    auto cached = _statsCache.find(guidLow);
    if (cached != _statsCache.end())
        return cached->second;

    RBGPlayerStats stats = LoadStats(guid);
    _statsCache[guidLow] = stats;
    return stats;
}

RBGPlayerStats RBGMgr::LoadStats(ObjectGuid guid)
{
    RBGPlayerStats stats;
    stats.Rating = _startRating;
    stats.MMR = _startMMR;
    stats.HighestRating = _startRating;

    QueryResult result = CharacterDatabase.Query(
        "SELECT rating, mmr, games, wins, season_games, season_wins, week_games, week_wins, "
        "week_conquest, highest_rating FROM rbg_stats WHERE guid = {}", guid.GetCounter());
    if (!result)
        return stats;

    Field* fields = result->Fetch();
    stats.Rating = fields[0].Get<uint32>();
    stats.MMR = fields[1].Get<uint32>();
    stats.Games = fields[2].Get<uint32>();
    stats.Wins = fields[3].Get<uint32>();
    stats.SeasonGames = fields[4].Get<uint32>();
    stats.SeasonWins = fields[5].Get<uint32>();
    stats.WeekGames = fields[6].Get<uint16>();
    stats.WeekWins = fields[7].Get<uint16>();
    stats.WeekConquest = fields[8].Get<uint16>();
    stats.HighestRating = fields[9].Get<uint32>();
    return stats;
}

void RBGMgr::SaveStats(ObjectGuid guid, RBGPlayerStats const& stats)
{
    _statsCache[guid.GetCounter()] = stats;
    CharacterDatabase.Execute(
        "INSERT INTO rbg_stats (guid, rating, mmr, games, wins, season_games, season_wins, "
        "week_games, week_wins, week_conquest, highest_rating) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE rating = {}, mmr = {}, games = {}, wins = {}, "
        "season_games = {}, season_wins = {}, week_games = {}, week_wins = {}, "
        "week_conquest = {}, highest_rating = {}",
        guid.GetCounter(), stats.Rating, stats.MMR, stats.Games, stats.Wins, stats.SeasonGames,
        stats.SeasonWins, stats.WeekGames, stats.WeekWins, stats.WeekConquest, stats.HighestRating,
        stats.Rating, stats.MMR, stats.Games, stats.Wins, stats.SeasonGames, stats.SeasonWins,
        stats.WeekGames, stats.WeekWins, stats.WeekConquest, stats.HighestRating);
}

uint32 RBGMgr::AverageStat(std::vector<ObjectGuid> const& members, bool mmr)
{
    if (members.empty())
        return mmr ? _startMMR : _startRating;

    uint64 total = 0;
    for (ObjectGuid const& guid : members)
    {
        RBGPlayerStats stats = GetStats(guid);
        total += mmr ? stats.MMR : stats.Rating;
    }

    return static_cast<uint32>(total / members.size());
}

float RBGMgr::GetChanceAgainst(uint32 ownRating, uint32 opponentRating)
{
    return 1.0f / (1.0f + std::exp(std::log(10.0f) *
        (static_cast<float>(opponentRating) - static_cast<float>(ownRating)) / 650.0f));
}

int32 RBGMgr::GetRatingMod(uint32 ownRating, uint32 opponentRating, bool won) const
{
    float chance = GetChanceAgainst(ownRating, opponentRating);
    float mod = 0.f;

    if (won)
    {
        if (ownRating < 1300)
        {
            if (ownRating < 1000)
                mod = _winRatingModifier1 * (1.0f - chance);
            else
            {
                mod = ((_winRatingModifier1 / 2.0f) +
                    ((_winRatingModifier1 / 2.0f) * (1300.0f - float(ownRating)) / 300.0f))
                    * (1.0f - chance);
            }
        }
        else
            mod = _winRatingModifier2 * (1.0f - chance);
    }
    else
        mod = _loseRatingModifier * (-chance);

    return static_cast<int32>(std::ceil(mod));
}

int32 RBGMgr::GetMMRMod(uint32 ownMMR, uint32 opponentMMR, bool won) const
{
    float chance = GetChanceAgainst(ownMMR, opponentMMR);
    float wonMod = won ? 1.0f : 0.0f;
    return static_cast<int32>(std::ceil((wonMod - chance) * _mmrModifier));
}

BattlegroundTypeId RBGMgr::PickMap() const
{
    if (_maps.empty())
        return BATTLEGROUND_WS;

    return Acore::Containers::SelectRandomContainerElement(_maps);
}

void RBGMgr::NotifyGroup(Group* group, std::string const& message) const
{
    if (!group)
        return;

    group->DoForAllMembers([&](Player* member)
    {
        ChatHandler handler(member->GetSession());
        handler.SendSysMessage(message);
        handler.SendNotification("{}", message);
    });
}

void RBGMgr::PruneQueue()
{
    for (auto itr = _queue.begin(); itr != _queue.end();)
    {
        Player* leader = ObjectAccessor::FindConnectedPlayer(itr->LeaderGuid);
        Group* group = leader ? leader->GetGroup() : nullptr;
        std::string error;
        if (leader && ValidateGroup(leader, group, error))
        {
            ++itr;
            continue;
        }

        for (ObjectGuid const& member : itr->Members)
        {
            _queuedPlayers.erase(member.GetCounter());
            if (Player* memberPlayer = ObjectAccessor::FindConnectedPlayer(member))
                ChatHandler(memberPlayer->GetSession()).SendSysMessage(
                    "Your raid was removed from the Rated Battleground queue.");
        }

        itr = _queue.erase(itr);
    }
}

void RBGMgr::TryMatch()
{
    PruneQueue();

    if (_queue.size() < 2)
        return;

    uint32 now = GameTime::GetGameTimeMS().count();

    for (auto itrA = _queue.begin(); itrA != _queue.end(); ++itrA)
    {
        uint32 waitA = now > itrA->JoinTime ? now - itrA->JoinTime : 0;
        uint32 windowA = _maxRatingDiff;
        if (_ratingDiscardTimer && waitA >= _ratingDiscardTimer)
            windowA = 10000;
        else if (_ratingDiscardTimer)
            windowA += (waitA * 400) / _ratingDiscardTimer;

        auto itrB = itrA;
        ++itrB;
        for (; itrB != _queue.end(); ++itrB)
        {
            if (!_allowSameFaction && itrA->Team == itrB->Team)
                continue;

            uint32 waitB = now > itrB->JoinTime ? now - itrB->JoinTime : 0;
            uint32 windowB = _maxRatingDiff;
            if (_ratingDiscardTimer && waitB >= _ratingDiscardTimer)
                windowB = 10000;
            else if (_ratingDiscardTimer)
                windowB += (waitB * 400) / _ratingDiscardTimer;

            uint32 window = std::max(windowA, windowB);
            uint32 diff = itrA->MMR > itrB->MMR ? itrA->MMR - itrB->MMR : itrB->MMR - itrA->MMR;
            if (diff > window)
                continue;

            RBGQueueEntry teamA = *itrA;
            RBGQueueEntry teamB = *itrB;
            _queue.erase(itrB);
            _queue.erase(itrA);

            if (!StartMatch(teamA, teamB))
            {
                _queue.push_front(teamB);
                _queue.push_front(teamA);
            }
            return;
        }
    }
}

bool RBGMgr::StartMatch(RBGQueueEntry& teamA, RBGQueueEntry& teamB)
{
    Player* leaderA = ObjectAccessor::FindConnectedPlayer(teamA.LeaderGuid);
    Player* leaderB = ObjectAccessor::FindConnectedPlayer(teamB.LeaderGuid);
    if (!leaderA || !leaderB)
        return false;

    Group* groupA = leaderA->GetGroup();
    Group* groupB = leaderB->GetGroup();
    std::string error;
    if (!ValidateGroup(leaderA, groupA, error) || !ValidateGroup(leaderB, groupB, error))
    {
        LOG_DEBUG("module.rbg", "Match aborted: {}", error);
        return false;
    }

    BattlegroundTypeId bgTypeId = PickMap();
    Battleground* bgTemplate = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);
    if (!bgTemplate)
        return false;

    PvPDifficultyEntry const* bracket = GetBattlegroundBracketByLevel(bgTemplate->GetMapId(), leaderA->GetLevel());
    if (!bracket)
        return false;

    Battleground* bg = sBattlegroundMgr->CreateNewBattleground(bgTypeId, bracket, 0, true);
    if (!bg)
    {
        LOG_ERROR("module.rbg", "Failed to create rated battleground {}", uint32(bgTypeId));
        return false;
    }

    bg->SetMaxPlayersPerTeam(_teamSize);
    bg->SetMinPlayersPerTeam(1);

    BattlegroundQueueTypeId queueTypeId = BattlegroundMgr::BGQueueTypeId(bgTypeId, 0);
    BattlegroundQueue& queue = sBattlegroundMgr->GetBattlegroundQueue(queueTypeId);

    GroupQueueInfo* ginfoA = queue.AddGroup(leaderA, groupA, bgTypeId, bracket, 0, false, true,
        teamA.Rating, teamA.MMR);
    GroupQueueInfo* ginfoB = queue.AddGroup(leaderB, groupB, bgTypeId, bracket, 0, false, true,
        teamB.Rating, teamB.MMR);

    auto addQueueSlots = [&](Group* group)
    {
        group->DoForAllMembers([&](Player* member)
        {
            uint32 slot = member->AddBattlegroundQueueId(queueTypeId);
            WorldPacket data;
            sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bgTemplate, slot, STATUS_WAIT_QUEUE,
                0, 0, 0, TEAM_NEUTRAL);
            member->SendDirectMessage(&data);
        });
    };
    addQueueSlots(groupA);
    addQueueSlots(groupB);

    TeamId sideA = TEAM_ALLIANCE;
    TeamId sideB = TEAM_HORDE;
    if (teamA.Team == TEAM_HORDE && teamB.Team == TEAM_ALLIANCE)
    {
        sideA = TEAM_HORDE;
        sideB = TEAM_ALLIANCE;
    }

    queue.InviteGroupToBG(ginfoA, bg, sideA);
    queue.InviteGroupToBG(ginfoB, bg, sideB);
    bg->StartBattleground();
    bg->RemoveFromBGFreeSlotQueue();

    RBGMatch match;
    match.InstanceId = bg->GetInstanceID();
    match.BgTypeId = bgTypeId;
    match.AllianceMMR = (sideA == TEAM_ALLIANCE) ? teamA.MMR : teamB.MMR;
    match.HordeMMR = (sideA == TEAM_HORDE) ? teamA.MMR : teamB.MMR;
    match.Alliance = (sideA == TEAM_ALLIANCE) ? teamA.Members : teamB.Members;
    match.Horde = (sideA == TEAM_HORDE) ? teamA.Members : teamB.Members;
    _matches[match.InstanceId] = match;

    for (ObjectGuid const& guid : teamA.Members)
        _queuedPlayers.erase(guid.GetCounter());
    for (ObjectGuid const& guid : teamB.Members)
        _queuedPlayers.erase(guid.GetCounter());

    NotifyGroup(groupA, "Rated Battleground found! Accept the battleground invite.");
    NotifyGroup(groupB, "Rated Battleground found! Accept the battleground invite.");
    LOG_INFO("module.rbg", "Started rated {} instance {} (mmr {} vs {})",
        uint32(bgTypeId), match.InstanceId, teamA.MMR, teamB.MMR);
    return true;
}

void RBGMgr::ApplyResult(ObjectGuid guid, uint32 opponentMMR, bool won)
{
    RBGPlayerStats stats = GetStats(guid);
    uint32 oldRating = stats.Rating;
    int32 ratingChange = GetRatingMod(stats.Rating, opponentMMR, won);
    int32 mmrChange = GetMMRMod(stats.MMR, opponentMMR, won);

    int32 newRating = static_cast<int32>(stats.Rating) + ratingChange;
    int32 newMMR = static_cast<int32>(stats.MMR) + mmrChange;
    stats.Rating = newRating < 0 ? 0 : static_cast<uint32>(newRating);
    stats.MMR = newMMR < 0 ? 0 : static_cast<uint32>(newMMR);
    if (stats.Rating > stats.HighestRating)
        stats.HighestRating = stats.Rating;

    stats.Games += 1;
    stats.SeasonGames += 1;
    stats.WeekGames += 1;
    if (won)
    {
        stats.Wins += 1;
        stats.SeasonWins += 1;
        stats.WeekWins += 1;
    }

    uint32 arenaPoints = 0;
    if (won)
    {
        float scale = 1.0f + (static_cast<int32>(stats.Rating) - 1500) / 2000.0f * _arenaPointsWinScale;
        if (scale < 0.4f)
            scale = 0.4f;
        arenaPoints = static_cast<uint32>(_arenaPointsWinBase * scale);
        if (arenaPoints < 1)
            arenaPoints = 1;

        uint32 remaining = _weeklyCap > stats.WeekConquest ? _weeklyCap - stats.WeekConquest : 0;
        if (arenaPoints > remaining)
            arenaPoints = remaining;
        stats.WeekConquest = static_cast<uint16>(stats.WeekConquest + arenaPoints);
    }

    SaveStats(guid, stats);

    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
    {
        if (player->GetBattleground() && player->GetBattleground()->isRated())
        {
            player->ModifyHonorPoints(won ? static_cast<int32>(_honorWin) : static_cast<int32>(_honorLoss));
            if (arenaPoints)
                player->ModifyArenaPoints(static_cast<int32>(arenaPoints));
        }

        ChatHandler(player->GetSession()).PSendSysMessage(
            "Rated BG {}: {} rating ({} -> {}), {} MMR. {} honor, {} arena points.",
            won ? "victory" : "defeat",
            ratingChange >= 0 ? Acore::StringFormat("+{}", ratingChange) : std::to_string(ratingChange),
            oldRating, stats.Rating,
            mmrChange >= 0 ? Acore::StringFormat("+{}", mmrChange) : std::to_string(mmrChange),
            won ? _honorWin : _honorLoss, arenaPoints);

        GrantTitleIfNeeded(player, stats.Rating);
    }
}

void RBGMgr::GrantTitleIfNeeded(Player* player, uint32 rating) const
{
    if (!_enableTitles || !player)
        return;

    uint32 titleId = player->GetTeamId() == TEAM_ALLIANCE
        ? AllianceTitleForRating(rating)
        : HordeTitleForRating(rating);
    if (!titleId)
        return;

    CharTitlesEntry const* title = sCharTitlesStore.LookupEntry(titleId);
    if (!title || player->HasTitle(title))
        return;

    player->SetTitle(title);
}

void RBGMgr::HandleBattlegroundEnd(Battleground* bg, TeamId winner)
{
    if (!bg)
        return;

    auto itr = _matches.find(bg->GetInstanceID());
    if (itr == _matches.end())
        return;

    RBGMatch match = itr->second;
    if (winner == TEAM_NEUTRAL)
    {
        LOG_INFO("module.rbg", "Rated BG instance {} ended in a draw; no rating change.", match.InstanceId);
        return;
    }

    uint32 allianceOpp = match.HordeMMR;
    uint32 hordeOpp = match.AllianceMMR;
    for (ObjectGuid const& guid : match.Alliance)
        ApplyResult(guid, allianceOpp, winner == TEAM_ALLIANCE);
    for (ObjectGuid const& guid : match.Horde)
        ApplyResult(guid, hordeOpp, winner == TEAM_HORDE);
}

void RBGMgr::HandleBattlegroundDestroy(Battleground* bg)
{
    if (!bg)
        return;

    _matches.erase(bg->GetInstanceID());
}

void RBGMgr::CheckWeekReset()
{
    uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
    if (!_weekStart)
        _weekStart = now;

    if (now < _weekStart + WEEK)
        return;

    _weekStart = now;
    CharacterDatabase.Execute("UPDATE rbg_state SET week_start = {} WHERE id = 1", _weekStart);
    CharacterDatabase.Execute("UPDATE rbg_stats SET week_games = 0, week_wins = 0, week_conquest = 0");
    for (auto& [guid, stats] : _statsCache)
    {
        stats.WeekGames = 0;
        stats.WeekWins = 0;
        stats.WeekConquest = 0;
    }

    LOG_INFO("module.rbg", "Rated BG weekly stats reset.");
}

std::vector<RBGLeaderboardRow> RBGMgr::GetLeaderboard(uint32 limit)
{
    std::vector<RBGLeaderboardRow> rows;
    QueryResult result = CharacterDatabase.Query(
        "SELECT c.name, s.rating, s.wins, (s.games - s.wins) FROM rbg_stats s "
        "INNER JOIN characters c ON c.guid = s.guid ORDER BY s.rating DESC, s.wins DESC LIMIT {}",
        limit);
    if (!result)
        return rows;

    do
    {
        Field* fields = result->Fetch();
        RBGLeaderboardRow row;
        row.Name = fields[0].Get<std::string>();
        row.Rating = fields[1].Get<uint32>();
        row.Wins = fields[2].Get<uint32>();
        row.Losses = fields[3].Get<uint32>();
        rows.push_back(row);
    } while (result->NextRow());

    return rows;
}
