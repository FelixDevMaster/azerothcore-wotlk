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
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
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
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include <algorithm>
#include <utility>
#include <unordered_map>
#include <unordered_set>

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
    switch (bracket)
    {
        case ARENA_SOLO_BRACKET_1V1:
            return "1v1";
        case ARENA_SOLO_BRACKET_2V2:
            return "2v2";
        default:
            return "3v3 SoloQ";
    }
}

Optional<uint8> ArenaSoloMgr::ParseBracket(std::string_view token)
{
    if (token == "1v1" || token == "1" || token == "solo1")
        return ARENA_SOLO_BRACKET_1V1;

    if (token == "2v2" || token == "2" || token == "duo")
        return ARENA_SOLO_BRACKET_2V2;

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

    auto loadBracket = [](ArenaSoloBracketConfig& config, char const* prefix, uint32 teamSize,
        uint32 groupSize, uint8 arenaType)
    {
        auto option = [prefix](char const* name)
        {
            return Acore::StringFormat("ArenaSolo.{}.{}", prefix, name);
        };

        config.Enabled = sConfigMgr->GetOption<bool>(option("Enable"), true);
        config.TeamSize = teamSize;
        config.GroupSize = groupSize;
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

    // 1v1 keeps a module-owned personal rating. 2v2 and 3v3 SoloQ each give
    // the character a personal arena_team (2v2 slot / 5v5 slot) so rating
    // lands on arena_team_member. 3v3 stays solo-entry and is assembled
    // from official compositions when the queue allows it.
    loadBracket(_brackets[ARENA_SOLO_BRACKET_1V1], "1v1", 1, 1, ARENA_TYPE_2v2);
    loadBracket(_brackets[ARENA_SOLO_BRACKET_2V2], "2v2", 2, 2, ARENA_TYPE_2v2);
    loadBracket(_brackets[ARENA_SOLO_BRACKET_3V3], "3v3", 3, 1, ARENA_TYPE_3v3);

    _brackets[ARENA_SOLO_BRACKET_1V1].RequireRoleBalance = false;
    _brackets[ARENA_SOLO_BRACKET_1V1].UseCoreArenaTeam = false;

    _brackets[ARENA_SOLO_BRACKET_2V2].RequireRoleBalance = false;
    _brackets[ARENA_SOLO_BRACKET_2V2].UseCoreArenaTeam = true;
    _brackets[ARENA_SOLO_BRACKET_2V2].ArenaTeamType = ARENA_TEAM_2v2;
    _brackets[ARENA_SOLO_BRACKET_2V2].ArenaTeamSlot = ARENA_SLOT_2v2;

    _brackets[ARENA_SOLO_BRACKET_3V3].RequireRoleBalance =
        sConfigMgr->GetOption<bool>("ArenaSolo.3v3.RequireHealer", true);
    _brackets[ARENA_SOLO_BRACKET_3V3].PreferComps =
        sConfigMgr->GetOption<bool>("ArenaSolo.3v3.PreferComps", true);
    _brackets[ARENA_SOLO_BRACKET_3V3].UseCoreArenaTeam = true;
    _brackets[ARENA_SOLO_BRACKET_3V3].ArenaTeamType = ARENA_TEAM_5v5;
    _brackets[ARENA_SOLO_BRACKET_3V3].ArenaTeamSlot = ARENA_SLOT_5v5;

    LOG_INFO("module.arenasolo", "Arenas: {} (1v1 {}, 2v2 team {}, 3v3 soloq {})",
        _enabled ? "enabled" : "disabled",
        _brackets[ARENA_SOLO_BRACKET_1V1].Enabled ? "on" : "off",
        _brackets[ARENA_SOLO_BRACKET_2V2].Enabled ? "on" : "off",
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

bool ArenaSoloMgr::UsesCoreArenaTeam(uint8 bracket) const
{
    return bracket < ARENA_SOLO_BRACKET_MAX && _brackets[bracket].UseCoreArenaTeam;
}

ArenaSoloBracketConfig const& ArenaSoloMgr::GetBracketConfig(uint8 bracket) const
{
    uint8 index = bracket < ARENA_SOLO_BRACKET_MAX ? bracket : uint8(ARENA_SOLO_BRACKET_1V1);
    return _brackets[index];
}

uint32 ArenaSoloMgr::GetQueuedCount(uint8 bracket) const
{
    if (bracket >= ARENA_SOLO_BRACKET_MAX)
        return 0;

    return static_cast<uint32>(_queues[bracket].size());
}

bool ArenaSoloMgr::CanQueuePlayer(Player* player, uint8 bracket, std::string& error) const
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];

    if (player->GetLevel() < config.MinLevel)
    {
        error = Acore::StringFormat("{} must be level {} to queue for {}.",
            player->GetName(), config.MinLevel, GetBracketName(bracket));
        return false;
    }

    if (player->InBattleground() || player->InBattlegroundQueue())
    {
        error = Acore::StringFormat("{} is already in a battleground or queue.", player->GetName());
        return false;
    }

    if (player->IsDeserter() || player->HasAura(SPELL_DESERTER))
    {
        error = Acore::StringFormat("{} is flagged as a Deserter.", player->GetName());
        return false;
    }

    if (!player->HasFreeBattlegroundQueueId())
    {
        error = Acore::StringFormat("{} has no free battleground queue slot.", player->GetName());
        return false;
    }

    return true;
}

bool ArenaSoloMgr::CollectMembers(Player* leader, uint8 bracket, std::vector<Player*>& members,
    std::string& error) const
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];
    Group* group = leader->GetGroup();

    if (config.GroupSize <= 1)
    {
        if (group)
        {
            error = Acore::StringFormat("{} is a solo queue: leave your group first.",
                GetBracketName(bracket));
            return false;
        }

        members.push_back(leader);
    }
    else
    {
        if (!group || group->GetLeaderGUID() != leader->GetGUID())
        {
            error = Acore::StringFormat("Only the party leader can queue for {}.", GetBracketName(bracket));
            return false;
        }

        if (group->isRaidGroup() || group->isBGGroup() || group->isBFGroup() || group->isLFGGroup())
        {
            error = Acore::StringFormat("{} requires a normal party.", GetBracketName(bracket));
            return false;
        }

        if (group->GetMembersCount() != config.GroupSize)
        {
            error = Acore::StringFormat("{} requires a party of exactly {} players (you have {}).",
                GetBracketName(bracket), config.GroupSize, group->GetMembersCount());
            return false;
        }

        group->DoForAllMembers([&members](Player* member)
        {
            members.push_back(member);
        });

        if (members.size() != config.GroupSize)
        {
            error = "All party members must be online and in the world.";
            return false;
        }
    }

    for (Player* member : members)
        if (!CanQueuePlayer(member, bracket, error))
            return false;

    return true;
}

