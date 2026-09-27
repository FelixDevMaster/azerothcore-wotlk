-- World DB: Mythic+ broker, keystone, residuum, Font of Power, affix helpers.
-- ScriptNames must stay npc_mythic_broker / gobject_mythic_font / item_mythic_keystone.
-- Spawn the broker with: .npc add 190020

DELETE FROM `creature` WHERE `id1` = 190020 AND `guid` BETWEEN 5900200 AND 5900203;
DELETE FROM `creature_template_locale` WHERE `entry` IN
    (190020, 190023, 190024, 190025, 190026, 190027, 190028, 190029, 190030, 190031, 190032, 190033);
DELETE FROM `creature_template_model` WHERE `CreatureID` IN
    (190020, 190023, 190024, 190025, 190026, 190027, 190028, 190029, 190030, 190031, 190032, 190033);
DELETE FROM `creature_template` WHERE `entry` IN
    (190020, 190023, 190024, 190025, 190026, 190027, 190028, 190029, 190030, 190031, 190032, 190033);
DELETE FROM `gossip_menu_option` WHERE `MenuID` = 190020;
DELETE FROM `gossip_menu` WHERE `MenuID` = 190020;
DELETE FROM `npc_text_locale` WHERE `ID` = 190020;
DELETE FROM `npc_text` WHERE `ID` = 190020;
DELETE FROM `gameobject` WHERE `id` = 254620 AND `guid` BETWEEN 5900210 AND 5900225;
DELETE FROM `gameobject_template_locale` WHERE `entry` = 254620;
DELETE FROM `gameobject_template` WHERE `entry` = 254620;
DELETE FROM `item_template_locale` WHERE `ID` IN (190022, 190034);
DELETE FROM `item_template` WHERE `entry` IN (190022, 190034);

INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `BroadcastTextID0`, `lang0`, `Probability0`,
    `em0_0`, `em0_1`, `em0_2`, `em0_3`, `em0_4`, `em0_5`, `VerifiedBuild`) VALUES
(190020,
    'The Keystone Broker keeps the Titan records of this realm.$B$BBring a Mythic Keystone to the Font of Power at the dungeon entrance. Weekly affixes follow Battle for Azeroth rules: Fortified or Tyrannical at +2, a second affix at +4, a third at +7, and the seasonal affix at +10.$B$BComplete the dungeon in time to upgrade your key. The weekly vault pays out from your best runs.',
    '', 0, 0, 1, 0, 0, 0, 0, 0, 0, 12340);

INSERT INTO `npc_text_locale` (`ID`, `Locale`, `Text0_0`, `Text0_1`) VALUES
(190020, 'esES',
    'El Corredor de Piedras guarda los registros titanicos de este reino.$B$BLleva una Piedra angular mitica a la Fuente de Poder en la entrada de la mazmorra. Los affijos semanales siguen las reglas de Battle for Azeroth: Fortificado o Tiranico en +2, un segundo affijo en +4, un tercero en +7 y el affijo de temporada en +10.$B$BCompleta a tiempo para mejorar tu piedra. El cofre semanal paga tus mejores runs.',
    ''),
(190020, 'esMX',
    'El Corredor de Piedras guarda los registros titanicos de este reino.$B$BLleva una Piedra angular mitica a la Fuente de Poder en la entrada de la mazmorra. Los affijos semanales siguen las reglas de Battle for Azeroth: Fortificado o Tiranico en +2, un segundo affijo en +4, un tercero en +7 y el affijo de temporada en +10.$B$BCompleta a tiempo para mejorar tu piedra. El cofre semanal paga tus mejores runs.',
    '');

INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES
(190020, 190020);

INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
    `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
    `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`,
    `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
    `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
    `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`,
    `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`,
    `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(190020, 0, 0, 0, 0, 0, 'Keystone Broker', 'Mythic Keystones', 'Speak', 190020, 80, 80, 2, 35, 1, 1, 1.14286,
    1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 2, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1,
    0, 2, 'npc_mythic_broker', 12340),
