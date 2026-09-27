local ok, aio = pcall(function()
    return AIO or require("AIO")
end)
if not ok or not aio then
    return
end
local AIO = aio

if AIO.AddAddon() then
    return
end

local L = {
    TITLE = "Mythic Keystone",
    SUB = "Battle for Azeroth",
    TAB_KEY = "Key",
    TAB_WEEK = "Week",
    TAB_VAULT = "Vault",
    TAB_BOARD = "Ranking",
    START = "Insert Keystone",
    TELEPORT = "Teleport",
    CLAIM = "Claim +2 Keystone",
    KEY = "Keystone",
    SCORE = "Score",
    WEEK_BEST = "Week best",
    RUNS = "Runs this week",
    DEPLETED = "Depleted",
    NONE = "No keystone",
    AFFIX_2 = "+2",
    AFFIX_4 = "+4",
    AFFIX_7 = "+7",
    AFFIX_10 = "+10 Seasonal",
    SLOT = "Slot",
    CLAIM_SLOT = "Claim",
    CLAIMED = "Claimed",
    LOCKED = "Locked",
    NAME = "Name",
    BOARD_SCORE = "Score",
    BEST = "Best",
    EMPTY = "No scores yet.",
    HUD_FORCES = "Forces",
    HUD_BOSSES = "Bosses",
    HUD_DEATHS = "Deaths",
    HUD_OVERTIME = "Time expired",
    SLASH = "/mplus  to toggle this window"
}

if GetLocale() == "esES" or GetLocale() == "esMX" then
    L.TITLE = "Piedra angular mitica"
    L.TAB_KEY = "Piedra"
    L.TAB_WEEK = "Semana"
    L.TAB_VAULT = "Cofre"
    L.TAB_BOARD = "Ranking"
    L.START = "Insertar piedra"
    L.TELEPORT = "Teletransportar"
    L.CLAIM = "Reclamar piedra +2"
    L.KEY = "Piedra"
    L.SCORE = "Puntuacion"
    L.WEEK_BEST = "Mejor de la semana"
    L.RUNS = "Runs esta semana"
    L.DEPLETED = "Agotada"
    L.NONE = "Sin piedra"
    L.AFFIX_10 = "+10 Temporada"
    L.SLOT = "Ranura"
    L.CLAIM_SLOT = "Reclamar"
    L.CLAIMED = "Reclamada"
    L.LOCKED = "Bloqueada"
    L.NAME = "Nombre"
    L.BOARD_SCORE = "Score"
    L.BEST = "Mejor"
    L.EMPTY = "Todavia no hay puntuaciones."
    L.HUD_FORCES = "Fuerzas"
    L.HUD_BOSSES = "Jefes"
    L.HUD_DEATHS = "Muertes"
    L.HUD_OVERTIME = "Tiempo agotado"
    L.SLASH = "/mplus  para abrir esta ventana"
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

local Handlers = AIO.AddHandlers("MPLUS", {})

local TAB_KEY, TAB_WEEK, TAB_VAULT, TAB_BOARD = 1, 2, 3, 4
local TAB_COUNT = 4

local state = {
    tab = TAB_KEY,
    season = 1,
    week = 1,
    profile = {
        dungeonId = 0, level = 0, depleted = 0, score = 0,
        weekBest = 0, weekDungeon = 0, weekRuns = 0, vault = 0,
        weekKeys = { 0, 0, 0 }
    },
    weekIds = { 0, 0, 0, 0 },
    board = {}
}

local GOLD = { 1, 0.82, 0 }
local CREAM = { 1, 0.96, 0.84 }
local MUTED = { 0.7, 0.65, 0.5 }

local frame = CreateFrame("Frame", "ACMythicFrame", UIParent)
frame:SetSize(640, 520)
frame:SetPoint("CENTER")
frame:SetFrameStrata("DIALOG")
frame:SetToplevel(true)
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
frame:SetBackdrop({
    bgFile = "Interface\\FrameGeneral\\UI-Background-Marble",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Gold-Border",
    tile = false,
    edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 }
})
frame:SetBackdropColor(0.10, 0.08, 0.12, 0.96)
frame:Hide()
tinsert(UISpecialFrames, "ACMythicFrame")

local titleBg = frame:CreateTexture(nil, "ARTWORK")
titleBg:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
titleBg:SetSize(380, 64)
titleBg:SetPoint("TOP", 0, 14)

