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
    TITLE = "Rated PvP",
    SUB = "Wrath of the Lich King",
    TAB_RBG = "RBG 10v10",
    TAB_1V1 = "1v1",
    TAB_3V3 = "3v3 SoloQ",
    TAB_BOARD = "Ranking",
    QUEUE = "Queue",
    LEAVE = "Leave Queue",
    RATING = "Rating",
    MMR = "MMR",
    PEAK = "peak",
    RECORD = "Record",
    WEEK = "Weekly",
    POINTS = "Arena Points",
    EMPTY = "No games recorded yet.",
    HINT_RBG = "Raid leader only. Exactly 10 players.",
    HINT_1V1 = "Solo only. Leave your group to queue.",
    HINT_3V3 = "Solo only. Teams are built as 1 healer + 2 damage.",
    SLASH = "/rbg to toggle this window."
}

if GetLocale() == "esES" or GetLocale() == "esMX" then
    L.TITLE = "PvP Puntuado"
    L.TAB_RBG = "CB 10c10"
    L.TAB_3V3 = "3c3 SoloQ"
    L.TAB_BOARD = "Clasificacion"
    L.QUEUE = "Encolar"
    L.LEAVE = "Salir de cola"
    L.RATING = "Indice"
    L.PEAK = "maximo"
    L.RECORD = "Historial"
    L.WEEK = "Semana"
    L.POINTS = "Puntos de arena"
    L.EMPTY = "Todavia no hay partidas."
    L.HINT_RBG = "Solo el lider de banda. Exactamente 10 jugadores."
    L.HINT_1V1 = "Solo en solitario. Sal del grupo para encolar."
    L.HINT_3V3 = "Solo en solitario. Equipos de 1 sanador + 2 de dano."
    L.SLASH = "/rbg para abrir esta ventana."
end

local Handlers = AIO.AddHandlers("RBG", {})

local TAB_RBG, TAB_1V1, TAB_3V3, TAB_BOARD = 1, 2, 3, 4

local emptyStats = { rating = 1500, mmr = 1500, games = 0, wins = 0,
    weekGames = 0, weekWins = 0, weekPoints = 0, highest = 1500 }

local state = {
    tab = TAB_RBG,
    queued = 0,
    boardTab = 1,
    board = {},
    rbg = emptyStats,
    solo1v1 = emptyStats,
    solo3v3 = emptyStats
}

local function RatingColor(rating)
    if rating >= 2200 then
        return 1, 0.82, 0
    end
    if rating >= 1900 then
        return 0.64, 0.21, 0.93
    end
    if rating >= 1600 then
        return 0.1, 0.5, 1
    end
    return 1, 1, 1
end

local frame = CreateFrame("Frame", "ACRBGFrame", UIParent)
frame:SetSize(440, 430)
frame:SetPoint("CENTER")
frame:SetFrameStrata("DIALOG")
frame:SetToplevel(true)
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
frame:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true,
    tileSize = 32,
    edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 }
})
frame:Hide()
tinsert(UISpecialFrames, "ACRBGFrame")

local titleBg = frame:CreateTexture(nil, "ARTWORK")
titleBg:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
titleBg:SetSize(340, 64)
titleBg:SetPoint("TOP", 0, 12)

local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
title:SetPoint("TOP", titleBg, "TOP", 0, -14)
title:SetText(L.TITLE)

local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -4, -4)

local tabs = {}
local tabLabels = { L.TAB_RBG, L.TAB_1V1, L.TAB_3V3, L.TAB_BOARD }
for i = 1, 4 do
    local tab = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    tab:SetSize(96, 22)
    tab:SetText(tabLabels[i])
    if i == 1 then
        tab:SetPoint("TOPLEFT", 20, -44)
    else
        tab:SetPoint("LEFT", tabs[i - 1], "RIGHT", 4, 0)
    end
    tabs[i] = tab
end