(190023, 0, 0, 0, 0, 0, 'Explosive Orb', '', '', 0, 80, 80, 2, 16, 0, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000,
    1, 1, 1, 4, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 0.15, 1, 1, 1, 0, 0, 1, 0, 64, '', 12340),
(190024, 0, 0, 0, 0, 0, 'Sanguine Ichor', '', '', 0, 80, 80, 2, 35, 0, 1, 1, 1, 1, 0, 0, 0, 1, 2000, 2000, 1, 1,
    1, 33554438, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 128, '', 12340),
(190025, 0, 0, 0, 0, 0, 'Spiteful Shade', '', '', 0, 82, 82, 2, 16, 0, 1, 1.4, 1, 1, 40, 1, 0, 1.5, 2000, 2000,
    1, 1, 1, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 0.6, 1, 1, 1, 0, 0, 1, 0, 64, '', 12340),
(190026, 0, 0, 0, 0, 0, 'Swirling Tempest', '', '', 0, 80, 80, 2, 16, 0, 1, 1.2, 1, 1, 0, 0, 0, 1, 2000, 2000,
    1, 1, 1, 33554438, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, '', 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 128, '', 12340),
(190027, 0, 0, 0, 0, 0, 'Spawn of G''huun', '', '', 0, 80, 80, 2, 16, 0, 1, 1.2, 1, 1, 30, 0, 0, 1.2, 2000, 2000,
    1, 1, 1, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 0.35, 1, 1, 1, 0, 0, 1, 0, 64, '', 12340),
(190028, 0, 0, 0, 0, 0, 'Reaping Revenant', '', '', 0, 82, 82, 2, 16, 0, 1, 1.2, 1, 1, 30, 1, 0, 1.4, 2000, 2000,
    1, 1, 1, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 0.8, 1, 1, 1, 0, 0, 1, 0, 64, '', 12340),
(190029, 0, 0, 0, 0, 0, 'Void-Touched Emissary', 'Beguiling', '', 0, 82, 82, 2, 16, 0, 1, 1.14286, 1, 1, 25, 1,
    0, 1.5, 2000, 2000, 1, 1, 1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 2, 1, 1, 1, 0, 0, 1, 0, 64, '',
    12340),
(190030, 0, 0, 0, 0, 0, 'Enchanted Emissary', 'Beguiling', '', 0, 82, 82, 2, 16, 0, 1, 1.14286, 1, 1, 25, 1, 0,
    1.5, 2000, 2000, 1, 1, 1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 2, 1, 1, 1, 0, 0, 1, 0, 64, '',
    12340),
(190031, 0, 0, 0, 0, 0, 'Emissary of the Tides', 'Beguiling', '', 0, 82, 82, 2, 16, 0, 1, 1.14286, 1, 1, 25, 1, 0,
    1.5, 2000, 2000, 1, 1, 1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 2, 1, 1, 1, 0, 0, 1, 0, 64, '',
    12340),
(190032, 0, 0, 0, 0, 0, 'Awakened Manifestation', '', '', 0, 83, 83, 2, 16, 0, 1, 1.14286, 1, 1, 30, 1, 0, 1.6,
    2000, 2000, 1, 1, 1, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 2.5, 1, 1, 1, 0, 0, 1, 0, 64, '',
    12340),
(190033, 0, 0, 0, 0, 0, 'Volcanic Plume', '', '', 0, 80, 80, 2, 35, 0, 1, 1, 1, 1, 0, 0, 0, 1, 2000, 2000, 1, 1,
    1, 33554438, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 128, '', 12340);

