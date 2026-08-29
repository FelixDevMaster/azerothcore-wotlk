# Rated PvP: RBG 10v10, 1v1, 2v2 and 3v3 SoloQ

Sistema de PvP puntuado para AzerothCore 3.3.5a: campos de batalla puntuados 10c10, arenas 1c1 y 3c3 en solitario (índice personal) y arenas 2c2 **con equipo de arena normal** (`arena_team` / `arena_team_member`). Incluye MMR, honor, puntos de arena e interfaz AIO (`/rbg`).

---

Rated PvP for AzerothCore 3.3.5a with four queues:

| Queue | Side | Entry |
| --- | --- | --- |
| Rated Battleground | 10 | premade raid of 10, leader queues |
| 1v1 Arena | 1 | solo |
| 2v2 Arena | 2 | party of 2 in the same 2v2 arena team, leader queues |
| 3v3 SoloQ | 3 | solo, sides built as 1 healer + 2 damage |

1v1 and 3v3 SoloQ keep a **module-owned personal rating** (MoP-style, stored in `arena_solo_stats`). **2v2 is the normal Lich King bracket:** both players must belong to the same 2v2 `arena_team`, the match is created rated, and the core writes **team rating** to `arena_team` and **personal rating** to `arena_team_member` (plus MMR in `character_arena_stats` slot 0). The in-game arena team pane, inspect, and weekly arena points all see those 2v2 games.

## Why C++ plus Lua

The 3.3.5 client has no RBG pane, its rated arena pane hard-requires a persistent arena team (which is exactly what MoP removed), and Eluna cannot create battleground instances. So the module splits the work:

| Layer | Role |
| --- | --- |
| C++ (`src/`) | queues, MMR matchmaking, battleground/arena creation, rating, rewards, commands, NPCs |
| Lua + AIO | in-game window: queue, stats, leaderboards |
| SQL | optional NPC templates; character tables are created on worldserver boot |

The Lua side never runs server commands. It writes a row into `rbg_request` / `arena_solo_request`, and the C++ module consumes it on the next tick. Commands and the NPCs work fine **without** Eluna/AIO.

No core file is modified: the module only uses existing script hooks and the public battleground/queue API.

## Requirements

- AzerothCore WotLK, with this module in `modules/mod-rbg-aio`
- Rebuild worldserver after adding the module
- **Optional UI:** [mod-ale](https://github.com/azerothcore/mod-ale) (or Eluna) + [AIO](https://github.com/Rochet2/AIO)

## Install

1. Place this folder in `azerothcore-wotlk/modules/mod-rbg-aio`.
2. Import the world SQL: `data/sql/world/rbg_npc.sql` and `data/sql/world/arena_solo_npc.sql`.
3. Merge `conf/rbg.conf.dist` into `worldserver.conf` (or copy it next to the other module configs).
4. Rebuild and restart worldserver.
5. Spawn the NPCs: `.npc add 190010` (RBG battlemaster) and `.npc add 190011` (solo queue master).

Character tables (`rbg_stats`, `arena_solo_stats`, request and state tables) are created automatically on startup. `data/sql/characters/` holds the same schema as a snapshot for operators who want to inspect or pre-create it.

### AIO UI (optional)

1. Install AIO: `AIO_Server` → `lua_scripts/` (next to `AIO.lua`), `AIO_Client` → `Interface/AddOns/` on every client.
2. The scripts already live in the repo `lua_scripts/` folder (`RBG_Server.lua`, `RBG_Client.lua`). If your worldserver reads another path, copy those two files next to `AIO.lua`.
3. Restart worldserver (or `.reload eluna` if you use that).
4. In-game: `/rbg` or the minimap badge. Tabs: **RBG | 1v1 | 2v2 | 3v3 | Ranking**.

The client addon is delivered by the server, so players only need the AIO client package.

## Play

**Rated Battleground** — form a raid of exactly 10 (`RatedBG.TeamSize`), all max level, same faction, no Deserter, not already queued. The leader queues with `.rbg queue`, the NPC, or the AIO button.

**1v1 / 3v3 SoloQ** — you must be **ungrouped**. Queue with `.solo 1v1` / `.solo 3v3`, the NPC, or the AIO button. The server picks opponents by MMR and, for 3v3, builds each side as 1 healer + 2 damage (healer detection uses the character's talent spec).

**2v2** — both characters must already be in the **same 2v2 arena team** (create it with a 2v2 charter like any other team). Group as a party of two; the **party leader** queues with `.solo 2v2`, the NPC, or the AIO button. Matchmaking uses the team's rating and the pair's MMR from `arena_team_member`. When the match ends, `Arena::EndBattleground` updates `arena_team` / `arena_team_member` the same way a normal rated 2v2 would. If the party or the team changes while queued, the entry is dropped.

When a match is found everyone gets the standard battleground/arena invite popup. Rating, honor and arena points are applied when the match ends.

Commands: `.rbg queue|leave|status|top` and `.solo 1v1|2v2|3v3|leave|status|top [1v1|2v2|3v3]`.

## Config highlights

See `conf/rbg.conf.dist` for the full list.

- `RatedBG.Maps = "2,3,7,9"` — WSG, AB, EotS, SotA. AV and Isle of Conquest stay out (40-player maps).
- `RatedBG.AllowSameFaction = 0` — Alliance vs Horde only for RBGs.
- `ArenaSolo.CrossFaction = 1` — lets the arena queues mix factions inside a team and match same-faction sides. These queues barely fill without it, and the core already handles cross-faction arena groups.
- `ArenaSolo.3v3.RequireHealer = 1` — enforce 1 healer per side. Off means any six players.
- `ArenaSolo.<1v1|3v3>.*` — personal rating, rewards and weekly cap for the solo brackets.
- `ArenaSolo.2v2.*` — queue window and enable flag only; rating comes from the core arena team.
- `RatedBG.EnableTitles = 0` — optional legacy PvP rank titles at rating breakpoints.

For a first test with few characters, drop `RatedBG.TeamSize` to 1 or 2 so a single GM can exercise the RBG path.

## Design notes

- **RBGs** are created as rated battlegrounds: objectives, flags and boats behave exactly as in unrated BGs, scaled to 10 per side.
- **2v2 is created rated** and the core updates `arena_team` / `arena_team_member`. **1v1 and 3v3 stay skirmish** because those queues have no persistent arena team; the module applies its own rating in `OnBattlegroundEnd` for those two only.
- Rating uses the same chance-to-win curve as arena: `1 / (1 + 10^((opponent - own) / 650))`.
- 1v1 rides on the core's 2v2 arena type with the per-side cap lowered to one, so no custom arena type or slot has to be registered.
- 1v1 / 3v3 players are queued as one-man groups; 2v2 is queued as the real party so `InviteGroupToBG` can set `Battleground::m_ArenaTeamIds`. The battleground still builds the raid group.
- Sides are balanced by combined MMR, so a strong and a weak player do not end up together against two average ones.
- Joining or leaving a group, logging out, or picking up Deserter drops you from an arena queue.
- Bracket ids are persisted in `arena_solo_stats.bracket` (1v1 = 0, 3v3 = 1, 2v2 = 2), so they are never renumbered.
- Weekly stats reset seven days after the first boot (timestamp in `rbg_state` / `arena_solo_state`).
