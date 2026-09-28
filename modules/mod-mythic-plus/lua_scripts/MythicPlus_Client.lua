local ok, aio = pcall(function()
    return AIO or require("AIO")
end)
if not ok or not aio then
    return
end
local AIO = aio

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

-- Server branch: runs when Eluna loads this file. AIO then ships the same
-- file to the client. Putting handlers here means /mplus works even if
-- MythicPlus_Server.lua is not on the Eluna path.
if AIO.AddAddon() then
    local Handlers = AIO.AddHandlers("MPLUS", {})

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

    print("[Mythic+] AIO server handlers loaded from MythicPlus_Client.lua")
    return
end

local Handlers = AIO.AddHandlers("MPLUS", {})

local L = {
    TITLE = "MYTHIC KEYSTONE",
    SUB = "Battle for Azeroth",
    TAB_KEY = "KEYSTONE",
    TAB_WEEK = "AFFIXES",
    TAB_VAULT = "GREAT VAULT",
    TAB_BOARD = "RANKING",
    START = "Insert Keystone",
    TELEPORT = "Teleport",
    CLAIM = "Claim +2 Keystone",
    KEY = "Current Keystone",
    SCORE = "Mythic+ Rating",
    WEEK_BEST = "Best this week",
    RUNS = "Runs this week",
    DEPLETED = "DEPLETED",
    NONE = "No keystone",
    AFFIX_2 = "+2",
    AFFIX_4 = "+4",
    AFFIX_7 = "+7",
    AFFIX_10 = "+10 Seasonal",
    CLAIM_SLOT = "Choose",
    VAULT_HINT = "Hover an item for stats and effects. Choose one.",
    VAULT_NEED = "Complete a keystone this week to unlock the vault.",
    VAULT_DONE = "Vault claimed. Resets Friday 20:00.",
    EMPTY = "No scores yet.",
    HUD_FORCES = "Enemy Forces",
    HUD_BOSSES = "Bosses",
    HUD_DEATHS = "Deaths",
    HUD_OVERTIME = "OVERTIME",
    SEASON = "Season",
    WEEK = "Week",
    SYNCING = "Syncing with server...",
    OFFLINE = "No server reply. Buttons still send requests. Try /reload."
}

if GetLocale() == "esES" or GetLocale() == "esMX" then
    L.TITLE = "PIEDRA ANGULAR MITICA"
    L.TAB_KEY = "PIEDRA"
    L.TAB_WEEK = "AFFIJOS"
    L.TAB_VAULT = "GRAN CAMARA"
    L.TAB_BOARD = "RANKING"
    L.START = "Insertar piedra"
    L.TELEPORT = "Teletransporte"
    L.CLAIM = "Reclamar piedra +2"
    L.KEY = "Piedra actual"
    L.SCORE = "Puntuacion mitica"
    L.WEEK_BEST = "Mejor de la semana"
    L.RUNS = "Runs esta semana"
    L.DEPLETED = "AGOTADA"
    L.NONE = "Sin piedra"
    L.AFFIX_10 = "+10 Temporada"
    L.CLAIM_SLOT = "Elegir"
    L.VAULT_HINT = "Pasa el raton para ver stats y efectos. Elige una."
    L.VAULT_NEED = "Completa una mitica esta semana para abrir la camara."
    L.VAULT_DONE = "Camara reclamada. Reset viernes 20:00."
    L.EMPTY = "Todavia no hay puntuaciones."
    L.HUD_FORCES = "Fuerzas enemigas"
    L.HUD_BOSSES = "Jefes"
    L.HUD_DEATHS = "Muertes"
    L.HUD_OVERTIME = "FUERA DE TIEMPO"
    L.SEASON = "Temporada"
    L.WEEK = "Semana"
    L.SYNCING = "Sincronizando con el servidor..."
    L.OFFLINE = "Sin respuesta del servidor. Los botones igual envian. Prueba /reload."
end

