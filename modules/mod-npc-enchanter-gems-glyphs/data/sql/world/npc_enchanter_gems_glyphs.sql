-- World DB: Enchanter (190013), last-patch gem vendor (190014), glyph vendor (190015).
-- ScriptNames must stay npc_gear_enchanter / npc_gear_gem_vendor / npc_gear_glyph_vendor.
-- Spawned in Dalaran (Magus Commerce Exchange). Also: .npc add 190013 / 190014 / 190015

DELETE FROM `creature` WHERE `guid` IN (5900130, 5900131, 5900132);
DELETE FROM `creature` WHERE `id1` IN (190013, 190014, 190015) AND `guid` BETWEEN 5900130 AND 5900132;
DELETE FROM `creature_template_locale` WHERE `entry` IN (190013, 190014, 190015);
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (190013, 190014, 190015);
DELETE FROM `creature_template` WHERE `entry` IN (190013, 190014, 190015);
DELETE FROM `gossip_menu` WHERE `MenuID` IN (190013, 190014, 190015);
DELETE FROM `npc_text_locale` WHERE `ID` IN (190013, 190014, 190015);
DELETE FROM `npc_text` WHERE `ID` IN (190013, 190014, 190015);
DELETE FROM `npc_vendor` WHERE `entry` = 190014 OR `entry` BETWEEN 1901400 AND 1901408
    OR `entry` BETWEEN 1901501 AND 1901511;

INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `BroadcastTextID0`, `lang0`, `Probability0`,
    `em0_0`, `em0_1`, `em0_2`, `em0_3`, `em0_4`, `em0_5`, `VerifiedBuild`) VALUES
(190013,
    'Choose a slot. I will apply any Grand Master enchant (level 78+) to the item you are wearing.',
    '', 0, 0, 1, 0, 0, 0, 0, 0, 0, 12340),
(190014,
    'Icecrown-era gems: epic cuts, Northrend meta gems, and prismatic stones such as Nightmare Tear.',
    '', 0, 0, 1, 0, 0, 0, 0, 0, 0, 12340),
(190015,
    'Major and minor glyphs, sorted by class. Start with your own class or browse another.',
    '', 0, 0, 1, 0, 0, 0, 0, 0, 0, 12340);

INSERT INTO `npc_text_locale` (`ID`, `Locale`, `Text0_0`, `Text0_1`) VALUES
(190013, 'esES',
    'Elige una pieza. Aplicare cualquier encantamiento de Gran Maestro (nivel 78+) al objeto que lleves puesto.',
    ''),
(190013, 'esMX',
    'Elige una pieza. Aplicare cualquier encantamiento de Gran Maestro (nivel 78+) al objeto que lleves puesto.',
    ''),
(190014, 'esES',
    'Gemas de la era de Corona de Hielo: tallas epicas, metas de Rasganorte y prismaticas como Lagrima de pesadilla.',
    ''),
(190014, 'esMX',
    'Gemas de la era de Corona de Hielo: tallas epicas, metas de Rasganorte y prismaticas como Lagrima de pesadilla.',
    ''),
(190015, 'esES',
    'Glifos mayores y menores, organizados por clase. Empieza por la tuya o mira otra.',
    ''),
(190015, 'esMX',
    'Glifos mayores y menores, organizados por clase. Empieza por la tuya o mira otra.',
    '');

INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES
(190013, 190013),
(190014, 190014),
(190015, 190015);

INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(190013, 0, 0, 0, 0, 0, 'Master Enchanter', 'Level 78+ Enchants', 'Speak', 190013, 80, 80, 2, 35, 1, 1, 1.14286,
    1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 2, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1,
    0, 2, 'npc_gear_enchanter', 12340),
(190014, 0, 0, 0, 0, 0, 'Gem Merchant', 'Last Patch Gems', 'Buy', 190014, 80, 80, 2, 35, 129, 1, 1.14286,
    1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 2, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1,
    0, 2, 'npc_gear_gem_vendor', 12340),
(190015, 0, 0, 0, 0, 0, 'Glyph Scribe', 'Class Glyphs', 'Speak', 190015, 80, 80, 2, 35, 129, 1, 1.14286,
    1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 2, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1,
    0, 2, 'npc_gear_glyph_vendor', 12340);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
(190013, 'esES', 'Maestro encantador', 'Encantamientos nivel 78+', 12340),
(190013, 'esMX', 'Maestro encantador', 'Encantamientos nivel 78+', 12340),
(190014, 'esES', 'Mercader de gemas', 'Gemas del ultimo parche', 12340),
(190014, 'esMX', 'Mercader de gemas', 'Gemas del ultimo parche', 12340),
(190015, 'esES', 'Escriba de glifos', 'Glifos de clase', 12340),
(190015, 'esMX', 'Escriba de glifos', 'Glifos de clase', 12340);

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(190013, 0, 25610, 1, 1, 12340),
(190014, 0, 26075, 1, 1, 12340),
(190015, 0, 25647, 1, 1, 12340);

INSERT INTO `creature` (`guid`, `id1`, `map`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`,
    `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`,
    `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`, `CreateObject`,
    `Comment`) VALUES
(5900130, 190013, 571, 1, 1, 0, 5838.5, 722.4, 641.8, 3.66519, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', 12340, 0,
    'Module enchanter - Dalaran'),
(5900131, 190014, 571, 1, 1, 0, 5876.2, 718.5, 643.2, 1.29154, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', 12340, 0,
    'Module gem vendor - Dalaran'),
(5900132, 190015, 571, 1, 1, 0, 5858.4, 704.2, 643.4, 4.15388, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', 12340, 0,
    'Module glyph vendor - Dalaran');

-- Last-patch gems: epic Northrend cuts plus ilvl 80 meta and prismatic (Nightmare Tear, etc.).
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `VerifiedBuild`)
SELECT 190014, 0, `entry`, 0, 0, 0, 12340
FROM `item_template`
WHERE `class` = 3
    AND (`Flags` & 16) = 0
    AND `ItemLevel` >= 80
    AND (`Quality` = 4 OR `subclass` IN (6, 8))
ORDER BY `name`;

INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `VerifiedBuild`)
SELECT 1901400 + `subclass`, 0, `entry`, 0, 0, 0, 12340
FROM `item_template`
WHERE `class` = 3
    AND (`Flags` & 16) = 0
    AND `ItemLevel` >= 80
    AND (`Quality` = 4 OR `subclass` IN (6, 8))
ORDER BY `name`;

-- All glyphs, one vendor list per class (item subclass matches class id).
INSERT INTO `npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `VerifiedBuild`)
SELECT 1901500 + `subclass`, 0, `entry`, 0, 0, 0, 12340
FROM `item_template`
WHERE `class` = 16
    AND (`Flags` & 16) = 0
    AND `subclass` IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 11)
ORDER BY `name`;
