# Mythic+ (Battle for Azeroth)

Mazmorras míticas con reglas de **Battle for Azeroth** para AzerothCore 3.3.5a: piedra angular, timer, Enemy Forces, affijos semanales, affijo de temporada en +10, score tipo Raider.IO, vault de 3 ranuras y addon AIO (`/mplus`).

---

Mythic Keystone dungeons for AzerothCore 3.3.5a using **Battle for Azeroth** rules. No core files are patched. The module only uses script hooks, the same pattern as `mod-rbg-aio`.

## Rules (BFA)

| Key level | Affixes |
| --- | --- |
| +2 | Fortified **or** Tyrannical |
| +4 | + first weekly affix |
| +7 | + second weekly affix |
| +10 | + **seasonal** affix |

Seasonal rotation (12-week seasons): **Infested → Reaping → Beguiling → Awakened**.

Weekly pool (BFA 8.3): Bolstering, Bursting, Raging, Sanguine, Inspiring, Spiteful, Explosive, Grievous, Necrotic, Quaking, Storming, Volcanic. Teeming and Skittish are implemented and can appear if the rotation is edited.

Other BFA rules:

- +8% compounding enemy health and damage per key level
- Fortified: trash +20% HP / +30% damage. Tyrannical: bosses +40% HP / +15% damage
- Timer per dungeon. Each death **+5 seconds**
- 100% of the configured Enemy Forces target + all dungeon bosses
- In time: key **+1**. 20% time left: **+2**. 40% left: **+3**. New random dungeon
- Overtime finish: loot at that level, key stays the same level
- Leave / empty instance: key **−1**
- Completing a pool heroic (M0) with no key grants a **+2**

## Dungeons

All 16 Northrend 5-mans (heroic):

Utgarde Keep, The Nexus, Azjol-Nerub, Ahn'kahet, Drak'Tharon Keep, Gundrak, Halls of Lightning, Utgarde Pinnacle, The Violet Hold, Halls of Stone, The Oculus, Culling of Stratholme, Trial of the Champion, Forge of Souls, Pit of Saron, Halls of Reflection.

## Score and vault

Each timed/overtime run writes a score (`25 + 5×level`, with +1/+2/+3 time bonuses or an overtime penalty). Fortified and Tyrannical bests are stored per dungeon. Overall score is `1.5×higher + 0.5×lower` per dungeon, then summed.

Weekly vault (BfA-style, **one claim per week**):

- Resets **Friday 20:00** on the worldserver clock (`MythicPlus.ResetWday = 5`, `ResetHour = 20`).
- Unlock it by completing at least one key that week. Item level follows your **highest** key.
- The chest rolls **3 pieces for your class and spec**. You pick **1 of 3**.
- **NPC:** talking to the broker prints 3 clickable item links in chat — hover a link for the full tooltip (stats and effects), then click **Claim 1/2/3** in the gossip menu.
- **`/mplus`:** the Vault tab shows three item cards with icons. Hover an icon for the native tooltip; click **Claim** on that card.
- Residuum is 2× the highest key. End-of-run loot is also spec-appropriate (one piece, no choice).

End-of-run and vault loot is existing WotLK gear + emblems, scaled by key level, plus **Echoes of Domination** (Titan Residuum analog, item `190034`).

The keystone item (`190022`) shows the live dungeon and level in its name (`Mythic Keystone: Utgarde Keep (+12)` / `Piedra angular mitica: Fortaleza de Utgarde (+12)`). Completing, failing, claiming, or `.mplus setkey` destroys and recreates the item and sends a custom item-query so the bag name updates. Icons use stock 3.3.5 `ItemDisplayInfo.dbc` ids (no client patch): **31029** (Key to the Focusing Iris) for the keystone and **56465** (Abyss Crystal) for residuum.

Broker text, gossip, commands, run announces, and the AIO window/HUD follow the client locale (`enUS` or `esES`/`esMX`).

## Why C++ plus Lua

The 3.3.5 client has no Challenge Mode UI. The module splits the work:

| Layer | Role |
| --- | --- |
| C++ (`src/`) | keys, seasons, scaling, affixes, timer, loot, score, vault, commands, NPCs |
| Lua + AIO | `/mplus` window and in-dungeon HUD |
| SQL | broker, font, keystone, affix helpers; character tables are created on boot |

The Lua side never runs server commands. It writes a row into `mythic_request`, and the C++ module consumes it on the next tick. Commands and the broker work **without** Eluna/AIO.

## Install

1. Place this folder in `azerothcore-wotlk/modules/mod-mythic-plus`.
2. Import `data/sql/world/mythic_plus.sql`.
3. Merge `conf/mythic_plus.conf.dist` into `worldserver.conf`.
4. Rebuild and restart worldserver.
5. If the broker is missing: `.npc add 190020`.

Character tables are created automatically on startup. `data/sql/characters/` is a schema snapshot.

### AIO UI (optional)

1. Install AIO: `AIO_Server` → `lua_scripts/` (next to `AIO.lua`), `AIO_Client` → `Interface/AddOns/` on every client.
2. Copy **both** `MythicPlus_Client.lua` and `MythicPlus_Server.lua` next to `AIO.lua`
   if your worldserver reads another path.
3. Restart worldserver or `.reload eluna`, then client `/reload` (or relog) so AIO
   ships the new file.
4. In-game: `/mplus` or `/mythic`. The window opens and asks the server for your
   key, score and vault. Tabs: **Keystone | Affixes | Great Vault | Ranking**.
   Each button writes a chat line. The server answers with a `Mythic+` message.
   If the window stays on "Syncing", AIO is not reaching the server. The HUD
   appears during a run.

## Play

Talk to the **Keystone Broker** (Dalaran, Stormwind, Orgrimmar, Argent Tournament) or use `.mplus`.

1. Claim a +2 key (or finish any pool heroic).
2. `.mplus teleport` or the AIO button starts the same **Dungeon Finder role check** as RDF.
   The group must be **5 players** with **1 tank / 1 healer / 3 DPS**. Everyone has to confirm
   a valid role; only then is the whole party ported to the key dungeon on **heroic**.
3. Use the **Font of Power** at the entrance (or `.mplus start`) to insert the key. The same
   5-man composition is required inside the instance.
4. Kill every dungeon boss and fill Enemy Forces before the timer.
5. After a key, talk to the broker (chat links + gossip) or `/mplus` Vault tab (icons + tooltips) and pick **1 of 3**. Next claim is Friday 20:00.

Commands: `.mplus status|key|week|start|teleport|vault [1-3]|top`

GM: `.mplus setkey <dungeon 1-16> [level]` and `.mplus complete`.

## Config highlights

See `conf/mythic_plus.conf.dist`.

- `MythicPlus.MinPlayers = 5` / `MythicPlus.RequireRoles = 1` — live realms need a full 1/1/3 party. Set `RequireRoles = 0` and `MinPlayers = 1` only to test alone.
- `MythicPlus.MaxKeyLevel = 25`
- `MythicPlus.ScalePerLevel = 0.08`
- `MythicPlus.ForcesPercent = 70` — complete the key after 70% of counted trash, not a full clear.
- `MythicPlus.SeasonId = 0` — 0 reads `mythic_state`; 1–4 force Infested/Reaping/Beguiling/Awakened.