local AFFIX = {
    [0] = { "None", "Ninguno" },
    [1] = { "Fortified", "Fortificado" },
    [2] = { "Tyrannical", "Tiranico" },
    [3] = { "Bolstering", "Potenciador" },
    [4] = { "Bursting", "Estallido" },
    [5] = { "Raging", "Enfurecido" },
    [6] = { "Sanguine", "Sanguino" },
    [7] = { "Teeming", "Bullente" },
    [8] = { "Necrotic", "Necrotico" },
    [9] = { "Skittish", "Asustadizo" },
    [10] = { "Volcanic", "Volcanico" },
    [11] = { "Explosive", "Explosivo" },
    [12] = { "Quaking", "Sismico" },
    [13] = { "Grievous", "Doloroso" },
    [14] = { "Inspiring", "Inspirador" },
    [15] = { "Spiteful", "Malicioso" },
    [16] = { "Storming", "Tormentoso" },
    [17] = { "Infested", "Infestado" },
    [18] = { "Reaping", "Siega" },
    [19] = { "Beguiling", "Seduccion" },
    [20] = { "Awakened", "Despertado" }
}

local DUNGEON = {
    [0] = { "None", "Ninguna" },
    [1] = { "Utgarde Keep", "Fortaleza de Utgarde" },
    [2] = { "The Nexus", "El Nexo" },
    [3] = { "Azjol-Nerub", "Azjol-Nerub" },
    [4] = { "Ahn'kahet", "Ahn'kahet" },
    [5] = { "Drak'Tharon Keep", "Fortaleza de Drak'Tharon" },
    [6] = { "Gundrak", "Gundrak" },
    [7] = { "Halls of Lightning", "Camaras de Relampagos" },
    [8] = { "Utgarde Pinnacle", "Pinaculo de Utgarde" },
    [9] = { "The Violet Hold", "El Bastion Violeta" },
    [10] = { "Halls of Stone", "Camaras de Piedra" },
    [11] = { "The Oculus", "El Oculus" },
    [12] = { "Culling of Stratholme", "La Matanza de Stratholme" },
    [13] = { "Trial of the Champion", "Prueba del Campeon" },
    [14] = { "The Forge of Souls", "La Forja de Almas" },
    [15] = { "Pit of Saron", "Foso de Saron" },
    [16] = { "Halls of Reflection", "Camaras de Reflexion" }
}

local function ES()
    return GetLocale() == "esES" or GetLocale() == "esMX"
end

local function AffixName(id)
    local row = AFFIX[id or 0] or AFFIX[0]
    return ES() and row[2] or row[1]
end

local function DungeonName(id)
    local row = DUNGEON[id or 0] or DUNGEON[0]
    return ES() and row[2] or row[1]
end

local function QualityRGB(q)
    if q == 5 then
        return 1.00, 0.50, 0.00
    elseif q == 4 then
        return 0.64, 0.21, 0.93
    elseif q == 3 then
        return 0.00, 0.44, 0.87
    elseif q == 2 then
        return 0.12, 1.00, 0.00
    end
    return 0.90, 0.90, 0.90
end

local function SkinBackdrop(widget, r, g, b, a, br, bg, bb, ba)
    if not widget or not widget.SetBackdrop then
        return
    end
    widget:SetBackdrop({
        bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 12,
        insets = { left = 3, right = 3, top = 3, bottom = 3 }
    })
    widget:SetBackdropColor(r or 0.05, g or 0.05, b or 0.07, a or 0.96)
    widget:SetBackdropBorderColor(br or 0.72, bg or 0.58, bb or 0.22, ba or 0.85)
end

local function Chat(msg)
    if DEFAULT_CHAT_FRAME then
        DEFAULT_CHAT_FRAME:AddMessage("|cffffcc00Mythic+|r " .. msg)
    end
end

local TAB_KEY, TAB_WEEK, TAB_VAULT, TAB_BOARD = 1, 2, 3, 4
local TAB_COUNT = 4