INSERT INTO `creature_template_locale` (`entry`, `locale`, `Name`, `Title`, `VerifiedBuild`) VALUES
(190020, 'esES', 'Corredor de Piedras', 'Piedras angulares miticas', 12340),
(190020, 'esMX', 'Corredor de Piedras', 'Piedras angulares miticas', 12340),
(190023, 'esES', 'Orbe explosivo', '', 12340),
(190023, 'esMX', 'Orbe explosivo', '', 12340),
(190024, 'esES', 'Icor sanguino', '', 12340),
(190024, 'esMX', 'Icor sanguino', '', 12340),
(190025, 'esES', 'Sombra maliciosa', '', 12340),
(190025, 'esMX', 'Sombra maliciosa', '', 12340),
(190026, 'esES', 'Tempestad giratoria', '', 12340),
(190026, 'esMX', 'Tempestad giratoria', '', 12340),
(190027, 'esES', 'Engendro de G''huun', '', 12340),
(190027, 'esMX', 'Engendro de G''huun', '', 12340),
(190028, 'esES', 'Aparecido de la siega', '', 12340),
(190028, 'esMX', 'Aparecido de la siega', '', 12340),
(190029, 'esES', 'Emisaria tocada por el Vacio', 'Seduccion', 12340),
(190029, 'esMX', 'Emisaria tocada por el Vacio', 'Seduccion', 12340),
(190030, 'esES', 'Emisaria encantada', 'Seduccion', 12340),
(190030, 'esMX', 'Emisaria encantada', 'Seduccion', 12340),
(190031, 'esES', 'Emisaria de las Mareas', 'Seduccion', 12340),
(190031, 'esMX', 'Emisaria de las Mareas', 'Seduccion', 12340),
(190032, 'esES', 'Manifestacion despertada', '', 12340),
(190032, 'esMX', 'Manifestacion despertada', '', 12340),
(190033, 'esES', 'Pluma volcanica', '', 12340),
(190033, 'esMX', 'Pluma volcanica', '', 12340);

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`,
    `VerifiedBuild`) VALUES
(190020, 0, 26246, 1, 1, 12340),
(190023, 0, 169, 0.6, 1, 12340),
(190024, 0, 11686, 1, 1, 12340),
(190025, 0, 146, 1, 1, 12340),
(190026, 0, 1126, 1.2, 1, 12340),
(190027, 0, 13612, 0.7, 1, 12340),
(190028, 0, 16171, 1, 1, 12340),
(190029, 0, 28087, 1, 1, 12340),
(190030, 0, 24991, 1, 1, 12340),
(190031, 0, 24910, 1, 1, 12340),
(190032, 0, 28087, 1.2, 1, 12340),
(190033, 0, 169, 0.8, 1, 12340);

INSERT INTO `creature` (`guid`, `id1`, `map`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`,
    `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`,
    `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`, `VerifiedBuild`, `CreateObject`,
    `Comment`) VALUES
(5900200, 190020, 571, 1, 1, 0, 5813.69, 647.19, 647.41, 4.71, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', 12340, 0,
    'Mythic+ broker - Dalaran'),
(5900201, 190020, 0, 1, 1, 0, -8867.70, 673.40, 98.00, 3.80, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', 12340, 0,
    'Mythic+ broker - Stormwind'),
(5900202, 190020, 1, 1, 1, 0, 1632.10, -4440.50, 15.40, 1.95, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', 12340, 0,
    'Mythic+ broker - Orgrimmar'),
(5900203, 190020, 571, 1, 1, 0, 8433.50, 744.00, 547.30, 5.60, 300, 0, 0, 1, 0, 0, 0, 0, 0, '', 12340, 0,
    'Mythic+ broker - Argent Tournament');

INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`,
    `size`, `Data0`, `Data1`, `Data2`, `Data3`, `Data4`, `Data5`, `Data6`, `Data7`, `Data8`, `Data9`, `Data10`,
    `Data11`, `Data12`, `Data13`, `Data14`, `Data15`, `Data16`, `Data17`, `Data18`, `Data19`, `Data20`, `Data21`,
    `Data22`, `Data23`, `AIName`, `ScriptName`, `VerifiedBuild`) VALUES
(254620, 2, 327, 'Font of Power', '', '', '', 1.4, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, '', 'gobject_mythic_font', 0);

INSERT INTO `gameobject_template_locale` (`entry`, `locale`, `name`, `castBarCaption`, `VerifiedBuild`) VALUES
(254620, 'esES', 'Fuente de Poder', '', 12340),
(254620, 'esMX', 'Fuente de Poder', '', 12340);

INSERT INTO `gameobject` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`,
    `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`,
    `ScriptName`, `VerifiedBuild`, `Comment`) VALUES
