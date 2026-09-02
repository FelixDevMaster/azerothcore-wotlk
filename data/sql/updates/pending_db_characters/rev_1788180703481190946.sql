-- Kick (1) without ban (2) for ni chat-frame Warden Lua checks.

DELETE FROM `warden_action` WHERE `wardenId` IN (800, 801, 802);
INSERT INTO `warden_action` (`wardenId`, `action`) VALUES
(800, 1),
(801, 1),
(802, 1);
