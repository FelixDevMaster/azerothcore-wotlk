-- Kick (1) without ban (2) when ni loader Warden Lua checks fail.
-- WardenActions: 0 = log, 1 = kick, 2 = ban.

DELETE FROM `warden_action` WHERE `wardenId` IN (797, 798, 799);
INSERT INTO `warden_action` (`wardenId`, `action`) VALUES
(797, 1),
(798, 1),
(799, 1);
