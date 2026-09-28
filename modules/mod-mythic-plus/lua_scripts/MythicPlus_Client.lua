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
    VAULT_RESET = "One choice · resets Friday 20:00",
    EMPTY = "No scores yet.",
    HUD_FORCES = "Enemy Forces",
    HUD_BOSSES = "Bosses",
    HUD_DEATHS = "Deaths",
    HUD_OVERTIME = "OVERTIME",
    SEASON = "Season",
    WEEK = "Week"
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
    L.VAULT_RESET = "Una eleccion · reset viernes 20:00"
    L.EMPTY = "Todavia no hay puntuaciones."
    L.HUD_FORCES = "Fuerzas enemigas"
    L.HUD_BOSSES = "Jefes"
    L.HUD_DEATHS = "Muertes"
    L.HUD_OVERTIME = "FUERA DE TIEMPO"
    L.SEASON = "Temporada"
    L.WEEK = "Semana"
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
        weekKeys = { 0, 0, 0 }, vaultItems = { 0, 0, 0 }
    },
    weekIds = { 0, 0, 0, 0 },
    board = {}
}

local Refresh

local frame = CreateFrame("Frame", "ACMythicFrame", UIParent)
frame:SetSize(720, 540)
frame:SetPoint("CENTER")
frame:SetFrameStrata("DIALOG")
frame:SetToplevel(true)
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
SkinBackdrop(frame, 0.04, 0.045, 0.06, 0.97, 0.78, 0.62, 0.22, 0.9)
frame:Hide()
tinsert(UISpecialFrames, "ACMythicFrame")

SLASH_ACMPLUS1 = "/mplus"
SLASH_ACMPLUS2 = "/mythic"
SlashCmdList.ACMPLUS = function()
    if frame:IsShown() then
        frame:Hide()
        return
    end
    frame:Show()
    AIO.Handle("MPLUS", "RequestOpen")
    pcall(function()
        if Refresh then
            Refresh()
        end
    end)
end

local header = CreateFrame("Frame", nil, frame)
header:SetPoint("TOPLEFT", 8, -8)
header:SetPoint("TOPRIGHT", -8, -8)
header:SetHeight(56)
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
tabBar:SetHeight(28)

local tabs = {}
local tabLabels = { L.TAB_KEY, L.TAB_WEEK, L.TAB_VAULT, L.TAB_BOARD }
for i = 1, TAB_COUNT do
    local tab = CreateFrame("Button", nil, tabBar)
    tab:SetSize(168, 26)
    if i == 1 then
        tab:SetPoint("LEFT", 4, 0)
    else
        tab:SetPoint("LEFT", tabs[i - 1], "RIGHT", 6, 0)
    end
    SkinBackdrop(tab, 0.08, 0.08, 0.10, 0.9, 0.25, 0.22, 0.16, 0.7)
    local fs = tab:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    fs:SetPoint("CENTER")
    fs:SetText(tabLabels[i])
    tab.label = fs
    tab.underline = tab:CreateTexture(nil, "ARTWORK")
    tab.underline:SetTexture("Interface\\ChatFrame\\ChatFrameBackground")
    tab.underline:SetPoint("BOTTOMLEFT", 8, 2)
    tab.underline:SetPoint("BOTTOMRIGHT", -8, 2)
    tab.underline:SetHeight(2)
    tab.underline:SetVertexColor(1, 0.82, 0.2)
    tab.underline:Hide()
    tabs[i] = tab
end

local pane = CreateFrame("Frame", nil, frame)
pane:SetPoint("TOPLEFT", 8, -102)
pane:SetPoint("BOTTOMRIGHT", -8, 56)
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

