# Rated PvP: RBG 10v10, 1v1, 2v2 and 3v3 SoloQ

Sistema de PvP puntuado para AzerothCore 3.3.5a al estilo Mists of Pandaria: campos de batalla puntuados 10c10, arenas 1c1, arenas 2c2 y arenas 3c3 en solitario. **El índice es personal en todas las colas, no por equipos de arena.** Incluye MMR, honor, puntos de arena e interfaz AIO (`/rbg`).

---

Rated PvP for AzerothCore 3.3.5a with four queues:

| Queue | Side | Entry |
| --- | --- | --- |
| Rated Battleground | 10 | premade raid of 10, leader queues |
| 1v1 Arena | 1 | solo |
| 2v2 Arena | 2 | party of 2, leader queues |
| 3v3 SoloQ | 3 | solo, sides built as 1 healer + 2 damage |

All four share the same rating model, and it is **MoP-style throughout: rating is personal, never per arena team.** Each character has a personal rating plus a hidden matchmaking rating (MMR) using the arena ELO curve, with honor and arena points on top and a weekly point cap. Nothing is written to `arena_team`, so players never create, pay for, or maintain an arena team — for 2v2 you just group with a partner and queue.

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

**2v2** — group with a partner in a normal party of two; the **party leader** queues with `.solo 2v2`, the NPC, or the AIO button. No arena team is involved: the entry is matched on the pair's average MMR, and each of the two gets their own personal rating change afterwards. If the party gains or loses a member while queued, the entry is dropped.

When a match is found everyone gets the standard battleground/arena invite popup. Rating, honor and arena points are applied when the match ends.

Commands: `.rbg queue|leave|status|top` and `.solo 1v1|2v2|3v3|leave|status|top [1v1|2v2|3v3]`.

## Config highlights

See `conf/rbg.conf.dist` for the full list.

- `RatedBG.Maps = "2,3,7,9"` — WSG, AB, EotS, SotA. AV and Isle of Conquest stay out (40-player maps).
- `RatedBG.AllowSameFaction = 0` — Alliance vs Horde only for RBGs.
- `ArenaSolo.CrossFaction = 1` — lets the arena queues mix factions inside a team and match same-faction sides. These queues barely fill without it, and the core already handles cross-faction arena groups.
- `ArenaSolo.3v3.RequireHealer = 1` — enforce 1 healer per side. Off means any six players.
- `ArenaSolo.<1v1|2v2|3v3>.*` — per-bracket rating, rewards and weekly cap.
- `RatedBG.EnableTitles = 0` — optional legacy PvP rank titles at rating breakpoints.

For a first test with few characters, drop `RatedBG.TeamSize` to 1 or 2 so a single GM can exercise the RBG path.

## Design notes

- **RBGs** are created as rated battlegrounds: objectives, flags and boats behave exactly as in unrated BGs, scaled to 10 per side.
- **The 1v1, 2v2 and 3v3 arenas are created as skirmish arenas on purpose.** The core's rated arena path (`Arena::EndBattleground`) dereferences persistent arena teams without null checks, and personal-rating queues have no such teams — so this module keeps the arena unrated for the core and applies its own rating layer instead. Gameplay (preparation aura, shadow sight, gates, scoreboard) is identical; the only cosmetic loss is the client's rating-change column, which the module reports in chat and in the AIO window.
- Rating uses the same chance-to-win curve as arena: `1 / (1 + 10^((opponent - own) / 650))`.
- 1v1 rides on the core's 2v2 arena type with the per-side cap lowered to one, so no custom arena type or slot has to be registered.
- Every player is queued as their own one-man group and then invited to the side the matcher picked; the battleground builds the team's raid group itself, so party frames and cross-heals work. Queueing a real party object would only make the core rebuild it.
- Sides are balanced by combined MMR, so a strong and a weak player do not end up together against two average ones.
- Joining or leaving a group, logging out, or picking up Deserter drops you from an arena queue.
- Bracket ids are persisted in `arena_solo_stats.bracket` (1v1 = 0, 3v3 = 1, 2v2 = 2), so they are never renumbered.
- Weekly stats reset seven days after the first boot (timestamp in `rbg_state` / `arena_solo_state`).
