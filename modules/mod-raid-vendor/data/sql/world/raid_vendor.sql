-- World DB: Raid Quartermaster (entry 190013).
-- ScriptName must stay npc_raid_vendor. Spawn with: .npc add 190013
-- Gossip text lives in npc_text so the client never shows ???.

DELETE FROM `creature` WHERE `id1` = 190013 AND `guid` = 5900130;
DELETE FROM `creature_template_locale` WHERE `entry` = 190013;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 190013;
DELETE FROM `creature_template` WHERE `entry` = 190013;
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 190013;
DELETE FROM `gossip_menu` WHERE `MenuID` = 190013;
DELETE FROM `npc_text_locale` WHERE `ID` = 190013;
DELETE FROM `npc_text` WHERE `ID` = 190013;

INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `BroadcastTextID0`, `lang0`, `Probability0`,
    `em0_0`, `em0_1`, `em0_2`, `em0_3`, `em0_4`, `em0_5`, `VerifiedBuild`) VALUES
(190013,
    'The spoils of Northrend are catalogued here.$B$BEach raid opens if your arena rating meets the requirement.',
    '', 0, 0, 1, 0, 0, 0, 0, 0, 0, 12340);

INSERT INTO `npc_text_locale` (`ID`, `Locale`, `Text0_0`, `Text0_1`) VALUES
(190013, 'esES',
    'Aqui estan los botines de Rasganorte.$B$BCada raid se abre si tu rating de arena alcanza el requisito.',
    ''),
(190013, 'esMX',
    'Aqui estan los botines de Rasganorte.$B$BCada raid se abre si tu rating de arena alcanza el requisito.',
    '');

INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES
(190013, 190013);

INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(190013, 0, 0, 0, 0, 0, 'Raid Quartermaster', 'Northrend Spoils', 'Speak', 190013, 80, 80, 2, 35, 129, 1, 1.14286,
    1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 2, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1,
    0, 16781314, 'npc_raid_vendor', 12340);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
(190013, 'esES', 'Intendente de Raids', 'Botin de Rasganorte', 12340),
(190013, 'esMX', 'Intendente de Raids', 'Botin de Rasganorte', 12340);

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(190013, 0, 29832, 1, 1, 12340);

INSERT INTO `creature` (`guid`, `id1`, `map`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`,
    `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`,
    `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`, `CreateObject`,
    `Comment`) VALUES
(5900130, 190013, 571, 1, 1, 0, 5809.55, 624.28, 647.67, 3.14159, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', 12340, 0,
    'Raid Quartermaster - Dalaran Runeweaver Square');
