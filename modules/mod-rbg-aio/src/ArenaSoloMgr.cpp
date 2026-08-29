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

#include "ArenaSoloMgr.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "Chat.h"
#include "Common.h"
#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "StringFormat.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include <algorithm>

namespace
{
uint32 constexpr SPELL_DESERTER = 26013;

// Stats are per (character, bracket); pack both into the cache key.
uint64 StatsKey(ObjectGuid guid, uint8 bracket)
{
    return (static_cast<uint64>(guid.GetCounter()) << 8) | bracket;
}
}

ArenaSoloMgr* ArenaSoloMgr::instance()
{
    static ArenaSoloMgr instance;
    return &instance;
}

char const* ArenaSoloMgr::GetBracketName(uint8 bracket)
{
    return bracket == ARENA_SOLO_BRACKET_1V1 ? "1v1" : "3v3 SoloQ";
}

Optional<uint8> ArenaSoloMgr::ParseBracket(std::string_view token)
{
    if (token == "1v1" || token == "1" || token == "solo1")
        return ARENA_SOLO_BRACKET_1V1;

    if (token == "3v3" || token == "3" || token == "soloq" || token == "solo3")
        return ARENA_SOLO_BRACKET_3V3;

    return {};
}

void ArenaSoloMgr::LoadConfig(bool /*reload*/)
{
    _enabled = sConfigMgr->GetOption<bool>("ArenaSolo.Enable", true);
    _crossFaction = sConfigMgr->GetOption<bool>("ArenaSolo.CrossFaction", true);
    _queueUpdateInterval = sConfigMgr->GetOption<uint32>("ArenaSolo.QueueUpdateInterval", 1000);
    _npcEntry = sConfigMgr->GetOption<uint32>("ArenaSolo.NPCEntry", 190011);
    _announceQueue = sConfigMgr->GetOption<bool>("ArenaSolo.AnnounceQueue", false);

    if (_queueUpdateInterval < 200)
        _queueUpdateInterval = 200;

    auto loadBracket = [](ArenaSoloBracketConfig& config, char const* prefix, uint32 teamSize, uint8 arenaType)
    {
        auto option = [prefix](char const* name)
        {
            return Acore::StringFormat("ArenaSolo.{}.{}", prefix, name);
        };

        config.Enabled = sConfigMgr->GetOption<bool>(option("Enable"), true);
        config.TeamSize = teamSize;
        config.ArenaType = arenaType;
        config.MinLevel = sConfigMgr->GetOption<uint32>(option("MinLevel"), 80);
        config.StartRating = sConfigMgr->GetOption<uint32>(option("StartRating"), 1500);
        config.StartMMR = sConfigMgr->GetOption<uint32>(option("StartMMR"), 1500);
        config.MaxRatingDiff = sConfigMgr->GetOption<uint32>(option("MaxRatingDiff"), 150);
        config.RatingDiscardTimer = sConfigMgr->GetOption<uint32>(option("RatingDiscardTimer"), 300000);
        config.HonorWin = sConfigMgr->GetOption<uint32>(option("HonorWin"), 189);
        config.HonorLoss = sConfigMgr->GetOption<uint32>(option("HonorLoss"), 94);
        config.PointsWin = sConfigMgr->GetOption<uint32>(option("ArenaPointsWin"), 30);
        config.WeeklyCap = sConfigMgr->GetOption<uint32>(option("WeeklyCap"), 1350);
        config.Elo.WinModifierLow = sConfigMgr->GetOption<float>(option("WinRatingModifier1"), 48.f);
        config.Elo.WinModifierHigh = sConfigMgr->GetOption<float>(option("WinRatingModifier2"), 24.f);
        config.Elo.LoseModifier = sConfigMgr->GetOption<float>(option("LoseRatingModifier"), 24.f);
        config.Elo.MatchmakerModifier = sConfigMgr->GetOption<float>(option("MatchmakerRatingModifier"), 24.f);
    };

    loadBracket(_brackets[ARENA_SOLO_BRACKET_1V1], "1v1", 1, ARENA_TYPE_2v2);
    loadBracket(_brackets[ARENA_SOLO_BRACKET_3V3], "3v3", 3, ARENA_TYPE_3v3);

    _brackets[ARENA_SOLO_BRACKET_1V1].RequireRoleBalance = false;
    _brackets[ARENA_SOLO_BRACKET_3V3].RequireRoleBalance =
        sConfigMgr->GetOption<bool>("ArenaSolo.3v3.RequireHealer", true);

    LOG_INFO("module.arenasolo", "Arena solo queue: {} (1v1 {}, 3v3 soloq {})",
        _enabled ? "enabled" : "disabled",
        _brackets[ARENA_SOLO_BRACKET_1V1].Enabled ? "on" : "off",
        _brackets[ARENA_SOLO_BRACKET_3V3].Enabled ? "on" : "off");
}

