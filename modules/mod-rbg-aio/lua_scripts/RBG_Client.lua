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
    TAB_RBG = "RBG",
    TAB_1V1 = "1v1",
    TAB_2V2 = "2v2",
    TAB_3V3 = "3v3",
    TAB_BOARD = "Ranking",
    QUEUE = "Enter Queue",
    LEAVE = "Leave Queue",
    RATING = "Personal Rating",
    MMR = "Matchmaking",
    PEAK = "Season High",
    RECORD = "Season Record",
    WEEK = "This Week",
    POINTS = "Arena Points",
    TEAM = "Arena Team",
    WINS = "Wins",
    LOSSES = "Losses",
    EMPTY = "No games recorded yet.",
    HINT_RBG = "Form a raid of 10. Only the raid leader can queue.",
    HINT_1V1 = "Solo queue. Leave your group before you queue.",
    HINT_2V2 = "Party of 2. Each player has a personal 2v2 team. The leader queues.",
    HINT_3V3 = "Solo queue. Personal 5v5 team. Sides are built as official 3v3 comps (RMP, MLS, Jungle, God Comp, Thunder...).",
    SLASH = "/rbg  to toggle this window"
}

if GetLocale() == "esES" or GetLocale() == "esMX" then
    L.TITLE = "PvP Puntuado"
    L.TAB_RBG = "CB"
    L.TAB_1V1 = "1c1"
    L.TAB_2V2 = "2c2"
    L.TAB_3V3 = "3c3"
    L.TAB_BOARD = "Ranking"
    L.QUEUE = "Encolar"
    L.LEAVE = "Salir de cola"
    L.RATING = "Indice personal"
    L.MMR = "Emparejamiento"
    L.PEAK = "Maximo de temporada"
    L.RECORD = "Historial"
    L.WEEK = "Esta semana"
    L.POINTS = "Puntos de arena"
    L.TEAM = "Equipo de arena"
    L.WINS = "Victorias"
    L.LOSSES = "Derrotas"
    L.EMPTY = "Todavia no hay partidas."
    L.HINT_RBG = "Forma una banda de 10. Solo el lider puede encolar."
    L.HINT_1V1 = "Cola en solitario. Sal del grupo antes de encolar."
    L.HINT_2V2 = "Grupo de 2. Cada uno tiene su equipo 2c2 personal. Encola el lider."
    L.HINT_3V3 = "Cola en solitario. Equipo 5c5 personal. Se arman comps oficiales (RMP, MLS, Jungle, God Comp, Thunder...)."
    L.SLASH = "/rbg  para abrir esta ventana"
end

local Handlers = AIO.AddHandlers("RBG", {})

local TAB_RBG, TAB_1V1, TAB_2V2, TAB_3V3, TAB_BOARD = 1, 2, 3, 4, 5
local TAB_COUNT = 5
local BRACKET_BY_TAB = { [TAB_1V1] = 0, [TAB_2V2] = 2, [TAB_3V3] = 1 }

local emptyStats = { rating = 1500, mmr = 1500, games = 0, wins = 0,
    weekGames = 0, weekWins = 0, weekPoints = 0, highest = 1500,
    teamRating = 0, teamName = "" }