local state = {
    tab = TAB_KEY,
    synced = false,
    season = 1,
    week = 1,
    dungeonId = 0, level = 0, depleted = 0, score = 0,
    weekBest = 0, weekDungeon = 0, weekRuns = 0, vault = 0,
    weekKeys = { 0, 0, 0 },
    vaultItems = { 0, 0, 0 },
    weekIds = WeekAffixes(1, 1),
    board = {}
}

local function ApplyData(data)
    if type(data) ~= "table" then
        return
    end
    if type(data.profile) == "table" then
        local p = data.profile
        data.dungeonId = data.dungeonId or p.dungeonId
        data.level = data.level or p.level
        data.depleted = data.depleted or p.depleted
        data.score = data.score or p.score
        data.weekBest = data.weekBest or p.weekBest
        data.weekDungeon = data.weekDungeon or p.weekDungeon
        data.weekRuns = data.weekRuns or p.weekRuns
        data.vault = data.vault or p.vault
        if type(p.weekKeys) == "table" then
            data.weekKey1 = data.weekKey1 or p.weekKeys[1]
            data.weekKey2 = data.weekKey2 or p.weekKeys[2]
            data.weekKey3 = data.weekKey3 or p.weekKeys[3]
        end
        if type(p.vaultItems) == "table" then
            data.vault1 = data.vault1 or p.vaultItems[1]
            data.vault2 = data.vault2 or p.vaultItems[2]
            data.vault3 = data.vault3 or p.vaultItems[3]
        end
    end
    state.synced = true
    state.season = tonumber(data.season) or state.season
    state.week = tonumber(data.week) or state.week
    state.dungeonId = tonumber(data.dungeonId) or state.dungeonId
    state.level = tonumber(data.level) or state.level
    state.depleted = tonumber(data.depleted) or state.depleted
    state.score = tonumber(data.score) or state.score
    state.weekBest = tonumber(data.weekBest) or state.weekBest
    state.weekDungeon = tonumber(data.weekDungeon) or state.weekDungeon
    state.weekRuns = tonumber(data.weekRuns) or state.weekRuns
    state.vault = tonumber(data.vault) or state.vault
    state.weekKeys = {
        tonumber(data.weekKey1) or state.weekKeys[1] or 0,
        tonumber(data.weekKey2) or state.weekKeys[2] or 0,
        tonumber(data.weekKey3) or state.weekKeys[3] or 0
    }
    state.vaultItems = {
        tonumber(data.vault1) or state.vaultItems[1] or 0,
        tonumber(data.vault2) or state.vaultItems[2] or 0,
        tonumber(data.vault3) or state.vaultItems[3] or 0
    }
    state.weekIds = WeekAffixes(state.week, state.season)
end

local Refresh

local frame = CreateFrame("Frame", "ACMythicFrame", UIParent)
frame:SetSize(720, 540)
frame:SetPoint("CENTER")
frame:SetFrameStrata("DIALOG")
frame:SetToplevel(true)
frame:SetMovable(true)
frame:EnableMouse(true)
SkinBackdrop(frame, 0.04, 0.045, 0.06, 0.97, 0.78, 0.62, 0.22, 0.9)
frame:Hide()
tinsert(UISpecialFrames, "ACMythicFrame")

local header = CreateFrame("Frame", nil, frame)
header:SetPoint("TOPLEFT", 8, -8)
header:SetPoint("TOPRIGHT", -8, -8)
header:SetHeight(56)
header:EnableMouse(true)
header:RegisterForDrag("LeftButton")
header:SetScript("OnDragStart", function()
    frame:StartMoving()
end)
header:SetScript("OnDragStop", function()
    frame:StopMovingOrSizing()
end)
SkinBackdrop(header, 0.07, 0.06, 0.04, 0.9, 0.55, 0.42, 0.12, 0.5)

local title = header:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
title:SetPoint("LEFT", 18, 8)
title:SetText(L.TITLE)
title:SetTextColor(1, 0.84, 0.22)