void ArenaSoloMgr::EnsureDatabase()
{
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `arena_solo_stats` ("
        " `guid` INT UNSIGNED NOT NULL,"
        " `bracket` TINYINT UNSIGNED NOT NULL,"
        " `rating` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,"
        " `mmr` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,"
        " `games` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `wins` INT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_games` SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_wins` SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " `week_points` SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
        " `highest_rating` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,"
        " PRIMARY KEY (`guid`, `bracket`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `arena_solo_request` ("
        " `guid` INT UNSIGNED NOT NULL,"
        " `bracket` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
        " `action` TINYINT UNSIGNED NOT NULL,"
        " `created_at` INT UNSIGNED NOT NULL DEFAULT 0,"
        " PRIMARY KEY (`guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `arena_solo_state` ("
        " `id` TINYINT UNSIGNED NOT NULL,"
        " `week_start` INT UNSIGNED NOT NULL DEFAULT 0,"
        " PRIMARY KEY (`id`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    QueryResult result = CharacterDatabase.Query("SELECT week_start FROM arena_solo_state WHERE id = 1");
    if (result)
        _weekStart = result->Fetch()[0].Get<uint32>();
    else
    {
        _weekStart = static_cast<uint32>(GameTime::GetGameTime().count());
        CharacterDatabase.Execute("INSERT INTO arena_solo_state (id, week_start) VALUES (1, {})", _weekStart);
    }
}

void ArenaSoloMgr::Update(uint32 diff)
{
    if (!_enabled)
        return;

    _updateTimer += diff;
    if (_updateTimer < _queueUpdateInterval)
        return;

    _updateTimer = 0;
    CheckWeekReset();
    ProcessLuaRequests();

    for (uint8 bracket = 0; bracket < ARENA_SOLO_BRACKET_MAX; ++bracket)
        TryMatch(bracket);
}

void ArenaSoloMgr::ProcessLuaRequests()
{
    QueryResult result = CharacterDatabase.Query("SELECT guid, bracket, action FROM arena_solo_request");
    if (!result)
        return;

    struct Request { uint32 GuidLow; uint8 Bracket; uint8 Action; };
    std::vector<Request> requests;
    do
    {
        Field* fields = result->Fetch();
        requests.push_back({ fields[0].Get<uint32>(), fields[1].Get<uint8>(), fields[2].Get<uint8>() });
    } while (result->NextRow());

    CharacterDatabase.Execute("DELETE FROM arena_solo_request");

    for (Request const& request : requests)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(request.GuidLow));
        if (!player)
            continue;

        if (request.Bracket >= ARENA_SOLO_BRACKET_MAX)
            continue;

        std::string error;
        bool ok = (request.Action == 1) ? Queue(player, request.Bracket, error) : Dequeue(player, error);
        if (!ok)
            ChatHandler(player->GetSession()).SendSysMessage(error);
    }
}

bool ArenaSoloMgr::IsBracketEnabled(uint8 bracket) const
{
    return _enabled && bracket < ARENA_SOLO_BRACKET_MAX && _brackets[bracket].Enabled;
}

ArenaSoloBracketConfig const& ArenaSoloMgr::GetBracketConfig(uint8 bracket) const
{
    return _brackets[bracket < ARENA_SOLO_BRACKET_MAX ? bracket : ARENA_SOLO_BRACKET_1V1];
}

uint32 ArenaSoloMgr::GetQueuedCount(uint8 bracket) const
{
    if (bracket >= ARENA_SOLO_BRACKET_MAX)
        return 0;

    return static_cast<uint32>(_queues[bracket].size());
}