local state = {
    tab = TAB_RBG,
    queued = 0,
    boardTab = 1,
    board = {},
    rbg = emptyStats,
    solo1v1 = emptyStats,
    solo2v2 = emptyStats,
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
    return 1, 0.96, 0.84
end

local function RankColor(rank)
    if rank == 1 then
        return 1, 0.82, 0
    end
    if rank == 2 then
        return 0.8, 0.8, 0.8
    end
    if rank == 3 then
        return 0.8, 0.5, 0.2
    end
    return 1, 0.96, 0.84
end

local GOLD = { 1, 0.82, 0 }
local CREAM = { 1, 0.96, 0.84 }
local MUTED = { 0.7, 0.65, 0.5 }

local frame = CreateFrame("Frame", "ACRBGFrame", UIParent)
frame:SetSize(620, 560)
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
frame:SetBackdropColor(0.12, 0.10, 0.08, 0.96)
frame:Hide()
tinsert(UISpecialFrames, "ACRBGFrame")

local titleBg = frame:CreateTexture(nil, "ARTWORK")
titleBg:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
titleBg:SetSize(420, 64)
titleBg:SetPoint("TOP", 0, 14)

local pvpIcon = frame:CreateTexture(nil, "OVERLAY")
pvpIcon:SetTexture("Interface\\PvPRankBadges\\PvPRank14")
pvpIcon:SetSize(28, 28)
pvpIcon:SetPoint("TOP", titleBg, "TOP", -118, -10)

local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
title:SetPoint("TOP", titleBg, "TOP", 8, -13)
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
local tabLabels = { L.TAB_RBG, L.TAB_1V1, L.TAB_2V2, L.TAB_3V3, L.TAB_BOARD }
for i = 1, TAB_COUNT do
    local tab = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    tab:SetSize(108, 26)
    tab:SetText(tabLabels[i])
    if i == 1 then
        tab:SetPoint("TOPLEFT", 24, -52)
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
pane:SetBackdropColor(0.08, 0.07, 0.05, 0.85)
pane:SetBackdropBorderColor(0.7, 0.55, 0.2, 0.9)

local ratingLabel = pane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
ratingLabel:SetPoint("TOPLEFT", 28, -22)
ratingLabel:SetText(L.RATING)
ratingLabel:SetTextColor(unpack(GOLD))

local ratingFS = pane:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
ratingFS:SetPoint("TOPRIGHT", -28, -16)

local divider = pane:CreateTexture(nil, "ARTWORK")
divider:SetTexture("Interface\\FriendsFrame\\UI-FriendsFrame-OnlineDivider")
divider:SetHeight(8)
divider:SetPoint("TOPLEFT", 20, -56)
divider:SetPoint("TOPRIGHT", -20, -56)

local function MakeStatRow(anchor, y)
    local label = pane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    label:SetPoint("TOPLEFT", 32, y)
    label:SetTextColor(unpack(MUTED))
    local value = pane:CreateFontString(nil, "OVERLAY", "GameFontHighlightLarge")
    value:SetPoint("TOPRIGHT", -32, y + 2)
    value:SetTextColor(unpack(CREAM))
    return label, value
end

local mmrLabel, mmrFS = MakeStatRow(nil, -78)
local peakLabel, peakFS = MakeStatRow(nil, -112)
local recordLabel, recordFS = MakeStatRow(nil, -146)
local weekLabel, weekFS = MakeStatRow(nil, -180)
local extraLabel, extraFS = MakeStatRow(nil, -214)

mmrLabel:SetText(L.MMR)
peakLabel:SetText(L.PEAK)
recordLabel:SetText(L.RECORD)
weekLabel:SetText(L.WEEK)

local hintFS = pane:CreateFontString(nil, "OVERLAY", "GameFontDisable")
hintFS:SetPoint("BOTTOMLEFT", 24, 18)
hintFS:SetPoint("BOTTOMRIGHT", -24, 18)
hintFS:SetJustifyH("CENTER")

local boardPane = CreateFrame("Frame", nil, frame)
boardPane:SetPoint("TOPLEFT", 22, -88)
boardPane:SetPoint("BOTTOMRIGHT", -22, 78)
boardPane:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background-Dark",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true,
    tileSize = 16,
    edgeSize = 16,
    insets = { left = 4, right = 4, top = 4, bottom = 4 }
})
boardPane:SetBackdropColor(0.08, 0.07, 0.05, 0.85)
boardPane:SetBackdropBorderColor(0.7, 0.55, 0.2, 0.9)
boardPane:Hide()

local boardTabs = {}
local boardLabels = { L.TAB_RBG, L.TAB_1V1, L.TAB_2V2, L.TAB_3V3 }
for i = 1, 4 do
    local tab = CreateFrame("Button", nil, boardPane, "UIPanelButtonTemplate")
    tab:SetSize(120, 22)
    tab:SetText(boardLabels[i])
    if i == 1 then
        tab:SetPoint("TOPLEFT", 16, -14)
    else
        tab:SetPoint("LEFT", boardTabs[i - 1], "RIGHT", 6, 0)
    end
    tab:SetScript("OnClick", function()
        state.boardTab = i
        AIO.Handle("RBG", "RequestBoard", i)
    end)
    boardTabs[i] = tab
end

local headerRank = boardPane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
headerRank:SetPoint("TOPLEFT", 22, -46)
headerRank:SetText("#")
headerRank:SetTextColor(unpack(GOLD))

local headerName = boardPane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
headerName:SetPoint("TOPLEFT", 56, -46)
headerName:SetText(L.TAB_BOARD)
headerName:SetTextColor(unpack(GOLD))

local headerRating = boardPane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
headerRating:SetPoint("TOPRIGHT", -140, -46)
headerRating:SetText(L.RATING)
headerRating:SetTextColor(unpack(GOLD))

local headerRecord = boardPane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
headerRecord:SetPoint("TOPRIGHT", -28, -46)
headerRecord:SetText(L.RECORD)
headerRecord:SetTextColor(unpack(GOLD))

