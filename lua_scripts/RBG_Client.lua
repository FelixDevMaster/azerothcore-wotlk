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
    TITLE = "Rated Battlegrounds",
    SUB = "10v10  |  Wrath of the Lich King",
    QUEUE = "Queue",
    LEAVE = "Leave Queue",
    CLOSE = "Close",
    PLAY = "Play",
    BOARD = "Leaderboard",
    RATING = "Rating",
    MMR = "MMR",
    RECORD = "Record",
    WEEK = "Weekly",
    CONQUEST = "Arena Points",
    MAPS = "Map pool",
    MAP_LIST = "Warsong Gulch\nArathi Basin\nEye of the Storm\nStrand of the Ancients",
    HINT = "Raid leader only. Exactly 10 players. /rbg to toggle.",
    EMPTY = "No games recorded yet.",
    QUEUED = "In queue..."
}

if GetLocale() == "esES" or GetLocale() == "esMX" then
    L.TITLE = "Campos de Batalla Puntuados"
    L.SUB = "10c10  |  Wrath of the Lich King"
    L.QUEUE = "Encolar"
    L.LEAVE = "Salir de cola"
    L.CLOSE = "Cerrar"
    L.PLAY = "Jugar"
    L.BOARD = "Clasificacion"
    L.RATING = "Indice"
    L.RECORD = "Historial"
    L.WEEK = "Semana"
    L.CONQUEST = "Puntos de arena"
    L.MAPS = "Mapas"
    L.MAP_LIST = "Garganta Grito de Guerra\nCuenca de Arathi\nOjo de la Tormenta\nPlaya de los Ancestros"
    L.HINT = "Solo el lider de banda. Exactamente 10 jugadores. /rbg para abrir."
    L.EMPTY = "Todavia no hay partidas."
    L.QUEUED = "En cola..."
end

local Handlers = AIO.AddHandlers("RBG", {})

local ui = {
    rating = 1500,
    mmr = 1500,
    games = 0,
    wins = 0,
    weekGames = 0,
    weekWins = 0,
    weekConquest = 0,
    highest = 1500,
    cap = 1650,
    queued = false,
    tab = 1,
    board = {}
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
frame:SetSize(430, 470)
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

local sub = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
sub:SetPoint("TOP", title, "BOTTOM", 0, -18)
sub:SetText(L.SUB)

local tabPlay = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
tabPlay:SetSize(120, 22)
tabPlay:SetPoint("TOPLEFT", 28, -58)
tabPlay:SetText(L.PLAY)

local tabBoard = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
tabBoard:SetSize(120, 22)
tabBoard:SetPoint("LEFT", tabPlay, "RIGHT", 8, 0)
tabBoard:SetText(L.BOARD)

local playPane = CreateFrame("Frame", nil, frame)
playPane:SetPoint("TOPLEFT", 24, -88)
playPane:SetPoint("BOTTOMRIGHT", -24, 70)

local boardPane = CreateFrame("Frame", nil, frame)
boardPane:SetPoint("TOPLEFT", 24, -88)
boardPane:SetPoint("BOTTOMRIGHT", -24, 70)
boardPane:Hide()

local ratingFS = playPane:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
ratingFS:SetPoint("TOP", 0, -8)

local mmrFS = playPane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
mmrFS:SetPoint("TOP", ratingFS, "BOTTOM", 0, -6)

local recordFS = playPane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
recordFS:SetPoint("TOP", mmrFS, "BOTTOM", 0, -14)

local weekFS = playPane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
weekFS:SetPoint("TOP", recordFS, "BOTTOM", 0, -6)

local capFS = playPane:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
capFS:SetPoint("TOP", weekFS, "BOTTOM", 0, -6)

local mapsTitle = playPane:CreateFontString(nil, "OVERLAY", "GameFontNormal")
mapsTitle:SetPoint("TOPLEFT", 12, -170)
mapsTitle:SetText(L.MAPS)

local mapsFS = playPane:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
mapsFS:SetPoint("TOPLEFT", mapsTitle, "BOTTOMLEFT", 0, -6)
mapsFS:SetJustifyH("LEFT")
mapsFS:SetText(L.MAP_LIST)

local hintFS = playPane:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
hintFS:SetPoint("BOTTOM", 0, 8)
hintFS:SetWidth(360)
hintFS:SetText(L.HINT)

local boardLines = {}
for i = 1, 15 do
    local line = boardPane:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    line:SetPoint("TOPLEFT", 8, -8 - ((i - 1) * 16))
    line:SetJustifyH("LEFT")
    boardLines[i] = line
end

local queueBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
queueBtn:SetSize(160, 24)
queueBtn:SetPoint("BOTTOM", 0, 28)

local function Refresh()
    local r, g, b = RatingColor(ui.rating)
    ratingFS:SetText(string.format("%s  %d", L.RATING, ui.rating))
    ratingFS:SetTextColor(r, g, b)
    mmrFS:SetText(string.format("%s %d   (peak %d)", L.MMR, ui.mmr, ui.highest))
    local losses = math.max(0, ui.games - ui.wins)
    recordFS:SetText(string.format("%s  %d - %d", L.RECORD, ui.wins, losses))
    local weekLosses = math.max(0, ui.weekGames - ui.weekWins)
    weekFS:SetText(string.format("%s  %d - %d", L.WEEK, ui.weekWins, weekLosses))
    capFS:SetText(string.format("%s  %d / %d", L.CONQUEST, ui.weekConquest, ui.cap))
    if ui.queued then
        queueBtn:SetText(L.LEAVE)
    else
        queueBtn:SetText(L.QUEUE)
    end

    if #ui.board == 0 then
        boardLines[1]:SetText(L.EMPTY)
        for i = 2, 15 do
            boardLines[i]:SetText("")
        end
    else
        for i = 1, 15 do
            local row = ui.board[i]
            if row then
                boardLines[i]:SetText(string.format("%2d.  %-16s  %d   (%d-%d)",
                    i, row.name, row.rating, row.wins, row.losses))
            else
                boardLines[i]:SetText("")
            end
        end
    end
end

local function ShowTab(id)
    ui.tab = id
    if id == 1 then
        playPane:Show()
        boardPane:Hide()
    else
        playPane:Hide()
        boardPane:Show()
        AIO.Handle("RBG", "RequestBoard")
    end
end

tabPlay:SetScript("OnClick", function()
    ShowTab(1)
end)

tabBoard:SetScript("OnClick", function()
    ShowTab(2)
end)

queueBtn:SetScript("OnClick", function()
    if ui.queued then
        AIO.Handle("RBG", "Leave")
    else
        AIO.Handle("RBG", "Queue")
    end
end)

function Handlers.ShowUI(_, rating, mmr, games, wins, weekGames, weekWins, weekConquest, highest, cap, queued)
    ui.rating = rating or 1500
    ui.mmr = mmr or 1500
    ui.games = games or 0
    ui.wins = wins or 0
    ui.weekGames = weekGames or 0
    ui.weekWins = weekWins or 0
    ui.weekConquest = weekConquest or 0
    ui.highest = highest or 1500
    ui.cap = cap or 1650
    ui.queued = queued == 1
    Refresh()
    frame:Show()
end

function Handlers.ShowBoard(_, board)
    ui.board = board or {}
    Refresh()
end

SLASH_ACRBG1 = "/rbg"
SLASH_ACRBG2 = "/ratedbg"
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