bool ArenaSoloMgr::CanQueue(Player* player, uint8 bracket, std::string& error) const
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];

    if (player->GetLevel() < config.MinLevel)
    {
        error = Acore::StringFormat("You must be level {} to queue for {}.",
            config.MinLevel, GetBracketName(bracket));
        return false;
    }

    if (player->GetGroup())
    {
        error = "Solo queue requires you to leave your group first.";
        return false;
    }

    if (player->InBattleground() || player->InBattlegroundQueue())
    {
        error = "You are already in a battleground or queue.";
        return false;
    }

    if (player->IsDeserter() || player->HasAura(SPELL_DESERTER))
    {
        error = "You cannot queue while flagged as a Deserter.";
        return false;
    }

    if (!player->HasFreeBattlegroundQueueId())
    {
        error = "You have no free battleground queue slot.";
        return false;
    }

    return true;
}

bool ArenaSoloMgr::Queue(Player* player, uint8 bracket, std::string& error)
{
    if (!player)
    {
        error = "Player not found.";
        return false;
    }

    if (bracket >= ARENA_SOLO_BRACKET_MAX || !IsBracketEnabled(bracket))
    {
        error = "That solo queue is disabled.";
        return false;
    }

    if (IsQueued(player->GetGUID()))
    {
        error = "You are already in a solo queue. Leave it first.";
        return false;
    }

    if (!CanQueue(player, bracket, error))
        return false;

    ArenaSoloBracketConfig const& config = _brackets[bracket];
    ArenaSoloStats stats = GetStats(player->GetGUID(), bracket);

    ArenaSoloQueueEntry entry;
    entry.Guid = player->GetGUID();
    entry.Team = player->GetTeamId();
    entry.Rating = stats.Rating;
    entry.MMR = stats.MMR;
    entry.JoinTime = GameTime::GetGameTimeMS().count();
    entry.Healer = config.RequireRoleBalance && player->HasHealSpec();

    _queues[bracket].push_back(entry);
    _queuedPlayers[player->GetGUID().GetCounter()] = bracket;

    ChatHandler handler(player->GetSession());
    handler.PSendSysMessage("Joined the {} solo queue as {} (rating {}, MMR {}). {} player(s) waiting.",
        GetBracketName(bracket), entry.Healer ? "healer" : "damage",
        entry.Rating, entry.MMR, _queues[bracket].size());

    if (_announceQueue)
    {
        std::string message = Acore::StringFormat("|cff00ff00Arena {}:|r {} player(s) in the solo queue.",
            GetBracketName(bracket), _queues[bracket].size());
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message);
    }

    return true;
}

bool ArenaSoloMgr::Dequeue(Player* player, std::string& error)
{
    if (!player)
    {
        error = "Player not found.";
        return false;
    }

    if (!IsQueued(player->GetGUID()))
    {
        error = "You are not in a solo queue.";
        return false;
    }

    RemovePlayer(player->GetGUID());
    ChatHandler(player->GetSession()).SendSysMessage("You left the solo queue.");
    return true;
}

bool ArenaSoloMgr::IsQueued(ObjectGuid guid) const
{
    return _queuedPlayers.find(guid.GetCounter()) != _queuedPlayers.end();
}

Optional<uint8> ArenaSoloMgr::GetQueuedBracket(ObjectGuid guid) const
{
    auto itr = _queuedPlayers.find(guid.GetCounter());
    if (itr == _queuedPlayers.end())
        return {};

    return itr->second;
}

void ArenaSoloMgr::RemovePlayer(ObjectGuid guid)
{
    auto queued = _queuedPlayers.find(guid.GetCounter());
    if (queued == _queuedPlayers.end())
        return;

    uint8 bracket = queued->second;
    _queuedPlayers.erase(queued);

    if (bracket >= ARENA_SOLO_BRACKET_MAX)
        return;

    std::list<ArenaSoloQueueEntry>& queue = _queues[bracket];
    for (auto itr = queue.begin(); itr != queue.end(); ++itr)
    {
        if (itr->Guid == guid)
        {
            queue.erase(itr);
            return;
        }
    }
}

ArenaSoloStats ArenaSoloMgr::GetStats(ObjectGuid guid, uint8 bracket)
{
    uint64 key = StatsKey(guid, bracket);
    auto cached = _statsCache.find(key);
    if (cached != _statsCache.end())
        return cached->second;

    ArenaSoloStats stats = LoadStats(guid, bracket);
    _statsCache[key] = stats;
    return stats;
}