(5900210, 254620, 574, 3, 1, 153.789, -86.548, 12.551, 0.304, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font UK'),
(5900211, 254620, 576, 3, 1, 145.870, -10.554, -16.636, 1.528, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font Nexus'),
(5900212, 254620, 601, 3, 1, 413.314, 795.968, 831.351, 5.500, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font AN'),
(5900213, 254620, 619, 3, 1, 333.351, -1109.940, 69.772, 0.553, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font AK'),
(5900214, 254620, 600, 3, 1, -517.343, -487.976, 11.010, 4.831, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font DTK'),
(5900215, 254620, 604, 3, 1, 1891.840, 832.169, 176.669, 2.109, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font GD'),
(5900216, 254620, 602, 3, 1, 1331.470, 259.619, 53.398, 4.772, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font HoL'),
(5900217, 254620, 575, 3, 1, 584.117, -327.974, 110.138, 3.122, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font UP'),
(5900218, 254620, 608, 3, 1, 1808.820, 803.930, 44.364, 6.282, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font VH'),
(5900219, 254620, 599, 3, 1, 1153.240, 806.164, 195.937, 4.715, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font HoS'),
(5900220, 254620, 578, 3, 1, 1055.930, 986.850, 361.070, 5.745, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font Oculus'),
(5900221, 254620, 595, 3, 1, 1431.100, 556.920, 36.690, 5.160, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font CoS'),
(5900222, 254620, 650, 3, 1, 805.227, 618.038, 412.393, 3.146, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font ToC'),
(5900223, 254620, 632, 3, 1, 4922.860, 2175.630, 638.734, 2.004, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font FoS'),
(5900224, 254620, 658, 3, 1, 435.743, 212.413, 528.709, 6.256, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font PoS'),
(5900225, 254620, 668, 3, 1, 5239.010, 1932.640, 707.695, 0.801, 0, 0, 0, 0, 300, 0, 1, '', 12340, 'M+ Font HoR');

-- displayid 31029 = Key to the Focusing Iris, 56465 = Abyss Crystal (stock 3.3.5 icons).
INSERT INTO `item_template`
(`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Flags`, `BuyCount`, `InventoryType`,
 `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `maxcount`, `stackable`, `bonding`,
 `description`, `Material`, `sheath`, `RequiredDisenchantSkill`, `ScriptName`, `VerifiedBuild`) VALUES
(190022, 15, 0, 'Mythic Keystone', 31029, 4, 0, 1, 0, -1, -1, 80, 80, 1, 1, 1,
    'Insert this keystone into the Font of Power at the dungeon entrance.', 4, 0, -1,
    'item_mythic_keystone', 12340),
(190034, 15, 0, 'Echoes of Domination', 56465, 3, 0, 1, 0, -1, -1, 80, 0, 0, 200, 1,
    'Titan residue gathered from Mythic Keystone dungeons. Spend it at the Keystone Broker.', 4, 0, -1,
    '', 12340);

INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`, `VerifiedBuild`) VALUES
(190022, 'esES', 'Piedra angular mitica',
    'Inserta esta piedra en la Fuente de Poder a la entrada de la mazmorra.', 12340),
(190022, 'esMX', 'Piedra angular mitica',
    'Inserta esta piedra en la Fuente de Poder a la entrada de la mazmorra.', 12340),
(190034, 'esES', 'Ecos de Dominio',
    'Residuo titanico de las mazmorras miticas. Se usa con el Corredor de Piedras.', 12340),
(190034, 'esMX', 'Ecos de Dominio',
    'Residuo titanico de las mazmorras miticas. Se usa con el Corredor de Piedras.', 12340);
