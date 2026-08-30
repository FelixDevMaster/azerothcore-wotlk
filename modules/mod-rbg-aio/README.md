# Rated PvP: RBG 10v10, 1v1, 2v2, 3v3 and 3v3 SoloQ

Sistema de PvP puntuado para AzerothCore 3.3.5a: campos de batalla puntuados 10c10, arenas 1c1 (índice personal), 2c2 y 3c3 **con equipo personal** (`arena_team` / `arena_team_member`, capitán = GUID), y SoloQ 3c3 **con equipo personal 5c5**. El SoloQ 3c3 arma comps oficiales (RMP, MLS, Jungle, God Comp, Thunder...). Incluye Desertor, MMR, honor e interfaz AIO (`/rbg`).

---

Rated PvP for AzerothCore 3.3.5a with five queues:

| Queue | Side | Entry |
| --- | --- | --- |
| Rated Battleground | 10 | premade raid of 10, leader queues |
| 1v1 Arena | 1 | solo |
| 2v2 Arena | 2 | party of 2 (personal 2v2 team each), leader queues |
| 3v3 Arena | 3 | party of 3 (personal 3v3 team each), leader queues |
| 3v3 SoloQ | 3 | solo; sides built as official 3v3 comps |

**1v1** keeps a module-owned personal rating in `arena_solo_stats`. **2v2**, **premade 3v3** and **3v3 SoloQ** each give the character a personal `arena_team` (captain = GUID): 2v2 uses the 2v2 slot, premade 3v3 uses the **3v3 slot**, SoloQ uses the **5v5 slot** so the three queues do not collide. After the match the module writes team rating to `arena_team` and personal rating to `arena_team_member` (MMR in `character_arena_stats`, slot 0 for 2v2, slot 1 for 3v3, slot 2 for SoloQ). 3v3 SoloQ is still solo-entry: the matcher prefers official compositions (RMP, Shadowplay, MLS, Jungle Cleave, God Comp, Thunder Cleave, TSG, WLS, RLS, PHD, KFC, …) and falls back to 1 healer + 2 damage. Leaving or missing the arena applies **Deserter**.

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
4. In-game: `/rbg` or the minimap badge. Tabs: **RBG | 1v1 | 2v2 | 3v3 | SoloQ 3v3 | Ranking**.

The client addon is delivered by the server, so players only need the AIO client package.

## Play

**Rated Battleground** — form a raid of exactly 10 (`RatedBG.TeamSize`), all max level, same faction, no Deserter, not already queued. The leader queues with `.rbg queue`, the NPC, or the AIO button.

**1v1** — you must be **ungrouped**. Queue with `.solo 1v1`, the NPC, or the AIO button.

**2v2** — each character gets a **personal 2v2 arena team** (captain = character GUID) on login or the first queue. Invite your partner to a normal party of two; the **party leader** queues with `.solo 2v2`, the NPC, or the AIO **2v2** tab. You do not need a shared charter.

**3v3** — same as 2v2, but a **party of three** and a personal **3v3** arena team (`arena_team` type 3, named `{Name} 3vs3`). The leader queues with `.solo 3v3`, the NPC, or the AIO **3v3** tab.

**3v3 SoloQ** — ungrouped. Each character gets a personal **5v5** arena team named `{Name} 3vs3 soloq`. The matcher builds sides as official 3v3 comps when the queued classes allow it (RMP, Shadowplay, MLS, Jungle Cleave, God Comp, Thunder Cleave, and the other named lists). Otherwise it falls back to 1 healer + 2 damage. Queue with `.solo soloq`, the NPC, or the AIO **SoloQ 3v3** tab.

Matchmaking for premade 2v2 / 3v3 uses the party's personal ratings / MMR from `arena_team_member`. When the match ends the module updates each player's `arena_team` / `arena_team_member`. If the party changes while queued, the entry is dropped.

When a match is found everyone gets the standard battleground/arena invite popup. Rating, honor and arena points are applied when the match ends.

Commands: `.rbg queue|leave|status|top` and `.solo 1v1|2v2|3v3|soloq|leave|status|top [1v1|2v2|3v3|soloq]`.

## Config highlights

See `conf/rbg.conf.dist` for the full list.

- `RatedBG.Maps = "2,3,7,9"` — WSG, AB, EotS, SotA. AV and Isle of Conquest stay out (40-player maps).
- `RatedBG.AllowSameFaction = 0` — Alliance vs Horde only for RBGs.
- `ArenaSolo.CrossFaction = 1` — lets the arena queues mix factions inside a team and match same-faction sides. These queues barely fill without it, and the core already handles cross-faction arena groups.
- `ArenaSolo.3v3.PreferComps = 1` — build official 3v3 compositions when the queue has the right classes.
- `ArenaSolo.3v3.RequireHealer = 1` — fallback pairing still needs one healer per side.
- `ArenaSolo.2v2.*` / `ArenaSolo.3v3Team.*` / `ArenaSolo.3v3.*` — queue window and honor. Rating is written to each player's personal `arena_team` / `arena_team_member`.
- `RatedBG.EnableTitles = 0` — optional legacy PvP rank titles at rating breakpoints.

For a first test with few characters, drop `RatedBG.TeamSize` to 1 or 2 so a single GM can exercise the RBG path.

## Design notes

- **RBGs** are created as rated battlegrounds: objectives, flags and boats behave exactly as in unrated BGs, scaled to 10 per side.
- **2v2, 3v3 and 3v3 SoloQ stay skirmish** because each player has a different personal team and the core only stores one arena team id per side. After the match the module calls `ArenaTeam::WonAgainst` / `MemberWon` (or the loss pair) on every personal team. **1v1** uses `arena_solo_stats`.
- Leaving an arena or ignoring the invite applies spell 26013 (Deserter) for this module's matches.
- Rating uses the same chance-to-win curve as arena: `1 / (1 + 10^((opponent - own) / 650))`.
- 1v1 rides on the core's 2v2 arena type with the per-side cap lowered to one, so no custom arena type or slot has to be registered.
- 1v1 / 3v3 SoloQ players are queued as one-man groups; 2v2 and premade 3v3 are queued as the real party so partners enter together. The battleground still builds the raid group.
- Sides are balanced by combined MMR, so a strong and a weak player do not end up together against two average ones.
- Joining or leaving a group, logging out, or picking up Deserter drops you from an arena queue.
- Bracket ids are persisted in `arena_solo_stats.bracket` (1v1 = 0, 3v3 SoloQ = 1, 2v2 = 2, premade 3v3 = 3), so they are never renumbered.
- Weekly stats reset seven days after the first boot (timestamp in `rbg_state` / `arena_solo_state`).
