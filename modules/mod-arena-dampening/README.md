# Arena Dampening

Módulo de **Dampening de arenas** para AzerothCore 3.3.5a, con el mismo ritmo que Cataclysm Classic / los servidores 4.3.4: tras 5 minutos de combate el sanado y los absorbs bajan un 1%, y ese porcentaje sube 1% cada 10 segundos.

Arena **Dampening** for AzerothCore 3.3.5a. After 5 minutes of combat, healing and absorbs are reduced by 1%, then by another 1% every 10 seconds — the Cataclysm Classic / 4.3.4-private-server ruleset.

## How it works

Retail *original* 4.3.4 did not ship the later stacking Dampening widget (that arrived in MoP 5.4 as spell `110310`). Cataclysm Classic and almost every 4.3.4 private server did back-port it. This module implements that combat rule on WotLK:

1. The match clock starts when the arena gates open (`OnArenaStart`).
2. Until the configured delay (default **300 s** for 2v2 / 3v3 / 5v5) there is **no** Cata Dampening. The WotLK static `-10%` from `spell_area` on spell **74410** is forced to `0%` so it does not pre-dampen the match.
3. At the delay, every player (and their pets) receives **74410** at `-1%`.
4. Every **10 seconds** the aura amount is increased by another `-1%`, up to `100%`.
5. `74410` is `SPELL_AURA_MOD_HEALING_DONE_PERCENT`, which the core already applies to outgoing heals. Priest Power Word: Shield and Paladin Sacred Shield already read this aura. Other school absorbs / mana shields are reduced when they are applied.

The 3.3.5a client has no Cata Dampening frame. Players see the **Arena - Dampening** aura (tooltip shows the current percent) plus an on-screen announcement when it starts and every 10%.

## Install

1. Place this folder in `azerothcore-wotlk/modules/mod-arena-dampening`.
2. Merge `conf/arena_dampening.conf.dist` into `worldserver.conf` (or copy it next to the other module configs; CMake installs the `.dist` automatically).
3. Rebuild worldserver and restart.

No SQL is required. The module reuses client spell `74410`.

## Config

| Key | Default | Meaning |
| --- | --- | --- |
| `ArenaDampening.Enable` | `1` | Master switch |
| `ArenaDampening.RatedOnly` | `0` | `1` = rated only |
| `ArenaDampening.ApplyToPets` | `1` | Apply 74410 to pets / guardians |
| `ArenaDampening.OverrideWotLKAura` | `1` | Force the WotLK `-10%` 74410 to `0%` until Cata Dampening starts |
| `ArenaDampening.Announce` | `1` | Center-screen + chat notices |
| `ArenaDampening.AnnounceEveryPercent` | `10` | Extra notice at 10 / 20 / … (`0` = start only) |
| `ArenaDampening.StartPercent` | `1` | Percent at the moment Dampening begins |
| `ArenaDampening.IncrementPercent` | `1` | Added on every tick |
| `ArenaDampening.MaxPercent` | `100` | Cap |
| `ArenaDampening.TickSeconds` | `10` | Seconds between increments |
| `ArenaDampening.StartDelay.2v2` | `300` | Combat seconds before 2v2 starts |
| `ArenaDampening.StartDelay.3v3` | `300` | Combat seconds before 3v3 starts |
| `ArenaDampening.StartDelay.5v5` | `300` | Combat seconds before 5v5 starts |

To mimic **MoP 5.4.7** instead (2v2 at 5 min, 3v3/5v5 at 10 min):

```ini
ArenaDampening.StartDelay.2v2 = 300
ArenaDampening.StartDelay.3v3 = 600
ArenaDampening.StartDelay.5v5 = 600
```

## Command

`.dampening` / `.dampening status` — shows the current percent and elapsed combat time (or seconds remaining until start). Localized for `esES` / `esMX`.