local keyHero = pane:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
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

    local well = CreateFrame("Frame", nil, card)
    well:SetSize(52, 52)
    well:SetPoint("TOP", 0, -22)
    SkinBackdrop(well, 0.02, 0.02, 0.03, 1, 0.7, 0.7, 0.7, 0.6)

    local iconBtn = CreateFrame("Button", nil, well)
    iconBtn:SetAllPoints()
    local iconTex = iconBtn:CreateTexture(nil, "ARTWORK")
    iconTex:SetPoint("TOPLEFT", 3, -3)
    iconTex:SetPoint("BOTTOMRIGHT", -3, 3)
    iconTex:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")

    local nameFS = card:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    nameFS:SetPoint("TOP", well, "BOTTOM", 0, -12)
    nameFS:SetWidth(190)
    nameFS:SetJustifyH("CENTER")

    local ilvlFS = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    ilvlFS:SetPoint("TOP", nameFS, "BOTTOM", 0, -6)

    local claim = CreateFrame("Button", nil, card, "UIPanelButtonTemplate")
    claim:SetSize(150, 24)
    claim:SetPoint("BOTTOM", 0, 16)
    claim:SetText(L.CLAIM_SLOT)
    claim:SetScript("OnClick", function()
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
    bindTooltip(iconBtn)

    vaultCards[i] = {
        frame = card, well = well, icon = iconBtn, tex = iconTex,
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
    if card.tex then
        card.tex:SetTexture(tex or "Interface\\Icons\\INV_Misc_QuestionMark")
    end
    local r, g, b = QualityRGB(quality or 1)
    card.name:SetText(name or ("#" .. itemId))
    card.name:SetTextColor(r, g, b)
    card.ilvl:SetText(ilvl and ("Item Level " .. ilvl) or "")
    if card.well and card.well.SetBackdropBorderColor then
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
footer:SetHeight(42)
SkinBackdrop(footer, 0.06, 0.055, 0.04, 0.9, 0.35, 0.28, 0.12, 0.45)

local startBtn = CreateFrame("Button", nil, footer, "UIPanelButtonTemplate")
startBtn:SetSize(168, 26)
startBtn:SetPoint("LEFT", 12, 0)
startBtn:SetText(L.START)

local teleportBtn = CreateFrame("Button", nil, footer, "UIPanelButtonTemplate")
teleportBtn:SetSize(140, 26)
teleportBtn:SetPoint("LEFT", startBtn, "RIGHT", 8, 0)
teleportBtn:SetText(L.TELEPORT)

local claimBtn = CreateFrame("Button", nil, footer, "UIPanelButtonTemplate")
claimBtn:SetSize(168, 26)
claimBtn:SetPoint("LEFT", teleportBtn, "RIGHT", 8, 0)
claimBtn:SetText(L.CLAIM)

local function StyleTab(i, selected)
    local tab = tabs[i]
    if selected then
        if tab.SetBackdropColor then
            tab:SetBackdropColor(0.18, 0.14, 0.06, 0.95)
            tab:SetBackdropBorderColor(0.9, 0.72, 0.2, 0.95)
        end
        tab.label:SetTextColor(1, 0.86, 0.28)
        tab.underline:Show()
    else
        if tab.SetBackdropColor then
            tab:SetBackdropColor(0.08, 0.08, 0.10, 0.9)
            tab:SetBackdropBorderColor(0.25, 0.22, 0.16, 0.7)
        end
        tab.label:SetTextColor(0.72, 0.68, 0.55)
        tab.underline:Hide()
    end
end

Refresh = function()
    for i = 1, TAB_COUNT do
        StyleTab(i, i == state.tab)
    end

    ClearLines()
    HideVaultCards()
    local p = state.profile or {}
    seasonFS:SetText(string.format("%s %d   ·   %s %d", L.SEASON, state.season or 1, L.WEEK, state.week or 1))

    if state.tab == TAB_KEY then
        lines[1]:SetTextColor(0.62, 0.58, 0.48)
        lines[1]:SetText(L.KEY)
        if (p.level or 0) > 0 then
            keyHero:SetText(string.format("+%d", p.level))
            lines[5]:SetText(DungeonName(p.dungeonId))
            lines[5]:SetTextColor(1, 0.92, 0.7)
            if (p.depleted or 0) > 0 then
                lines[6]:SetText(L.DEPLETED)
                lines[6]:SetTextColor(1, 0.35, 0.28)
            end
        else
            keyHero:SetText("+")
            lines[5]:SetText(L.NONE)
        end
        lines[8]:SetTextColor(0.92, 0.90, 0.82)
        lines[8]:SetText(string.format("%s    %.1f", L.SCORE, p.score or 0))
        lines[9]:SetText(string.format("%s    +%d  %s", L.WEEK_BEST, p.weekBest or 0, DungeonName(p.weekDungeon)))
        lines[10]:SetText(string.format("%s    %d", L.RUNS, p.weekRuns or 0))
        startBtn:Show()
        teleportBtn:Show()
        claimBtn:Show()
        if (p.level or 0) > 0 then
            claimBtn:Disable()
        else
            claimBtn:Enable()
        end
    elseif state.tab == TAB_WEEK then
        lines[1]:SetTextColor(1, 0.84, 0.28)
        lines[1]:SetText(string.format("%s %d", L.WEEK, state.week or 1))
        local gates = { L.AFFIX_2, L.AFFIX_4, L.AFFIX_7, L.AFFIX_10 }
        for i = 1, 4 do
            lines[i + 2]:SetText(string.format("%s     %s", gates[i], AffixName(state.weekIds[i])))
            lines[i + 2]:SetTextColor(0.95, 0.88, 0.62)
        end
        startBtn:Hide()
        teleportBtn:Hide()
        claimBtn:Hide()
    elseif state.tab == TAB_VAULT then
        local key = p.weekBest or 0
        if key <= 0 and p.weekKeys then
            key = p.weekKeys[1] or 0
        end
        lines[1]:SetTextColor(1, 0.84, 0.28)
        if (p.vault or 0) > 0 then
            lines[1]:SetText(L.VAULT_DONE)
        elseif (p.weekRuns or 0) <= 0 then
            lines[1]:SetText(L.VAULT_NEED)
        else
            lines[1]:SetText(string.format("%s   +%d     %s", L.TAB_VAULT, key, L.VAULT_HINT))
            local items = p.vaultItems or { 0, 0, 0 }
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
    AIO.Handle("MPLUS", "RequestHud")
end)
teleportBtn:SetScript("OnClick", function()
    AIO.Handle("MPLUS", "Teleport")
end)
claimBtn:SetScript("OnClick", function()
    AIO.Handle("MPLUS", "ClaimKey")
end)

function Handlers.ShowUI(_, data)
    if type(data) == "table" then
        state.season = data.season or 1
        state.week = data.week or 1
        state.profile = data.profile or state.profile
    end
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

local hudKey = hud:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
hudKey:SetPoint("TOPLEFT", 16, -10)
hudKey:SetTextColor(1, 0.82, 0.2)

local hudDungeon = hud:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
hudDungeon:SetPoint("LEFT", hudKey, "RIGHT", 12, 0)
hudDungeon:SetTextColor(0.92, 0.88, 0.72)

local hudTimer = hud:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
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
    return table.concat(names, "    ")
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
    hudAffixes:SetText(ActiveAffixes(live.affixes))
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

local ticker = CreateFrame("Frame")
local acc = 0
ticker:SetScript("OnUpdate", function(_, elapsed)
    acc = acc + elapsed
    if acc < 1 then
        return
    end
    acc = 0
    if hud:IsShown() or InPartyInstance() then
        AIO.Handle("MPLUS", "RequestHud")
    end
end)

local zoneWatch = CreateFrame("Frame")
zoneWatch:RegisterEvent("PLAYER_ENTERING_WORLD")
zoneWatch:SetScript("OnEvent", function()
    AIO.Handle("MPLUS", "RequestHud")
end)