local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
title:SetPoint("TOP", titleBg, "TOP", 0, -13)
title:SetText(L.TITLE)
title:SetTextColor(unpack(GOLD))

local subtitle = frame:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
subtitle:SetPoint("TOP", title, "BOTTOM", 0, -18)
subtitle:SetText(L.SUB)

local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -2, -2)
close:SetWidth(32)
close:SetHeight(32)

local tabs = {}
local tabLabels = { L.TAB_KEY, L.TAB_WEEK, L.TAB_VAULT, L.TAB_BOARD }
for i = 1, TAB_COUNT do
    local tab = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    tab:SetSize(110, 26)
    tab:SetText(tabLabels[i])
    if i == 1 then
        tab:SetPoint("TOPLEFT", 28, -52)
    else
        tab:SetPoint("LEFT", tabs[i - 1], "RIGHT", 6, 0)
    end
    tabs[i] = tab
end

local pane = CreateFrame("Frame", nil, frame)
pane:SetPoint("TOPLEFT", 22, -88)
pane:SetPoint("BOTTOMRIGHT", -22, 78)
pane:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true,
    tileSize = 16,
    edgeSize = 16,
    insets = { left = 4, right = 4, top = 4, bottom = 4 }
})
pane:SetBackdropColor(0.08, 0.06, 0.10, 0.85)
pane:SetBackdropBorderColor(0.55, 0.35, 0.75, 0.9)

local lines = {}
for i = 1, 12 do
    local fs = pane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    fs:SetPoint("TOPLEFT", 28, -18 - ((i - 1) * 24))
    fs:SetPoint("TOPRIGHT", -28, -18 - ((i - 1) * 24))
    fs:SetJustifyH("LEFT")
    fs:SetTextColor(unpack(CREAM))
    lines[i] = fs
end

local function ClearLines()
    for i = 1, #lines do
        lines[i]:SetText("")
    end
end

local startBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
startBtn:SetSize(160, 30)
startBtn:SetPoint("BOTTOMLEFT", 36, 28)
startBtn:SetText(L.START)

local teleportBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
teleportBtn:SetSize(140, 30)
teleportBtn:SetPoint("LEFT", startBtn, "RIGHT", 8, 0)
teleportBtn:SetText(L.TELEPORT)

local claimBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
claimBtn:SetSize(170, 30)
claimBtn:SetPoint("LEFT", teleportBtn, "RIGHT", 8, 0)
claimBtn:SetText(L.CLAIM)

local slashFS = frame:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
slashFS:SetPoint("BOTTOM", 0, 12)
slashFS:SetText(L.SLASH)

