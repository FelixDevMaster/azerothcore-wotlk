-- Mythic+ BFA, AIO server side.
-- The UI never runs server commands: it writes mythic_request and the C++
-- module consumes the row on the next tick. Live HUD reads mythic_run_live.
local ok, aio = pcall(function()
    return AIO or require("AIO")
end)
if not ok or not aio then
    print("[Mythic+] AIO not found. UI disabled; use .mplus or the Keystone Broker.")
    return
end
local AIO = aio

local Handlers = AIO.AddHandlers("MPLUS", {})

local ACTION_START = 1
local ACTION_VAULT = 2
local ACTION_TELEPORT = 3
local ACTION_CLAIM = 4

local function GuidLow(player)
    return player:GetGUIDLow()
end

local function LoadProfile(player)
    local q = CharDBQuery(string.format(
        "SELECT dungeon_id, key_level, depleted, overall_score, week_best_level, "
            .. "week_best_dungeon, week_runs, vault_claimed, week_key1, week_key2, week_key3, season_id, "
            .. "vault_item1, vault_item2, vault_item3 "
            .. "FROM character_mythic_profile WHERE guid = %d", GuidLow(player)))
    if not q then
        return {
            dungeonId = 0, level = 0, depleted = 0, score = 0,
            weekBest = 0, weekDungeon = 0, weekRuns = 0, vault = 0,
            weekKeys = { 0, 0, 0 }, vaultItems = { 0, 0, 0 }, seasonId = 1
        }
    end
    return {
        dungeonId = q:GetUInt32(0),
        level = q:GetUInt32(1),
        depleted = q:GetUInt32(2),
        score = q:GetFloat(3),
        weekBest = q:GetUInt32(4),
        weekDungeon = q:GetUInt32(5),
        weekRuns = q:GetUInt32(6),
        vault = q:GetUInt32(7),
        weekKeys = { q:GetUInt32(8), q:GetUInt32(9), q:GetUInt32(10) },
        seasonId = q:GetUInt32(11),
        vaultItems = { q:GetUInt32(12), q:GetUInt32(13), q:GetUInt32(14) }
    }
end

local function LoadState()
    local q = CharDBQuery("SELECT week_index, season_id FROM mythic_state WHERE id = 1")
    if not q then
        return { week = 1, season = 1 }
    end
    return { week = q:GetUInt32(0) + 1, season = q:GetUInt32(1) }
end

local function LoadLive(player)
    local q = CharDBQuery(string.format(
        "SELECT l.dungeon_id, l.level, l.affix0, l.affix1, l.affix2, l.affix3, "
            .. "l.elapsed_ms, l.limit_ms, l.deaths, l.forces, l.forces_req, l.bosses, l.bosses_req, l.active "
            .. "FROM mythic_run_member m INNER JOIN mythic_run_live l ON l.instance_id = m.instance_id "
            .. "WHERE m.guid = %d", GuidLow(player)))
    if not q then
        return nil
    end
    return {
        dungeonId = q:GetUInt32(0),
        level = q:GetUInt32(1),
        affixes = { q:GetUInt32(2), q:GetUInt32(3), q:GetUInt32(4), q:GetUInt32(5) },
        elapsed = q:GetUInt32(6),
        limit = q:GetUInt32(7),
        deaths = q:GetUInt32(8),
        forces = q:GetUInt32(9),
        forcesReq = q:GetUInt32(10),
        bosses = q:GetUInt32(11),
        bossesReq = q:GetUInt32(12),
        active = q:GetUInt32(13)
    }
end

local function PushRequest(player, action, extra)
    CharDBExecute(string.format(
        "REPLACE INTO mythic_request (guid, action, extra, created_at) "
            .. "VALUES (%d, %d, %d, UNIX_TIMESTAMP())",
        GuidLow(player), action, extra or 0))
end

function Handlers.RequestOpen(player)
    AIO.Handle(player, "MPLUS", "ShowUI", {
        season = LoadState().season,
        week = LoadState().week,
        profile = LoadProfile(player)
    })
end

function Handlers.RequestWeek(player)
    local st = LoadState()
    local rotation = {
        { 1, 3, 4 }, { 2, 5, 10 }, { 1, 6, 16 }, { 2, 4, 11 },
        { 1, 15, 12 }, { 2, 14, 8 }, { 1, 6, 13 }, { 2, 3, 11 },
        { 1, 4, 10 }, { 2, 5, 8 }, { 1, 14, 16 }, { 2, 15, 13 }
    }
    local seasonal = { 17, 18, 19, 20 }
    local idx = ((st.week - 1) % 12) + 1
    local row = rotation[idx]
    AIO.Handle(player, "MPLUS", "ShowWeek", {
        week = st.week,
        season = st.season,
        ids = { row[1], row[2], row[3], seasonal[((st.season - 1) % 4) + 1] }
    })
end

function Handlers.RequestBoard(player)
    local rows = {}
    local q = CharDBQuery(
        "SELECT c.name, p.overall_score, p.week_best_level, p.week_runs "
            .. "FROM character_mythic_profile p INNER JOIN characters c ON c.guid = p.guid "
            .. "WHERE p.overall_score > 0 ORDER BY p.overall_score DESC LIMIT 15")
    if q then
        repeat
            rows[#rows + 1] = {
                name = q:GetString(0),
                score = q:GetFloat(1),
                weekBest = q:GetUInt32(2),
                runs = q:GetUInt32(3)
            }
        until not q:NextRow()
    end
    AIO.Handle(player, "MPLUS", "ShowBoard", rows)
end

function Handlers.RequestHud(player)
    local live = LoadLive(player)
    if not live or live.active == 0 then
        AIO.Handle(player, "MPLUS", "HideHud")
        return
    end
    AIO.Handle(player, "MPLUS", "ShowHud", live)
end

local function PushHudToLiveMembers()
    local q = CharDBQuery(
        "SELECT m.guid FROM mythic_run_member m INNER JOIN mythic_run_live l "
            .. "ON l.instance_id = m.instance_id WHERE l.active = 1")
    if not q then
        return
    end
    local wanted = {}
    repeat
        wanted[q:GetUInt32(0)] = true
    until not q:NextRow()
    if not GetPlayersInWorld then
        return
    end
    for _, player in pairs(GetPlayersInWorld()) do
        if player and wanted[GuidLow(player)] then
            Handlers.RequestHud(player)
        end
    end
end

function Handlers.Start(player)
    PushRequest(player, ACTION_START, 0)
    if CreateLuaEvent then
        CreateLuaEvent(function()
            Handlers.RequestHud(player)
        end, 700, 4)
    end
end

function Handlers.Teleport(player)
    PushRequest(player, ACTION_TELEPORT, 0)
end

function Handlers.ClaimKey(player)
    PushRequest(player, ACTION_CLAIM, 0)
end

function Handlers.ClaimVault(player, slot)
    slot = tonumber(slot) or 1
    if slot < 1 or slot > 3 then
        return
    end
    PushRequest(player, ACTION_VAULT, slot)
    if CreateLuaEvent then
        CreateLuaEvent(function()
            Handlers.RequestOpen(player)
        end, 700, 2)
    end
end

if CreateLuaEvent then
    CreateLuaEvent(PushHudToLiveMembers, 1000, 0)
end

print("[Mythic+] AIO server handlers loaded.")
