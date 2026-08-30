-- Rated Battlegrounds + solo queue arenas, AIO server side.
-- The UI never runs server commands: it writes a request row and the C++
-- module consumes it on the next tick.
local ok, aio = pcall(function()
    return AIO or require("AIO")
end)
if not ok or not aio then
    print("[RBG] AIO not found. UI disabled; use .rbg / .solo or the NPCs.")
    return
end
local AIO = aio

local Handlers = AIO.AddHandlers("RBG", {})

local ACTION_QUEUE = 1
local ACTION_LEAVE = 2

-- Must match ArenaSoloBracket in the C++ module. Never renumber persisted ids.
local BRACKET_1V1 = 0
local BRACKET_3V3 = 1
local BRACKET_2V2 = 2
local BRACKET_3V3_TEAM = 3

local function GuidLow(player)
    return player:GetGUIDLow()
end

local function LoadRbgStats(player)
    local q = CharDBQuery(string.format(
        "SELECT rating, mmr, games, wins, week_games, week_wins, week_conquest, highest_rating "
            .. "FROM rbg_stats WHERE guid = %d", GuidLow(player)))
    if not q then
        return { rating = 1500, mmr = 1500, games = 0, wins = 0,
            weekGames = 0, weekWins = 0, weekPoints = 0, highest = 1500 }
    end
    return {
        rating = q:GetUInt32(0),
        mmr = q:GetUInt32(1),
        games = q:GetUInt32(2),
        wins = q:GetUInt32(3),
        weekGames = q:GetUInt32(4),
        weekWins = q:GetUInt32(5),
        weekPoints = q:GetUInt32(6),
        highest = q:GetUInt32(7)
    }
end

local function LoadArenaTeamMemberStats(player, teamType, slot)
    -- 2v2 uses type 2 / slot 0. Premade 3v3 uses type 3 / slot 1.
    -- 3v3 SoloQ uses type 5 / slot 2.
    local q = CharDBQuery(string.format(
        "SELECT atm.personalRating, IFNULL(cas.matchMakerRating, 0), atm.seasonGames, atm.seasonWins, "
            .. "atm.weekGames, atm.weekWins, at.rating, atm.personalRating, at.name "
            .. "FROM arena_team_member atm "
            .. "INNER JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId AND at.type = %d "
            .. "LEFT JOIN character_arena_stats cas ON cas.guid = atm.guid AND cas.slot = %d "
            .. "WHERE atm.guid = %d", teamType, slot, GuidLow(player)))
    if not q then
        return { rating = 0, mmr = 0, games = 0, wins = 0,
            weekGames = 0, weekWins = 0, weekPoints = 0, highest = 0,
            teamRating = 0, teamName = "" }
    end
    return {
        rating = q:GetUInt32(0),
        mmr = q:GetUInt32(1),
        games = q:GetUInt32(2),
        wins = q:GetUInt32(3),
        weekGames = q:GetUInt32(4),
        weekWins = q:GetUInt32(5),
        weekPoints = 0,
        highest = q:GetUInt32(7),
        teamRating = q:GetUInt32(6),
        teamName = q:GetString(8) or ""
    }
end

local function LoadSoloStats(player, bracket)
    if bracket == BRACKET_2V2 then
        return LoadArenaTeamMemberStats(player, 2, 0)
    end
    if bracket == BRACKET_3V3_TEAM then
        return LoadArenaTeamMemberStats(player, 3, 1)
    end
    if bracket == BRACKET_3V3 then
        return LoadArenaTeamMemberStats(player, 5, 2)
    end

    local q = CharDBQuery(string.format(
        "SELECT rating, mmr, games, wins, week_games, week_wins, week_points, highest_rating "
            .. "FROM arena_solo_stats WHERE guid = %d AND bracket = %d", GuidLow(player), bracket))
    if not q then
        return { rating = 1500, mmr = 1500, games = 0, wins = 0,
            weekGames = 0, weekWins = 0, weekPoints = 0, highest = 1500 }
    end
    return {
        rating = q:GetUInt32(0),
        mmr = q:GetUInt32(1),
        games = q:GetUInt32(2),
        wins = q:GetUInt32(3),
        weekGames = q:GetUInt32(4),
        weekWins = q:GetUInt32(5),
        weekPoints = q:GetUInt32(6),
        highest = q:GetUInt32(7)
    }
end

local function SendState(player, queuedTab)
    AIO.Handle(player, "RBG", "ShowUI", {
        rbg = LoadRbgStats(player),
        solo1v1 = LoadSoloStats(player, BRACKET_1V1),
        solo2v2 = LoadSoloStats(player, BRACKET_2V2),
        solo3v3 = LoadSoloStats(player, BRACKET_3V3),
        solo3v3Team = LoadSoloStats(player, BRACKET_3V3_TEAM),
        queued = queuedTab or 0
    })
end

function Handlers.RequestOpen(player)
    SendState(player, 0)
end