local function Refresh()
    for i = 1, TAB_COUNT do
        if i == state.tab then
            tabs[i]:Disable()
        else
            tabs[i]:Enable()
        end
    end

    ClearLines()
    local p = state.profile or {}
    local spanishHint = ES()

    if state.tab == TAB_KEY then
        lines[1]:SetTextColor(unpack(GOLD))
        lines[1]:SetText(string.format("%s  —  %s %d  %s %d", L.TITLE, spanishHint and "Temporada" or "Season",
            state.season or 1, spanishHint and "semana" or "week", state.week or 1))
        if (p.level or 0) > 0 then
            lines[3]:SetText(string.format("%s  +%d  %s%s", L.KEY, p.level, DungeonName(p.dungeonId),
                (p.depleted or 0) > 0 and ("  (" .. L.DEPLETED .. ")") or ""))
        else
            lines[3]:SetText(L.NONE)
        end
        lines[5]:SetText(string.format("%s:  %.1f", L.SCORE, p.score or 0))
        lines[6]:SetText(string.format("%s:  +%d  %s", L.WEEK_BEST, p.weekBest or 0, DungeonName(p.weekDungeon)))
        lines[7]:SetText(string.format("%s:  %d", L.RUNS, p.weekRuns or 0))
        startBtn:Show()
        teleportBtn:Show()
        claimBtn:Show()
        if (p.level or 0) > 0 then
            claimBtn:Disable()
        else
            claimBtn:Enable()
        end
    elseif state.tab == TAB_WEEK then
        lines[1]:SetTextColor(unpack(GOLD))
        lines[1]:SetText(string.format("%s %d  —  %s %d", spanishHint and "Semana" or "Week",
            state.week or 1, spanishHint and "Temporada" or "Season", state.season or 1))
        local gates = { L.AFFIX_2, L.AFFIX_4, L.AFFIX_7, L.AFFIX_10 }
        for i = 1, 4 do
            lines[i + 2]:SetText(string.format("%s   %s", gates[i], AffixName(state.weekIds[i])))
        end
        startBtn:Hide()
        teleportBtn:Hide()
        claimBtn:Hide()
    elseif state.tab == TAB_VAULT then
        lines[1]:SetTextColor(unpack(GOLD))
        lines[1]:SetText(L.TAB_VAULT)
        local need = { 1, 4, 8 }
        for i = 1, 3 do
            local claimed = (math.floor((p.vault or 0) / (2 ^ (i - 1))) % 2) > 0
            local key = (p.weekKeys and p.weekKeys[i]) or 0
            local status = claimed and L.CLAIMED or (((p.weekRuns or 0) >= need[i] and key > 0) and L.CLAIM_SLOT or L.LOCKED)
            lines[i + 2]:SetText(string.format("%s %d   +%d   (%d %s)   %s",
                L.SLOT, i, key, need[i], spanishHint and "runs" or "runs", status))
        end
        startBtn:Hide()
        teleportBtn:Hide()
        claimBtn:Hide()
    else
        lines[1]:SetTextColor(unpack(GOLD))
        lines[1]:SetText(L.TAB_BOARD)
        if #state.board == 0 then
            lines[3]:SetText(L.EMPTY)
        else
            for i = 1, math.min(8, #state.board) do
                local row = state.board[i]
                lines[i + 2]:SetText(string.format("%d.  %s   %.1f   (+%d)",
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
        if i == TAB_WEEK then
            AIO.Handle("MPLUS", "RequestWeek")
        elseif i == TAB_BOARD then
            AIO.Handle("MPLUS", "RequestBoard")
        end
        Refresh()
    end)
end

startBtn:SetScript("OnClick", function()
    AIO.Handle("MPLUS", "Start")
end)
teleportBtn:SetScript("OnClick", function()
    AIO.Handle("MPLUS", "Teleport")
end)
claimBtn:SetScript("OnClick", function()
    AIO.Handle("MPLUS", "ClaimKey")
end)

local vaultBtns = {}
for i = 1, 3 do
    local btn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    btn:SetSize(90, 22)
    btn:SetPoint("BOTTOMRIGHT", -36, 28 + ((3 - i) * 26))
    btn:SetText(L.CLAIM_SLOT .. " " .. i)
    btn:SetScript("OnClick", function()
        AIO.Handle("MPLUS", "ClaimVault", i)
    end)
    btn:Hide()
    vaultBtns[i] = btn
end

local oldRefresh = Refresh
Refresh = function()
    oldRefresh()
    local showVault = state.tab == TAB_VAULT
    for i = 1, 3 do
        if showVault then
            vaultBtns[i]:Show()
        else
            vaultBtns[i]:Hide()
        end
    end
end

function Handlers.ShowUI(_, data)
    if type(data) ~= "table" then
        return
    end
    state.season = data.season or 1
    state.week = data.week or 1
    state.profile = data.profile or state.profile
    frame:Show()
    Refresh()
end

function Handlers.ShowWeek(_, data)
    if type(data) ~= "table" then
        return
    end
    state.week = data.week or state.week
    state.season = data.season or state.season
    state.weekIds = data.ids or state.weekIds
    Refresh()
end

function Handlers.ShowBoard(_, rows)
    state.board = rows or {}
    Refresh()
end

local hud = CreateFrame("Frame", "ACMythicHud", UIParent)
hud:SetSize(340, 154)
hud:SetPoint("TOP", 0, -22)
hud:SetFrameStrata("HIGH")
hud:SetMovable(true)
hud:EnableMouse(true)
hud:RegisterForDrag("LeftButton")
hud:SetScript("OnDragStart", hud.StartMoving)
hud:SetScript("OnDragStop", hud.StopMovingOrSizing)
hud:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true,
    tileSize = 16,
    edgeSize = 16,
    insets = { left = 4, right = 4, top = 4, bottom = 4 }
})
hud:SetBackdropColor(0.04, 0.03, 0.07, 0.92)
hud:SetBackdropBorderColor(0.72, 0.52, 0.18, 0.95)
hud:Hide()

local hudKey = hud:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
hudKey:SetPoint("TOPLEFT", 16, -12)
hudKey:SetTextColor(unpack(GOLD))
hudKey:SetJustifyH("LEFT")

local hudDungeon = hud:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
hudDungeon:SetPoint("TOPLEFT", hudKey, "BOTTOMLEFT", 0, -3)
hudDungeon:SetPoint("TOPRIGHT", -16, -28)
hudDungeon:SetJustifyH("LEFT")
hudDungeon:SetTextColor(unpack(CREAM))

