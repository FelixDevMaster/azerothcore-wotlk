/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MODULE_PVP_SHARED_H
#define MODULE_PVP_SHARED_H

#include "Define.h"
#include <string>

struct PvPLeaderboardRow
{
    std::string Name;
    uint32 Rating = 0;
    uint32 Wins = 0;
    uint32 Losses = 0;
};

namespace PvPRating
{
    // Same tuning knobs as the core arena config, per queue instead of global.
    struct EloConfig
    {
        float WinModifierLow = 48.f;
        float WinModifierHigh = 24.f;
        float LoseModifier = 24.f;
        float MatchmakerModifier = 24.f;
    };

    float ChanceAgainst(uint32 ownRating, uint32 opponentRating);
    int32 RatingMod(EloConfig const& elo, uint32 ownRating, uint32 opponentRating, bool won);
    int32 MatchmakerMod(EloConfig const& elo, uint32 ownMmr, uint32 opponentMmr, bool won);

    // Widens the pairing window the longer a team waits, then drops it entirely
    // once discardTimer is reached so nobody is stuck at the rating extremes.
    uint32 MatchmakingWindow(uint32 baseWindow, uint32 waitedMs, uint32 discardTimerMs);
}

#endif