function Handlers.QueueRbg(player)
    local group = player:GetGroup()
    if not group or group:GetLeaderGUID() ~= player:GetGUID() then
        player:SendBroadcastMessage("|cffff0000Rated BG:|r only the raid leader can queue.")
        SendState(player, 0)
        return
    end

    CharDBExecute(string.format(
        "REPLACE INTO rbg_request (guid, action, created_at) VALUES (%d, %d, UNIX_TIMESTAMP())",
        GuidLow(player), ACTION_QUEUE))
    player:SendBroadcastMessage("|cff00ff00Rated BG:|r searching for a match...")
    SendState(player, 1)
end

function Handlers.LeaveRbg(player)
    CharDBExecute(string.format(
        "REPLACE INTO rbg_request (guid, action, created_at) VALUES (%d, %d, UNIX_TIMESTAMP())",
        GuidLow(player), ACTION_LEAVE))
    player:SendBroadcastMessage("|cffff0000Rated BG:|r leave request sent.")
    SendState(player, 0)
end

function Handlers.QueueSolo(player, bracket)
    local group = player:GetGroup()

    if bracket == BRACKET_2V2 or bracket == BRACKET_3V3_TEAM then
        -- Premade 2v2 / 3v3: party of 2 or 3. C++ creates a personal team per player.
        if not group or group:GetLeaderGUID() ~= player:GetGUID() then
            player:SendBroadcastMessage("|cffff0000Arena:|r only the party leader can queue.")
            SendState(player, 0)
            return
        end
    elseif bracket == BRACKET_1V1 or bracket == BRACKET_3V3 then
        if group then
            player:SendBroadcastMessage("|cffff0000Solo queue:|r leave your group first.")
            SendState(player, 0)
            return
        end
    else
        return
    end

    CharDBExecute(string.format(
        "REPLACE INTO arena_solo_request (guid, bracket, action, created_at) "
            .. "VALUES (%d, %d, %d, UNIX_TIMESTAMP())",
        GuidLow(player), bracket, ACTION_QUEUE))
    player:SendBroadcastMessage("|cff00ff00Arena:|r searching for a match...")

    -- Tabs: 1 RBG, 2 1v1, 3 2v2, 4 3v3, 5 SoloQ 3v3.
    local tab = 2
    if bracket == BRACKET_2V2 then
        tab = 3
    elseif bracket == BRACKET_3V3_TEAM then
        tab = 4
    elseif bracket == BRACKET_3V3 then
        tab = 5
    end

    SendState(player, tab)
end

function Handlers.LeaveSolo(player)
    CharDBExecute(string.format(
        "REPLACE INTO arena_solo_request (guid, bracket, action, created_at) "
            .. "VALUES (%d, 0, %d, UNIX_TIMESTAMP())",
        GuidLow(player), ACTION_LEAVE))
    player:SendBroadcastMessage("|cffff0000Solo queue:|r leave request sent.")
    SendState(player, 0)
end

local boardBracketByTab = {
    [2] = BRACKET_1V1,
    [3] = BRACKET_2V2,
    [4] = BRACKET_3V3_TEAM,
    [5] = BRACKET_3V3
}

local boardTeamTypeByTab = {
    [3] = 2,
    [4] = 3,
    [5] = 5
}

function Handlers.RequestBoard(player, tab)
    local rows = {}
    local query

    if tab == 1 then
        query = "SELECT c.name, s.rating, s.wins, (s.games - s.wins) FROM rbg_stats s "
            .. "INNER JOIN characters c ON c.guid = s.guid ORDER BY s.rating DESC, s.wins DESC LIMIT 15"
    elseif boardTeamTypeByTab[tab] then
        query = string.format(
            "SELECT c.name, atm.personalRating, atm.seasonWins, (atm.seasonGames - atm.seasonWins) "
                .. "FROM arena_team_member atm "
                .. "INNER JOIN arena_team at ON at.arenaTeamId = atm.arenaTeamId AND at.type = %d "
                .. "INNER JOIN characters c ON c.guid = atm.guid "
                .. "ORDER BY atm.personalRating DESC, atm.seasonWins DESC LIMIT 15", boardTeamTypeByTab[tab])
    else
        query = string.format(
            "SELECT c.name, s.rating, s.wins, (s.games - s.wins) FROM arena_solo_stats s "
                .. "INNER JOIN characters c ON c.guid = s.guid WHERE s.bracket = %d "
                .. "ORDER BY s.rating DESC, s.wins DESC LIMIT 15", boardBracketByTab[tab] or BRACKET_1V1)
    end

    local q = CharDBQuery(query)
    if q then
        repeat
            rows[#rows + 1] = {
                name = q:GetString(0),
                rating = q:GetUInt32(1),
                wins = q:GetUInt32(2),
                losses = q:GetUInt32(3)
            }
        until not q:NextRow()
    end

    AIO.Handle(player, "RBG", "ShowBoard", tab, rows)
end

print("[RBG] AIO server handlers loaded (RBG + solo queue).")