ArenaSoloStats ArenaSoloMgr::LoadStats(ObjectGuid guid, uint8 bracket)
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];

    ArenaSoloStats stats;
    stats.Rating = config.StartRating;
    stats.MMR = config.StartMMR;
    stats.HighestRating = config.StartRating;

    QueryResult result = CharacterDatabase.Query(
        "SELECT rating, mmr, games, wins, week_games, week_wins, week_points, highest_rating "
        "FROM arena_solo_stats WHERE guid = {} AND bracket = {}", guid.GetCounter(), bracket);
    if (!result)
        return stats;

    Field* fields = result->Fetch();
    stats.Rating = fields[0].Get<uint32>();
    stats.MMR = fields[1].Get<uint32>();
    stats.Games = fields[2].Get<uint32>();
    stats.Wins = fields[3].Get<uint32>();
    stats.WeekGames = fields[4].Get<uint16>();
    stats.WeekWins = fields[5].Get<uint16>();
    stats.WeekPoints = fields[6].Get<uint16>();
    stats.HighestRating = fields[7].Get<uint32>();
    return stats;
}

void ArenaSoloMgr::SaveStats(ObjectGuid guid, uint8 bracket, ArenaSoloStats const& stats)
{
    _statsCache[StatsKey(guid, bracket)] = stats;
    CharacterDatabase.Execute(
        "INSERT INTO arena_solo_stats (guid, bracket, rating, mmr, games, wins, week_games, "
        "week_wins, week_points, highest_rating) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE rating = {}, mmr = {}, games = {}, wins = {}, week_games = {}, "
        "week_wins = {}, week_points = {}, highest_rating = {}",
        guid.GetCounter(), bracket, stats.Rating, stats.MMR, stats.Games, stats.Wins,
        stats.WeekGames, stats.WeekWins, stats.WeekPoints, stats.HighestRating,
        stats.Rating, stats.MMR, stats.Games, stats.Wins, stats.WeekGames, stats.WeekWins,
        stats.WeekPoints, stats.HighestRating);
}

uint32 ArenaSoloMgr::AverageMMR(std::vector<ArenaSoloQueueEntry> const& team)
{
    if (team.empty())
        return 1500;

    uint64 total = 0;
    for (ArenaSoloQueueEntry const& entry : team)
        total += entry.MMR;

    return static_cast<uint32>(total / team.size());
}

void ArenaSoloMgr::PruneQueue(uint8 bracket)
{
    std::list<ArenaSoloQueueEntry>& queue = _queues[bracket];
    for (auto itr = queue.begin(); itr != queue.end();)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(itr->Guid);
        std::string error;
        if (player && CanQueue(player, bracket, error))
        {
            ++itr;
            continue;
        }

        if (player)
            ChatHandler(player->GetSession()).PSendSysMessage(
                "Removed from the {} solo queue: {}", GetBracketName(bracket), error);

        _queuedPlayers.erase(itr->Guid.GetCounter());
        itr = queue.erase(itr);
    }
}