local pane = CreateFrame("Frame", nil, frame)
pane:SetPoint("TOPLEFT", 24, -76)
pane:SetPoint("BOTTOMRIGHT", -24, 64)

local ratingFS = pane:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
ratingFS:SetPoint("TOP", 0, -6)

local mmrFS = pane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
mmrFS:SetPoint("TOP", ratingFS, "BOTTOM", 0, -8)

local recordFS = pane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
recordFS:SetPoint("TOP", mmrFS, "BOTTOM", 0, -14)

local weekFS = pane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
weekFS:SetPoint("TOP", recordFS, "BOTTOM", 0, -6)

local pointsFS = pane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
pointsFS:SetPoint("TOP", weekFS, "BOTTOM", 0, -6)

local hintFS = pane:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
hintFS:SetPoint("BOTTOM", 0, 4)
hintFS:SetWidth(370)

local boardPane = CreateFrame("Frame", nil, frame)
boardPane:SetPoint("TOPLEFT", 24, -76)
boardPane:SetPoint("BOTTOMRIGHT", -24, 64)
boardPane:Hide()

local boardTabs = {}
local boardLabels = { L.TAB_RBG, L.TAB_1V1, L.TAB_3V3 }
for i = 1, 3 do
    local tab = CreateFrame("Button", nil, boardPane, "UIPanelButtonTemplate")
    tab:SetSize(120, 20)
    tab:SetText(boardLabels[i])
    if i == 1 then
        tab:SetPoint("TOPLEFT", 4, 0)
    else
        tab:SetPoint("LEFT", boardTabs[i - 1], "RIGHT", 4, 0)
    end
    tab:SetScript("OnClick", function()
        state.boardTab = i
        AIO.Handle("RBG", "RequestBoard", i)
    end)
    boardTabs[i] = tab
end

local boardLines = {}
for i = 1, 15 do
    local line = boardPane:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    line:SetPoint("TOPLEFT", 6, -26 - ((i - 1) * 15))
    line:SetJustifyH("LEFT")
    boardLines[i] = line
end

local actionBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
actionBtn:SetSize(170, 24)
actionBtn:SetPoint("BOTTOM", 0, 26)

local slashFS = frame:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
slashFS:SetPoint("BOTTOM", 0, 12)
slashFS:SetText(L.SLASH)

-- queued: 0 none, 1 RBG, 2 solo 1v1, 3 solo 3v3
local function StatsForTab(tab)
    if tab == TAB_1V1 then
        return state.solo1v1
    end
    if tab == TAB_3V3 then
        return state.solo3v3
    end
    return state.rbg
end

local function QueuedTabMatches(tab)
    return (tab == TAB_RBG and state.queued == 1)
        or (tab == TAB_1V1 and state.queued == 2)
        or (tab == TAB_3V3 and state.queued == 3)
end

