-- Fallback AIO server handlers. The same handlers also register from
-- MythicPlus_Client.lua inside AIO.AddAddon() so /mplus works even when
-- this file is missing from the Eluna path. If both load, keep a full set
-- here so the last AddHandlers("MPLUS") still has RequestOpen/Start/etc.
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

local ROTATION = {
    { 1, 3, 4 }, { 2, 5, 10 }, { 1, 6, 16 }, { 2, 4, 11 },
    { 1, 15, 12 }, { 2, 14, 8 }, { 1, 6, 13 }, { 2, 3, 11 },
    { 1, 4, 10 }, { 2, 5, 8 }, { 1, 14, 16 }, { 2, 15, 13 }
}
local SEASONAL = { 17, 18, 19, 20 }

local function WeekAffixes(week, season)
    local idx = ((((tonumber(week) or 1) - 1) % 12) + 1)
    local row = ROTATION[idx] or ROTATION[1]
    local seas = SEASONAL[((((tonumber(season) or 1) - 1) % 4) + 1)]
    return { row[1], row[2], row[3], seas }
end

local function ToGuidLow(value)
    if type(value) == "number" then
        return value > 0 and value or nil
    end
    if type(value) == "string" then
        local n = tonumber(value) or tonumber(value:match("(%d+)$"))
        return n and n > 0 and n or nil
    end
    if type(value) == "userdata" then
        local okc, counter = pcall(function()
            return value.GetCounter and value:GetCounter()
        end)
        if okc then
            return ToGuidLow(counter)
        end
    end
    return nil
end

local function GuidLow(player)
    if not player then
        return nil
    end
    local tries = {
        function()
            return player:GetGUIDLow()
        end,
        function()
            return player:GetGUID()
        end,
        function()
            return player:GetGUID():GetCounter()
        end
    }
    for i = 1, #tries do
        local okv, value = pcall(tries[i])
        if okv then
            local low = ToGuidLow(value)
            if low then
                return low
            end
        end
    end
    return nil
end

local function Say(player, en, es)
    if not player or not player.SendBroadcastMessage then
        return
    end
    local loc = ""
    pcall(function()
        loc = player:GetLocale() or ""
    end)
    player:SendBroadcastMessage("|cffff8800Mythic+|r " .. ((loc == "esES" or loc == "esMX") and es or en))
end

local function SafeQuery(sql)
    if not CharDBQuery then
        return nil
    end
    local okq, result = pcall(CharDBQuery, sql)
    if okq then
        return result
    end
    return nil
end

local function EmptyProfile()
    return {
        season = 1, week = 1,
        dungeonId = 0, level = 0, depleted = 0, score = 0,
        weekBest = 0, weekDungeon = 0, weekRuns = 0, vault = 0,
        weekKey1 = 0, weekKey2 = 0, weekKey3 = 0,
        vault1 = 0, vault2 = 0, vault3 = 0
    }
end

local function LoadState()
    local q = SafeQuery("SELECT week_index, season_id FROM mythic_state WHERE id = 1")
    if not q then
        return { week = 1, season = 1 }
    end
    return { week = (q:GetUInt32(0) or 0) + 1, season = q:GetUInt32(1) or 1 }
end

local function LoadProfile(player)
    local data = EmptyProfile()
    local st = LoadState()
    data.week = st.week
    data.season = st.season
    local guid = GuidLow(player)
    if not guid then
        return data
    end
    local q = SafeQuery(string.format(
        "SELECT dungeon_id, key_level, depleted, overall_score, week_best_level, "
            .. "week_best_dungeon, week_runs, vault_claimed, week_key1, week_key2, week_key3, "
            .. "vault_item1, vault_item2, vault_item3 "
            .. "FROM character_mythic_profile WHERE guid = %d", guid))
    if not q then
        q = SafeQuery(string.format(
            "SELECT dungeon_id, key_level, depleted, overall_score, week_best_level, "
                .. "week_best_dungeon, week_runs, vault_claimed, week_key1, week_key2, week_key3 "
                .. "FROM character_mythic_profile WHERE guid = %d", guid))
    end
    if not q then
        return data
    end
    data.dungeonId = q:GetUInt32(0) or 0
    data.level = q:GetUInt32(1) or 0
    data.depleted = q:GetUInt32(2) or 0
    data.score = q:GetFloat(3) or 0
    data.weekBest = q:GetUInt32(4) or 0
    data.weekDungeon = q:GetUInt32(5) or 0
    data.weekRuns = q:GetUInt32(6) or 0
    data.vault = q:GetUInt32(7) or 0
    data.weekKey1 = q:GetUInt32(8) or 0
    data.weekKey2 = q:GetUInt32(9) or 0
    data.weekKey3 = q:GetUInt32(10) or 0
    pcall(function()
        data.vault1 = q:GetUInt32(11) or 0
        data.vault2 = q:GetUInt32(12) or 0
        data.vault3 = q:GetUInt32(13) or 0
    end)
    return data
