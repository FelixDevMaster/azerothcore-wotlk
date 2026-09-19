# Enchanter, Gems, Glyphs

Módulo de AzerothCore 3.3.5a con tres NPCs de servicio en Dalaran (Magus Commerce Exchange):

| Entry | Nombre | Qué hace |
| --- | --- | --- |
| 190034 | Maestro encantador | Gossip por pieza. Encantamientos de Gran Maestro (nivel 78+) en el objeto equipado. |
| 190035 | Mercader de gemas | Gossip por color. Gemas épicas de Rasganorte, metas ilvl 80 y prismáticas. |
| 190036 | Escriba de glifos | Gossip por clase. Todos los glifos mayores y menores de cada clase. |

También: `.npc add 190034`, `.npc add 190035`, `.npc add 190036`.

---

AzerothCore 3.3.5a module with three service NPCs in Dalaran (Magus Commerce Exchange):

| Entry | Name | Role |
| --- | --- | --- |
| 190034 | Master Enchanter | Gossip by slot. Applies Grand Master (level 78+) Enchanting spells to the equipped item. |
| 190035 | Gem Merchant | Gossip by color. Sells Icecrown-era epic cuts, ilvl 80 meta gems, and prismatic gems. |
| 190036 | Glyph Scribe | Gossip by class. Sells every major and minor glyph. |

Or spawn anywhere with `.npc add 190034` / `190035` / `190036`.

## Enchants

The enchanter always loads the known WotLK Grand Master spells (gloves Crusher, cloak
Major Agility, weapon Berserking, etc.) and also scans Enchanting recipes with:

- `SpellItemEnchantment.requiredLevel`, `SpellLevel`, or `BaseLevel` >= `EnchantMinLevel` (default **78**), or
- Enchanting skill rank >= `EnchantMinSkill` (default **350**, Grand Master), for recipes that store level 0.

Menus are hidden when a slot has no matching recipe (Enchanting has no head/leg kits). Gossip shows
the enchant name plus its stat description. Rings and 1H weapons ask which equipped item to use.

## Gems and glyphs

Vendor lists are filled from `item_template` when the module SQL runs:

- Gems: `class = 3`, `ItemLevel >= 80`, and either `Quality = 4` (epic cuts) or meta/prismatic subclasses.
- Glyphs: `class = 16`, grouped by subclass (the class id).

Prices use each item's `BuyPrice` (many glyphs are 0 copper).

## Install

1. Place this folder in `azerothcore-wotlk/modules/mod-npc-enchanter-gems-glyphs`.
2. Rebuild worldserver so the module is linked.
3. Copy `conf/npc_enchanter_gems_glyphs.conf.dist` next to `worldserver.conf` (or merge it).
4. Start worldserver once so the updater applies `data/sql/world/npc_enchanter_gems_glyphs.sql`.

## Config

See `conf/npc_enchanter_gems_glyphs.conf.dist`. `EnchanterGemsGlyphs.EnchantCost` is copper per
enchant (`10000` = 1 gold). NPC entries must stay in sync with the SQL.