local function Refresh()
    for i = 1, 4 do
        if i == state.tab then
            tabs[i]:Disable()
        else
            tabs[i]:Enable()
        end
    end

    if state.tab == TAB_BOARD then
        pane:Hide()
        boardPane:Show()
        actionBtn:Hide()

        for i = 1, 3 do
            if i == state.boardTab then
                boardTabs[i]:Disable()
            else
                boardTabs[i]:Enable()
            end
        end

        if #state.board == 0 then
            boardLines[1]:SetText(L.EMPTY)
            for i = 2, 15 do
                boardLines[i]:SetText("")
            end
        else
            for i = 1, 15 do
                local row = state.board[i]
                if row then
                    boardLines[i]:SetText(string.format("%2d.  %-16s  %d   (%d-%d)",
                        i, row.name, row.rating, row.wins, row.losses))
                else
                    boardLines[i]:SetText("")
                end
            end
        end
        return
    end

    boardPane:Hide()
    pane:Show()
    actionBtn:Show()

    local stats = StatsForTab(state.tab)
    local r, g, b = RatingColor(stats.rating)
    ratingFS:SetText(string.format("%s  %d", L.RATING, stats.rating))
    ratingFS:SetTextColor(r, g, b)
    mmrFS:SetText(string.format("%s %d   (%s %d)", L.MMR, stats.mmr, L.PEAK, stats.highest))

    local losses = math.max(0, stats.games - stats.wins)
    recordFS:SetText(string.format("%s  %d - %d", L.RECORD, stats.wins, losses))

    local weekLosses = math.max(0, stats.weekGames - stats.weekWins)
    weekFS:SetText(string.format("%s  %d - %d", L.WEEK, stats.weekWins, weekLosses))
    pointsFS:SetText(string.format("%s  %d", L.POINTS, stats.weekPoints))

    if state.tab == TAB_RBG then
        hintFS:SetText(L.HINT_RBG)
    elseif state.tab == TAB_1V1 then
        hintFS:SetText(L.HINT_1V1)
    else
        hintFS:SetText(L.HINT_3V3)
    end

    if QueuedTabMatches(state.tab) then
        actionBtn:SetText(L.LEAVE)
    else
        actionBtn:SetText(L.QUEUE)
    end

    -- Already waiting in another queue: block the button instead of failing server side.
    if state.queued ~= 0 and not QueuedTabMatches(state.tab) then
        actionBtn:Disable()
    else
        actionBtn:Enable()
    end
end

for i = 1, 4 do
    tabs[i]:SetScript("OnClick", function()
        state.tab = i
        if i == TAB_BOARD then
            AIO.Handle("RBG", "RequestBoard", state.boardTab)
        end
        Refresh()
    end)
end

actionBtn:SetScript("OnClick", function()
    if QueuedTabMatches(state.tab) then
        if state.tab == TAB_RBG then
            AIO.Handle("RBG", "LeaveRbg")
        else
            AIO.Handle("RBG", "LeaveSolo")
        end
        return
    end

    if state.tab == TAB_RBG then
        AIO.Handle("RBG", "QueueRbg")
    elseif state.tab == TAB_1V1 then
        AIO.Handle("RBG", "QueueSolo", 0)
    else
        AIO.Handle("RBG", "QueueSolo", 1)
    end
end)

function Handlers.ShowUI(_, data)
    if data then
        state.rbg = data.rbg or emptyStats
        state.solo1v1 = data.solo1v1 or emptyStats
        state.solo3v3 = data.solo3v3 or emptyStats
        state.queued = data.queued or 0
    end

    Refresh()
    frame:Show()
end

function Handlers.ShowBoard(_, tab, board)
    state.boardTab = tab or 1
    state.board = board or {}
    Refresh()
end

SLASH_ACRBG1 = "/rbg"
SLASH_ACRBG2 = "/pvp2"
SlashCmdList["ACRBG"] = function()
    if frame:IsShown() then
        frame:Hide()
    else
        AIO.Handle("RBG", "RequestOpen")
    end
end

local mini = CreateFrame("Button", "ACRBGMinimap", Minimap)
mini:SetSize(31, 31)
mini:SetFrameStrata("MEDIUM")
mini:SetPoint("TOPLEFT", Minimap, "TOPLEFT", -12, -64)
mini:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")

local miniIcon = mini:CreateTexture(nil, "BACKGROUND")
miniIcon:SetTexture("Interface\\PvPRankBadges\\PvPRank12")
miniIcon:SetSize(20, 20)
miniIcon:SetPoint("CENTER")

local miniBorder = mini:CreateTexture(nil, "OVERLAY")
miniBorder:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
miniBorder:SetSize(53, 53)
miniBorder:SetPoint("TOPLEFT")

mini:SetScript("OnClick", function()
    if frame:IsShown() then
        frame:Hide()
    else
        AIO.Handle("RBG", "RequestOpen")
    end
end)

mini:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:SetText(L.TITLE)
    GameTooltip:AddLine("/rbg", 1, 1, 1)
    GameTooltip:Show()
end)

mini:SetScript("OnLeave", function()
    GameTooltip:Hide()
end)

Refresh()
