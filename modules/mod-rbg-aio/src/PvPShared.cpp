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

#include "PvPShared.h"
#include <cmath>

namespace PvPRating
{
float ChanceAgainst(uint32 ownRating, uint32 opponentRating)
{
    return 1.0f / (1.0f + std::exp(std::log(10.0f) *
        (static_cast<float>(opponentRating) - static_cast<float>(ownRating)) / 650.0f));
}

int32 RatingMod(EloConfig const& elo, uint32 ownRating, uint32 opponentRating, bool won)
{
    float chance = ChanceAgainst(ownRating, opponentRating);
    float mod;

    if (!won)
        mod = elo.LoseModifier * (-chance);
    else if (ownRating >= 1300)
        mod = elo.WinModifierHigh * (1.0f - chance);
    else if (ownRating < 1000)
        mod = elo.WinModifierLow * (1.0f - chance);
    else
    {
        float half = elo.WinModifierLow / 2.0f;
        mod = (half + (half * (1300.0f - static_cast<float>(ownRating)) / 300.0f)) * (1.0f - chance);
    }

    return static_cast<int32>(std::ceil(mod));
}

int32 MatchmakerMod(EloConfig const& elo, uint32 ownMmr, uint32 opponentMmr, bool won)
{
    float chance = ChanceAgainst(ownMmr, opponentMmr);
    float wonMod = won ? 1.0f : 0.0f;
    return static_cast<int32>(std::ceil((wonMod - chance) * elo.MatchmakerModifier));
}

uint32 MatchmakingWindow(uint32 baseWindow, uint32 waitedMs, uint32 discardTimerMs)
{
    if (!discardTimerMs)
        return baseWindow;

    if (waitedMs >= discardTimerMs)
        return 10000;

    return baseWindow + ((waitedMs * 400) / discardTimerMs);
}
}