namespace
{
uint32 constexpr PERSONAL_TEAM_NAME_MAX = 24;

ArenaTeam* FindPersonalArenaTeam(ObjectGuid guid, uint8 type)
{
    uint32 teamId = 0;
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        teamId = player->GetArenaTeamId(ArenaTeam::GetSlotByType(type));
    else
        teamId = Player::GetArenaTeamIdFromDB(guid, type);

    ArenaTeam* team = teamId ? sArenaTeamMgr->GetArenaTeamById(teamId) : nullptr;
    if (!team || team->GetType() != type)
        return nullptr;

    return team;
}

std::string MakePersonalArenaTeamName(Player* player, uint8 type)
{
    std::string name;
    if (type == ARENA_TEAM_5v5)
    {
        // Fits in arena_team.name (varchar 24): 12-char character + " 3vs3 soloq".
        std::string const label = " 3vs3 soloq";
        name = player->GetName();
        if (name.size() + label.size() > PERSONAL_TEAM_NAME_MAX)
            name.resize(PERSONAL_TEAM_NAME_MAX - label.size());
        name += label;
    }
    else
        name = player->GetName();

    if (name.size() > PERSONAL_TEAM_NAME_MAX)
        name.resize(PERSONAL_TEAM_NAME_MAX);

    if (!sArenaTeamMgr->GetArenaTeamByName(name))
        return name;

    std::string suffix = Acore::StringFormat("-{}", player->GetGUID().GetCounter());
    if (name.size() + suffix.size() > PERSONAL_TEAM_NAME_MAX)
        name = name.substr(0, PERSONAL_TEAM_NAME_MAX - suffix.size()) + suffix;
    else
        name += suffix;

    if (name.size() > PERSONAL_TEAM_NAME_MAX)
        name.resize(PERSONAL_TEAM_NAME_MAX);

    return name;
}

void RenameLegacySoloQTeam(ArenaTeam* team, Player* player, uint8 type)
{
    if (!team || !player || type != ARENA_TEAM_5v5)
        return;

    std::string const& current = team->GetName();
    if (current.size() < 3 || current.compare(current.size() - 3, 3, "-5s") != 0)
        return;

    std::string wanted = MakePersonalArenaTeamName(player, type);
    if (wanted == current)
        return;

    if (team->SetName(wanted))
        LOG_INFO("module.arenasolo", "Renamed personal 3v3 team {} -> {} ({})",
            current, wanted, player->GetGUID().ToString());
}
}

bool ArenaSoloMgr::EnsurePersonalArenaTeam(Player* player, uint8 bracket, std::string& error)
{
    if (!player)
    {
        error = "Player not found.";
        return false;
    }

    if (bracket >= ARENA_SOLO_BRACKET_MAX || !_brackets[bracket].UseCoreArenaTeam)
    {
        error = "That bracket does not use arena teams.";
        return false;
    }

    uint8 type = _brackets[bracket].ArenaTeamType;
    uint8 slot = _brackets[bracket].ArenaTeamSlot;

    if (ArenaTeam* team = FindPersonalArenaTeam(player->GetGUID(), type))
    {
        RenameLegacySoloQTeam(team, player, type);
        return true;
    }

    if (ArenaTeam* existing = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), type))
    {
        if (!player->GetArenaTeamId(slot) && existing->IsMember(player->GetGUID()))
            player->SetInArenaTeam(existing->GetId(), slot, type);
        RenameLegacySoloQTeam(existing, player, type);
        return true;
    }

    ArenaTeam* team = new ArenaTeam();
    if (!team->Create(player->GetGUID(), type, MakePersonalArenaTeamName(player, type),
        4293102085, 101, 4293253939, 4, 4284049911))
    {
        delete team;
        error = Acore::StringFormat("Failed to create a personal {} arena team for {}.",
            GetBracketName(bracket), player->GetName());
        return false;
    }

    sArenaTeamMgr->AddArenaTeam(team);
    LOG_INFO("module.arenasolo", "Created personal {} arena team {} ({}) for {} ({})",
        GetBracketName(bracket), team->GetName(), team->GetId(), player->GetName(),
        player->GetGUID().ToString());
    return true;
}