bool ArenaSoloMgr::BuildTeams(uint8 bracket, std::vector<ArenaSoloQueueEntry>& alliance,
    std::vector<ArenaSoloQueueEntry>& horde)
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];
    std::list<ArenaSoloQueueEntry>& queue = _queues[bracket];
    uint32 needed = config.TeamSize * 2;

    if (queue.size() < needed)
        return false;

    uint32 now = GameTime::GetGameTimeMS().count();
    ArenaSoloQueueEntry const& anchor = queue.front();
    uint32 anchorWait = now > anchor.JoinTime ? now - anchor.JoinTime : 0;
    uint32 anchorWindow = PvPRating::MatchmakingWindow(config.MaxRatingDiff, anchorWait, config.RatingDiscardTimer);

    // Everyone whose MMR is close enough to the longest-waiting player.
    std::vector<ArenaSoloQueueEntry> pool;
    for (ArenaSoloQueueEntry const& entry : queue)
    {
        uint32 wait = now > entry.JoinTime ? now - entry.JoinTime : 0;
        uint32 window = std::max(anchorWindow,
            PvPRating::MatchmakingWindow(config.MaxRatingDiff, wait, config.RatingDiscardTimer));
        uint32 diff = entry.MMR > anchor.MMR ? entry.MMR - anchor.MMR : anchor.MMR - entry.MMR;
        if (diff <= window)
            pool.push_back(entry);
    }

    if (pool.size() < needed)
        return false;

    auto totalMMR = [](std::vector<ArenaSoloQueueEntry> const& side)
    {
        uint64 total = 0;
        for (ArenaSoloQueueEntry const& entry : side)
            total += entry.MMR;

        return total;
    };

    auto assign = [&](ArenaSoloQueueEntry const& entry)
    {
        bool allianceFull = alliance.size() >= config.TeamSize;
        bool hordeFull = horde.size() >= config.TeamSize;
        if (allianceFull && hordeFull)
            return;

        // Send the player to the side that is behind, so both ends up with a
        // comparable combined MMR instead of all the top players on one side.
        bool toAlliance = hordeFull || (!allianceFull && totalMMR(alliance) <= totalMMR(horde));
        if (toAlliance)
            alliance.push_back(entry);
        else
            horde.push_back(entry);
    };

    if (!_crossFaction)
    {
        std::vector<ArenaSoloQueueEntry> alliancePool;
        std::vector<ArenaSoloQueueEntry> hordePool;
        for (ArenaSoloQueueEntry const& entry : pool)
        {
            if (entry.Team == TEAM_ALLIANCE)
                alliancePool.push_back(entry);
            else
                hordePool.push_back(entry);
        }

        if (alliancePool.size() < config.TeamSize || hordePool.size() < config.TeamSize)
            return false;

        auto takeSide = [&](std::vector<ArenaSoloQueueEntry>& source, std::vector<ArenaSoloQueueEntry>& side)
        {
            if (config.RequireRoleBalance)
            {
                auto healer = std::find_if(source.begin(), source.end(),
                    [](ArenaSoloQueueEntry const& entry) { return entry.Healer; });
                if (healer == source.end())
                    return false;

                side.push_back(*healer);
                source.erase(healer);
            }

            for (ArenaSoloQueueEntry const& entry : source)
            {
                if (side.size() >= config.TeamSize)
                    break;

                side.push_back(entry);
            }

            return side.size() == config.TeamSize;
        };

        if (!takeSide(alliancePool, alliance) || !takeSide(hordePool, horde))
        {
            alliance.clear();
            horde.clear();
            return false;
        }

        return true;
    }

    if (config.RequireRoleBalance)
    {
        std::vector<ArenaSoloQueueEntry> healers;
        std::vector<ArenaSoloQueueEntry> damage;
        for (ArenaSoloQueueEntry const& entry : pool)
        {
            if (entry.Healer)
                healers.push_back(entry);
            else
                damage.push_back(entry);
        }

        if (healers.size() < 2 || damage.size() < needed - 2)
            return false;

        alliance.push_back(healers[0]);
        horde.push_back(healers[1]);

        for (ArenaSoloQueueEntry const& entry : damage)
        {
            if (alliance.size() + horde.size() >= needed)
                break;

            assign(entry);
        }
    }
    else
    {
        for (ArenaSoloQueueEntry const& entry : pool)
        {
            if (alliance.size() + horde.size() >= needed)
                break;

            assign(entry);
        }
    }

    if (alliance.size() != config.TeamSize || horde.size() != config.TeamSize)
    {
        alliance.clear();
        horde.clear();
        return false;
    }

    return true;
}

void ArenaSoloMgr::TryMatch(uint8 bracket)
{
    if (!IsBracketEnabled(bracket))
        return;

    PruneQueue(bracket);

    std::vector<ArenaSoloQueueEntry> alliance;
    std::vector<ArenaSoloQueueEntry> horde;
    if (!BuildTeams(bracket, alliance, horde))
        return;

    // Only drop the picked players from the queue once the arena really exists.
    if (!StartMatch(bracket, alliance, horde))
        return;

    for (ArenaSoloQueueEntry const& entry : alliance)
        RemovePlayer(entry.Guid);

    for (ArenaSoloQueueEntry const& entry : horde)
        RemovePlayer(entry.Guid);
}