local subtitle = header:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
subtitle:SetPoint("LEFT", 18, -12)
subtitle:SetText(L.SUB)

local seasonFS = header:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
seasonFS:SetPoint("RIGHT", -44, 0)
seasonFS:SetTextColor(0.85, 0.78, 0.55)

local close = CreateFrame("Button", nil, header, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", 4, 4)
close:SetWidth(28)
close:SetHeight(28)
close:SetScript("OnClick", function()
    frame:Hide()
end)

local tabBar = CreateFrame("Frame", nil, frame)
tabBar:SetPoint("TOPLEFT", 8, -68)
tabBar:SetPoint("TOPRIGHT", -8, -68)
tabBar:SetHeight(32)

local tabs = {}
local tabLabels = { L.TAB_KEY, L.TAB_WEEK, L.TAB_VAULT, L.TAB_BOARD }
for i = 1, TAB_COUNT do
    local tab = CreateFrame("Button", "ACMythicTab" .. i, tabBar, "UIPanelButtonTemplate")
    tab:SetSize(168, 26)
    tab:SetText(tabLabels[i])
    tab:EnableMouse(true)
    if i == 1 then
        tab:SetPoint("LEFT", 4, 0)
    else
        tab:SetPoint("LEFT", tabs[i - 1], "RIGHT", 6, 0)
    end
    tabs[i] = tab
end

local pane = CreateFrame("Frame", nil, frame)
pane:SetPoint("TOPLEFT", 8, -106)
pane:SetPoint("BOTTOMRIGHT", -8, 58)
SkinBackdrop(pane, 0.035, 0.038, 0.05, 0.92, 0.35, 0.30, 0.18, 0.55)

local lines = {}
for i = 1, 12 do
    local fs = pane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    fs:SetPoint("TOPLEFT", 24, -16 - ((i - 1) * 22))
    fs:SetPoint("TOPRIGHT", -24, -16 - ((i - 1) * 22))
    fs:SetJustifyH("LEFT")
    fs:SetTextColor(0.92, 0.90, 0.82)
    lines[i] = fs
end

local keyHero = pane:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
keyHero:SetPoint("TOPLEFT", 28, -52)
keyHero:SetTextColor(1, 0.82, 0.18)

local function ClearLines()
    for i = 1, #lines do
        lines[i]:SetText("")
    end
    keyHero:SetText("")
end

local function ShowItemTooltip(owner, itemId)
    if not owner or not itemId or itemId <= 0 or not GameTooltip then
        return
    end
    GameTooltip:SetOwner(owner, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("item:" .. itemId)
    GameTooltip:Show()
end

local vaultCards = {}
for i = 1, 3 do
    local card = CreateFrame("Frame", "ACMythicVaultCard" .. i, pane)
    card:SetSize(210, 250)
    card:SetPoint("TOPLEFT", 22 + ((i - 1) * 224), -64)
    SkinBackdrop(card, 0.06, 0.055, 0.08, 0.95, 0.45, 0.38, 0.18, 0.8)
    card:EnableMouse(true)
    card:Hide()

    local well = CreateFrame("Button", "ACMythicVaultIcon" .. i, card)
    well:SetSize(52, 52)
    well:SetPoint("TOP", 0, -22)
    well:EnableMouse(true)
    if well.SetBackdrop then
        SkinBackdrop(well, 0.02, 0.02, 0.03, 1, 0.7, 0.7, 0.7, 0.6)
    end

    local iconTex = well:CreateTexture(nil, "ARTWORK")
    iconTex:SetPoint("TOPLEFT", 3, -3)
    iconTex:SetPoint("BOTTOMRIGHT", -3, 3)
    iconTex:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")

    local nameFS = card:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    nameFS:SetPoint("TOP", well, "BOTTOM", 0, -12)
    nameFS:SetWidth(190)
    nameFS:SetJustifyH("CENTER")

    local ilvlFS = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    ilvlFS:SetPoint("TOP", nameFS, "BOTTOM", 0, -6)

    local claim = CreateFrame("Button", "ACMythicVaultClaim" .. i, card, "UIPanelButtonTemplate")
    claim:SetSize(150, 24)
    claim:SetPoint("BOTTOM", 0, 16)
    claim:SetText(L.CLAIM_SLOT)
    claim:EnableMouse(true)
    claim:SetScript("OnClick", function()
        Chat((ES() and "Reclamando opcion " or "Claiming choice ") .. i)
        AIO.Handle("MPLUS", "ClaimVault", i)
    end)

    local function bindTooltip(widget)
        widget:SetScript("OnEnter", function(self)
            ShowItemTooltip(self, card.itemId)
        end)
        widget:SetScript("OnLeave", function()
            if GameTooltip then
                GameTooltip:Hide()
            end
        end)
    end
    bindTooltip(card)
    bindTooltip(well)

    vaultCards[i] = {
        frame = card, well = well, tex = iconTex,
        name = nameFS, ilvl = ilvlFS, claim = claim
    }
end

local function SetVaultCard(i, itemId)
    local card = vaultCards[i]
    if not card then
        return
    end
    card.frame.itemId = itemId or 0
    if not itemId or itemId <= 0 then
        card.frame:Hide()
        return
    end
    local name, _, quality, ilvl, _, _, _, _, _, tex = GetItemInfo(itemId)
    card.tex:SetTexture(tex or "Interface\\Icons\\INV_Misc_QuestionMark")
    local r, g, b = QualityRGB(quality or 1)
    card.name:SetText(name or ("#" .. itemId))
    card.name:SetTextColor(r, g, b)
    card.ilvl:SetText(ilvl and ("Item Level " .. ilvl) or "")
    if card.well.SetBackdropBorderColor then
        card.well:SetBackdropBorderColor(r, g, b, 0.95)
    end
    card.frame:Show()
end

local function HideVaultCards()
    for i = 1, 3 do
        if vaultCards[i] then
            vaultCards[i].frame:Hide()
        end
    end
end

local footer = CreateFrame("Frame", nil, frame)
footer:SetPoint("BOTTOMLEFT", 8, 8)
footer:SetPoint("BOTTOMRIGHT", -8, 8)
footer:SetHeight(44)
footer:SetFrameLevel(frame:GetFrameLevel() + 6)
SkinBackdrop(footer, 0.06, 0.055, 0.04, 0.9, 0.35, 0.28, 0.12, 0.45)

local startBtn = CreateFrame("Button", "ACMythicStart", footer, "UIPanelButtonTemplate")
startBtn:SetSize(168, 26)
startBtn:SetPoint("LEFT", 12, 0)
startBtn:SetText(L.START)
startBtn:EnableMouse(true)

local teleportBtn = CreateFrame("Button", "ACMythicTeleport", footer, "UIPanelButtonTemplate")
teleportBtn:SetSize(140, 26)
teleportBtn:SetPoint("LEFT", startBtn, "RIGHT", 8, 0)
teleportBtn:SetText(L.TELEPORT)
teleportBtn:EnableMouse(true)

local claimBtn = CreateFrame("Button", "ACMythicClaim", footer, "UIPanelButtonTemplate")
claimBtn:SetSize(168, 26)
claimBtn:SetPoint("LEFT", teleportBtn, "RIGHT", 8, 0)
claimBtn:SetText(L.CLAIM)
claimBtn:EnableMouse(true)

Refresh = function()
    for i = 1, TAB_COUNT do
        if i == state.tab then
            tabs[i]:Disable()
        else
            tabs[i]:Enable()
        end
    end

    ClearLines()
    HideVaultCards()
    seasonFS:SetText(string.format("%s %d   ·   %s %d", L.SEASON, state.season or 1, L.WEEK, state.week or 1))

    if not state.synced then
        lines[12]:SetTextColor(1, 0.72, 0.28)
        lines[12]:SetText(L.SYNCING)
    end

    if state.tab == TAB_KEY then
        lines[1]:SetTextColor(0.62, 0.58, 0.48)
        lines[1]:SetText(L.KEY)
        if (state.level or 0) > 0 then
            keyHero:SetText(string.format("+%d", state.level))
            lines[5]:SetText(DungeonName(state.dungeonId))
            lines[5]:SetTextColor(1, 0.92, 0.7)
            if (state.depleted or 0) > 0 then
                lines[6]:SetText(L.DEPLETED)
                lines[6]:SetTextColor(1, 0.35, 0.28)
            end
        else
            keyHero:SetText("+")
            lines[5]:SetText(L.NONE)
        end
        lines[8]:SetTextColor(0.92, 0.90, 0.82)
        lines[8]:SetText(string.format("%s    %.1f", L.SCORE, state.score or 0))
        lines[9]:SetText(string.format("%s    +%d  %s", L.WEEK_BEST, state.weekBest or 0, DungeonName(state.weekDungeon)))
        lines[10]:SetText(string.format("%s    %d", L.RUNS, state.weekRuns or 0))
        startBtn:Show()
        teleportBtn:Show()
        claimBtn:Show()
        if (state.level or 0) > 0 then
            claimBtn:Disable()
        else
            claimBtn:Enable()
        end
    elseif state.tab == TAB_WEEK then
        local ids = state.weekIds or WeekAffixes(state.week, state.season)
        lines[1]:SetTextColor(1, 0.84, 0.28)
        lines[1]:SetText(string.format("%s %d", L.WEEK, state.week or 1))
        local gates = { L.AFFIX_2, L.AFFIX_4, L.AFFIX_7, L.AFFIX_10 }
        for i = 1, 4 do
            lines[i + 2]:SetText(string.format("%s     %s", gates[i], AffixName(ids[i])))
            lines[i + 2]:SetTextColor(0.95, 0.88, 0.62)
        end
        startBtn:Hide()
        teleportBtn:Hide()
        claimBtn:Hide()
    elseif state.tab == TAB_VAULT then
        local key = state.weekBest or 0
        if key <= 0 then
            key = (state.weekKeys and state.weekKeys[1]) or 0
        end
        lines[1]:SetTextColor(1, 0.84, 0.28)
        if (state.vault or 0) > 0 then
            lines[1]:SetText(L.VAULT_DONE)
        elseif (state.weekRuns or 0) <= 0 then
            lines[1]:SetText(L.VAULT_NEED)
        else
            lines[1]:SetText(string.format("%s   +%d     %s", L.TAB_VAULT, key, L.VAULT_HINT))
            local items = state.vaultItems or { 0, 0, 0 }
            for i = 1, 3 do
                SetVaultCard(i, items[i] or 0)
            end
        end
        startBtn:Hide()
        teleportBtn:Hide()
        claimBtn:Hide()
    else
        lines[1]:SetTextColor(1, 0.84, 0.28)
        lines[1]:SetText(L.TAB_BOARD)
        if #state.board == 0 then
            lines[3]:SetText(L.EMPTY)
        else
            for i = 1, math.min(8, #state.board) do
                local row = state.board[i]
                lines[i + 2]:SetText(string.format("%d     %s     %.1f     (+%d)",
                    i, row.name or "?", row.score or 0, row.weekBest or 0))
            end
        end
        startBtn:Hide()
        teleportBtn:Hide()
        claimBtn:Hide()
    end
end

for i = 1, TAB_COUNT do
    tabs[i]:SetScript("OnClick", function()
        state.tab = i
        if i == TAB_BOARD then
            AIO.Handle("MPLUS", "RequestBoard")
        end
        Refresh()
    end)
end

startBtn:SetScript("OnClick", function()
    Chat(L.START)
    AIO.Handle("MPLUS", "Start")
    AIO.Handle("MPLUS", "RequestHud")
end)
teleportBtn:SetScript("OnClick", function()
    Chat(L.TELEPORT)
    AIO.Handle("MPLUS", "Teleport")
end)
claimBtn:SetScript("OnClick", function()
    Chat(L.CLAIM)
    AIO.Handle("MPLUS", "ClaimKey")
end)

function Handlers.ShowUI(_, data)
    ApplyData(data)
    frame:Show()
    Refresh()
end

function Handlers.ShowWeek(_, data)
    if type(data) == "table" then
        state.week = tonumber(data.week) or state.week
        state.season = tonumber(data.season) or state.season
        if data.a0 then
            state.weekIds = { data.a0, data.a1, data.a2, data.a3 }
        elseif type(data.ids) == "table" then
            state.weekIds = data.ids
        end
    end
    Refresh()
end

function Handlers.ShowBoard(_, rows)
    state.board = {}
    if type(rows) == "table" then
        for i = 1, #rows do
            local row = rows[i]
            if type(row) == "string" then
                local name, score, best = strsplit("|", row)
                state.board[#state.board + 1] = {
                    name = name, score = tonumber(score) or 0, weekBest = tonumber(best) or 0
                }
            elseif type(row) == "table" then
                state.board[#state.board + 1] = row
            end
        end
    end
    Refresh()
end

local hud = CreateFrame("Frame", "ACMythicHud", UIParent)
hud:SetSize(360, 132)
hud:SetPoint("TOP", 0, -18)
hud:SetFrameStrata("HIGH")
hud:SetMovable(true)
hud:EnableMouse(true)
hud:RegisterForDrag("LeftButton")
hud:SetScript("OnDragStart", hud.StartMoving)
hud:SetScript("OnDragStop", hud.StopMovingOrSizing)
SkinBackdrop(hud, 0.03, 0.03, 0.04, 0.92, 0.78, 0.62, 0.2, 0.85)
hud:Hide()

local hudKey = hud:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
hudKey:SetPoint("TOPLEFT", 16, -10)
hudKey:SetTextColor(1, 0.82, 0.2)

local hudDungeon = hud:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
hudDungeon:SetPoint("LEFT", hudKey, "RIGHT", 12, 0)
hudDungeon:SetTextColor(0.92, 0.88, 0.72)

local hudTimer = hud:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
hudTimer:SetPoint("TOPRIGHT", -16, -10)
hudTimer:SetTextColor(0.35, 0.92, 0.40)

local hudStatus = hud:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
hudStatus:SetPoint("TOPRIGHT", hudTimer, "BOTTOMRIGHT", 0, -1)

local forceBar = CreateFrame("StatusBar", nil, hud)
forceBar:SetSize(328, 14)
forceBar:SetPoint("TOPLEFT", 16, -52)
forceBar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
forceBar:SetStatusBarColor(0.62, 0.32, 0.86)
forceBar:SetMinMaxValues(0, 1)
forceBar:SetValue(0)

local forceBg = forceBar:CreateTexture(nil, "BACKGROUND")
forceBg:SetAllPoints()
forceBg:SetTexture("Interface\\TargetingFrame\\UI-StatusBar")
forceBg:SetVertexColor(0.10, 0.08, 0.12)

local forceText = forceBar:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
forceText:SetPoint("CENTER")

local hudBosses = hud:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
hudBosses:SetPoint("TOPLEFT", forceBar, "BOTTOMLEFT", 0, -8)
hudBosses:SetTextColor(0.88, 0.84, 0.7)

local hudDeaths = hud:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
hudDeaths:SetPoint("TOPRIGHT", forceBar, "BOTTOMRIGHT", 0, -8)
hudDeaths:SetJustifyH("RIGHT")
hudDeaths:SetTextColor(0.88, 0.84, 0.7)

local hudAffixes = hud:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
hudAffixes:SetPoint("BOTTOMLEFT", 16, 10)
hudAffixes:SetPoint("BOTTOMRIGHT", -16, 10)
hudAffixes:SetJustifyH("LEFT")
hudAffixes:SetTextColor(0.78, 0.70, 0.48)

local function FormatTime(ms)
    ms = math.max(0, ms or 0)
    local total = math.floor(ms / 1000)
    return string.format("%02d:%02d", math.floor(total / 60), total % 60)
end

function Handlers.ShowHud(_, live)
    if type(live) ~= "table" then
        return
    end
    local elapsed = live.elapsed or 0
    local limit = live.limit or 0
    local remain = limit - elapsed
    local forces = live.forces or 0
    local forcesReq = math.max(1, live.forcesReq or 1)
    local ratio = 0
    if limit > 0 then
        ratio = remain / limit
    end
    local affixes = live.affixes
    if type(affixes) ~= "table" then
        affixes = { live.affix0, live.affix1, live.affix2, live.affix3 }
    end

    hudKey:SetText(string.format("+%d", live.level or 0))
    hudDungeon:SetText(DungeonName(live.dungeonId))

    if remain <= 0 then
        hudTimer:SetText("00:00")
        hudTimer:SetTextColor(1, 0.22, 0.22)
        hudStatus:SetText(L.HUD_OVERTIME)
        hudStatus:SetTextColor(1, 0.32, 0.28)
    else
        hudTimer:SetText(FormatTime(remain))
        hudStatus:SetText("")
        if ratio >= 0.40 then
            hudTimer:SetTextColor(0.35, 0.92, 0.40)
        elseif ratio >= 0.20 then
            hudTimer:SetTextColor(1, 0.78, 0.20)
        else
            hudTimer:SetTextColor(1, 0.42, 0.16)
        end
    end

    forceBar:SetValue(math.min(1, forces / forcesReq))
    if forces >= forcesReq then
        forceBar:SetStatusBarColor(0.28, 0.78, 0.38)
    else
        forceBar:SetStatusBarColor(0.62, 0.32, 0.86)
    end
    forceText:SetText(string.format("%s   %d / %d", L.HUD_FORCES, forces, live.forcesReq or 0))
    hudBosses:SetText(string.format("%s   %d / %d", L.HUD_BOSSES, live.bosses or 0, live.bossesReq or 0))
    hudDeaths:SetText(string.format("%s   %d", L.HUD_DEATHS, live.deaths or 0))
    local names = {}
    for i = 1, 4 do
        local id = affixes[i] or 0
        if id > 0 then
            names[#names + 1] = AffixName(id)
        end
    end
    hudAffixes:SetText(table.concat(names, "    "))
    hud:Show()
end

function Handlers.HideHud()
    hud:Hide()
end

local function InPartyInstance()
    if not IsInInstance then
        return false
    end
    local inside, kind = IsInInstance()
    return inside and kind == "party"
end

local retries = 0
local acc = 0
local ticker = CreateFrame("Frame")
ticker:SetScript("OnUpdate", function(_, elapsed)
    acc = acc + elapsed
    if acc < 1 then
        return
    end
    acc = 0
    if frame:IsShown() and not state.synced and retries < 6 then
        retries = retries + 1
        AIO.Handle("MPLUS", "RequestOpen")
        if retries == 6 then
            lines[12]:SetTextColor(1, 0.45, 0.3)
            lines[12]:SetText(L.OFFLINE)
        end
    end
    if hud:IsShown() or InPartyInstance() then
        AIO.Handle("MPLUS", "RequestHud")
    end
end)

local zoneWatch = CreateFrame("Frame")
zoneWatch:RegisterEvent("PLAYER_ENTERING_WORLD")
zoneWatch:SetScript("OnEvent", function()
    AIO.Handle("MPLUS", "RequestHud")
end)

local function OpenWindow()
    retries = 0
    frame:Show()
    Refresh()
    AIO.Handle("MPLUS", "RequestOpen")
    AIO.Handle("MPLUS", "RequestWeek")
end

SLASH_ACMPLUS1 = "/mplus"
SLASH_ACMPLUS2 = "/mythic"
SlashCmdList.ACMPLUS = function()
    if frame:IsShown() then
        frame:Hide()
        return
    end
    OpenWindow()
end