bool ArenaSoloMgr::BindArenaTeam(std::vector<Player*> const& members, uint8 bracket,
    ArenaSoloQueueEntry& entry, std::string& error)
{
    if (members.empty())
    {
        error = Acore::StringFormat("{} queue is empty.", GetBracketName(bracket));
        return false;
    }

    uint8 type = _brackets[bracket].ArenaTeamType;
    uint64 totalRating = 0;
    uint64 totalMMR = 0;
    uint32 ratedMembers = 0;

    for (Player* member : members)
    {
        if (!EnsurePersonalArenaTeam(member, bracket, error))
            return false;

        ArenaTeam* team = FindPersonalArenaTeam(member->GetGUID(), type);
        if (!team)
        {
            error = Acore::StringFormat("{} has no personal {} arena team.",
                member->GetName(), GetBracketName(bracket));
            return false;
        }

        if (team->IsFighting())
        {
            error = Acore::StringFormat("{}'s arena team {} is already in a match.",
                member->GetName(), team->GetName());
            return false;
        }

        ArenaTeamMember* row = team->GetMember(member->GetGUID());
        totalRating += row ? row->PersonalRating : team->GetRating();
        totalMMR += row ? row->MatchMakerRating : team->GetRating();
        ++ratedMembers;
    }

    entry.ArenaTeamId = members.front()->GetArenaTeamId(_brackets[bracket].ArenaTeamSlot);
    entry.Rating = ratedMembers ? static_cast<uint32>(totalRating / ratedMembers) : 1500;
    entry.MMR = ratedMembers ? static_cast<uint32>(totalMMR / ratedMembers) : 1500;
    return true;
}

bool ArenaSoloMgr::BuildEntry(Player* leader, uint8 bracket, ArenaSoloQueueEntry& entry, std::string& error)
{
    std::vector<Player*> members;
    if (!CollectMembers(leader, bracket, members, error))
        return false;

    ArenaSoloBracketConfig const& config = _brackets[bracket];

    for (Player* member : members)
    {
        if (IsQueued(member->GetGUID()))
        {
            error = Acore::StringFormat("{} is already in an arena queue.", member->GetName());
            return false;
        }

        entry.Members.push_back(member->GetGUID());
        if (member->HasHealSpec())
            ++entry.Healers;
    }

    entry.LeaderGuid = leader->GetGUID();
    entry.Team = leader->GetTeamId();
    entry.JoinTime = GameTime::GetGameTimeMS().count();
    entry.PlayerClass = leader->getClass();
    entry.TalentTab = leader->GetSpec();

    if (config.UseCoreArenaTeam)
        return BindArenaTeam(members, bracket, entry, error);

    uint64 totalRating = 0;
    uint64 totalMMR = 0;
    for (Player* member : members)
    {
        ArenaSoloStats stats = GetStats(member->GetGUID(), bracket);
        totalRating += stats.Rating;
        totalMMR += stats.MMR;
    }

    entry.Rating = static_cast<uint32>(totalRating / members.size());
    entry.MMR = static_cast<uint32>(totalMMR / members.size());
    return true;
}

bool ArenaSoloMgr::RevalidateEntry(ArenaSoloQueueEntry const& entry, uint8 bracket, std::string& error)
{
    Player* leader = ObjectAccessor::FindConnectedPlayer(entry.LeaderGuid);
    if (!leader)
    {
        error = "The queue leader went offline.";
        return false;
    }

    std::vector<Player*> members;
    if (!CollectMembers(leader, bracket, members, error))
        return false;

    // The party must still be the exact same people that queued.
    if (members.size() != entry.Members.size())
    {
        error = "The party changed while queued.";
        return false;
    }

    for (Player* member : members)
    {
        if (!entry.Contains(member->GetGUID()))
        {
            error = "The party changed while queued.";
            return false;
        }
    }

    if (_brackets[bracket].UseCoreArenaTeam)
    {
        ArenaSoloQueueEntry check;
        if (!BindArenaTeam(members, bracket, check, error))
            return false;

        if (check.ArenaTeamId != entry.ArenaTeamId)
        {
            error = "The arena team changed while queued.";
            return false;
        }
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
        error = "That arena queue is disabled.";
        return false;
    }

    if (IsQueued(player->GetGUID()))
    {
        error = "You are already in an arena queue. Leave it first.";
        return false;
    }

    ArenaSoloQueueEntry entry;
    if (!BuildEntry(player, bracket, entry, error))
        return false;

    for (ObjectGuid const& guid : entry.Members)
        _queuedPlayers[guid.GetCounter()] = bracket;

    _queues[bracket].push_back(entry);

    std::string joined = Acore::StringFormat(
        "Joined the {} queue (rating {}, MMR {}). {} entr{} waiting.",
        GetBracketName(bracket), entry.Rating, entry.MMR,
        _queues[bracket].size(), _queues[bracket].size() == 1 ? "y" : "ies");

    for (ObjectGuid const& guid : entry.Members)
        if (Player* member = ObjectAccessor::FindConnectedPlayer(guid))
            ChatHandler(member->GetSession()).SendSysMessage(joined);

    if (_announceQueue)
    {
        std::string message = Acore::StringFormat("|cff00ff00Arena {}:|r {} entr{} in the queue.",
            GetBracketName(bracket), _queues[bracket].size(), _queues[bracket].size() == 1 ? "y" : "ies");
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
        error = "You are not in an arena queue.";
        return false;
    }

    RemovePlayer(player->GetGUID());
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
    if (bracket >= ARENA_SOLO_BRACKET_MAX)
    {
        _queuedPlayers.erase(queued);
        return;
    }

    // A premade entry leaves as a whole: one member dropping out invalidates it.
    std::list<ArenaSoloQueueEntry>& queue = _queues[bracket];
    for (auto itr = queue.begin(); itr != queue.end(); ++itr)
    {
        if (!itr->Contains(guid))
            continue;

        for (ObjectGuid const& member : itr->Members)
        {
            _queuedPlayers.erase(member.GetCounter());
            if (Player* memberPlayer = ObjectAccessor::FindConnectedPlayer(member))
                ChatHandler(memberPlayer->GetSession()).PSendSysMessage(
                    "You left the {} queue.", GetBracketName(bracket));
        }

        queue.erase(itr);
        return;
    }

    _queuedPlayers.erase(guid.GetCounter());
}