bool ArenaSoloMgr::StartMatch(uint8 bracket, std::vector<ArenaSoloQueueEntry> const& alliance,
    std::vector<ArenaSoloQueueEntry> const& horde)
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];

    Battleground* arenaTemplate = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
    if (!arenaTemplate)
    {
        LOG_ERROR("module.arenasolo", "All Arenas template not found; cannot start solo match.");
        return false;
    }

    Player* first = ObjectAccessor::FindConnectedPlayer(alliance.front().Guid);
    if (!first)
        return false;

    PvPDifficultyEntry const* bracketEntry =
        GetBattlegroundBracketByLevel(arenaTemplate->GetMapId(), first->GetLevel());
    if (!bracketEntry)
        return false;

    // Skirmish flag: the core's rated path requires persistent arena teams, which
    // solo queue has none of. Rating is applied by this module instead.
    Battleground* bg = sBattlegroundMgr->CreateNewBattleground(BATTLEGROUND_AA, bracketEntry,
        config.ArenaType, false);
    if (!bg)
    {
        LOG_ERROR("module.arenasolo", "Failed to create solo arena for bracket {}", GetBracketName(bracket));
        return false;
    }

    bg->SetMaxPlayersPerTeam(config.TeamSize);
    bg->SetMinPlayersPerTeam(config.TeamSize);

    BattlegroundQueueTypeId queueTypeId = BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_AA, config.ArenaType);
    BattlegroundQueue& queue = sBattlegroundMgr->GetBattlegroundQueue(queueTypeId);

    // Each solo player is queued as their own one-man group, then invited to the
    // side we picked; the battleground itself builds the team's raid group.
    auto inviteSide = [&](std::vector<ArenaSoloQueueEntry> const& side, TeamId teamId)
    {
        for (ArenaSoloQueueEntry const& entry : side)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(entry.Guid);
            if (!player)
                continue;

            GroupQueueInfo* ginfo = queue.AddGroup(player, nullptr, BATTLEGROUND_AA, bracketEntry,
                config.ArenaType, false, false, entry.Rating, entry.MMR);

            uint32 slot = player->AddBattlegroundQueueId(queueTypeId);
            WorldPacket data;
            sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, arenaTemplate, slot,
                STATUS_WAIT_QUEUE, 0, 0, config.ArenaType, TEAM_NEUTRAL);
            player->SendDirectMessage(&data);

            queue.InviteGroupToBG(ginfo, bg, teamId);

            ChatHandler(player->GetSession()).PSendSysMessage(
                "{} match found! Accept the arena invite.", GetBracketName(bracket));
        }
    };

    inviteSide(alliance, TEAM_ALLIANCE);
    inviteSide(horde, TEAM_HORDE);

    bg->StartBattleground();
    bg->RemoveFromBGFreeSlotQueue();

    ArenaSoloMatch match;
    match.InstanceId = bg->GetInstanceID();
    match.Bracket = bracket;
    match.AllianceMMR = AverageMMR(alliance);
    match.HordeMMR = AverageMMR(horde);
    for (ArenaSoloQueueEntry const& entry : alliance)
        match.Alliance.push_back(entry.Guid);
    for (ArenaSoloQueueEntry const& entry : horde)
        match.Horde.push_back(entry.Guid);

    _matches[match.InstanceId] = match;

    LOG_INFO("module.arenasolo", "Started {} arena instance {} on map {} (mmr {} vs {})",
        GetBracketName(bracket), match.InstanceId, bg->GetMapId(), match.AllianceMMR, match.HordeMMR);
    return true;
}