local hudTimer = hud:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
hudTimer:SetPoint("TOPRIGHT", -16, -12)
hudTimer:SetTextColor(1, 0.86, 0.28)

local hudStatus = hud:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
hudStatus:SetPoint("TOPRIGHT", hudTimer, "BOTTOMRIGHT", 0, -2)
hudStatus:SetJustifyH("RIGHT")

local forceBar = CreateFrame("StatusBar", nil, hud)
forceBar:SetSize(308, 16)
forceBar:SetPoint("TOPLEFT", 16, -62)
forceBar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
forceBar:SetStatusBarColor(0.55, 0.36, 0.82)
forceBar:SetMinMaxValues(0, 1)
forceBar:SetValue(0)

local forceBg = forceBar:CreateTexture(nil, "BACKGROUND")
forceBg:SetAllPoints()
forceBg:SetTexture("Interface\\TargetingFrame\\UI-StatusBar")
forceBg:SetVertexColor(0.12, 0.10, 0.16)

local forceText = forceBar:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
forceText:SetPoint("CENTER")
forceText:SetTextColor(1, 1, 1)

local hudBosses = hud:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
hudBosses:SetPoint("TOPLEFT", forceBar, "BOTTOMLEFT", 0, -10)
hudBosses:SetTextColor(unpack(CREAM))

local hudDeaths = hud:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
hudDeaths:SetPoint("TOPRIGHT", forceBar, "BOTTOMRIGHT", 0, -10)
hudDeaths:SetJustifyH("RIGHT")
hudDeaths:SetTextColor(unpack(CREAM))

local hudAffixes = hud:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
hudAffixes:SetPoint("BOTTOMLEFT", 16, 12)
hudAffixes:SetPoint("BOTTOMRIGHT", -16, 12)
hudAffixes:SetJustifyH("LEFT")
hudAffixes:SetTextColor(0.82, 0.74, 0.55)

local function FormatTime(ms)
    ms = math.max(0, ms or 0)
    local total = math.floor(ms / 1000)
    return string.format("%02d:%02d", math.floor(total / 60), total % 60)
end

local function ActiveAffixes(ids)
    local names = {}
    if type(ids) ~= "table" then
        return ""
    end
    for i = 1, #ids do
        local id = ids[i] or 0
        if id > 0 then
            names[#names + 1] = AffixName(id)
        end
    end
    return table.concat(names, "   ·   ")
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

    hudKey:SetText(string.format("+%d", live.level or 0))
    hudDungeon:SetText(DungeonName(live.dungeonId))

    if remain <= 0 then
        hudTimer:SetText("00:00")
        hudTimer:SetTextColor(1, 0.18, 0.18)
        hudStatus:SetText(L.HUD_OVERTIME)
        hudStatus:SetTextColor(1, 0.28, 0.28)
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
        forceBar:SetStatusBarColor(0.55, 0.36, 0.82)
    end
    forceText:SetText(string.format("%s   %d / %d", L.HUD_FORCES, forces, live.forcesReq or 0))
    hudBosses:SetText(string.format("%s   %d / %d", L.HUD_BOSSES, live.bosses or 0, live.bossesReq or 0))
    hudDeaths:SetText(string.format("%s   %d", L.HUD_DEATHS, live.deaths or 0))
    hudAffixes:SetText(ActiveAffixes(live.affixes))
    hud:Show()
end

function Handlers.HideHud()
    hud:Hide()
end

local ticker = CreateFrame("Frame")
local acc = 0
ticker:SetScript("OnUpdate", function(_, elapsed)
    if not hud:IsShown() then
        acc = 0
        return
    end
    acc = acc + elapsed
    if acc < 1 then
        return
    end
    acc = 0
    AIO.Handle("MPLUS", "RequestHud")
end)

local zoneWatch = CreateFrame("Frame")
zoneWatch:RegisterEvent("PLAYER_ENTERING_WORLD")
zoneWatch:SetScript("OnEvent", function()
    AIO.Handle("MPLUS", "RequestHud")
end)

SLASH_ACMPLUS1 = "/mplus"
SLASH_ACMPLUS2 = "/mythic"
SlashCmdList.ACMPLUS = function()
    if frame:IsShown() then
        frame:Hide()
        return
    end
    AIO.Handle("MPLUS", "RequestOpen")
end