ArenaSoloStats ArenaSoloMgr::GetStats(ObjectGuid guid, uint8 bracket)
{
    if (UsesCoreArenaTeam(bracket))
        return LoadArenaTeamMemberStats(guid, bracket);

    uint64 key = StatsKey(guid, bracket);
    auto cached = _statsCache.find(key);
    if (cached != _statsCache.end())
        return cached->second;

    ArenaSoloStats stats = LoadStats(guid, bracket);
    _statsCache[key] = stats;
    return stats;
}

ArenaSoloStats ArenaSoloMgr::LoadArenaTeamMemberStats(ObjectGuid guid, uint8 bracket) const
{
    ArenaSoloStats stats;
    stats.Rating = 0;
    stats.MMR = 0;
    stats.HighestRating = 0;

    if (bracket >= ARENA_SOLO_BRACKET_MAX)
        return stats;

    ArenaTeam* team = FindPersonalArenaTeam(guid, _brackets[bracket].ArenaTeamType);
    if (!team)
        return stats;

    ArenaTeamMember* member = team->GetMember(guid);
    if (!member)
        return stats;

    stats.Rating = member->PersonalRating;
    stats.MMR = member->MatchMakerRating;
    stats.Games = member->SeasonGames;
    stats.Wins = member->SeasonWins;
    stats.WeekGames = member->WeekGames;
    stats.WeekWins = member->WeekWins;
    stats.HighestRating = member->PersonalRating;
    stats.TeamRating = team->GetRating();
    stats.TeamName = team->GetName();
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

uint32 ArenaSoloMgr::SidePlayerCount(std::vector<ArenaSoloQueueEntry> const& side)
{
    uint32 count = 0;
    for (ArenaSoloQueueEntry const& entry : side)
        count += static_cast<uint32>(entry.Members.size());

    return count;
}

uint32 ArenaSoloMgr::TotalMMR(std::vector<ArenaSoloQueueEntry> const& side)
{
    uint32 total = 0;
    for (ArenaSoloQueueEntry const& entry : side)
        total += entry.MMR * static_cast<uint32>(entry.Members.size());

    return total;
}

uint32 ArenaSoloMgr::AverageMMR(std::vector<ArenaSoloQueueEntry> const& side)
{
    uint32 players = SidePlayerCount(side);
    if (!players)
        return 1500;

    return TotalMMR(side) / players;
}

void ArenaSoloMgr::PruneQueue(uint8 bracket)
{
    std::list<ArenaSoloQueueEntry>& queue = _queues[bracket];
    for (auto itr = queue.begin(); itr != queue.end();)
    {
        std::string error;
        if (RevalidateEntry(*itr, bracket, error))
        {
            ++itr;
            continue;
        }

        for (ObjectGuid const& guid : itr->Members)
        {
            _queuedPlayers.erase(guid.GetCounter());
            if (Player* member = ObjectAccessor::FindConnectedPlayer(guid))
                ChatHandler(member->GetSession()).PSendSysMessage(
                    "Removed from the {} queue: {}", GetBracketName(bracket), error);
        }

        itr = queue.erase(itr);
    }
}

namespace
{
struct CompSlot
{
    uint8 Class = 0;
    uint32 TalentTab = 0;
    bool MustHeal = false;
    bool ForbidShadow = false;
};

struct OfficialComp
{
    char const* Name;
    CompSlot Slots[3];
};

bool FitsCompSlot(ArenaSoloQueueEntry const& entry, CompSlot const& slot)
{
    if (slot.Class && entry.PlayerClass != slot.Class)
        return false;
    if (slot.TalentTab && entry.TalentTab != slot.TalentTab)
        return false;
    if (slot.MustHeal && !entry.Healers)
        return false;
    if (slot.ForbidShadow && entry.TalentTab == TALENT_TREE_PRIEST_SHADOW)
        return false;
    return true;
}

bool TakeComp(std::vector<ArenaSoloQueueEntry> const& pool, OfficialComp const& comp,
    std::vector<bool>& used, std::vector<ArenaSoloQueueEntry>& out)
{
    size_t picked[3] = { pool.size(), pool.size(), pool.size() };
    for (uint8 slot = 0; slot < 3; ++slot)
    {
        bool found = false;
        for (size_t i = 0; i < pool.size(); ++i)
        {
            if (used[i])
                continue;

            bool already = false;
            for (uint8 prev = 0; prev < slot; ++prev)
                if (picked[prev] == i)
                    already = true;
            if (already)
                continue;

            if (!FitsCompSlot(pool[i], comp.Slots[slot]))
                continue;

            picked[slot] = i;
            found = true;
            break;
        }

        if (!found)
            return false;
    }

    out.clear();
    for (uint8 slot = 0; slot < 3; ++slot)
    {
        used[picked[slot]] = true;
        out.push_back(pool[picked[slot]]);
    }
    return true;
}

// Named 3v3 comps first (more specific before generic). Fallback is 1h+2d.
OfficialComp const OfficialComps[] =
{
    { "Shadowplay", {
        { CLASS_ROGUE, 0, false, false },
        { CLASS_MAGE, 0, false, false },
        { CLASS_PRIEST, TALENT_TREE_PRIEST_SHADOW, false, false }
    }},
    { "RMP", {
        { CLASS_ROGUE, 0, false, false },
        { CLASS_MAGE, 0, false, false },
        { CLASS_PRIEST, 0, true, true }
    }},
    { "MLS", {
        { CLASS_MAGE, 0, false, false },
        { CLASS_WARLOCK, 0, false, false },
        { CLASS_SHAMAN, 0, false, false }
    }},
    { "Jungle Cleave", {
        { CLASS_DRUID, TALENT_TREE_DRUID_FERAL_COMBAT, false, false },
        { CLASS_HUNTER, 0, false, false },
        { 0, 0, true, false }
    }},
    { "God Comp", {
        { CLASS_ROGUE, 0, false, false },
        { CLASS_WARRIOR, 0, false, false },
        { CLASS_PALADIN, TALENT_TREE_PALADIN_HOLY, true, false }
    }},
    { "Thunder Cleave", {
        { CLASS_SHAMAN, TALENT_TREE_SHAMAN_ENHANCEMENT, false, false },
        { CLASS_HUNTER, 0, false, false },
        { 0, 0, true, false }
    }},
    { "TSG", {
        { CLASS_WARRIOR, 0, false, false },
        { CLASS_DEATH_KNIGHT, 0, false, false },
        { CLASS_PALADIN, TALENT_TREE_PALADIN_HOLY, true, false }
    }},
    { "WLS", {
        { CLASS_WARRIOR, 0, false, false },
        { CLASS_WARLOCK, 0, false, false },
        { CLASS_SHAMAN, 0, false, false }
    }},
    { "RLS", {
        { CLASS_ROGUE, 0, false, false },
        { CLASS_WARLOCK, 0, false, false },
        { CLASS_SHAMAN, 0, false, false }
    }},
    { "RPS", {
        { CLASS_ROGUE, 0, false, false },
        { CLASS_PRIEST, 0, true, true },
        { CLASS_SHAMAN, 0, false, false }
    }},
    { "PHD", {
        { CLASS_PALADIN, TALENT_TREE_PALADIN_HOLY, true, false },
        { CLASS_HUNTER, 0, false, false },
        { CLASS_DRUID, 0, false, false }
    }},
    { "KFC", {
        { CLASS_HUNTER, 0, false, false },
        { CLASS_MAGE, 0, false, false },
        { 0, 0, true, false }
    }},
    { "LSD", {
        { CLASS_WARLOCK, 0, false, false },
        { CLASS_SHAMAN, 0, false, false },
        { CLASS_DRUID, 0, false, false }
    }},
    { "WRP", {
        { CLASS_WARRIOR, 0, false, false },
        { CLASS_ROGUE, 0, false, false },
        { CLASS_PRIEST, 0, true, true }
    }},
    { "Rogue Mage Paladin", {
        { CLASS_ROGUE, 0, false, false },
        { CLASS_MAGE, 0, false, false },
        { CLASS_PALADIN, TALENT_TREE_PALADIN_HOLY, true, false }
    }},
    { "Beastcleave", {
        { CLASS_HUNTER, 0, false, false },
        { CLASS_DEATH_KNIGHT, 0, false, false },
        { 0, 0, true, false }
    }},
    { "Feral Rogue Priest", {
        { CLASS_DRUID, TALENT_TREE_DRUID_FERAL_COMBAT, false, false },
        { CLASS_ROGUE, 0, false, false },
        { CLASS_PRIEST, 0, true, true }
    }},
    { "Ret Mage Priest", {
        { CLASS_PALADIN, TALENT_TREE_PALADIN_RETRIBUTION, false, false },
        { CLASS_MAGE, 0, false, false },
        { CLASS_PRIEST, 0, true, true }
    }}
};
}

bool ArenaSoloMgr::BuildOfficialComps(std::vector<ArenaSoloQueueEntry> const& pool,
    std::vector<ArenaSoloQueueEntry>& alliance, std::vector<ArenaSoloQueueEntry>& horde,
    std::string& allianceComp, std::string& hordeComp) const
{
    if (pool.size() < 6)
        return false;

    ObjectGuid anchor = pool.front().LeaderGuid;

    for (OfficialComp const& first : OfficialComps)
    {
        std::vector<bool> used(pool.size(), false);
        std::vector<ArenaSoloQueueEntry> teamA;
        if (!TakeComp(pool, first, used, teamA))
            continue;

        for (OfficialComp const& second : OfficialComps)
        {
            std::vector<bool> usedB = used;
            std::vector<ArenaSoloQueueEntry> teamB;
            if (!TakeComp(pool, second, usedB, teamB))
                continue;

            bool hasAnchor = false;
            for (ArenaSoloQueueEntry const& entry : teamA)
                if (entry.LeaderGuid == anchor)
                    hasAnchor = true;
            for (ArenaSoloQueueEntry const& entry : teamB)
                if (entry.LeaderGuid == anchor)
                    hasAnchor = true;
            if (!hasAnchor)
                continue;

            uint32 mmrA = AverageMMR(teamA);
            uint32 mmrB = AverageMMR(teamB);
            if (mmrA <= mmrB)
            {
                alliance = std::move(teamA);
                horde = std::move(teamB);
                allianceComp = first.Name;
                hordeComp = second.Name;
            }
            else
            {
                alliance = std::move(teamB);
                horde = std::move(teamA);
                allianceComp = second.Name;
                hordeComp = first.Name;
            }
            return true;
        }
    }

    return false;
}

bool ArenaSoloMgr::BuildTeams(uint8 bracket, std::vector<ArenaSoloQueueEntry>& alliance,
    std::vector<ArenaSoloQueueEntry>& horde, std::string& allianceComp, std::string& hordeComp)
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];
    std::list<ArenaSoloQueueEntry>& queue = _queues[bracket];

    // A premade bracket fills a whole side with one entry; a solo bracket needs
    // TeamSize entries per side.
    uint32 entriesPerSide = config.TeamSize / config.GroupSize;
    uint32 neededEntries = entriesPerSide * 2;

    if (!entriesPerSide || queue.size() < neededEntries)
        return false;

    uint32 now = GameTime::GetGameTimeMS().count();
    ArenaSoloQueueEntry const& anchor = queue.front();
    uint32 anchorWait = now > anchor.JoinTime ? now - anchor.JoinTime : 0;
    uint32 anchorWindow = PvPRating::MatchmakingWindow(config.MaxRatingDiff, anchorWait, config.RatingDiscardTimer);

    // Everyone whose MMR is close enough to the longest-waiting entry.
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

    if (pool.size() < neededEntries)
        return false;

    if (config.PreferComps && BuildOfficialComps(pool, alliance, horde, allianceComp, hordeComp))
        return true;

    allianceComp.clear();
    hordeComp.clear();

    auto assign = [&](ArenaSoloQueueEntry const& entry)
    {
        bool allianceFull = alliance.size() >= entriesPerSide;
        bool hordeFull = horde.size() >= entriesPerSide;
        if (allianceFull && hordeFull)
            return;

        // Send the entry to the side that is behind, so both end up with a
        // comparable combined MMR instead of all the top players on one side.
        bool toAlliance = hordeFull || (!allianceFull && TotalMMR(alliance) <= TotalMMR(horde));
        if (toAlliance)
            alliance.push_back(entry);
        else
            horde.push_back(entry);
    };

    auto sidesComplete = [&]()
    {
        return SidePlayerCount(alliance) == config.TeamSize && SidePlayerCount(horde) == config.TeamSize;
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

        if (alliancePool.size() < entriesPerSide || hordePool.size() < entriesPerSide)
            return false;

        auto takeSide = [&](std::vector<ArenaSoloQueueEntry>& source, std::vector<ArenaSoloQueueEntry>& side)
        {
            if (config.RequireRoleBalance)
            {
                auto healer = std::find_if(source.begin(), source.end(),
                    [](ArenaSoloQueueEntry const& entry) { return entry.Healers > 0; });
                if (healer == source.end())
                    return false;

                side.push_back(*healer);
                source.erase(healer);
            }

            for (ArenaSoloQueueEntry const& entry : source)
            {
                if (side.size() >= entriesPerSide)
                    break;

                side.push_back(entry);
            }

            return side.size() == entriesPerSide;
        };

        if (!takeSide(alliancePool, alliance) || !takeSide(hordePool, horde) || !sidesComplete())
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
            if (entry.Healers > 0)
                healers.push_back(entry);
            else
                damage.push_back(entry);
        }

        if (healers.size() < 2 || damage.size() < neededEntries - 2)
            return false;

        alliance.push_back(healers[0]);
        horde.push_back(healers[1]);

        for (ArenaSoloQueueEntry const& entry : damage)
        {
            if (alliance.size() + horde.size() >= neededEntries)
                break;

            assign(entry);
        }
    }
    else
    {
        for (ArenaSoloQueueEntry const& entry : pool)
        {
            if (alliance.size() + horde.size() >= neededEntries)
                break;

            assign(entry);
        }
    }

    if (!sidesComplete())
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
    std::string allianceComp;
    std::string hordeComp;
    if (!BuildTeams(bracket, alliance, horde, allianceComp, hordeComp))
        return;

    // Only drop the picked entries from the queue once the arena really exists.
    if (!StartMatch(bracket, alliance, horde, allianceComp, hordeComp))
        return;

    for (ArenaSoloQueueEntry const& entry : alliance)
        RemovePlayer(entry.LeaderGuid);

    for (ArenaSoloQueueEntry const& entry : horde)
        RemovePlayer(entry.LeaderGuid);
}

