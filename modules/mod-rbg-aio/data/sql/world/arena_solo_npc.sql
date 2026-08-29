-- World DB: solo queue arena master (entry 190011).
-- ScriptName must stay npc_arena_solo_master. Spawn with: .npc add 190011

DELETE FROM `creature_template_model` WHERE `CreatureID` = 190011;
DELETE FROM `creature_template` WHERE `entry` = 190011;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`,
`name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`,
`speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`,
`BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`,
`pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`,
`HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`,
`CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(190011, 0, 0, 0, 0, 0, 'Solo Queue Arena Master', '1v1 & 3v3 SoloQ', 'Speak', 0, 80, 80, 2, 35, 1, 1, 1.14286, 1, 1, 20, 0, 0, 1,
2000, 2000, 1, 1, 1, 2, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 2, 'npc_arena_solo_master', 12340);

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES (190011, 0, 20857, 1, 1, 12340);