end

local function SendUI(player)
    if not player then
        return
    end
    AIO.Handle(player, "MPLUS", "ShowUI", LoadProfile(player))
end

local function PushRequest(player, action, extra)
    local guid = GuidLow(player)
    if not guid then
        Say(player, "Could not read your character id. Try .mplus instead.",
            "No pude leer tu personaje. Usa .mplus.")
        return false
    end
    if not CharDBExecute then
        return false
    end
    CharDBExecute(string.format(
        "REPLACE INTO mythic_request (guid, action, extra, created_at) "
            .. "VALUES (%d, %d, %d, UNIX_TIMESTAMP())",
        guid, action, extra or 0))
    return true
end

function Handlers.RequestOpen(player)
    local okp, err = pcall(SendUI, player)
    if not okp then
        Say(player, "UI sync failed. Use .mplus status.",
            "Fallo al sincronizar. Usa .mplus status.")
        print("[Mythic+] RequestOpen: " .. tostring(err))
    end
end

function Handlers.RequestWeek(player)
    local st = LoadState()
    local ids = WeekAffixes(st.week, st.season)
    AIO.Handle(player, "MPLUS", "ShowWeek", {
        week = st.week, season = st.season,
        a0 = ids[1], a1 = ids[2], a2 = ids[3], a3 = ids[4]
    })
end

function Handlers.RequestBoard(player)
    local rows = {}
    local q = SafeQuery(
        "SELECT c.name, p.overall_score, p.week_best_level "
            .. "FROM character_mythic_profile p INNER JOIN characters c ON c.guid = p.guid "
            .. "WHERE p.overall_score > 0 ORDER BY p.overall_score DESC LIMIT 15")
    if q then
        repeat
            rows[#rows + 1] = string.format("%s|%.1f|%d",
                q:GetString(0) or "?", q:GetFloat(1) or 0, q:GetUInt32(2) or 0)
        until not q:NextRow()
    end
    AIO.Handle(player, "MPLUS", "ShowBoard", rows)
end

local function LoadLive(player)
    local guid = GuidLow(player)
    if not guid then
        return nil
    end
    local q = SafeQuery(string.format(
        "SELECT l.dungeon_id, l.level, l.affix0, l.affix1, l.affix2, l.affix3, "
            .. "l.elapsed_ms, l.limit_ms, l.deaths, l.forces, l.forces_req, l.bosses, l.bosses_req, l.active "
            .. "FROM mythic_run_member m INNER JOIN mythic_run_live l ON l.instance_id = m.instance_id "
            .. "WHERE m.guid = %d", guid))
    if not q then
        return nil
    end
    return {
        dungeonId = q:GetUInt32(0), level = q:GetUInt32(1),
        affix0 = q:GetUInt32(2), affix1 = q:GetUInt32(3),
        affix2 = q:GetUInt32(4), affix3 = q:GetUInt32(5),
        elapsed = q:GetUInt32(6), limit = q:GetUInt32(7),
        deaths = q:GetUInt32(8), forces = q:GetUInt32(9),
        forcesReq = q:GetUInt32(10), bosses = q:GetUInt32(11),
        bossesReq = q:GetUInt32(12), active = q:GetUInt32(13)
    }
end

function Handlers.RequestHud(player)
    local live = LoadLive(player)
    if not live or (live.active or 0) == 0 then
        AIO.Handle(player, "MPLUS", "HideHud")
        return
    end
    AIO.Handle(player, "MPLUS", "ShowHud", live)
end

function Handlers.Start(player)
    if PushRequest(player, ACTION_START, 0) then
        Say(player, "Inserting keystone...", "Insertando piedra...")
    end
    pcall(SendUI, player)
end

function Handlers.Teleport(player)
    if PushRequest(player, ACTION_TELEPORT, 0) then
        Say(player, "Teleport requested...", "Teletransporte pedido...")
    end
end

function Handlers.ClaimKey(player)
    if PushRequest(player, ACTION_CLAIM, 0) then
        Say(player, "Claiming a +2 keystone...", "Reclamando piedra +2...")
    end
    pcall(SendUI, player)
end

function Handlers.ClaimVault(player, slot)
    slot = tonumber(slot) or 1
    if slot < 1 or slot > 3 then
        return
    end
    if PushRequest(player, ACTION_VAULT, slot) then
        Say(player, "Claiming vault choice " .. slot .. "...",
            "Reclamando opcion " .. slot .. " del cofre...")
    end
    pcall(SendUI, player)
end

print("[Mythic+] AIO server handlers loaded from MythicPlus_Server.lua")