void ArenaSoloMgr::ApplyResult(ObjectGuid guid, uint8 bracket, uint32 opponentMMR, bool won)
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];
    ArenaSoloStats stats = GetStats(guid, bracket);

    uint32 oldRating = stats.Rating;
    int32 ratingChange = PvPRating::RatingMod(config.Elo, stats.Rating, opponentMMR, won);
    int32 mmrChange = PvPRating::MatchmakerMod(config.Elo, stats.MMR, opponentMMR, won);

    int32 newRating = static_cast<int32>(stats.Rating) + ratingChange;
    int32 newMMR = static_cast<int32>(stats.MMR) + mmrChange;
    stats.Rating = newRating < 0 ? 0 : static_cast<uint32>(newRating);
    stats.MMR = newMMR < 0 ? 0 : static_cast<uint32>(newMMR);
    if (stats.Rating > stats.HighestRating)
        stats.HighestRating = stats.Rating;

    stats.Games += 1;
    stats.WeekGames += 1;
    if (won)
    {
        stats.Wins += 1;
        stats.WeekWins += 1;
    }

    uint32 arenaPoints = 0;
    if (won && config.PointsWin)
    {
        arenaPoints = config.PointsWin;
        uint32 remaining = config.WeeklyCap > stats.WeekPoints ? config.WeeklyCap - stats.WeekPoints : 0;
        if (arenaPoints > remaining)
            arenaPoints = remaining;

        stats.WeekPoints = static_cast<uint16>(stats.WeekPoints + arenaPoints);
    }

    SaveStats(guid, bracket, stats);

    Player* player = ObjectAccessor::FindConnectedPlayer(guid);
    if (!player)
        return;

    player->ModifyHonorPoints(won ? static_cast<int32>(config.HonorWin) : static_cast<int32>(config.HonorLoss));
    if (arenaPoints)
        player->ModifyArenaPoints(static_cast<int32>(arenaPoints));

    ChatHandler(player->GetSession()).PSendSysMessage(
        "{} {}: {} rating ({} -> {}), {} MMR. {} honor, {} arena points.",
        GetBracketName(bracket), won ? "victory" : "defeat",
        ratingChange >= 0 ? Acore::StringFormat("+{}", ratingChange) : std::to_string(ratingChange),
        oldRating, stats.Rating,
        mmrChange >= 0 ? Acore::StringFormat("+{}", mmrChange) : std::to_string(mmrChange),
        won ? config.HonorWin : config.HonorLoss, arenaPoints);
}

void ArenaSoloMgr::HandleBattlegroundEnd(Battleground* bg, TeamId winner)
{
    if (!bg)
        return;

    auto itr = _matches.find(bg->GetInstanceID());
    if (itr == _matches.end())
        return;

    ArenaSoloMatch match = itr->second;
    if (winner == TEAM_NEUTRAL)
    {
        LOG_INFO("module.arenasolo", "{} instance {} ended in a draw; no rating change.",
            GetBracketName(match.Bracket), match.InstanceId);
        return;
    }

    for (ObjectGuid const& guid : match.Alliance)
        ApplyResult(guid, match.Bracket, match.HordeMMR, winner == TEAM_ALLIANCE);
    for (ObjectGuid const& guid : match.Horde)
        ApplyResult(guid, match.Bracket, match.AllianceMMR, winner == TEAM_HORDE);
}

void ArenaSoloMgr::HandleBattlegroundDestroy(Battleground* bg)
{
    if (!bg)
        return;

    _matches.erase(bg->GetInstanceID());
}

void ArenaSoloMgr::CheckWeekReset()
{
    uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
    if (!_weekStart)
        _weekStart = now;

    if (now < _weekStart + WEEK)
        return;

    _weekStart = now;
    CharacterDatabase.Execute("UPDATE arena_solo_state SET week_start = {} WHERE id = 1", _weekStart);
    CharacterDatabase.Execute("UPDATE arena_solo_stats SET week_games = 0, week_wins = 0, week_points = 0");
    for (auto& [key, stats] : _statsCache)
    {
        stats.WeekGames = 0;
        stats.WeekWins = 0;
        stats.WeekPoints = 0;
    }

    LOG_INFO("module.arenasolo", "Arena solo queue weekly stats reset.");
}

std::vector<PvPLeaderboardRow> ArenaSoloMgr::GetLeaderboard(uint8 bracket, uint32 limit)
{
    std::vector<PvPLeaderboardRow> rows;
    if (bracket >= ARENA_SOLO_BRACKET_MAX)
        return rows;

    QueryResult result = CharacterDatabase.Query(
        "SELECT c.name, s.rating, s.wins, (s.games - s.wins) FROM arena_solo_stats s "
        "INNER JOIN characters c ON c.guid = s.guid WHERE s.bracket = {} "
        "ORDER BY s.rating DESC, s.wins DESC LIMIT {}", bracket, limit);
    if (!result)
        return rows;

    do
    {
        Field* fields = result->Fetch();
        PvPLeaderboardRow row;
        row.Name = fields[0].Get<std::string>();
        row.Rating = fields[1].Get<uint32>();
        row.Wins = fields[2].Get<uint32>();
        row.Losses = fields[3].Get<uint32>();
        rows.push_back(row);
    } while (result->NextRow());

    return rows;
}
