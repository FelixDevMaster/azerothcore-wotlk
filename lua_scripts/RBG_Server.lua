-- Rated Battlegrounds AIO server. Queue/leave is written to rbg_request.
local ok, aio = pcall(function()
    return AIO or require("AIO")
end)
if not ok or not aio then
    print("[RBG] AIO not found. UI disabled; use .rbg or the battlemaster NPC.")
    return
end
local AIO = aio

local Handlers = AIO.AddHandlers("RBG", {})

local ACTION_QUEUE = 1
local ACTION_LEAVE = 2

local function GuidLow(player)
    return player:GetGUIDLow()
end

local function Request(player, action)
    CharDBExecute(string.format(
        "REPLACE INTO rbg_request (guid, action, created_at) VALUES (%d, %d, UNIX_TIMESTAMP())",
        GuidLow(player), action))
end

local function LoadStats(player)
    local guid = GuidLow(player)
    local q = CharDBQuery(string.format(
        "SELECT rating, mmr, games, wins, week_games, week_wins, week_conquest, highest_rating "
            .. "FROM rbg_stats WHERE guid = %d", guid))
    if not q then
        return 1500, 1500, 0, 0, 0, 0, 0, 1500
    end
    return q:GetUInt32(0), q:GetUInt32(1), q:GetUInt32(2), q:GetUInt32(3),
        q:GetUInt32(4), q:GetUInt32(5), q:GetUInt32(6), q:GetUInt32(7)
end

local function LoadLeaderboard()
    local rows = {}
    local q = CharDBQuery(
        "SELECT c.name, s.rating, s.wins, (s.games - s.wins) FROM rbg_stats s "
            .. "INNER JOIN characters c ON c.guid = s.guid "
            .. "ORDER BY s.rating DESC, s.wins DESC LIMIT 15")
    if not q then
        return rows
    end
    repeat
        rows[#rows + 1] = {
            name = q:GetString(0),
            rating = q:GetUInt32(1),
            wins = q:GetUInt32(2),
            losses = q:GetUInt32(3)
        }
    until not q:NextRow()
    return rows
end

function Handlers.RequestOpen(player)
    local rating, mmr, games, wins, weekGames, weekWins, weekConquest, highest = LoadStats(player)
    AIO.Handle(player, "RBG", "ShowUI",
        rating, mmr, games, wins, weekGames, weekWins, weekConquest, highest, 1650, 0)
end

function Handlers.Queue(player)
    local group = player:GetGroup()
    if not group or group:GetLeaderGUID() ~= player:GetGUID() then
        player:SendBroadcastMessage("|cffff0000Rated BG:|r only the raid leader can queue.")
        return
    end
    Request(player, ACTION_QUEUE)
    player:SendBroadcastMessage("|cff00ff00Rated BG:|r searching for a match...")
    local rating, mmr, games, wins, weekGames, weekWins, weekConquest, highest = LoadStats(player)
    AIO.Handle(player, "RBG", "ShowUI",
        rating, mmr, games, wins, weekGames, weekWins, weekConquest, highest, 1650, 1)
end

function Handlers.Leave(player)
    Request(player, ACTION_LEAVE)
    player:SendBroadcastMessage("|cffff0000Rated BG:|r leave request sent.")
    local rating, mmr, games, wins, weekGames, weekWins, weekConquest, highest = LoadStats(player)
    AIO.Handle(player, "RBG", "ShowUI",
        rating, mmr, games, wins, weekGames, weekWins, weekConquest, highest, 1650, 0)
end

function Handlers.RequestBoard(player)
    AIO.Handle(player, "RBG", "ShowBoard", LoadLeaderboard())
end

print("[RBG] AIO server handlers loaded.")
