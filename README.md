# Rated Battlegrounds (MoP → Lich King)

Campos de batalla puntuados 10c10 al estilo MoP para AzerothCore 3.3.5a. Cola de premades, MMR, índice personal, honor y puntos de arena (equivalente a Conquest), con UI AIO (`/rbg`).

---

10v10 **Rated Battlegrounds** for AzerothCore 3.3.5a, modelled on the Mists of Pandaria system:

- Premade raid of 10 queues as a team
- Random map from the WotLK pool (Warsong Gulch, Arathi Basin, Eye of the Storm, Strand of the Ancients)
- Personal rating + MMR with the arena ELO formula
- Extra honor and arena points (LK stand-in for Conquest), with a weekly cap
- UI via [AIO](https://github.com/Rochet2/AIO) (`/rbg` and a minimap button)

The native 3.3.5 client has no RBG pane, and Eluna cannot create battleground instances. This module therefore splits the work:

| Layer | Role |
| --- | --- |
| C++ (`src/`) | Queue, MMR matching, 10v10 rated BG invite, rating, rewards, `.rbg` commands, battlemaster NPC |
| Lua + AIO | In-game window (queue, stats, leaderboard). Talks to C++ through the `rbg_request` table |
| SQL | Optional NPC template; character tables are created on worldserver boot |

## Requirements

- AzerothCore WotLK
- This module cloned/copied to `modules/mod-rbg-aio`
- Rebuild worldserver after adding the module
- **Optional UI:** [mod-ale](https://github.com/azerothcore/mod-ale) (or Eluna) + [AIO](https://github.com/Rochet2/AIO)

Commands and the NPC work **without** Lua/AIO.

## Install

1. Place this folder in `azerothcore-wotlk/modules/mod-rbg-aio`.
2. Import `data/sql/db-world/base/rbg_npc.sql` into the world database.
3. Merge `conf/rbg.conf.dist` into `worldserver.conf` (or copy it next to the other module configs).
4. Rebuild and restart worldserver.
5. Spawn the battlemaster: `.npc add 190010`

### AIO UI (optional)

1. Install AIO: `AIO_Server` → `lua_scripts/`, `AIO_Client` → `Interface/AddOns/` on every client.
2. Copy `lua_scripts/RBG_Server.lua` and `lua_scripts/RBG_Client.lua` into the server `lua_scripts/` folder (same place as `AIO.lua`).
3. Restart worldserver (or `.reload eluna` if you use that).
4. In-game: `/rbg` or the minimap badge.

The AIO client addon is delivered by the server; players only need the AIO client package, not a separate RBG addon.

## Play

1. Form a **raid of exactly 10** (configurable `RatedBG.TeamSize`).
2. Everyone must be max level (`RatedBG.MinLevel`, default 80), same faction, no Deserter, not already in a BG queue.
3. The leader queues with `.rbg queue`, the NPC, or the AIO **Queue** button.
4. When two teams match, everyone gets the normal battleground invite popup.
5. After the match, personal rating/MMR, honor, and arena points are applied.

Other commands: `.rbg leave`, `.rbg status`, `.rbg top`.

## Config highlights

See `conf/rbg.conf.dist`.

- `RatedBG.Maps = "2,3,7,9"` — WSG, AB, EotS, SotA. AV and Isle of Conquest stay out (40-player maps).
- `RatedBG.AllowSameFaction = 0` — Alliance vs Horde only. Set to 1 for same-faction pairing (needs a cross-faction BG module for heals/buffs on the opposite side).
- `RatedBG.WeeklyCap` — weekly arena-point cap from RBG wins.
- `RatedBG.EnableTitles = 0` — optional legacy PvP rank titles at rating breakpoints.

## Notes

- Matches are **rated battlegrounds**, not arenas: flags, bases, and boats work as in unrated BGs, scaled to 10 per side.
- Rating uses the same chance-to-win curve as arena (`1 / (1 + 10^((opp-own)/650))`).
- Weekly stats reset seven days after the first boot (timestamp in `rbg_state`).
- Lua never executes server commands; it only writes `rbg_request`. The C++ module consumes that table each tick.