bool ArenaSoloMgr::StartMatch(uint8 bracket, std::vector<ArenaSoloQueueEntry> const& alliance,
    std::vector<ArenaSoloQueueEntry> const& horde, std::string const& allianceComp,
    std::string const& hordeComp)
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];

    Battleground* arenaTemplate = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
    if (!arenaTemplate)
    {
        LOG_ERROR("module.arenasolo", "All Arenas template not found; cannot start solo match.");
        return false;
    }

    Player* first = ObjectAccessor::FindConnectedPlayer(alliance.front().LeaderGuid);
    if (!first)
        return false;

    PvPDifficultyEntry const* bracketEntry =
        GetBattlegroundBracketByLevel(arenaTemplate->GetMapId(), first->GetLevel());
    if (!bracketEntry)
        return false;

    auto leaderGroupReady = [](ArenaSoloQueueEntry const& entry) -> bool
    {
        Player* leader = ObjectAccessor::FindConnectedPlayer(entry.LeaderGuid);
        return leader && leader->GetGroup();
    };

    if (config.UseCoreArenaTeam && config.GroupSize > 1)
    {
        for (ArenaSoloQueueEntry const& entry : alliance)
            if (!leaderGroupReady(entry))
                return false;
        for (ArenaSoloQueueEntry const& entry : horde)
            if (!leaderGroupReady(entry))
                return false;
    }

    // Always skirmish: each player has a personal arena team and the core
    // only writes a single team id per side.
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

    auto inviteSide = [&](std::vector<ArenaSoloQueueEntry> const& side, TeamId teamId)
    {
        for (ArenaSoloQueueEntry const& entry : side)
        {
            if (config.UseCoreArenaTeam && config.GroupSize > 1)
            {
                Player* leader = ObjectAccessor::FindConnectedPlayer(entry.LeaderGuid);
                if (!leader || !leader->GetGroup())
                    continue;

                GroupQueueInfo* ginfo = queue.AddGroup(leader, leader->GetGroup(), BATTLEGROUND_AA,
                    bracketEntry, config.ArenaType, false, true, entry.Rating, entry.MMR);

                for (ObjectGuid const& guid : entry.Members)
                {
                    Player* player = ObjectAccessor::FindConnectedPlayer(guid);
                    if (!player)
                        continue;

                    uint32 slot = player->AddBattlegroundQueueId(queueTypeId);
                    WorldPacket data;
                    sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, arenaTemplate, slot,
                        STATUS_WAIT_QUEUE, 0, 0, config.ArenaType, TEAM_NEUTRAL);
                    player->SendDirectMessage(&data);
                    ChatHandler(player->GetSession()).PSendSysMessage(
                        "{} match found! Accept the arena invite.", GetBracketName(bracket));
                }

                queue.InviteGroupToBG(ginfo, bg, teamId);
                continue;
            }

            // Solo brackets: each player is their own one-man group. The
            // battleground builds the team's raid after invites.
            for (ObjectGuid const& guid : entry.Members)
            {
                Player* player = ObjectAccessor::FindConnectedPlayer(guid);
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
    match.AllianceComp = allianceComp;
    match.HordeComp = hordeComp;
    for (ArenaSoloQueueEntry const& entry : alliance)
        match.Alliance.insert(match.Alliance.end(), entry.Members.begin(), entry.Members.end());
    for (ArenaSoloQueueEntry const& entry : horde)
        match.Horde.insert(match.Horde.end(), entry.Members.begin(), entry.Members.end());

    _matches[match.InstanceId] = match;

    if (!allianceComp.empty() || !hordeComp.empty())
    {
        auto announce = [&](std::vector<ObjectGuid> const& side, std::string const& own, std::string const& enemy)
        {
            for (ObjectGuid const& guid : side)
                if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
                    ChatHandler(player->GetSession()).PSendSysMessage(
                        "{} composition: {} vs {}.", GetBracketName(bracket),
                        own.empty() ? "pickup" : own, enemy.empty() ? "pickup" : enemy);
        };
        announce(match.Alliance, allianceComp, hordeComp);
        announce(match.Horde, hordeComp, allianceComp);
    }

    LOG_INFO("module.arenasolo", "Started {} arena instance {} on map {} (mmr {} vs {}{}{})",
        GetBracketName(bracket), match.InstanceId, bg->GetMapId(), match.AllianceMMR, match.HordeMMR,
        allianceComp.empty() ? "" : Acore::StringFormat(", {}", allianceComp),
        hordeComp.empty() ? "" : Acore::StringFormat(" vs {}", hordeComp));
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

void ArenaSoloMgr::ApplyArenaTeamSide(uint8 bracket, std::vector<ObjectGuid> const& side, uint32 opponentMMR,
    bool won, Map const* bgMap)
{
    ArenaSoloBracketConfig const& config = _brackets[bracket];
    std::unordered_map<uint32, ArenaTeam*> teams;
    std::unordered_map<uint32, uint32> teamMMR;
    std::unordered_map<uint32, uint32> teamMMRCount;

    for (ObjectGuid const& guid : side)
    {
        ArenaTeam* team = FindPersonalArenaTeam(guid, config.ArenaTeamType);
        if (!team)
            continue;

        teams[team->GetId()] = team;
        if (ArenaTeamMember* row = team->GetMember(guid))
        {
            teamMMR[team->GetId()] += row->MatchMakerRating;
            ++teamMMRCount[team->GetId()];
        }
    }

    std::unordered_map<uint32, int32> mmrChangeByTeam;
    for (auto const& [teamId, team] : teams)
    {
        uint32 ownMMR = teamMMRCount[teamId] ? (teamMMR[teamId] / teamMMRCount[teamId]) : team->GetRating();
        int32 ratingChange = 0;
        mmrChangeByTeam[teamId] = won
            ? team->WonAgainst(ownMMR, opponentMMR, ratingChange, bgMap)
            : team->LostAgainst(ownMMR, opponentMMR, ratingChange, bgMap);
    }

    for (ObjectGuid const& guid : side)
    {
        ArenaTeam* team = FindPersonalArenaTeam(guid, config.ArenaTeamType);
        if (!team)
            continue;

        int32 mmrChange = mmrChangeByTeam[team->GetId()];
        uint32 oldRating = 0;
        if (ArenaTeamMember* row = team->GetMember(guid))
            oldRating = row->PersonalRating;

        Player* player = ObjectAccessor::FindConnectedPlayer(guid);
        if (player)
        {
            if (won)
                team->MemberWon(player, opponentMMR, mmrChange);
            else
                team->MemberLost(player, opponentMMR, mmrChange);

            player->ModifyHonorPoints(won ? static_cast<int32>(config.HonorWin) : static_cast<int32>(config.HonorLoss));

            uint32 newRating = oldRating;
            if (ArenaTeamMember* row = team->GetMember(guid))
                newRating = row->PersonalRating;

            int32 ratingDelta = static_cast<int32>(newRating) - static_cast<int32>(oldRating);
            ChatHandler(player->GetSession()).PSendSysMessage(
                "{} {}: {} personal rating ({} -> {}), team {}. {} honor.",
                GetBracketName(bracket), won ? "victory" : "defeat",
                ratingDelta >= 0 ? Acore::StringFormat("+{}", ratingDelta) : std::to_string(ratingDelta),
                oldRating, newRating, team->GetName(),
                won ? config.HonorWin : config.HonorLoss);
        }
        else if (ArenaTeamMember* row = team->GetMember(guid))
        {
            int32 mod = team->GetRatingMod(row->PersonalRating, opponentMMR, won);
            row->ModifyPersonalRating(nullptr, mod, team->GetType());
            row->ModifyMatchmakerRating(mmrChange, team->GetSlot());
            row->WeekGames += 1;
            row->SeasonGames += 1;
            if (won)
            {
                row->SeasonWins += 1;
                row->WeekWins += 1;
            }
        }
    }

    for (auto const& kv : teams)
    {
        kv.second->SaveToDB(true);
        kv.second->NotifyStatsChanged();
    }
}

void ArenaSoloMgr::HandleBattlegroundEnd(Battleground* bg, TeamId winner)
{
    if (!bg)
        return;

    auto itr = _matches.find(bg->GetInstanceID());
    if (itr == _matches.end())
        return;

    ArenaSoloMatch match = itr->second;
    if (UsesCoreArenaTeam(match.Bracket))
    {
        if (winner == TEAM_NEUTRAL)
        {
            LOG_INFO("module.arenasolo", "{} instance {} ended in a draw; no rating change.",
                GetBracketName(match.Bracket), match.InstanceId);
            return;
        }

        Map const* bgMap = bg->FindBgMap();
        ApplyArenaTeamSide(match.Bracket, match.Alliance, match.HordeMMR, winner == TEAM_ALLIANCE, bgMap);
        ApplyArenaTeamSide(match.Bracket, match.Horde, match.AllianceMMR, winner == TEAM_HORDE, bgMap);
        LOG_INFO("module.arenasolo", "{} instance {} ended; personal arena_team_member rows updated.",
            GetBracketName(match.Bracket), match.InstanceId);
        return;
    }

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
    for (auto& kv : _statsCache)
    {
        kv.second.WeekGames = 0;
        kv.second.WeekWins = 0;
        kv.second.WeekPoints = 0;
    }

    LOG_INFO("module.arenasolo", "Arena solo queue weekly stats reset.");
}

std::vector<PvPLeaderboardRow> ArenaSoloMgr::GetLeaderboard(uint8 bracket, uint32 limit)
{
    std::vector<PvPLeaderboardRow> rows;
    if (bracket >= ARENA_SOLO_BRACKET_MAX)
        return rows;

    QueryResult result;
    if (UsesCoreArenaTeam(bracket))
        result = CharacterDatabase.Query(
            "SELECT c.name, atm.personalRating, atm.seasonWins, (atm.seasonGames - atm.seasonWins) "
            "FROM arena_team_member atm "
            "INNER JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId AND at.type = {} "
            "INNER JOIN characters c ON c.guid = atm.guid "
            "ORDER BY atm.personalRating DESC, atm.seasonWins DESC LIMIT {}",
            uint32(_brackets[bracket].ArenaTeamType), limit);
    else
        result = CharacterDatabase.Query(
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

bool ArenaSoloMgr::IsInMatch(ObjectGuid guid) const
{
    for (auto const& kv : _matches)
    {
        for (ObjectGuid const& member : kv.second.Alliance)
            if (member == guid)
                return true;
        for (ObjectGuid const& member : kv.second.Horde)
            if (member == guid)
                return true;
    }

    return false;
}

void ArenaSoloMgr::HandleDesertion(Player* player, uint8 desertionType)
{
    if (!player || !IsInMatch(player->GetGUID()))
        return;

    switch (desertionType)
    {
        case ARENA_DESERTION_TYPE_LEAVE_BG:
        case ARENA_DESERTION_TYPE_LEAVE_QUEUE:
        case ARENA_DESERTION_TYPE_NO_ENTER_BUTTON:
        case ARENA_DESERTION_TYPE_INVITE_LOGOUT:
            break;
        default:
            return;
    }

    if (!player->HasAura(SPELL_DESERTER))
        player->CastSpell(player, SPELL_DESERTER, true);

    ChatHandler(player->GetSession()).SendSysMessage(
        "You received Deserter for leaving or missing the arena.");
    LOG_INFO("module.arenasolo", "{} ({}) received Deserter (type {})",
        player->GetName(), player->GetGUID().ToString(), desertionType);
}
