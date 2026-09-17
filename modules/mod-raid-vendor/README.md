# Raid multi-vendor

NPC intendente que vende el botin de las raids de Wrath of the Lich King (nivel 80). Cada gossip es una raid; al abrirla aparecen todos sus items (paginados de 150 en 150, limite del cliente 3.3.5). Cada lista solo se abre si el personaje tiene el rating de arena configurado en el `.conf`.

---

A Dalaran quartermaster that sells WotLK level-80 raid loot. Gossip is one option per raid; opening it lists that raid's drops. Access is gated by arena personal rating (highest of 2v2 / 3v3 / 5v5 by default).

| Raid | Map | Default rating |
| --- | --- | --- |
| Naxxramas | 533 | 1000 |
| The Obsidian Sanctum | 615 | 1000 |
| The Eye of Eternity | 616 | 1000 |
| Vault of Archavon | 624 | 1200 |
| Ulduar | 603 | 1400 |
| Trial of the Crusader | 649 | 1600 |
| Icecrown Citadel | 631 | 1800 |
| The Ruby Sanctum | 724 | 2000 |
| Onyxia's Lair | 249 | 1000 |

Items are collected at worldserver startup from creature and chest loot on those maps, including 10/25 and heroic difficulty templates. Quest items and world-drop reference tables are skipped. Default minimum quality is Rare (blue).

The 3.3.5 client can only show 150 vendor items at once. If a raid has more, the gossip shows pages (`Naxxramas (1/3)`, …).

## Config

Copy `conf/raid_vendor.conf.dist` into the worldserver config directory.

```
RaidVendor.Enable = 1
RaidVendor.MinQuality = 3
RaidVendor.RatingSlot = -1
RaidVendor.IcecrownCitadel.Rating = 1800
```

`RatingSlot`: `-1` uses the highest personal rating among 2v2, 3v3 and 5v5. `0` / `1` / `2` lock the check to that slot. Set a raid's rating to `0` to leave it open.

Gold price is the item's `BuyPrice` (often 0 for drops, so many items are free — the gate is arena rating).

## Install

1. Place this folder in `modules/mod-raid-vendor`
2. Import `data/sql/world/raid_vendor.sql` into the world database
3. Rebuild worldserver
4. `.npc add 190013` if you do not want the Dalaran spawn from the SQL

No core files are modified.