local boardLines = {}
for i = 1, 15 do
    local line = boardPane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    line:SetPoint("TOPLEFT", 22, -68 - ((i - 1) * 20))
    line:SetPoint("TOPRIGHT", -22, -68 - ((i - 1) * 20))
    line:SetJustifyH("LEFT")
    boardLines[i] = line
end

local actionBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
actionBtn:SetSize(220, 32)
actionBtn:SetPoint("BOTTOM", 0, 28)

local slashFS = frame:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
slashFS:SetPoint("BOTTOM", 0, 12)
slashFS:SetText(L.SLASH)

local function StatsForTab(tab)
    if tab == TAB_1V1 then
        return state.solo1v1
    end
    if tab == TAB_2V2 then
        return state.solo2v2
    end
    if tab == TAB_3V3 then
        return state.solo3v3
    end
    return state.rbg
end

local function QueuedTabMatches(tab)
    return state.queued ~= 0 and state.queued == tab
end

local function UsesTeamPane(tab)
    return tab == TAB_2V2 or tab == TAB_3V3
end

local function Refresh()
    for i = 1, TAB_COUNT do
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

        for i = 1, 4 do
            if i == state.boardTab then
                boardTabs[i]:Disable()
            else
                boardTabs[i]:Enable()
            end
        end

        if #state.board == 0 then
            boardLines[1]:SetText(L.EMPTY)
            boardLines[1]:SetTextColor(unpack(MUTED))
            for i = 2, 15 do
                boardLines[i]:SetText("")
            end
        else
            for i = 1, 15 do
                local row = state.board[i]
                if row then
                    local r, g, b = RankColor(i)
                    boardLines[i]:SetText(string.format("%2d     %-18s          %4d          %d - %d",
                        i, row.name, row.rating, row.wins, row.losses))
                    boardLines[i]:SetTextColor(r, g, b)
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
    local r, g, b = RatingColor(stats.rating or 0)
    ratingFS:SetText(tostring(stats.rating or 0))
    ratingFS:SetTextColor(r, g, b)

    mmrFS:SetText(tostring(stats.mmr or 0))
    peakFS:SetText(tostring(stats.highest or stats.rating or 0))

    local losses = math.max(0, (stats.games or 0) - (stats.wins or 0))
    recordFS:SetText(string.format("%d  -  %d", stats.wins or 0, losses))

    local weekLosses = math.max(0, (stats.weekGames or 0) - (stats.weekWins or 0))
    weekFS:SetText(string.format("%d  -  %d", stats.weekWins or 0, weekLosses))

    if UsesTeamPane(state.tab) then
        extraLabel:SetText(L.TEAM)
        if stats.teamName and stats.teamName ~= "" then
            extraFS:SetText(string.format("%s   (%d)", stats.teamName, stats.teamRating or 0))
        else
            extraFS:SetText("—")
        end
    else
        extraLabel:SetText(L.POINTS)
        extraFS:SetText(tostring(stats.weekPoints or 0))
    end

    if state.tab == TAB_RBG then
        hintFS:SetText(L.HINT_RBG)
    elseif state.tab == TAB_1V1 then
        hintFS:SetText(L.HINT_1V1)
    elseif state.tab == TAB_2V2 then
        hintFS:SetText(L.HINT_2V2)
    else
        hintFS:SetText(L.HINT_3V3)
    end

    if QueuedTabMatches(state.tab) then
        actionBtn:SetText(L.LEAVE)
    else
        actionBtn:SetText(L.QUEUE)
    end

    if state.queued ~= 0 and not QueuedTabMatches(state.tab) then
        actionBtn:Disable()
    else
        actionBtn:Enable()
    end
end

for i = 1, TAB_COUNT do
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
    else
        AIO.Handle("RBG", "QueueSolo", BRACKET_BY_TAB[state.tab])
    end
end)

function Handlers.ShowUI(_, data)
    if data then
        state.rbg = data.rbg or emptyStats
        state.solo1v1 = data.solo1v1 or emptyStats
        state.solo2v2 = data.solo2v2 or emptyStats
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
mini:SetSize(32, 32)
mini:SetFrameStrata("MEDIUM")
mini:SetPoint("TOPLEFT", Minimap, "TOPLEFT", -12, -64)
mini:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")

local miniIcon = mini:CreateTexture(nil, "BACKGROUND")
miniIcon:SetTexture("Interface\\PvPRankBadges\\PvPRank14")
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
    GameTooltip:SetText(L.TITLE, 1, 0.82, 0)
    GameTooltip:AddLine(L.SUB, 1, 1, 1)
    GameTooltip:AddLine("/rbg", 0.7, 0.7, 0.7)
    GameTooltip:Show()
end)

mini:SetScript("OnLeave", function()
    GameTooltip:Hide()
end)

Refresh()
